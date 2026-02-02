import asyncio
import base64
import io
import os
import sys
import traceback
import serial
import json
import time
import requests
from dotenv import load_dotenv
from winsdk.windows.media.control import GlobalSystemMediaTransportControlsSessionManager
from winsdk.windows.storage.streams import Buffer, InputStreamOptions


def log_err(msg: str, exc: BaseException | None = None, trace: bool = True):
    """Печать ошибки в stderr; при exc и trace=True — вывести traceback."""
    print(f"[!] {msg}", file=sys.stderr, flush=True)
    if exc is not None and trace:
        traceback.print_exc(file=sys.stderr)
        sys.stderr.flush()

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

load_dotenv()

# --- КОНФИГ (из .env, см. .env.example) ---
SERIAL_PORT = os.getenv("SERIAL_PORT", "COM6")
LHM_URL = os.getenv("LHM_URL", "http://localhost:8085/data.json")
TCP_HOST = os.getenv("TCP_HOST", "0.0.0.0")
TCP_PORT = int(os.getenv("TCP_PORT", "8888"))
PC_IP = os.getenv("PC_IP", "192.168.1.2")
TRANSPORT = os.getenv("TRANSPORT", "serial").lower()  # serial, tcp, both

# Карта сенсоров (ID из LibHardwareMonitor)
TARGETS = {
    "ct": "/amdcpu/0/temperature/2",       # CPU Temp
    "gt": "/gpu-nvidia/0/temperature/0",     # GPU Core
    "gth": "/gpu-nvidia/0/temperature/2",   # GPU Hot Spot
    "vt": "/lpc/it8688e/0/temperature/4", # VRM MOS
    "cpu_load": "/amdcpu/0/load/0",        # CPU Total %
    "cpu_pwr": "/amdcpu/0/power/0",        # CPU Package W
    "gpu_load": "/gpu-nvidia/0/load/0",    # GPU Core %
    "gpu_pwr": "/gpu-nvidia/0/power/0",    # GPU Package W
    "p": "/lpc/it8688e/0/fan/0",           # Pump
    "r": "/lpc/it8688e/0/fan/1",           # Rad
    "c": "/lpc/it8688e/0/fan/2",           # Case
    "gf": "/gpu-nvidia/0/fan/1",           # GPU Fan
    "gck": "/gpu-nvidia/0/clock/0",        # GPU Clock
    "ram_u": "/ram/data/0",                # RAM Used
    "ram_a": "/ram/data/1",                # RAM Avail
    "ram_pct": "/ram/load/0",              # RAM %
    "vram_u": "/gpu-nvidia/0/smalldata/1", # VRAM Used
    "vram_t": "/gpu-nvidia/0/smalldata/2", # VRAM Total
    "vcore": "/lpc/it8688e/0/voltage/0",  # Vcore
    "c1": "/amdcpu/0/clock/3", "c2": "/amdcpu/0/clock/5",
    "c3": "/amdcpu/0/clock/7", "c4": "/amdcpu/0/clock/9",
    "c5": "/amdcpu/0/clock/11", "c6": "/amdcpu/0/clock/13",
    "nvme2_t": "/nvme/2/temperature/0",   # NVMe temp (opt)
}

def clean_val(v):
    try:
        return float(str(v).replace(",", ".").split()[0])
    except (ValueError, TypeError):
        return 0.0

def get_lhm_data():
    for attempt in range(3):
        try:
            r = requests.get(LHM_URL, timeout=1)
            if r.status_code != 200:
                continue
            results = {}
            def walk(node):
                if isinstance(node, list):
                    for i in node: walk(i)
                elif isinstance(node, dict):
                    sid = node.get("SensorId")
                    if sid in TARGETS.values():
                        for k, v in TARGETS.items():
                            if sid == v: results[k] = clean_val(node["Value"])
                    if "Children" in node: walk(node["Children"])
            walk(r.json())
            return results
        except Exception as e:
            if attempt == 2:
                log_err(f"LHM timeout after 3 attempts: {e}", e)
            continue
    return {}

COVER_SIZE = 48
COVER_BYTES = (COVER_SIZE * COVER_SIZE) // 8


async def get_media():
    try:
        sessions = await GlobalSystemMediaTransportControlsSessionManager.request_async()
        s = sessions.get_current_session()
        if s:
            props = await s.try_get_media_properties_async()
            info = s.get_playback_info()
            art = (props.artist or "Unknown")[:15]
            trk = (props.title or "No Title")[:25]
            play = 1 if info.playback_status == 4 else 0
            out = {"art": art, "trk": trk, "play": play}
            if HAS_PIL and props and getattr(props, "thumbnail", None):
                try:
                    thumb_ref = props.thumbnail
                    if thumb_ref:
                        stream = await thumb_ref.open_read_async()
                        cap = min(getattr(stream, "size", 256 * 1024) or 256 * 1024, 256 * 1024)
                        buf = Buffer(cap)
                        await stream.read_async(buf, cap, InputStreamOptions.READ_AHEAD)
                        n = getattr(buf, "length", cap)
                        raw = bytes(memoryview(buf)[:n]) if hasattr(buf, "__buffer__") else b""
                        if raw:
                            img = Image.open(io.BytesIO(raw)).convert("RGB")
                            img = img.resize((COVER_SIZE, COVER_SIZE), Image.Resampling.LANCZOS)
                            img = img.convert("1")
                            pixels = img.load()
                            out_arr = bytearray(COVER_BYTES)
                            for y in range(COVER_SIZE):
                                for x in range(COVER_SIZE):
                                    if pixels[x, y]:
                                        out_arr[y * (COVER_SIZE // 8) + x // 8] |= 1 << (7 - x % 8)
                            out["cover_b64"] = base64.b64encode(out_arr).decode("ascii")
                        else:
                            out["cover_b64"] = ""
                    else:
                        out["cover_b64"] = ""
                except Exception as e:
                    log_err(f"Cover art: {e}", e, trace=False)
                    out["cover_b64"] = ""
            else:
                out["cover_b64"] = ""
            return out
    except Exception as e:
        log_err(f"Media session: {e}", e, trace=False)
    return {"art": "", "trk": "Stopped", "play": 0, "cover_b64": ""}

def build_payload(hw, media, hw_ok, include_cover=True):
    """Сборка JSON-пакета. При пустом hw используются нули/кэш; hw_ok=0/1. cover_b64 только при include_cover."""
    payload = {
        "hw_ok": 1 if hw_ok else 0,
        "ct": int(hw.get("ct", 0)),
        "gt": int(hw.get("gt", 0)),
        "gth": int(hw.get("gth", 0)),
        "vt": int(hw.get("vt", 0)),
        "cpu_load": int(hw.get("cpu_load", 0)),
        "cpu_pwr": round(hw.get("cpu_pwr", 0), 1),
        "gpu_load": int(hw.get("gpu_load", 0)),
        "gpu_pwr": round(hw.get("gpu_pwr", 0), 1),
        "p": int(hw.get("p", 0)),
        "r": int(hw.get("r", 0)),
        "c": int(hw.get("c", 0)),
        "gf": int(hw.get("gf", 0)),
        "gck": int(hw.get("gck", 0)),
        "c1": int(hw.get("c1", 0)), "c2": int(hw.get("c2", 0)),
        "c3": int(hw.get("c3", 0)), "c4": int(hw.get("c4", 0)),
        "c5": int(hw.get("c5", 0)), "c6": int(hw.get("c6", 0)),
        "ru": round(hw.get("ram_u", 0), 1),
        "rt": round(hw.get("ram_u", 0) + hw.get("ram_a", 0), 1),
        "rp": int(hw.get("ram_pct", 0)),
        "vu": round(hw.get("vram_u", 0) / 1024, 1),
        "vt_tot": round(hw.get("vram_t", 0) / 1024, 1),
        "vcore": round(hw.get("vcore", 0), 2),
        "nvme2_t": int(hw.get("nvme2_t", 0)),
        "art": media.get("art", ""),
        "trk": media.get("trk", ""),
        "play": media.get("play", 0),
    }
    if include_cover and media.get("cover_b64"):
        payload["cover_b64"] = media["cover_b64"]
    return payload


async def run_serial(ser, cache, send_fn):
    """Один тик: опрос LHM + media, всегда отправка (кэш при пустом hw)."""
    hw = get_lhm_data()
    media = await get_media()
    hw_ok = bool(hw)
    if hw_ok:
        cache.update(hw)
        cache.update(media)
    else:
        hw = dict(cache)
        if not hw:
            hw = {"ct": 0, "gt": 0, "gth": 0, "vt": 0, "cpu_load": 0, "cpu_pwr": 0,
                  "gpu_load": 0, "gpu_pwr": 0, "p": 0, "r": 0, "c": 0, "gf": 0, "gck": 0,
                  "c1": 0, "c2": 0, "c3": 0, "c4": 0, "c5": 0, "c6": 0,
                  "ram_u": 0, "ram_a": 0, "ram_pct": 0, "vram_u": 0, "vram_t": 0,
                  "vcore": 0, "nvme2_t": 0}
        media = {k: cache.get(k, media.get(k)) for k in ("art", "trk", "play")}
    payload = build_payload(hw, media, hw_ok)
    data_str = json.dumps(payload) + "\n"
    send_fn(data_str.encode("utf-8"))


# Клиенты TCP (для рассылки одного и того же JSON)
tcp_clients = []


async def tcp_handler(reader, writer):
    addr = writer.get_extra_info("peername")
    tcp_clients.append(writer)
    print(f"[TCP] Подключён {addr}")
    try:
        while await reader.read(100):
            pass
    except (ConnectionResetError, BrokenPipeError, asyncio.IncompleteReadError):
        pass
    finally:
        if writer in tcp_clients:
            tcp_clients.remove(writer)
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass
        print(f"[TCP] Отключён {addr}")


async def run():
    ser = None
    if TRANSPORT in ("serial", "both"):
        try:
            ser = serial.Serial(SERIAL_PORT, 115200, timeout=0.05)
            print(f"[*] Serial: {SERIAL_PORT}")
        except Exception as e:
            log_err(f"Serial open {SERIAL_PORT}: {e}", e)
            if TRANSPORT == "serial":
                return

    server = None
    if TRANSPORT in ("tcp", "both"):
        bind_host = TCP_HOST
        try:
            server = await asyncio.start_server(tcp_handler, bind_host, TCP_PORT)
            print(f"[*] TCP: {bind_host}:{TCP_PORT}")
        except OSError as e:
            if bind_host != "0.0.0.0":
                try:
                    server = await asyncio.start_server(tcp_handler, "0.0.0.0", TCP_PORT)
                    print(f"[*] TCP: 0.0.0.0:{TCP_PORT} (fallback, bind to {bind_host} failed: {e})")
                except Exception as e2:
                    log_err(f"TCP server: {e2}", e2)
                    if TRANSPORT == "tcp":
                        if ser:
                            ser.close()
                        return
            else:
                log_err(f"TCP server {bind_host}:{TCP_PORT}: {e}", e)
                if TRANSPORT == "tcp":
                    if ser:
                        ser.close()
                    return
        except Exception as e:
            log_err(f"TCP server {bind_host}:{TCP_PORT}: {e}", e)
            if TRANSPORT == "tcp":
                if ser:
                    ser.close()
                return

    if TRANSPORT == "both" and ser is None and server is None:
        log_err("Neither Serial nor TCP available. Exit.")
        if ser:
            ser.close()
        return

    cache = {}
    interval = 0.8
    last_track_key = ""
    last_cover_sent = 0.0
    COVER_INTERVAL = 8.0

    def send_serial(data: bytes):
        if ser and ser.is_open:
            try:
                ser.write(data)
            except Exception as e:
                log_err(f"Serial write: {e}", e)

    async def send_all(data: bytes):
        send_serial(data)
        dead = []
        for w in tcp_clients:
            try:
                w.write(data)
                await w.drain()
            except Exception:
                dead.append(w)
        for w in dead:
            if w in tcp_clients:
                tcp_clients.remove(w)
            try:
                w.close()
                await w.wait_closed()
            except Exception as e:
                log_err(f"TCP close: {e}", trace=False)

    while True:
        hw = get_lhm_data()
        media = await get_media()
        hw_ok = bool(hw)
        if hw_ok:
            cache.update(hw)
            cache.update(media)
        else:
            hw = dict(cache)
            if not hw:
                hw = {"ct": 0, "gt": 0, "gth": 0, "vt": 0, "cpu_load": 0, "cpu_pwr": 0,
                  "gpu_load": 0, "gpu_pwr": 0, "p": 0, "r": 0, "c": 0, "gf": 0, "gck": 0,
                  "c1": 0, "c2": 0, "c3": 0, "c4": 0, "c5": 0, "c6": 0,
                  "ram_u": 0, "ram_a": 0, "ram_pct": 0, "vram_u": 0, "vram_t": 0,
                  "vcore": 0, "nvme2_t": 0}
            media = {k: cache.get(k, media.get(k)) for k in ("art", "trk", "play", "cover_b64")}
        track_key = (media.get("art", "") or "") + "|" + (media.get("trk", "") or "")
        now_sec = time.time()
        include_cover = (track_key != last_track_key) or (now_sec - last_cover_sent >= COVER_INTERVAL)
        if include_cover and media.get("cover_b64"):
            last_cover_sent = now_sec
        last_track_key = track_key
        payload = build_payload(hw, media, hw_ok, include_cover=include_cover)
        data_str = json.dumps(payload) + "\n"
        data = data_str.encode("utf-8")
        await send_all(data)
        await asyncio.sleep(interval)

if __name__ == "__main__":
    print("[INIT] Запуск мониторинга...", flush=True)
    try:
        asyncio.run(run())
    except Exception as e:
        log_err(f"Fatal: {e}", e)
        if getattr(sys, "frozen", False):
            input("Нажми Enter для выхода...")
        sys.exit(1)