#!/usr/bin/env python3
"""
Forza "Data Out" UDP decoder + transparent relay.

Lets you SEE the live telemetry on the PC while the Nocturne board keeps
receiving it. Point Forza at this PC; the script decodes every packet and
forwards it on to the board so nothing breaks.

USAGE
  1. In Forza:  HUD/Settings -> Data Out -> ON
       IP   = <this PC's IP on the 10.77.77.0 network>
       Port = 5300
  2. Run:
       python tools/forza_udp_capture.py --forward 10.77.77.10:5300
     (omit --forward, or pass --no-forward, to just sniff)
  3. Drive. A live line prints ~5x/sec; every 3s a field-validity summary
     prints so we can confirm which packet offsets are correct for YOUR game.

Forza formats (auto-detected by length):
    232 = FM7 "Sled"          311 = FM7 "Dash"
    324 = FH4/FH5 "Dash"      331 = FM (2023) "Dash"
The Dash block layout differs by +12 bytes for Horizon (CarType placeholder).
"""
import argparse
import socket
import struct
import sys
import time


def f32(b, off):
    return struct.unpack_from("<f", b, off)[0] if off + 4 <= len(b) else 0.0


def s32(b, off):
    return struct.unpack_from("<i", b, off)[0] if off + 4 <= len(b) else 0


def u16(b, off):
    return struct.unpack_from("<H", b, off)[0] if off + 2 <= len(b) else 0


def u8(b, off):
    return b[off] if off < len(b) else 0


def s8(b, off):
    v = b[off] if off < len(b) else 0
    return v - 256 if v > 127 else v


def decode(b):
    """Decode the documented Forza V2 layout. Dash offsets shift +12 for FH."""
    n = len(b)
    is_horizon = (n >= 323 and n <= 325)  # FH4/FH5 dash
    d = n - 311 if n >= 311 else 0        # generic dash shift vs FM7
    if is_horizon:
        d = 12

    out = {
        "len": n,
        "fmt": "FH-dash" if is_horizon else ("FM-dash" if n >= 311 else "Sled"),
        # --- Sled (stable across all formats) ---
        "race_on": s32(b, 0),
        "max_rpm": f32(b, 8),
        "idle_rpm": f32(b, 12),
        "rpm": f32(b, 16),
        "accel_z": f32(b, 28),       # forward G
        "vel_z": f32(b, 40),
        # combined tire slip (good "are we sliding" signal)
        "slip_fl": f32(b, 180),
        "slip_fr": f32(b, 184),
        "slip_rl": f32(b, 188),
        "slip_rr": f32(b, 192),
    }
    if n >= 311:
        # --- Dash block (documented), with +d shift for Horizon ---
        out.update({
            "speed_ms": f32(b, 244 + d),
            "power_w": f32(b, 248 + d),
            "torque": f32(b, 252 + d),
            "tiretemp_fl": f32(b, 256 + d),
            "tiretemp_fr": f32(b, 260 + d),
            "tiretemp_rl": f32(b, 264 + d),
            "tiretemp_rr": f32(b, 268 + d),
            "boost": f32(b, 272 + d),
            "fuel": f32(b, 276 + d),          # 0..1  (firmware currently 280!)
            "dist": f32(b, 280 + d),
            "best_lap": f32(b, 284 + d),
            "last_lap": f32(b, 288 + d),
            "cur_lap": f32(b, 292 + d),
            "race_time": f32(b, 296 + d),
            "lap_no": u16(b, 300 + d),        # firmware currently 296!
            "race_pos": u8(b, 302 + d),       # firmware currently 298!
            "accel": u8(b, 303 + d),
            "brake": u8(b, 304 + d),
            "clutch": u8(b, 305 + d),
            "handbrake": u8(b, 306 + d),
            "gear": u8(b, 307 + d),           # firmware 307 / 319 -> CORRECT
            "steer": s8(b, 308 + d),
        })
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=5300)
    ap.add_argument("--forward", default="10.77.77.10:5300",
                    help="HOST:PORT of the board to relay to (default board)")
    ap.add_argument("--no-forward", action="store_true",
                    help="sniff only, do not relay to the board")
    args = ap.parse_args()

    fwd = None
    if not args.no_forward and args.forward:
        host, _, p = args.forward.partition(":")
        fwd = (host, int(p or 5300))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(0.5)
    try:
        sock.bind(("0.0.0.0", args.port))
    except OSError as e:
        print(f"Bind failed on :{args.port}: {e}")
        sys.exit(1)

    tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) if fwd else None
    print(f"Listening 0.0.0.0:{args.port}"
          + (f"  ->  relaying to {fwd[0]}:{fwd[1]}" if fwd else "  (sniff only)"))
    print("Point Forza Data Out at THIS PC. Ctrl+C to stop.\n")

    count = 0
    last_line = 0.0
    last_sum = 0.0
    seen_max = {}  # field -> max abs value, to flag "always zero" offsets
    while True:
        try:
            data, _ = sock.recvfrom(1024)
        except socket.timeout:
            continue
        except KeyboardInterrupt:
            break
        if tx:
            try:
                tx.sendto(data, fwd)
            except OSError:
                pass
        count += 1
        d = decode(data)
        if d["race_on"] <= 0:
            continue
        for k, v in d.items():
            if isinstance(v, (int, float)):
                seen_max[k] = max(seen_max.get(k, 0.0), abs(v))

        now = time.time()
        if now - last_line >= 0.2:
            last_line = now
            print(f"[{d['fmt']:7s}] G={d.get('gear','?'):>2} "
                  f"RPM={d['rpm']:5.0f}/{d['max_rpm']:5.0f} "
                  f"{d.get('speed_ms',0)*3.6:5.1f}km/h "
                  f"thr={d.get('accel',0):3d} brk={d.get('brake',0):3d} "
                  f"fuel={d.get('fuel',0):4.2f} pos={d.get('race_pos',0):2d} "
                  f"lap={d.get('lap_no',0):2d} boost={d.get('boost',0):4.1f}")

        if now - last_sum >= 3.0:
            last_sum = now
            populated = [k for k, v in seen_max.items() if v > 0.0001]
            zeros = [k for k in seen_max if k not in populated]
            print(f"  -- {count} pkts | populated: {','.join(sorted(populated))}")
            if zeros:
                print(f"  -- always-zero (suspect offset/unused): "
                      f"{','.join(sorted(zeros))}")

    sock.close()
    print("\nDone.")


if __name__ == "__main__":
    main()
