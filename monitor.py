#!/usr/bin/env python3
"""
NOCTURNE_OS — PC Monitor Server (Cyberdeck backend)

ARCHITECTURE:
- True async: aiohttp for LHM and Weather (no blocking HTTP)
- ThreadPoolExecutor only for blocking ops (ping, psutil deltas, top processes)
- Smart polling: send full payload only when values change beyond hysteresis threshold
- Heartbeat every 2s to keep connection alive
- Media: "idle" when Spotify/session paused (show IDLE + sleep icon on deck)

DATA SOURCES:
- LibreHardwareMonitor (aiohttp)
- Open-Meteo Weather (aiohttp)
- psutil, winsdk, ping (executor or async)
"""

import asyncio
import base64
import io
import json
import os
import platform
import subprocess
import sys
import time
import traceback
from concurrent.futures import ThreadPoolExecutor
from typing import Dict, List, Optional, Any, Tuple

import aiohttp
import psutil
from dotenv import load_dotenv

try:
    from winsdk.windows.media.control import (
        GlobalSystemMediaTransportControlsSessionManager,
    )
    from winsdk.windows.storage.streams import Buffer, InputStreamOptions
    HAS_WINSDK = True
except ImportError:
    HAS_WINSDK = False

try:
    import pystray
    from pystray import Menu, MenuItem
    HAS_PYSTRAY = True
except ImportError:
    HAS_PYSTRAY = False

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# ============================================================================
# CONFIGURATION
# ============================================================================

load_dotenv()

LHM_URL = os.getenv("LHM_URL", "http://localhost:8085/data.json")
TCP_HOST = os.getenv("TCP_HOST", "0.0.0.0")
TCP_PORT = int(os.getenv("TCP_PORT", "8888"))

WEATHER_LAT = os.getenv("WEATHER_LAT", "55.7558")
WEATHER_LON = os.getenv("WEATHER_LON", "37.6173")
WEATHER_URL = (
    f"https://api.open-meteo.com/v1/forecast?"
    f"latitude={WEATHER_LAT}&longitude={WEATHER_LON}"
    "&current=temperature_2m,weather_code"
    "&daily=temperature_2m_max,temperature_2m_min,weather_code"
    "&timezone=auto&forecast_days=8"
)
WEATHER_TIMEOUT = 10
WEATHER_UPDATE_INTERVAL = 10 * 60  # 10 minutes

PING_TARGET = "8.8.8.8"  # Google DNS
PING_TIMEOUT = 2

# Album cover: 64x64 1-bit → 512 bytes → base64
COVER_SIZE = 64
COVER_BYTES = (COVER_SIZE * COVER_SIZE) // 8
TOP_PROCS_CPU_N = 3
TOP_PROCS_RAM_N = 2
CLIENT_LINE_MAX = 4096
TOP_PROCS_CACHE_TTL = 2.5

# NOCTURNE_OS: poll interval 1s, send logic
POLL_INTERVAL = 1.0          # Collect data every 1s
HEARTBEAT_INTERVAL = 2.0     # Must send at least something every 2s to keep link alive
HYSTERESIS_TEMP = 2          # Send if temp changed by >= 2°C
HYSTERESIS_LOAD = 5          # Send if load changed by >= 5%
HYSTERESIS_NET_KB = 100       # Send if net changed by >= 100 KB/s

# ============================================================================
# DATA MAPPING — Exact sensor paths (Ryzen 5 5600X, RTX 4070, Gigabyte B550M / ITE IT8688E)
# Primary: nvidiagpu; fallback alias: gpu-nvidia for LHM variants
# ============================================================================

# Primary paths (mandatory spec)
TARGETS = {
    # CPU (Ryzen 5 5600X)
    "ct": "/amdcpu/0/temperature/2",   # Package
    "cl": "/amdcpu/0/load/0",          # Total
    "pw": "/amdcpu/0/power/0",         # Package (W)
    "cc": "/amdcpu/0/clock/1",         # Core #1 (MHz)
    # GPU (RTX 4070)
    "gt": "/nvidiagpu/0/temperature/0",  # Core
    "gh": "/nvidiagpu/0/temperature/1",  # HotSpot
    "gl": "/nvidiagpu/0/load/0",        # Core load
    "gf": "/nvidiagpu/0/fan/0",
    "gv": "/nvidiagpu/0/load/1",       # VRAM load (%)
    # VRAM (MB → GB in parser)
    "vu": "/nvidiagpu/0/smalldata/1",
    "vt": "/nvidiagpu/0/smalldata/2",
    # RAM (LHM: used + available → total = used+available)
    "ru": "/ram/data/0",
    "ra": "/ram/data/1",
    # Storage: SSD /hdd/0, HDD /hdd/1
    "su": "/hdd/0/load/0",             # SSD Samsung load
    "nt": "/hdd/0/temperature/0",       # SSD temp (optional)
    "du": "/hdd/1/load/0",             # HDD load
}
# Fallback: some LHM versions use gpu-nvidia instead of nvidiagpu
TARGETS_ALIAS = {
    "/gpu-nvidia/0/temperature/0": "gt",
    "/gpu-nvidia/0/temperature/1": "gh",
    "/gpu-nvidia/0/load/0": "gl",
    "/gpu-nvidia/0/fan/0": "gf",
    "/gpu-nvidia/0/fan/1": "gf",
    "/gpu-nvidia/0/load/1": "gv",
    "/gpu-nvidia/0/smalldata/1": "vu",
    "/gpu-nvidia/0/smalldata/2": "vt",
}
# IT8688E: cf, s1, s2, ch filled by heuristic (Type Fan / Temperature under /lpc/it8688e/0)
IT8688E_PREFIX = "/lpc/it8688e/0"

# ============================================================================
# GLOBAL STATE
# ============================================================================

stop_requested = False
executor: Optional[ThreadPoolExecutor] = None

weather_cache: Dict = {
    "temp": 0,
    "desc": "",
    "icon": 0,
    "day_high": 0,
    "day_low": 0,
    "day_code": 0,
    "week_high": [],
    "week_low": [],
    "week_code": [],
}
_weather_first_ok = False
_last_weather_log_time = 0.0

tcp_clients: List = []
client_screens: Dict = {}
client_buffers: Dict = {}
client_connect_times: Dict = {}  # v7: Track when clients connected
last_track_key = ""

top_procs_cache: List = []
top_procs_ram_cache: List = []
last_top_procs_time: float = 0.0

last_net_bytes = {"sent": 0, "recv": 0, "time": 0.0}
last_disk_bytes = {"read": 0, "write": 0, "time": 0.0}
ping_latency_ms = 0

# Global cache for immediate send to new clients
global_data_cache: Dict = {
    "hw": {},
    "weather": {},
    "media": {"art": "", "trk": "", "play": False, "idle": False, "cover_b64": ""},
    "top_procs": [],
    "top_procs_ram": [],
    "net": (0, 0),
    "disk": (0, 0),
    "ping": 0,
}
# For hysteresis: last payload snapshot we sent (so we only send when changed enough)
_last_sent_snapshot: Optional[Tuple] = None
_last_heartbeat_time: float = 0.0

# Alarm config (loaded from config.json)
alarm_config: Dict = {"gpu_temp_limit": 80, "cpu_temp_limit": 75}


def load_config_json() -> Dict:
    """Load config.json from same directory as script. Returns defaults on failure."""
    global alarm_config
    paths = [
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json"),
        "config.json",
    ]
    for p in paths:
        try:
            if os.path.isfile(p):
                with open(p, "r", encoding="utf-8") as f:
                    data = json.load(f)
                if "gpu_temp_limit" in data:
                    alarm_config["gpu_temp_limit"] = int(data["gpu_temp_limit"])
                if "cpu_temp_limit" in data:
                    alarm_config["cpu_temp_limit"] = int(data["cpu_temp_limit"])
                log_info(f"Loaded config: {p}")
                return alarm_config
        except Exception as e:
            log_debug(f"Config load {p}: {e}")
    return alarm_config

# ============================================================================
# UTILITIES
# ============================================================================

def log_err(msg: str, exc: Optional[BaseException] = None, trace_bt: bool = True):
    err_stream = getattr(sys, "stderr", None) or sys.stdout
    try:
        print(f"[ERROR] {msg}", file=err_stream, flush=True)
        if exc is not None and trace_bt:
            traceback.print_exc(file=err_stream)
            if getattr(err_stream, "flush", None):
                err_stream.flush()
    except (OSError, AttributeError):
        pass


def log_info(msg: str):
    print(f"[INFO] {msg}", flush=True)


def log_debug(msg: str):
    """Debug logging (can be disabled for production)"""
    if os.getenv("DEBUG", "0") == "1":
        print(f"[DEBUG] {msg}", flush=True)


def clean_val(v: Any) -> float:
    """Extract numeric value from LHM sensor string (e.g., '53.5 °C' -> 53.5)"""
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


# ============================================================================
# DATA COLLECTION — aiohttp for HTTP (non-blocking), executor for the rest
# ============================================================================

def _parse_lhm_json(data: Dict) -> Dict[str, float]:
    results: Dict[str, float] = {}
    targets_set = set(TARGETS.values())
    it8688e_fans: List[Tuple[str, float]] = []   # (SensorId, value) for stable order
    it8688e_temps: List[Tuple[str, float]] = []

    def walk(node: Any) -> None:
        if isinstance(node, list):
            for item in node:
                walk(item)
        elif isinstance(node, dict):
            sid = node.get("SensorId") or node.get("SensorID")
            raw = node.get("Value") or node.get("RawValue") or ""
            val = clean_val(raw)
            if sid:
                if sid in targets_set:
                    for k, v in TARGETS.items():
                        if v == sid:
                            results[k] = val
                            break
                elif sid in TARGETS_ALIAS:
                    results[TARGETS_ALIAS[sid]] = val
                elif sid.startswith(IT8688E_PREFIX):
                    stype = (node.get("Type") or node.get("SensorType") or "").lower()
                    if "fan" in stype:
                        it8688e_fans.append((sid, val))
                    elif "temperature" in stype or "temp" in stype:
                        it8688e_temps.append((sid, val))
            if "Children" in node:
                walk(node["Children"])

    walk(data)
    # IT8688E heuristic: assign first 3 fans to cf, s1, s2; first temp to ch
    it8688e_fans.sort(key=lambda x: x[0])
    it8688e_temps.sort(key=lambda x: x[0])
    for i, (_, v) in enumerate(it8688e_fans[:3]):
        results["cf" if i == 0 else "s1" if i == 1 else "s2"] = v
    if it8688e_temps:
        results["ch"] = it8688e_temps[0][1]
    # RAM: ra = used + available (total)
    if "ru" in results and "ra" in results:
        results["ra"] = results["ru"] + results["ra"]
    # VRAM MB → GB
    if "vu" in results:
        results["vu"] = round(results["vu"] / 1024.0, 1)
    if "vt" in results:
        results["vt"] = round(results["vt"] / 1024.0, 1)
    # CPU clock: may be in MHz; keep as-is (firmware shows 4.6GHz from 4600)
    return results


async def get_lhm_data_async(session: aiohttp.ClientSession) -> Dict[str, float]:
    """Fetch LHM via aiohttp. If this fails, the user is likely offline or the API is dead."""
    for attempt in range(3):
        try:
            async with session.get(LHM_URL, timeout=aiohttp.ClientTimeout(total=2)) as r:
                if r.status != 200:
                    continue
                data = await r.json()
                return _parse_lhm_json(data)
        except Exception as e:
            if attempt == 2:
                log_debug(f"LHM failed after 3 attempts: {e}")
            continue
    return {}


def _weather_desc_from_code(code: int) -> str:
    """Convert WMO weather code to short description"""
    if code == 0:
        return "Clear"
    if 1 <= code <= 3:
        return "Cloudy"
    if 45 <= code <= 48:
        return "Fog"
    if 51 <= code <= 67:
        return "Rain"
    if 71 <= code <= 77:
        return "Snow"
    if 80 <= code <= 82:
        return "Showers"
    if 85 <= code <= 86:
        return "Snow"
    if 95 <= code <= 99:
        return "Storm"
    return "Cloudy"


async def get_weather_async(session: aiohttp.ClientSession) -> Dict:
    """Fetch weather from Open-Meteo. Never raises; returns cache on failure."""
    global weather_cache, _weather_first_ok, _last_weather_log_time
    now = time.time()
    for attempt in range(2):
        try:
            async with session.get(WEATHER_URL, timeout=aiohttp.ClientTimeout(total=WEATHER_TIMEOUT)) as r:
                if r.status != 200:
                    continue
                data = await r.json()
            cur = data.get("current") or {}
            temp = int(cur.get("temperature_2m", 0) or 0)
            code = int(cur.get("weather_code", 0) or 0)
            desc = (_weather_desc_from_code(code) or "Cloudy")[:20]
            daily = data.get("daily") or {}
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
                "temp": temp,
                "desc": desc,
                "icon": code,
                "day_high": day_high,
                "day_low": day_low,
                "day_code": day_code,
                "week_high": week_high,
                "week_low": week_low,
                "week_code": week_code,
            }
            if not _weather_first_ok:
                _weather_first_ok = True
                log_info(f"Weather: {temp}°C, {desc}")
            return weather_cache
        except Exception as e:
            if attempt == 1 and (now - _last_weather_log_time > 300):
                log_err(f"Weather fetch failed: {e}", trace_bt=False)
                _last_weather_log_time = now
    return weather_cache


def get_network_speed_sync() -> tuple:
    """
    Calculate network speed (KB/s) using psutil delta.
    Returns (upload_kbps, download_kbps). Blocking.
    """
    try:
        counters = psutil.net_io_counters()
        now = time.time()
        if last_net_bytes["time"] == 0:
            last_net_bytes["sent"] = counters.bytes_sent
            last_net_bytes["recv"] = counters.bytes_recv
            last_net_bytes["time"] = now
            return 0, 0
        dt = now - last_net_bytes["time"]
        if dt < 0.1:
            return 0, 0
        up = int((counters.bytes_sent - last_net_bytes["sent"]) / dt / 1024)
        down = int((counters.bytes_recv - last_net_bytes["recv"]) / dt / 1024)
        last_net_bytes["sent"] = counters.bytes_sent
        last_net_bytes["recv"] = counters.bytes_recv
        last_net_bytes["time"] = now
        return max(0, up), max(0, down)
    except Exception:
        return 0, 0


def get_disk_speed_sync() -> tuple:
    """
    Calculate disk I/O speed (KB/s) using psutil delta.
    Returns (read_kbps, write_kbps). Blocking.
    """
    try:
        counters = psutil.disk_io_counters()
        if counters is None:
            return 0, 0
        now = time.time()
        if last_disk_bytes["time"] == 0:
            last_disk_bytes["read"] = counters.read_bytes
            last_disk_bytes["write"] = counters.write_bytes
            last_disk_bytes["time"] = now
            return 0, 0
        dt = now - last_disk_bytes["time"]
        if dt < 0.1:
            return 0, 0
        read_speed = int((counters.read_bytes - last_disk_bytes["read"]) / dt / 1024)
        write_speed = int((counters.write_bytes - last_disk_bytes["write"]) / dt / 1024)
        last_disk_bytes["read"] = counters.read_bytes
        last_disk_bytes["write"] = counters.write_bytes
        last_disk_bytes["time"] = now
        return max(0, read_speed), max(0, write_speed)
    except Exception:
        return 0, 0


def get_ping_latency_sync() -> int:
    """
    Ping Google DNS (8.8.8.8) and return latency in ms.
    Returns 0 on failure. Blocking.
    """
    try:
        if platform.system().lower() == "windows":
            cmd = ["ping", "-n", "1", "-w", str(PING_TIMEOUT * 1000), PING_TARGET]
        else:
            cmd = ["ping", "-c", "1", "-W", str(PING_TIMEOUT), PING_TARGET]
        
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=PING_TIMEOUT + 1,
            text=True
        )
        
        if result.returncode == 0:
            output = result.stdout
            # Parse latency from output
            if "time=" in output.lower():
                for part in output.split():
                    if "time=" in part.lower():
                        time_str = part.split("=")[1].replace("ms", "").strip()
                        return int(float(time_str))
            elif "average" in output.lower():
                # Windows format: "Average = Xms"
                for line in output.split("\n"):
                    if "average" in line.lower():
                        parts = line.split("=")
                        if len(parts) > 1:
                            time_str = parts[-1].replace("ms", "").strip()
                            return int(float(time_str))
        return 0
    except Exception:
        return 0


def get_top_processes_cpu_sync(n: int = 3) -> List[Dict]:
    """
    Get top N processes by CPU usage.
    Returns list of {n: name, c: cpu_percent}. Blocking.
    """
    try:
        procs = []
        for p in psutil.process_iter(["name", "cpu_percent"]):
            try:
                info = p.info
                if info.get("cpu_percent") and info["cpu_percent"] > 0:
                    procs.append({"n": (info.get("name") or "")[:20], "c": int(info["cpu_percent"])})
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        procs.sort(key=lambda x: x["c"], reverse=True)
        return procs[:n]
    except Exception:
        return []


def get_top_processes_ram_sync(n: int = 2) -> List[Dict]:
    """
    Get top N processes by RAM usage.
    Returns list of {n: name, r: ram_mb}. Blocking.
    """
    try:
        procs = []
        for p in psutil.process_iter(["name", "memory_info"]):
            try:
                info = p.info
                name = (info.get("name") or "").strip()
                if "memcompression" in name.lower() or "memory compression" in name.lower():
                    continue
                if info.get("memory_info"):
                    mb = info["memory_info"].rss / (1024 * 1024)
                    if mb > 10:
                        procs.append({"n": name[:20], "r": int(mb)})
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        procs.sort(key=lambda x: x["r"], reverse=True)
        return procs[:n]
    except Exception:
        return []


async def get_media_info() -> Dict:
    """
    Get Windows media info (Artist, Track, Playing, Idle).
    idle=True when session exists but paused — deck shows IDLE + sleep icon.
    """
    if not HAS_WINSDK:
        return {"art": "", "trk": "", "play": False, "idle": False, "cover_b64": ""}
    try:
        manager = await GlobalSystemMediaTransportControlsSessionManager.request_async()
        session = manager.get_current_session()
        if not session:
            return {"art": "", "trk": "", "play": False, "idle": False, "cover_b64": ""}
        info = await session.try_get_media_properties_async()
        playback = session.get_playback_info()
        artist = info.artist if info and info.artist else ""
        track = info.title if info and info.title else ""
        is_playing = playback and getattr(playback, "playback_status", 0) == 4
        # Idle = we have a session (artist/track) but not playing — show IDLE + zzz on deck
        is_idle = bool(artist or track) and not is_playing
        cover_b64 = ""
        if HAS_PIL and info and is_playing:
            try:
                thumb_ref = getattr(info, "thumbnail", None)
                if thumb_ref:
                    thumb_stream = await thumb_ref.open_read_async()
                    if thumb_stream:
                        chunk_size = 32768
                        chunks = []
                        while True:
                            buf = Buffer(chunk_size)
                            n = await thumb_stream.read_async(buf, chunk_size, InputStreamOptions.READ_AHEAD)
                            if n == 0:
                                break
                            chunks.append(bytes(buf)[:n])
                        img_bytes = b"".join(chunks)
                        if img_bytes:
                            img = Image.open(io.BytesIO(img_bytes))
                            img = img.convert("L").resize((COVER_SIZE, COVER_SIZE), Image.Resampling.LANCZOS)
                            # Floyd-Steinberg dithering (1-bit)
                            img = img.convert("1", dither=Image.Dither.FLOYDSTEINBERG)
                            bitmap = []
                            for y in range(COVER_SIZE):
                                for x in range(0, COVER_SIZE, 8):
                                    byte = 0
                                    for bit in range(8):
                                        if img.getpixel((x + bit, y)):
                                            byte |= (1 << bit)
                                    bitmap.append(byte)
                            cover_b64 = base64.b64encode(bytes(bitmap)).decode("ascii")
            except Exception:
                pass
        return {"art": artist[:30], "trk": track[:30], "play": is_playing, "idle": is_idle, "cover_b64": cover_b64}
    except Exception:
        return {"art": "", "trk": "", "play": False, "idle": False, "cover_b64": ""}


# ============================================================================
# PAYLOAD — 2-char keys, int for loads/temps, 1-decimal float for RAM/VRAM
# Weather: never send 0/null after first success (use cache)
# ============================================================================

def build_payload(
    hw: Dict,
    media: Dict,
    weather: Dict,
    top_procs: List,
    top_procs_ram: List,
    net: tuple,
    disk: tuple,
    ping_ms: int,
) -> Dict:
    """
    Build JSON payload with 2-char keys for bandwidth efficiency.
    
    Key mapping:
    - ct: CPU temp (°C)
    - gt: GPU temp (°C)
    - cl: CPU load (%)
    - gl: GPU load (%)
    - ru: RAM used (GB, 1 decimal)
    - ra: RAM total (GB, 1 decimal)
    - nd: Network down (KB/s)
    - nu: Network up (KB/s)
    - pg: Ping latency (ms)
    - cf: CPU fan (RPM)
    - s1: System fan 1 (RPM)
    - s2: System fan 2 (RPM)
    - gf: GPU fan (RPM)
    - su: System drive usage (%)
    - du: Data drive usage (%)
    - vu: VRAM used (GB, 1 decimal)
    - vt: VRAM total (GB, 1 decimal)
    - ch: Chipset temp (°C)
    - dr: Disk read (KB/s)
    - dw: Disk write (KB/s)
    - wt: Weather temp (°C)
    - wd: Weather description
    - wi: Weather icon code
    - tp: Top processes by CPU [{n: name, c: percent}]
    - tr: Top processes by RAM [{n: name, r: MB}]
    - art: Media artist
    - trk: Media track
    - mp: Media playing (bool)
    - idle: True when paused / session exists but not playing (deck shows IDLE + sleep icon)
    - cov: Album cover (base64)
    """
    # Weather: after first success, always send cached; never 0/null for temp/desc
    w = weather if _weather_first_ok else weather
    wt_val = w.get("temp", 0)
    wd_val = w.get("desc", "") or ""
    if _weather_first_ok and (wt_val == 0 and weather_cache.get("temp", 0) != 0):
        wt_val = weather_cache["temp"]
    if _weather_first_ok and not wd_val and weather_cache.get("desc"):
        wd_val = weather_cache["desc"]

    ram_used = hw.get("ru", 0.0)
    ram_total = hw.get("ra", 0.0)
    ram_used_f = round(float(ram_used), 1)
    ram_total_f = round(float(ram_total), 1)

    payload = {
        # Core metrics
        "ct": int(hw.get("ct", 0)),
        "gt": int(hw.get("gt", 0)),
        "cl": int(hw.get("cl", 0)),
        "gl": int(hw.get("gl", 0)),
        "pw": int(hw.get("pw", 0)),   # CPU power (W)
        "cc": int(hw.get("cc", 0)),   # CPU clock (MHz)
        "gh": int(hw.get("gh", 0)),   # GPU HotSpot (°C)
        "gv": int(hw.get("gv", 0)),   # VRAM load (%)
        
        # Memory
        "ru": ram_used_f,
        "ra": ram_total_f,
        
        # Network
        "nd": net[1],  # download
        "nu": net[0],  # upload
        "pg": ping_ms,
        
        # Fans
        "cf": int(hw.get("cf", 0)),
        "s1": int(hw.get("s1", 0)),
        "s2": int(hw.get("s2", 0)),
        "gf": int(hw.get("gf", 0)),
        
        # Storage
        "su": int(hw.get("su", 0)),
        "du": int(hw.get("du", 0)),
        
        # VRAM
        "vu": round(float(hw.get("vu", 0)), 1),
        "vt": round(float(hw.get("vt", 0)), 1),
        
        # Chipset
        "ch": int(hw.get("ch", 0)),
        
        # Disk I/O
        "dr": disk[0],
        "dw": disk[1],
        
        # Weather
        "wt": wt_val,
        "wd": wd_val,
        "wi": int(w.get("icon", 0)),
        
        # Processes
        "tp": top_procs,
        "tr": top_procs_ram,
        
        # Media
        "art": media.get("art", ""),
        "trk": media.get("trk", ""),
        "mp": media.get("play", False),
        "idle": media.get("idle", False),
        "cov": media.get("cover_b64", ""),
    }
    
    # Alarm: if limit exceeded, set alert and target_screen (auto-switch on display)
    gpu_limit = alarm_config.get("gpu_temp_limit", 80)
    cpu_limit = alarm_config.get("cpu_temp_limit", 75)
    ct_val = payload["ct"]
    gt_val = payload["gt"]
    if gt_val >= gpu_limit or ct_val >= cpu_limit:
        payload["alert"] = "CRITICAL"
        payload["target_screen"] = "GPU" if gt_val >= gpu_limit else "CPU"
    else:
        payload["alert"] = ""
        payload["target_screen"] = ""
    
    return payload


def _payload_snapshot(payload: Dict) -> Tuple:
    """Key values for hysteresis: only send when these change beyond threshold."""
    return (
        payload.get("ct", 0),
        payload.get("gt", 0),
        payload.get("cl", 0),
        payload.get("gl", 0),
        payload.get("nd", 0),
        payload.get("nu", 0),
        payload.get("ru", 0),
        payload.get("ra", 0),
    )


def should_send_payload(payload: Dict, now: float) -> bool:
    """True if we must send: heartbeat interval reached, or data changed beyond hysteresis."""
    global _last_sent_snapshot, _last_heartbeat_time
    if _last_sent_snapshot is None:
        return True
    if now - _last_heartbeat_time >= HEARTBEAT_INTERVAL:
        return True
    cur = _payload_snapshot(payload)
    prev = _last_sent_snapshot
    if abs(cur[0] - prev[0]) >= HYSTERESIS_TEMP or abs(cur[1] - prev[1]) >= HYSTERESIS_TEMP:
        return True
    if abs(cur[2] - prev[2]) >= HYSTERESIS_LOAD or abs(cur[3] - prev[3]) >= HYSTERESIS_LOAD:
        return True
    if abs(cur[4] - prev[4]) >= HYSTERESIS_NET_KB or abs(cur[5] - prev[5]) >= HYSTERESIS_NET_KB:
        return True
    if cur[6] != prev[6] or cur[7] != prev[7]:  # RAM changed
        return True
    return False


# ============================================================================
# TCP SERVER — v7 with immediate data send
# ============================================================================

async def send_data_to_client(writer, payload: Dict) -> bool:
    """
    Send JSON payload to a single client.
    Returns True on success, False on failure.
    """
    try:
        data = json.dumps(payload, separators=(",", ":")) + "\n"
        writer.write(data.encode("utf-8"))
        await writer.drain()
        return True
    except Exception as e:
        log_debug(f"Send failed: {e}")
        return False


async def handle_client(reader, writer):
    """
    Handle TCP client connection.
    v7: Immediately sends cached data on connect to prevent timeout.
    """
    addr = writer.get_extra_info("peername")
    log_info(f"Client connected: {addr}")
    tcp_clients.append(writer)
    client_screens[writer] = 0
    client_buffers[writer] = ""
    client_connect_times[writer] = time.time()
    
    # v7 CRITICAL: Send cached data IMMEDIATELY on connect
    # This prevents the client from timing out waiting for first data
    try:
        if global_data_cache["hw"]:
            payload = build_payload(
                hw=global_data_cache["hw"],
                media=global_data_cache["media"],
                weather=global_data_cache["weather"],
                top_procs=global_data_cache["top_procs"],
                top_procs_ram=global_data_cache["top_procs_ram"],
                net=global_data_cache["net"],
                disk=global_data_cache["disk"],
                ping_ms=global_data_cache["ping"],
            )
            success = await send_data_to_client(writer, payload)
            if success:
                log_debug(f"Sent initial data to {addr}")
            else:
                log_debug(f"Failed to send initial data to {addr}")
        else:
            # No cached data yet - send minimal heartbeat
            heartbeat = {"ct": 0, "gt": 0, "cl": 0, "gl": 0, "ru": 0, "ra": 0}
            await send_data_to_client(writer, heartbeat)
            log_debug(f"Sent heartbeat to {addr}")
    except Exception as e:
        log_debug(f"Initial send error: {e}")
    
    try:
        while not stop_requested:
            try:
                data = await asyncio.wait_for(reader.read(256), timeout=1.0)
            except asyncio.TimeoutError:
                # Check if connection is still alive
                if writer.is_closing():
                    break
                continue
            if not data:
                break
            try:
                text = data.decode("utf-8")
                client_buffers[writer] += text
                if len(client_buffers[writer]) >= CLIENT_LINE_MAX:
                    client_buffers[writer] = ""
                while "\n" in client_buffers[writer]:
                    line, client_buffers[writer] = client_buffers[writer].split("\n", 1)
                    line = line.strip()
                    # Handle HELO handshake from v7 firmware
                    if line == "HELO":
                        log_debug(f"Received HELO from {addr}")
                        continue
                    if line.startswith("screen:"):
                        try:
                            client_screens[writer] = int(line.split(":", 1)[1])
                        except (ValueError, IndexError):
                            pass
            except UnicodeDecodeError:
                client_buffers[writer] = ""
    except Exception as e:
        log_debug(f"Client handler error: {e}")
    finally:
        if writer in tcp_clients:
            tcp_clients.remove(writer)
        client_screens.pop(writer, None)
        client_buffers.pop(writer, None)
        client_connect_times.pop(writer, None)
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass
        log_info(f"Client disconnected: {addr}")


# ============================================================================
# MAIN ASYNC LOOP — NOCTURNE_OS: aiohttp, heartbeat, hysteresis
# ============================================================================

async def run():
    """
    Main async loop. True async:
    - aiohttp for LHM and Weather (no blocking HTTP)
    - Executor only for ping, psutil, top processes
    - Send only when heartbeat (2s) or data changed beyond hysteresis
    """
    global executor, last_track_key, top_procs_cache, top_procs_ram_cache
    global last_top_procs_time, ping_latency_ms, global_data_cache
    global _last_sent_snapshot, _last_heartbeat_time

    executor = ThreadPoolExecutor(max_workers=6)
    load_config_json()

    log_info(f"Starting TCP server on {TCP_HOST}:{TCP_PORT}")

    try:
        server = await asyncio.start_server(handle_client, TCP_HOST, TCP_PORT)
    except OSError as e:
        if "bind" in str(e).lower() or "address" in str(e).lower():
            log_err(f"Cannot bind to {TCP_HOST}:{TCP_PORT}. Check port and interface.", e, trace_bt=True)
        raise

    log_info("Server ready. Waiting for clients...")

    loop = asyncio.get_event_loop()
    last_lhm_time = 0.0
    last_weather_time = 0.0
    last_ping_time = 0.0
    WEATHER_INTERVAL_F = float(WEATHER_UPDATE_INTERVAL)
    PING_INTERVAL = 5.0

    # One session for all HTTP (LHM, Weather) — non-blocking
    session = aiohttp.ClientSession()

    log_info("Collecting initial data...")
    try:
        global_data_cache["hw"] = await get_lhm_data_async(session)
        global_data_cache["weather"] = await get_weather_async(session)
        global_data_cache["net"] = await loop.run_in_executor(executor, get_network_speed_sync)
        global_data_cache["disk"] = await loop.run_in_executor(executor, get_disk_speed_sync)
        global_data_cache["media"] = await get_media_info()
        top_procs_cache = await loop.run_in_executor(
            executor, lambda: get_top_processes_cpu_sync(TOP_PROCS_CPU_N)
        )
        top_procs_ram_cache = await loop.run_in_executor(
            executor, lambda: get_top_processes_ram_sync(TOP_PROCS_RAM_N)
        )
        global_data_cache["top_procs"] = top_procs_cache
        global_data_cache["top_procs_ram"] = top_procs_ram_cache
        last_top_procs_time = time.time()
        ping_latency_ms = await loop.run_in_executor(executor, get_ping_latency_sync)
        global_data_cache["ping"] = ping_latency_ms
        log_info("Initial data collected.")
    except Exception as e:
        log_err(f"Initial data collection failed: {e}", trace_bt=False)

    try:
        while not stop_requested:
            now = time.time()

            # Poll LHM + net + disk at POLL_INTERVAL
            if now - last_lhm_time >= POLL_INTERVAL:
                last_lhm_time = now
                try:
                    hw_data = await get_lhm_data_async(session)
                    if hw_data:
                        global_data_cache["hw"] = hw_data
                    global_data_cache["net"] = await loop.run_in_executor(executor, get_network_speed_sync)
                    global_data_cache["disk"] = await loop.run_in_executor(executor, get_disk_speed_sync)
                except Exception as e:
                    log_debug(f"Data collection error: {e}")

            if now - last_top_procs_time >= TOP_PROCS_CACHE_TTL:
                try:
                    top_procs_cache = await loop.run_in_executor(
                        executor, lambda: get_top_processes_cpu_sync(TOP_PROCS_CPU_N)
                    )
                    top_procs_ram_cache = await loop.run_in_executor(
                        executor, lambda: get_top_processes_ram_sync(TOP_PROCS_RAM_N)
                    )
                    last_top_procs_time = now
                    global_data_cache["top_procs"] = top_procs_cache
                    global_data_cache["top_procs_ram"] = top_procs_ram_cache
                except Exception as e:
                    log_debug(f"Process collection error: {e}")

            if now - last_weather_time >= WEATHER_INTERVAL_F:
                last_weather_time = now
                try:
                    global_data_cache["weather"] = await get_weather_async(session)
                except Exception as e:
                    log_debug(f"Weather collection error: {e}")

            if now - last_ping_time >= PING_INTERVAL:
                last_ping_time = now
                try:
                    ping_latency_ms = await loop.run_in_executor(executor, get_ping_latency_sync)
                    global_data_cache["ping"] = ping_latency_ms
                except Exception as e:
                    log_debug(f"Ping error: {e}")

            try:
                global_data_cache["media"] = await get_media_info()
            except Exception as e:
                log_debug(f"Media info error: {e}")

            track_key = f"{global_data_cache['media'].get('art', '')}|{global_data_cache['media'].get('trk', '')}"
            if track_key != last_track_key and last_track_key:
                log_info("Track changed.")
            last_track_key = track_key

            payload = build_payload(
                hw=global_data_cache["hw"],
                media=global_data_cache["media"],
                weather=global_data_cache["weather"],
                top_procs=global_data_cache["top_procs"],
                top_procs_ram=global_data_cache["top_procs_ram"],
                net=global_data_cache["net"],
                disk=global_data_cache["disk"],
                ping_ms=global_data_cache["ping"],
            )

            # Send only when heartbeat due or data changed beyond hysteresis
            if should_send_payload(payload, now):
                _last_heartbeat_time = now
                _last_sent_snapshot = _payload_snapshot(payload)
                dead = []
                for writer in list(tcp_clients):
                    success = await send_data_to_client(writer, payload)
                    if not success:
                        dead.append(writer)
                for writer in dead:
                    if writer in tcp_clients:
                        tcp_clients.remove(writer)
                    client_screens.pop(writer, None)
                    client_buffers.pop(writer, None)
                    client_connect_times.pop(writer, None)
                    try:
                        writer.close()
                        await writer.wait_closed()
                    except Exception:
                        pass

            await asyncio.sleep(0.1)
    finally:
        await session.close()

    if executor:
        executor.shutdown(wait=False)
    tasks = [t for t in asyncio.all_tasks() if t is not asyncio.current_task()]
    for t in tasks:
        t.cancel()
    if tasks:
        await asyncio.gather(*tasks, return_exceptions=True)
    if server:
        server.close()
        await server.wait_closed()


# ============================================================================
# SYSTEM TRAY (Windows)
# ============================================================================

def _make_tray_icon_image():
    if not HAS_PIL:
        return None
    try:
        from PIL import ImageDraw
        img = Image.new("RGBA", (32, 32), (10, 10, 20, 255))
        draw = ImageDraw.Draw(img)
        # Cyberpunk cyan accent
        draw.ellipse((4, 4, 28, 28), fill=(0, 255, 255), outline=(255, 255, 255))
        return img
    except Exception:
        return None


def _run_tray_thread(icon):
    try:
        icon.run()
    except Exception as e:
        log_err(f"Tray error: {e}", trace_bt=False)


def _start_tray():
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

    def on_add_autostart(icon, item):
        log_info("Add to autostart" if _add_to_autostart() else "Autostart add failed")

    def on_remove_autostart(icon, item):
        log_info("Remove from autostart" if _remove_from_autostart() else "Autostart remove failed")

    menu = Menu(
        MenuItem("Add to Autostart", on_add_autostart),
        MenuItem("Remove from Autostart", on_remove_autostart),
        MenuItem("Exit", on_quit, default=True),
    )
    icon = pystray.Icon("nocturne", img, "NOCTURNE_OS Monitor", menu=menu)
    import threading
    t = threading.Thread(target=_run_tray_thread, args=(icon,), daemon=True)
    t.start()
    return icon


def _is_in_autostart() -> bool:
    if sys.platform != "win32":
        return False
    try:
        import winreg
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0, winreg.KEY_READ,
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
    if sys.platform != "win32":
        return False
    try:
        import winreg
        exe_path = f'"{sys.executable}" "{os.path.abspath(__file__)}"' if not getattr(sys, "frozen", False) else os.path.abspath(sys.executable)
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0, winreg.KEY_SET_VALUE,
        )
        winreg.SetValueEx(key, "HeltecMonitor", 0, winreg.REG_SZ, exe_path)
        winreg.CloseKey(key)
        return True
    except Exception as e:
        log_err(f"Autostart add failed: {e}", trace_bt=False)
        return False


def _remove_from_autostart() -> bool:
    if sys.platform != "win32":
        return False
    try:
        import winreg
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0, winreg.KEY_SET_VALUE,
        )
        try:
            winreg.DeleteValue(key, "HeltecMonitor")
            return True
        except OSError:
            return False
        finally:
            winreg.CloseKey(key)
    except Exception as e:
        log_err(f"Autostart remove failed: {e}", trace_bt=False)
        return False


# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    log_info("=" * 60)
    log_info("NOCTURNE_OS — PC Monitor Server")
    log_info("Cyberdeck backend (aiohttp, heartbeat, hysteresis)")
    log_info("=" * 60)

    if sys.platform == "win32" and not getattr(sys, "frozen", False) and not _is_in_autostart():
        try:
            print("Add to Windows autostart? (Y/N): ", end="", flush=True)
            if input().strip().upper() in ("Y", "YES"):
                _add_to_autostart()
        except Exception:
            pass

    tray_icon = _start_tray() if sys.platform == "win32" else None

    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        log_info("Shutting down...")
    except Exception as e:
        log_err(f"Fatal: {e}", e)
        if getattr(sys, "frozen", False) and sys.platform == "win32":
            try:
                import ctypes
                ctypes.windll.user32.MessageBoxW(None, str(e), "NOCTURNE_OS Monitor", 0x10)
            except Exception:
                pass
        sys.exit(1)
    log_info("Goodbye.")
