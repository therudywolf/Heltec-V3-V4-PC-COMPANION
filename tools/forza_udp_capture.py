#!/usr/bin/env python3
"""
Forza UDP telemetry capture — shows packet format and gear byte.
Run: python forza_udp_capture.py

1. In Forza: Data Out IP = your PC IP (or 127.0.0.1), Port = 5300
2. Run this script on PC
3. Start driving — packet size and gear byte will be printed
"""
import socket
import sys
import time

PORT = 5300
BUF_SIZE = 400

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(0.5)
    try:
        sock.bind(("0.0.0.0", PORT))
    except OSError as e:
        print(f"Bind failed: {e}")
        print("Is another app using port 5300? Close Forza or the Wolf device.")
        sys.exit(1)
    print(f"Listening on 0.0.0.0:{PORT}")
    print("Set Forza Data Out to THIS PC IP:5300, then drive.")
    print("Press Ctrl+C to stop.\n")

    last_print = 0.0
    count = 0
    while True:
        try:
            data, addr = sock.recvfrom(BUF_SIZE)
        except socket.timeout:
            continue
        except KeyboardInterrupt:
            break
        count += 1
        n = len(data)
        now = time.time()
        if now - last_print < 0.2 and count > 1:
            continue
        last_print = now

        gear_307 = data[307] if n > 307 else None
        gear_319 = data[319] if n > 319 else None
        # FM7: RacePos@302 Accel@303 Brake@304 Clutch@305 HandBr@306 Gear@307
        # FH4: same +12 -> Gear@319
        rp = data[302] if n > 302 else 0
        accel = data[303] if n > 303 else 0
        brake = data[304] if n > 304 else 0
        clutch = data[305] if n > 305 else 0
        hbrake = data[306] if n > 306 else 0
        print(f"len={n:3d}  Pos={rp} A={accel} B={brake} C={clutch} H={hbrake} "
              f"G@307={gear_307} G@319={gear_319}")

    sock.close()
    print("\nDone.")

if __name__ == "__main__":
    main()
