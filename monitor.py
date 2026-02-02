import asyncio
import base64
import io
import os
import sys
import threading
import traceback
import json
import time
import requests
import psutil
from dotenv import load_dotenv
from winsdk.windows.media.control import GlobalSystemMediaTransportControlsSessionManager
from winsdk.windows.storage.streams import Buffer, InputStreamOptions

try:
    import pystray
    from pystray import Menu, MenuItem
    HAS_PYSTRAY = True
except ImportError:
    HAS_PYSTRAY = False


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

# Флаг остановки по запросу из трея (пункт «Выход»)
stop_requested = False

# --- КОНФИГ (из .env, см. .env.example) ---
LHM_URL = os.getenv("LHM_URL", "http://localhost:8085/data.json")
TCP_HOST = os.getenv("TCP_HOST", "0.0.0.0")
TCP_PORT = int(os.getenv("TCP_PORT", "8888"))
PC_IP = os.getenv("PC_IP", "192.168.1.2")

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
    "nvme2_t": "/nvme/2/temperature/0",    # NVMe temp (opt)
    "chipset_t": "/lpc/it8688e/0/temperature/0",  # Chipset temp
}

def clean_val(v):
    if v is None:
        return 0.0
    try:
        s = str(v).strip().replace(",", ".")
        if not s:
            return 0.0
        num_part = s.split()[0] if s.split() else s
        return float(num_part)
    except (ValueError, TypeError):
        return 0.0

def get_lhm_data():
    for attempt in range(3):
        try:
            r = requests.get(LHM_URL, timeout=2)
            if r.status_code != 200:
                continue
            data = r.json()
            results = {}
            targets_set = set(TARGETS.values())

            def walk(node):
                if isinstance(node, list):
                    for i in node:
                        walk(i)
                elif isinstance(node, dict):
                    sid = node.get("SensorId") or node.get("SensorID")
                    if sid and sid in targets_set:
                        raw = node.get("Value") or node.get("RawValue") or ""
                        for k, v in TARGETS.items():
                            if v == sid:
                                results[k] = clean_val(raw)
                                break
                    if "Children" in node:
                        walk(node["Children"])
                    return
                return

            if isinstance(data, list):
                walk(data)
            else:
                walk(data)
            return results
        except Exception as e:
            if attempt == 2:
                log_err(f"LHM after 3 attempts: {e}", e, trace=False)
            continue
    return {}

COVER_SIZE = 48
COVER_BYTES = (COVER_SIZE * COVER_SIZE) // 8

# Погода (Open-Meteo). Координаты из .env или Москва по умолчанию
WEATHER_LAT = os.getenv("WEATHER_LAT", "55.7558")
WEATHER_LON = os.getenv("WEATHER_LON", "37.6173")
WEATHER_URL = (
    f"https://api.open-meteo.com/v1/forecast?latitude={WEATHER_LAT}&longitude={WEATHER_LON}"
    "&current=temperature_2m,weather_code"
    "&daily=temperature_2m_max,temperature_2m_min,weather_code"
    "&timezone=auto&forecast_days=8"
)
WEATHER_TIMEOUT = 10
WEATHER_UPDATE_INTERVAL = 10 * 60  # 10 минут (фоновая задача)
weather_cache = {
    "temp": 0, "desc": "", "icon": 0,
    "day_high": 0, "day_low": 0, "day_code": 0,
    "week_high": [], "week_low": [], "week_code": [],
}
last_weather_update = 0.0
_last_weather_log_time = 0.0
_weather_first_ok = False  # лог при первой успешной загрузке

# Сеть и диски (для расчёта скорости)
last_net_bytes = {"sent": 0, "recv": 0, "time": 0.0}
last_disk_bytes = {"read": 0, "write": 0, "time": 0.0}

# WMO weather_code -> short desc (Open-Meteo)
def _weather_desc_from_code(code):
    if code == 0: return "Clear"
    if 1 <= code <= 3: return "Cloudy"
    if 45 <= code <= 48: return "Fog"
    if 51 <= code <= 67: return "Rain"
    if 71 <= code <= 77: return "Snow"
    if 80 <= code <= 82: return "Showers"
    if 85 <= code <= 86: return "Snow"
    if 95 <= code <= 99: return "Storm"
    return "Cloudy"


def get_weather():
    """Синхронный запрос погоды (Open-Meteo): текущая + дневной и недельный прогноз."""
    global weather_cache, last_weather_update, _last_weather_log_time, _weather_first_ok
    now = time.time()
    for attempt in range(2):
        try:
            r = requests.get(WEATHER_URL, timeout=WEATHER_TIMEOUT)
            if r.status_code == 200:
                data = r.json()
                cur = data.get("current", {})
                temp = int(cur.get("temperature_2m", 0))
                code = int(cur.get("weather_code", 0))
                desc = _weather_desc_from_code(code)[:20]
                daily = data.get("daily", {})
                dmax = daily.get("temperature_2m_max") or []
                dmin = daily.get("temperature_2m_min") or []
                dcode = daily.get("weather_code") or []
                day_high = int(dmax[0]) if len(dmax) > 0 else temp
                day_low = int(dmin[0]) if len(dmin) > 0 else temp
                day_code = int(dcode[0]) if len(dcode) > 0 else code
                week_high = [int(x) for x in dmax[:7]] if isinstance(dmax, list) else []
                week_low = [int(x) for x in dmin[:7]] if isinstance(dmin, list) else []
                week_code = [int(x) for x in dcode[:7]] if isinstance(dcode, list) else []
                weather_cache = {
                    "temp": temp, "desc": desc, "icon": code,
                    "day_high": day_high, "day_low": day_low, "day_code": day_code,
                    "week_high": week_high, "week_low": week_low, "week_code": week_code,
                }
                last_weather_update = now
                if not _weather_first_ok:
                    _weather_first_ok = True
                    print(f"[Weather] OK: {temp} C, {desc}, day {day_low}-{day_high}", flush=True)
                return weather_cache
        except Exception as e:
            if attempt == 1 and (now - _last_weather_log_time) > 60:
                log_err(f"Weather fetch failed after retry: {e}", e, trace=False)
                _last_weather_log_time = now
            continue
    return weather_cache

def get_core_loads():
    """Per-core CPU load 0–100% (до 6 ядер)."""
    try:
        per = psutil.cpu_percent(percpu=True, interval=None)
        if not per:
            return [0] * 6
        loads = [int(min(100, max(0, round(x)))) for x in per[:6]]
        while len(loads) < 6:
            loads.append(0)
        return loads[:6]
    except Exception as e:
        log_err(f"Core loads: {e}", e, trace=False)
        return [0] * 6


def get_top_processes():
    """Получить топ-3 процесса по CPU"""
    try:
        procs = []
        for p in psutil.process_iter(['name', 'cpu_percent']):
            try:
                info = p.info
                cpu = info.get('cpu_percent', 0.0)
                if cpu and cpu > 0:
                    name = info.get('name', '')[:12]
                    procs.append({"n": name, "c": int(cpu)})
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                pass
        procs.sort(key=lambda x: x["c"], reverse=True)
        return procs[:3]
    except Exception as e:
        log_err(f"Top processes: {e}", e, trace=False)
        return []

def get_net_speed():
    """Получить скорость сети (upload/download KB/s)"""
    global last_net_bytes
    try:
        counters = psutil.net_io_counters()
        now = time.time()
        sent = counters.bytes_sent
        recv = counters.bytes_recv
        
        if last_net_bytes["time"] > 0:
            delta_time = now - last_net_bytes["time"]
            if delta_time > 0:
                up = int((sent - last_net_bytes["sent"]) / delta_time / 1024)
                down = int((recv - last_net_bytes["recv"]) / delta_time / 1024)
                last_net_bytes = {"sent": sent, "recv": recv, "time": now}
                return {"up": up, "down": down}
        
        last_net_bytes = {"sent": sent, "recv": recv, "time": now}
        return {"up": 0, "down": 0}
    except Exception as e:
        log_err(f"Net speed: {e}", e, trace=False)
        return {"up": 0, "down": 0}

def get_disk_speed():
    """Получить скорость дисков (read/write MB/s)"""
    global last_disk_bytes
    try:
        counters = psutil.disk_io_counters()
        if not counters:
            return {"read": 0, "write": 0}
        
        now = time.time()
        read = counters.read_bytes
        write = counters.write_bytes
        
        if last_disk_bytes["time"] > 0:
            delta_time = now - last_disk_bytes["time"]
            if delta_time > 0:
                read_mb = int((read - last_disk_bytes["read"]) / delta_time / 1024 / 1024)
                write_mb = int((write - last_disk_bytes["write"]) / delta_time / 1024 / 1024)
                last_disk_bytes = {"read": read, "write": write, "time": now}
                return {"read": read_mb, "write": write_mb}
        
        last_disk_bytes = {"read": read, "write": write, "time": now}
        return {"read": 0, "write": 0}
    except Exception as e:
        log_err(f"Disk speed: {e}", e, trace=False)
        return {"read": 0, "write": 0}

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
            has_thumb = bool(HAS_PIL and props and getattr(props, "thumbnail", None))
            if has_thumb:
                try:
                    thumb_ref = props.thumbnail
                    if thumb_ref:
                        stream = await thumb_ref.open_read_async()
                        cap = min(getattr(stream, "size", 256 * 1024) or 256 * 1024, 256 * 1024)
                        buf = Buffer(cap)
                        await stream.read_async(buf, cap, InputStreamOptions.READ_AHEAD)
                        n = getattr(buf, "length", cap) or cap
                        raw = b""
                        try:
                            raw = bytes(memoryview(buf)[:n])
                        except Exception:
                            try:
                                raw = bytes(list(buf)[:n])
                            except Exception:
                                pass
                        if raw:
                            img = Image.open(io.BytesIO(raw)).convert("RGB")
                            img = img.resize((COVER_SIZE, COVER_SIZE), Image.Resampling.LANCZOS)
                            img = img.convert("1")  # 0=black, 255=white; bit 7=left pixel for U8g2
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

# Поля по экранам для полного пакета (0=Main .. 9=Network); общие (алерты) всегда
_SCREEN_KEYS = {
    0: ("gth", "vt", "ru", "rt", "rp", "vu", "vt_tot"),
    1: ("cl1", "cl2", "cl3", "cl4", "cl5", "cl6"),
    2: ("gck", "gf", "gth", "vu", "vt_tot", "gpu_pwr"),
    3: ("ru", "rt", "rp", "vu", "vt_tot"),
    4: ("art", "trk", "cover_b64"),
    5: (),  # Equalizer: play уже в общих
    6: ("vcore", "cpu_pwr", "gpu_pwr", "nvme2_t"),
    7: ("p", "r", "c", "gf"),  # Fans: Pump, Rad, Case, GPU
    8: ("wt", "wd", "wi"),
    9: ("tp",),
    10: ("nu", "nd", "dr", "dw"),
    11: (),  # Wolf Game: только heartbeat
}
_COMMON_KEYS = ("hw_ok", "ct", "gt", "cpu_load", "gpu_load", "play", "wt", "wd", "wi")


def build_payload(hw, media, hw_ok, include_cover=True, weather=None, top_procs=None, net=None, disk=None, core_loads=None, screen=None):
    """Сборка JSON. screen=N — только поля для экрана N + общие (алерты). Иначе полный пакет."""
    if weather is None:
        weather = {"temp": 0, "desc": "", "icon": 0}
    if top_procs is None:
        top_procs = []
    if net is None:
        net = {"up": 0, "down": 0}
    if disk is None:
        disk = {"read": 0, "write": 0}
    if core_loads is None:
        core_loads = [0] * 6

    cl = core_loads[:6]
    while len(cl) < 6:
        cl.append(0)

    full = {
        "hw_ok": 1 if hw_ok else 0,
        "ct": int(hw.get("ct", 0)),
        "gt": int(hw.get("gt", 0)),
        "gth": int(hw.get("gth", 0)),
        "vt": int(hw.get("vt", 0)),
        "cpu_load": int(hw.get("cpu_load", 0)),
        "cpu_pwr": round(hw.get("cpu_pwr", 0), 1),
        "gpu_load": int(hw.get("gpu_load", 0)),
        "gpu_pwr": round(hw.get("gpu_pwr", 0), 1),
        "p": int(hw.get("p", 0)), "r": int(hw.get("r", 0)), "c": int(hw.get("c", 0)),
        "gf": int(hw.get("gf", 0)), "gck": int(hw.get("gck", 0)),
        "cl1": cl[0], "cl2": cl[1], "cl3": cl[2], "cl4": cl[3], "cl5": cl[4], "cl6": cl[5],
        "c1": int(hw.get("c1", 0)), "c2": int(hw.get("c2", 0)), "c3": int(hw.get("c3", 0)),
        "c4": int(hw.get("c4", 0)), "c5": int(hw.get("c5", 0)), "c6": int(hw.get("c6", 0)),
        "ru": round(hw.get("ram_u", 0), 1),
        "rt": round(hw.get("ram_u", 0) + hw.get("ram_a", 0), 1),
        "rp": int(hw.get("ram_pct", 0)),
        "vu": round(hw.get("vram_u", 0) / 1024, 1),
        "vt_tot": round(hw.get("vram_t", 0) / 1024, 1),
        "vcore": round(hw.get("vcore", 0), 2),
        "nvme2_t": int(hw.get("nvme2_t", 0)),
        "chipset_t": int(hw.get("chipset_t", 0)),
        "nu": net.get("up", 0), "nd": net.get("down", 0),
        "dr": disk.get("read", 0), "dw": disk.get("write", 0),
        "wt": weather.get("temp", 0), "wd": weather.get("desc", ""), "wi": weather.get("icon", 0),
        "wday_high": weather.get("day_high", 0), "wday_low": weather.get("day_low", 0),
        "wday_code": weather.get("day_code", 0),
        "week_high": weather.get("week_high", [])[:7],
        "week_low": weather.get("week_low", [])[:7],
        "week_code": weather.get("week_code", [])[:7],
        "tp": top_procs,
        "art": media.get("art", ""),
        "trk": media.get("trk", ""),
        "play": media.get("play", 0),
    }

    # Всегда отправляем полный пакет, чтобы на устройстве не было пустых экранов
    payload = dict(full)
    if (screen is not None and screen == 4) or include_cover:
        if media.get("cover_b64"):
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


# Клиенты TCP: writer -> last screen (0–11)
tcp_clients = []
client_screens = {}  # writer -> int
client_buffers = {}  # writer -> str


def _parse_screen_line(line: str) -> int | None:
    s = line.strip()
    if s.startswith("screen:") and len(s) < 20:
        try:
            n = int(s.split(":")[1].strip())
            if 0 <= n <= 11:
                return n
        except (ValueError, IndexError):
            pass
    return None


async def tcp_handler(reader, writer):
    addr = writer.get_extra_info("peername")
    tcp_clients.append(writer)
    client_screens[writer] = 0
    client_buffers[writer] = ""
    print(f"[TCP] Подключён {addr}")
    try:
        while True:
            data = await reader.read(100)
            if not data:
                break
            client_buffers[writer] += data.decode("utf-8", errors="ignore")
            while "\n" in client_buffers[writer]:
                line, client_buffers[writer] = client_buffers[writer].split("\n", 1)
                n = _parse_screen_line(line)
                if n is not None:
                    client_screens[writer] = n
    except (ConnectionResetError, BrokenPipeError, asyncio.IncompleteReadError):
        pass
    finally:
        if writer in tcp_clients:
            tcp_clients.remove(writer)
        client_screens.pop(writer, None)
        client_buffers.pop(writer, None)
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass
        print(f"[TCP] Отключён {addr}")


async def run():
    bind_host = TCP_HOST
    server = None
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
                return
        else:
            log_err(f"TCP server {bind_host}:{TCP_PORT}: {e}", e)
            return
    except Exception as e:
        log_err(f"TCP server {bind_host}:{TCP_PORT}: {e}", e)
        return

    if server is None:
        log_err("TCP server failed to start. Exit.")
        return

    async def weather_loop():
        """Фоновая задача: обновление погоды каждые 10 мин, не блокирует основной цикл."""
        await asyncio.sleep(2)  # дать серверу стартовать
        while True:
            await asyncio.to_thread(get_weather)
            await asyncio.sleep(WEATHER_UPDATE_INTERVAL)

    asyncio.create_task(weather_loop())

    cache = {}
    last_track_key = ""
    last_cover_sent = 0.0
    COVER_INTERVAL = 8.0
    HEARTBEAT_INTERVAL = 0.3
    FULL_INTERVAL = 0.8
    MEDIA_FULL_INTERVAL = 0.28  # чаще полный пакет для экранов Player/EQ
    last_heartbeat = 0.0
    last_full = 0.0

    # Первый тик: заполнить cache для heartbeat
    hw0 = get_lhm_data()
    if hw0:
        cache.update(hw0)
    media0 = await get_media()
    cache.update(media0)
    await asyncio.to_thread(get_weather)  # первая загрузка погоды в фоне

    async def send_all(data: bytes):
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
            client_screens.pop(w, None)
            client_buffers.pop(w, None)
            try:
                w.close()
                await w.wait_closed()
            except Exception as e:
                log_err(f"TCP close: {e}", trace=False)

    while True:
        if stop_requested:
            break
        now_sec = time.time()

        # Heartbeat каждые 0.3 s — алерты + медиа (art, trk, play) для быстрого обновления плеера/эквалайзера
        if now_sec - last_heartbeat >= HEARTBEAT_INTERVAL:
            last_heartbeat = now_sec
            hw_c = cache if cache else {"ct": 0, "gt": 0, "cpu_load": 0, "gpu_load": 0}
            media_c = cache if cache else {}
            hb = {
                "hw_ok": 1 if cache else 0,
                "ct": int(hw_c.get("ct", 0)),
                "gt": int(hw_c.get("gt", 0)),
                "cpu_load": int(hw_c.get("cpu_load", 0)),
                "gpu_load": int(hw_c.get("gpu_load", 0)),
                "play": media_c.get("play", 0),
                "art": (media_c.get("art") or "")[:18],
                "trk": (media_c.get("trk") or "")[:28],
            }
            await send_all((json.dumps(hb) + "\n").encode("utf-8"))

        any_media_screen = any(client_screens.get(w, 0) in (4, 5) for w in tcp_clients)
        full_interval = MEDIA_FULL_INTERVAL if any_media_screen else FULL_INTERVAL
        if now_sec - last_full >= full_interval:
            last_full = now_sec
            hw = get_lhm_data()
            media = await get_media()
            weather = weather_cache
            top_procs = get_top_processes()
            core_loads = get_core_loads()
            net = get_net_speed()
            disk = get_disk_speed()
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
                          "vcore": 0, "nvme2_t": 0, "chipset_t": 0}
                media = {k: cache.get(k, media.get(k)) for k in ("art", "trk", "play", "cover_b64")}
            track_key = (media.get("art", "") or "") + "|" + (media.get("trk", "") or "")
            include_cover = (track_key != last_track_key) or (now_sec - last_cover_sent >= COVER_INTERVAL)
            if track_key != last_track_key and last_track_key:
                cov_len = len(media.get("cover_b64") or "")
                print(f"[Cover] track change, cover_b64 len={cov_len}" + (" (install PIL for cover)" if not HAS_PIL and not cov_len else ""), flush=True)
            if include_cover and media.get("cover_b64"):
                last_cover_sent = now_sec
            last_track_key = track_key

            dead = []
            for w in list(tcp_clients):
                screen = client_screens.get(w, 0)
                payload = build_payload(hw, media, hw_ok, include_cover=include_cover, weather=weather,
                                        top_procs=top_procs, net=net, disk=disk, core_loads=core_loads, screen=screen)
                try:
                    w.write((json.dumps(payload) + "\n").encode("utf-8"))
                    await w.drain()
                except Exception:
                    dead.append(w)
            for w in dead:
                if w in tcp_clients:
                    tcp_clients.remove(w)
                client_screens.pop(w, None)
                client_buffers.pop(w, None)
                try:
                    w.close()
                    await w.wait_closed()
                except Exception:
                    pass

        await asyncio.sleep(0.05)

    # Выход по запросу из трея (Выход): закрыть сервер и отменить фоновые задачи
    tasks = [t for t in asyncio.all_tasks() if t is not asyncio.current_task()]
    for t in tasks:
        t.cancel()
    if tasks:
        await asyncio.gather(*tasks, return_exceptions=True)
    if server:
        server.close()
        await server.wait_closed()
    return


def _make_tray_icon_image():
    """Создать изображение иконки 32x32 для трея (PIL)."""
    if not HAS_PIL:
        return None
    try:
        from PIL import ImageDraw
        img = Image.new("RGBA", (32, 32), (30, 30, 40, 255))
        draw = ImageDraw.Draw(img)
        draw.ellipse((4, 4, 28, 28), fill=(80, 140, 220), outline=(200, 200, 220))
        return img
    except Exception:
        return None


def _run_tray_thread(icon):
    """Запуск иконки трея в текущем потоке (вызывается из потока)."""
    try:
        icon.run()
    except Exception as e:
        log_err(f"Tray icon: {e}", e, trace=False)


def _start_tray():
    """Запустить иконку в трее в отдельном потоке. Возвращает объект Icon или None."""
    global stop_requested
    if sys.platform != "win32" or not HAS_PYSTRAY:
        return None
    img = _make_tray_icon_image()
    if img is None and HAS_PIL:
        img = Image.new("RGBA", (32, 32), (80, 80, 80, 255))
    if img is None:
        return None

    def on_quit(icon, item):
        global stop_requested
        stop_requested = True
        icon.stop()

    menu = Menu(MenuItem("Выход", on_quit, default=True))
    icon = pystray.Icon("heltec", img, "Heltec Monitor", menu=menu)
    t = threading.Thread(target=_run_tray_thread, args=(icon,), daemon=True)
    t.start()
    return icon


def _is_in_autostart() -> bool:
    """Проверить, добавлен ли уже в автозапуск Windows (реестр Run)."""
    if sys.platform != "win32":
        return False
    try:
        import winreg
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0,
            winreg.KEY_READ,
        )
        try:
            winreg.QueryValueEx(key, "HeltecMonitor")
            return True
        except OSError:
            return False
        finally:
            winreg.CloseKey(key)
    except Exception:
        return False


def _add_to_autostart() -> bool:
    """Добавить в автозапуск Windows (реестр Run)."""
    if sys.platform != "win32":
        return False
    try:
        import winreg
        exe_path = sys.executable
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0,
            winreg.KEY_SET_VALUE,
        )
        winreg.SetValueEx(key, "HeltecMonitor", 0, winreg.REG_SZ, exe_path)
        winreg.CloseKey(key)
        return True
    except Exception as e:
        log_err(f"Autostart add failed: {e}", e, trace=False)
        return False


if __name__ == "__main__":
    print("[INIT] Запуск мониторинга...", flush=True)
    # Автозапуск: только при запуске не из EXE (есть консоль); в EXE без консоли не спрашиваем
    if sys.platform == "win32" and not getattr(sys, "frozen", False):
        if not _is_in_autostart():
            try:
                print("Поместить сервер в автозапуск Windows? (Y/N): ", end="", flush=True)
                ans = input().strip().upper()
                if ans in ("Y", "YES", "Д", "ДА"):
                    if _add_to_autostart():
                        print("[OK] Добавлено в автозапуск.", flush=True)
                    else:
                        print("[!] Не удалось добавить в автозапуск.", flush=True)
            except Exception:
                pass
    tray_icon = None
    if sys.platform == "win32":
        tray_icon = _start_tray()
    try:
        asyncio.run(run())
    except Exception as e:
        log_err(f"Fatal: {e}", e)
        if getattr(sys, "frozen", False) and sys.platform == "win32":
            try:
                ctypes = __import__("ctypes")
                ctypes.windll.user32.MessageBoxW(None, str(e), "Heltec Monitor", 0x10)
            except Exception:
                pass
        sys.exit(1)