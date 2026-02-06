#!/usr/bin/env python3
"""
NOCTURNE_OS — PC monitor server (backend for ESP32 display).

- Polls LHM (config.json lhm_url), weather, media (Windows SDK).
- TCP server (host/port in config); sends JSON (hw, weather, media, …).
- Media: sends media_status (PLAYING|PAUSED) only; no cover art.
- Tray: Add/Remove startup, Restart, Close. Logs: nocturne.log.
"""

import asyncio
import base64
import io
import json
import logging
import os
import platform
import subprocess
import sys
import threading
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

load_dotenv()

# ---------------------------------------------------------------------------
# Logging: always to file (nocturne.log); when frozen, next to exe for autostart.
# ---------------------------------------------------------------------------
_PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if getattr(sys, "frozen", False):
    _LOG_FILE = os.path.join(os.path.dirname(sys.executable), "nocturne.log")
else:
    _LOG_FILE = os.path.join(_PROJECT_ROOT, "nocturne.log")

def _setup_logging(console: bool = False) -> None:
    handlers: List[logging.Handler] = [
        logging.FileHandler(_LOG_FILE, encoding="utf-8"),
    ]
    if console:
        handlers.append(logging.StreamHandler(sys.stdout))
    logging.basicConfig(
        level=logging.DEBUG if os.getenv("DEBUG", "0") == "1" else logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
        handlers=handlers,
        force=True,
    )
    # Prevent duplicate logs from asyncio/aiohttp
    logging.getLogger("asyncio").setLevel(logging.WARNING)
    logging.getLogger("aiohttp").setLevel(logging.WARNING)

# ---------------------------------------------------------------------------
# Config from config.json (host, port, lhm_url, limits, weather_city)
# When frozen (exe): look next to executable.
# ---------------------------------------------------------------------------
def _get_config_path() -> str:
    if getattr(sys, "frozen", False):
        base = os.path.dirname(sys.executable)
        return os.path.join(base, "config.json")
    base = os.path.dirname(os.path.abspath(__file__))
    for candidate in [os.path.join(base, "..", "config.json"), os.path.join(base, "config.json")]:
        if os.path.isfile(candidate):
            return candidate
    return "config.json"

CONFIG_PATH = _get_config_path()

def load_config() -> Dict:
    out = {
        "host": "0.0.0.0",
        "port": 8090,
        "lhm_url": "http://localhost:8085/data.json",
        "limits": {"gpu": 80, "cpu": 75},
        "weather_city": "Moscow",
    }
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
        if "host" in data:
            out["host"] = data["host"]
        if "port" in data:
            out["port"] = int(data["port"])
        if "lhm_url" in data:
            out["lhm_url"] = data["lhm_url"]
        if "limits" in data:
            out["limits"] = {**out["limits"], **data["limits"]}
        if "weather_city" in data:
            out["weather_city"] = data["weather_city"]
    except Exception:
        pass
    return out

# ---------------------------------------------------------------------------
# Autostart (Windows HKCU Run). Used by tray menu.
# ---------------------------------------------------------------------------
AUTOSTART_NAME = "NOCTURNE_OS"
_RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"


def _get_autostart_cmd() -> str:
    """Command to run for autostart: exe path or pythonw + script."""
    if getattr(sys, "frozen", False):
        return sys.executable
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "monitor.py")
    exe = sys.executable
    if "pythonw" not in os.path.basename(exe).lower():
        base = os.path.dirname(exe)
        pw = os.path.join(base, "pythonw.exe")
        if os.path.isfile(pw):
            exe = pw
    return f'"{exe}" "{script}"'


def is_autostart_enabled() -> bool:
    if sys.platform != "win32":
        return False
    try:
        import winreg
        key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, _RUN_KEY, 0, winreg.KEY_READ)
        try:
            winreg.QueryValueEx(key, AUTOSTART_NAME)
            return True
        except FileNotFoundError:
            return False
        finally:
            winreg.CloseKey(key)
    except Exception:
        return False


def set_autostart(enable: bool) -> None:
    if sys.platform != "win32":
        return
    try:
        import winreg
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER, _RUN_KEY, 0,
            winreg.KEY_SET_VALUE | winreg.KEY_QUERY_VALUE,
        )
        if enable:
            winreg.SetValueEx(key, AUTOSTART_NAME, 0, winreg.REG_SZ, _get_autostart_cmd())
            log_info("Autostart enabled (HKCU Run).")
        else:
            try:
                winreg.DeleteValue(key, AUTOSTART_NAME)
                log_info("Autostart removed.")
            except FileNotFoundError:
                pass
        winreg.CloseKey(key)
    except OSError as e:
        log_err(f"Autostart failed: {e}")


_config = load_config()
LHM_URL = os.getenv("LHM_URL", _config["lhm_url"])
TCP_HOST = os.getenv("TCP_HOST", _config["host"])
TCP_PORT = int(os.getenv("TCP_PORT", str(_config["port"])))
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
WEATHER_UPDATE_INTERVAL = 10 * 60
PING_TARGET = "8.8.8.8"
PING_TIMEOUT = 2
COVER_SIZE = 64
TOP_PROCS_CPU_N = 3
TOP_PROCS_RAM_N = 2
CLIENT_LINE_MAX = 4096
TOP_PROCS_CACHE_TTL = 2.5
POLL_INTERVAL = 0.5
HEARTBEAT_INTERVAL = 2.8  # Send at most every 2.8s when unchanged (reduces traffic)
HYSTERESIS_TEMP = 2
HYSTERESIS_LOAD = 5
HYSTERESIS_NET_KB = 100

# Sensor paths (LHM) — exact paths from serverpars.txt
# CPU: Core #1 clock only (not package)
# GPU: RTX 4070 — load/0=Graphics %, load/1=Memory %, clock/0=Core, clock/1=VRAM, power/0=TDP
# Fans: it8688e fan/0=CPU, fan/1=Pump; nvidiagpu fan/0=GPU
TARGETS = {
    "ct": "/amdcpu/0/temperature/2",
    "cl": "/amdcpu/0/load/0",
    "pw": "/amdcpu/0/power/0",
    "cc": "/amdcpu/0/clock/1",  # Core #1 only
    "gt": "/nvidiagpu/0/temperature/0",
    "gh": "/nvidiagpu/0/temperature/1",
    "gl": "/nvidiagpu/0/load/0",   # Graphics %
    "gv": "/nvidiagpu/0/load/1",   # VRAM %
    "gclock": "/nvidiagpu/0/clock/0",   # Core MHz
    "vclock": "/nvidiagpu/0/clock/1",   # VRAM MHz
    "gtdp": "/nvidiagpu/0/power/0",
    "gf": "/nvidiagpu/0/fan/0",
    "vu": "/nvidiagpu/0/smalldata/1",
    "vt": "/nvidiagpu/0/smalldata/2",
    "ru": "/ram/data/0",
    "ra": "/ram/data/1",
}
# Storage (serverpars): Free = data/31, Total = data/32, Temp = temperature/0. Used = Total - Free.
# Devices: /hdd/N, /nvme/N, /ssd/N. No load % — display Used/Total GB only.
FAN_PATHS = [
    "/lpc/it8688e/0/fan/0",   # CPU
    "/lpc/it8688e/0/fan/1",   # Pump
    "/nvidiagpu/0/fan/0",     # GPU
]
# Optional 4th fan (case) — some boards have it8688e fan/2
FAN_CASE_PATH = "/lpc/it8688e/0/fan/2"

TARGETS_ALIAS = {
    "/gpu-nvidia/0/temperature/0": "gt",
    "/gpu-nvidia/0/temperature/1": "gh",
    "/gpu-nvidia/0/temperature/2": "gh",   # Hot Spot (serverpars uses temp/2)
    "/gpu-nvidia/0/load/0": "gl",
    "/gpu-nvidia/0/load/1": "gv",
    "/gpu-nvidia/0/clock/0": "gclock",
    "/gpu-nvidia/0/clock/1": "vclock",
    "/gpu-nvidia/0/clock/4": "vclock",   # LHM may use clock/4 for "GPU Memory" MHz
    "/gpu-nvidia/0/power/0": "gtdp",
    "/gpu-nvidia/0/fan/0": "gf",
    "/gpu-nvidia/0/smalldata/1": "vu",
    "/gpu-nvidia/0/smalldata/2": "vt",
    "/gpu-nvidia/0/data/1": "vu",
    "/gpu-nvidia/0/data/2": "vt",
}
IT8688E_PREFIX = "/lpc/it8688e/0"

stop_requested = False
executor: Optional[ThreadPoolExecutor] = None
_server_thread_holder: List[Optional[threading.Thread]] = [None]  # for Restart
weather_cache: Dict = {"temp": 0, "desc": "", "icon": 0}
_weather_first_ok = False
tcp_clients: List = []
client_screens: Dict = {}
client_buffers: Dict = {}
last_sent_track_key: str = ""
top_procs_cache: List = []
top_procs_ram_cache: List = []
last_top_procs_time: float = 0.0
last_net_bytes = {"sent": 0, "recv": 0, "time": 0.0}
last_disk_bytes = {"read": 0, "write": 0, "time": 0.0}
ping_latency_ms = 0
global_data_cache: Dict = {
    "hw": {}, "weather": {},
    "media": {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"},
    "top_procs": [], "top_procs_ram": [], "net": (0, 0), "disk": (0, 0), "ping": 0,
}
_last_sent_snapshot: Optional[Tuple] = None
_last_heartbeat_time: float = 0.0


def log_err(msg: str, exc: Optional[BaseException] = None) -> None:
    try:
        logging.error(msg, exc_info=exc is not None)
    except Exception:
        pass


def log_info(msg: str) -> None:
    logging.info(msg)


def log_debug(msg: str) -> None:
    logging.debug(msg)


def clean_val(v: Any) -> float:
    if v is None:
        return 0.0
    try:
        s = str(v).strip().replace(",", ".")
        if not s:
            return 0.0
        return float(s.split()[0] if s.split() else s)
    except (ValueError, TypeError):
        return 0.0


def _parse_lhm_json(data: Dict) -> Dict[str, Any]:
    """Parse LHM JSON; returns dict with float values and 'hdd' / 'fans' arrays."""
    results: Dict[str, Any] = {}
    path_to_val: Dict[str, float] = {}
    targets_set = set(TARGETS.values())
    it8688e_fans: List[Tuple[str, float]] = []
    it8688e_temps: List[Tuple[str, float]] = []
    gpu_memory_sensors: List[Tuple[str, float, str]] = []  # (sid, val, name_lower)
    fan_controls: List[int] = [0, 0, 0, 0]  # CPU, Pump/Sys#1, GPU, Case/Sys#2

    def walk(node: Any) -> None:
        if isinstance(node, list):
            for item in node:
                walk(item)
        elif isinstance(node, dict):
            sid = node.get("SensorId") or node.get("SensorID")
            raw = node.get("Value") or node.get("RawValue") or ""
            val = clean_val(raw)
            if sid:
                path_to_val[sid] = val
                if sid in targets_set:
                    for k, v in TARGETS.items():
                        if v == sid:
                            results[k] = val
                            break
                elif sid in TARGETS_ALIAS:
                    results[TARGETS_ALIAS[sid]] = val
                elif sid.startswith(IT8688E_PREFIX):
                    stype = (node.get("Type") or "").lower()
                    if "fan" in stype:
                        it8688e_fans.append((sid, val))
                    elif "temperature" in stype or "temp" in stype:
                        it8688e_temps.append((sid, val))
                    elif "control" in stype:
                        # Fan control %: /lpc/it8688e/0/control/0=CPU, /1=Pump/Sys#1, /2=Case/Sys#2
                        if "/control/0" in sid:
                            fan_controls[0] = int(val)
                        elif "/control/1" in sid:
                            fan_controls[1] = int(val)
                        elif "/control/2" in sid:
                            fan_controls[3] = int(val)
                # GPU fan control: /gpu-nvidia/0/control/1
                if sid and "/gpu-nvidia/0/control/1" in sid:
                    stype = (node.get("Type") or "").lower()
                    if "control" in stype:
                        fan_controls[2] = int(val)
                # Collect GPU memory sensors for name-based fallback (vu/vt)
                if ("/nvidiagpu/" in sid or "/gpu-nvidia/" in sid) and val > 0:
                    stype = (node.get("Type") or "").lower()
                    if "data" in stype or "smalldata" in stype:
                        name = (node.get("Name") or node.get("Text") or "").lower()
                        if "memory" in name or "used" in name or "total" in name or "limit" in name:
                            gpu_memory_sensors.append((sid, val, name))
            if "Children" in node:
                walk(node["Children"])
    walk(data)
    # VRAM fallback: if vu/vt not set by path, try to match by sensor name (values in MB; /1024 applied below)
    if "vu" not in results or "vt" not in results:
        for _sid, val, name in gpu_memory_sensors:
            if "used" in name and "vu" not in results:
                results["vu"] = val
            if ("total" in name or "limit" in name) and "vt" not in results:
                results["vt"] = val
    it8688e_fans.sort(key=lambda x: x[0])
    it8688e_temps.sort(key=lambda x: x[0])
    # Fans: CPU, Pump, GPU (LHM gpu-nvidia uses fan/1 not fan/0), Case (optional)
    fans: List[int] = []
    for path in FAN_PATHS:
        fans.append(int(path_to_val.get(path, 0)))
    fans.append(int(path_to_val.get(FAN_CASE_PATH, 0)))
    # GPU fan: serverpars has /gpu-nvidia/0/fan/1 for "GPU Fan" RPM (not fan/0)
    gf_val = int(path_to_val.get("/gpu-nvidia/0/fan/1", 0)) or (fans[2] if len(fans) > 2 else 0)
    if len(fans) > 2:
        fans[2] = gf_val
    results["fans"] = fans
    results["cf"] = fans[0] if len(fans) > 0 else 0
    results["s1"] = fans[1] if len(fans) > 1 else 0
    results["gf"] = gf_val
    results["s2"] = fans[3] if len(fans) > 3 else 0
    results["fan_controls"] = fan_controls
    if it8688e_temps:
        results["ch"] = it8688e_temps[0][1]
    # Motherboard temps: System, VSoC MOS, VRM MOS, Chipset
    results["mb_sys"] = int(path_to_val.get("/lpc/it8688e/0/temperature/0", 0))
    results["mb_vsoc"] = int(path_to_val.get("/lpc/it8688e/0/temperature/1", 0))
    results["mb_vrm"] = int(path_to_val.get("/lpc/it8688e/0/temperature/4", 0))
    results["mb_chipset"] = int(path_to_val.get("/lpc/it8688e/0/temperature/5", 0))
    if "ru" in results and "ra" in results:
        results["ra"] = results["ru"] + results["ra"]
    if "vu" in results:
        results["vu"] = round(results["vu"] / 1024.0, 1)
    if "vt" in results:
        results["vt"] = round(results["vt"] / 1024.0, 1)
    # Storage: collect all /hdd/N, /nvme/N, /ssd/N with data/31 (free), data/32 (total), temperature/0
    devices_seen: set = set()
    storage_devices: List[Tuple[str, int, float, float, float]] = []  # (prefix, num, used_gb, total_gb, temp)
    for sid in path_to_val:
        parts = sid.strip("/").split("/")
        if len(parts) >= 2 and parts[0] in ("hdd", "nvme", "ssd"):
            try:
                num = int(parts[1])
                prefix = parts[0]
                key = (prefix, num)
                if key in devices_seen:
                    continue
                devices_seen.add(key)
                free_gb = float(path_to_val.get(f"/{prefix}/{num}/data/31", 0) or 0)
                total_gb = float(path_to_val.get(f"/{prefix}/{num}/data/32", 0) or 0)
                temp = float(path_to_val.get(f"/{prefix}/{num}/temperature/0", 0) or 0)
                if total_gb > 0 or free_gb > 0:
                    used_gb = total_gb - free_gb if total_gb > 0 else 0.0
                    if used_gb < 0:
                        used_gb = 0.0
                    storage_devices.append((prefix, num, used_gb, total_gb, temp))
            except (ValueError, IndexError):
                pass
    storage_devices.sort(key=lambda x: (x[0], x[1]))
    drive_letters = ("C", "D", "E", "F")
    hdd: List[Dict[str, Any]] = []
    for idx in range(4):
        if idx < len(storage_devices):
            _pre, _num, used_gb, total_gb, temp = storage_devices[idx]
            hdd.append({
                "n": drive_letters[idx] if idx < len(drive_letters) else "?",
                "u": round(used_gb, 1),
                "tot": round(total_gb, 1),
                "t": int(temp),
            })
        else:
            hdd.append({"n": drive_letters[idx] if idx < len(drive_letters) else "?", "u": 0.0, "tot": 0.0, "t": 0})
    results["hdd"] = hdd
    return results


async def get_lhm_data_async(session: aiohttp.ClientSession) -> Dict[str, Any]:
    for attempt in range(3):
        try:
            async with session.get(LHM_URL, timeout=aiohttp.ClientTimeout(total=2)) as r:
                if r.status != 200:
                    continue
                return _parse_lhm_json(await r.json())
        except Exception as e:
            if attempt == 2:
                log_debug(f"LHM failed: {e}")
    return {}


def _weather_desc_from_code(code: int) -> str:
    if code == 0:
        return "Clear"
    if 1 <= code <= 3:
        return "Cloudy"
    if 45 <= code <= 48:
        return "Fog"
    if 51 <= code <= 67:
        return "Rain"
    if 71 <= code <= 86:
        return "Snow"
    if 80 <= code <= 82:
        return "Showers"
    if 95 <= code <= 99:
        return "Storm"
    return "Cloudy"


async def get_weather_async(session: aiohttp.ClientSession) -> Dict:
    global weather_cache, _weather_first_ok
    try:
        async with session.get(WEATHER_URL, timeout=aiohttp.ClientTimeout(total=WEATHER_TIMEOUT)) as r:
            if r.status != 200:
                return weather_cache
            data = await r.json()
        cur = data.get("current") or {}
        temp = int(cur.get("temperature_2m", 0) or 0)
        code = int(cur.get("weather_code", 0) or 0)
        weather_cache = {"temp": temp, "desc": _weather_desc_from_code(code)[:20], "icon": code}
        _weather_first_ok = True
        return weather_cache
    except Exception as e:
        log_debug(f"Weather: {e}")
    return weather_cache


def get_network_speed_sync() -> tuple:
    try:
        c = psutil.net_io_counters()
        now = time.time()
        if last_net_bytes["time"] == 0:
            last_net_bytes["sent"], last_net_bytes["recv"] = c.bytes_sent, c.bytes_recv
            last_net_bytes["time"] = now
            return 0, 0
        dt = now - last_net_bytes["time"]
        if dt < 0.1:
            return 0, 0
        up = int((c.bytes_sent - last_net_bytes["sent"]) / dt / 1024)
        down = int((c.bytes_recv - last_net_bytes["recv"]) / dt / 1024)
        last_net_bytes["sent"], last_net_bytes["recv"] = c.bytes_sent, c.bytes_recv
        last_net_bytes["time"] = now
        return max(0, up), max(0, down)
    except Exception:
        return 0, 0


def get_disk_speed_sync() -> tuple:
    try:
        c = psutil.disk_io_counters()
        if c is None:
            return 0, 0
        now = time.time()
        if last_disk_bytes["time"] == 0:
            last_disk_bytes["read"] = c.read_bytes
            last_disk_bytes["write"] = c.write_bytes
            last_disk_bytes["time"] = now
            return 0, 0
        dt = now - last_disk_bytes["time"]
        if dt < 0.1:
            return 0, 0
        r = int((c.read_bytes - last_disk_bytes["read"]) / dt / 1024)
        w = int((c.write_bytes - last_disk_bytes["write"]) / dt / 1024)
        last_disk_bytes["read"] = c.read_bytes
        last_disk_bytes["write"] = c.write_bytes
        last_disk_bytes["time"] = now
        return max(0, r), max(0, w)
    except Exception:
        return 0, 0


def get_ping_latency_sync() -> int:
    try:
        cmd = ["ping", "-n", "1", "-w", str(PING_TIMEOUT * 1000), PING_TARGET] if platform.system().lower() == "windows" else ["ping", "-c", "1", "-W", str(PING_TIMEOUT), PING_TARGET]
        kw: Dict[str, Any] = {"stdout": subprocess.PIPE, "stderr": subprocess.PIPE, "timeout": PING_TIMEOUT + 1, "text": True}
        if sys.platform == "win32":
            kw["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)
        r = subprocess.run(cmd, **kw)
        if r.returncode != 0:
            return 0
        out = r.stdout or ""
        if "time=" in out.lower():
            for part in out.split():
                if "time=" in part.lower():
                    return int(float(part.split("=")[1].replace("ms", "").strip()))
    except Exception:
        pass
    return 0


def get_top_processes_cpu_sync(n: int = 3) -> List[Dict]:
    try:
        procs = []
        for p in psutil.process_iter(["name", "cpu_percent"]):
            try:
                i = p.info
                if i.get("cpu_percent") and i["cpu_percent"] > 0:
                    procs.append({"n": (i.get("name") or "")[:20], "c": int(i["cpu_percent"])})
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        procs.sort(key=lambda x: x["c"], reverse=True)
        return procs[:n]
    except Exception:
        return []


def get_top_processes_ram_sync(n: int = 2) -> List[Dict]:
    try:
        procs = []
        for p in psutil.process_iter(["name", "memory_info"]):
            try:
                i = p.info
                name = (i.get("name") or "").strip()
                if "memcompression" in name.lower():
                    continue
                if i.get("memory_info"):
                    mb = i["memory_info"].rss / (1024 * 1024)
                    if mb > 10:
                        procs.append({"n": name[:20], "r": int(mb)})
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        procs.sort(key=lambda x: x["r"], reverse=True)
        return procs[:n]
    except Exception:
        return []


async def _get_media_info_async_impl() -> Dict:
    """Media: no Base64 art; only status PLAYING|PAUSED for procedural cassette animation."""
    if not HAS_WINSDK:
        return {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"}
    try:
        manager = await GlobalSystemMediaTransportControlsSessionManager.request_async()
        session = manager.get_current_session()
        if not session:
            return {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"}
        info = await session.try_get_media_properties_async()
        playback = session.get_playback_info()
        artist = (info.artist if info and info.artist else "")[:30]
        track = (info.title if info and info.title else "")[:30]
        is_playing = playback and getattr(playback, "playback_status", 0) == 4
        is_idle = bool(artist or track) and not is_playing
        media_status = "PLAYING" if is_playing else "PAUSED"
        return {"art": artist, "trk": track, "play": is_playing, "idle": is_idle, "media_status": media_status}
    except Exception:
        return {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"}


async def get_media_info(_loop: asyncio.AbstractEventLoop) -> Dict:
    """Winsdk runs in asyncio loop to avoid COM/threading issues."""
    try:
        return await _get_media_info_async_impl()
    except Exception as e:
        log_debug(f"Media: {e}")
    return {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"}


# Alert thresholds (RED ALERT when any is met; must match config.h)
CPU_TEMP_ALERT = 87
GPU_TEMP_ALERT = 68
CPU_LOAD_ALERT = 90
GPU_LOAD_ALERT = 100
VRAM_LOAD_ALERT = 95
RAM_LOAD_ALERT = 90   # unused when RAM_GB_ALERT is set; RAM alert by used GB
RAM_GB_ALERT = 30    # alert when used RAM >= 30 GB
# Hysteresis: clear alert only when value drops below (threshold - HYST_*)
CPU_TEMP_HYST = 5
GPU_TEMP_HYST = 5
LOAD_HYST = 5
RAM_LOAD_HYST = 5
RAM_GB_HYST = 2      # clear RAM alert when used < 28 GB
# Last active alert (target, metric) for hysteresis
_last_alert: tuple = (None, None)  # ("CPU"|"GPU"|"RAM", "ct"|"gt"|"cl"|"gl"|"gv"|"ram")


def build_payload(hw: Dict, media: Dict, weather: Dict, top_procs: List, top_procs_ram: List,
                  net: tuple, disk: tuple, ping_ms: int, now: float) -> Dict:
    global last_sent_track_key, _last_sent_snapshot, _last_heartbeat_time

    w = weather if _weather_first_ok else weather
    wt_val = w.get("temp", 0)
    wd_val = (w.get("desc") or "")[:20]
    ram_used_f = round(float(hw.get("ru", 0)), 1)
    ram_total_f = round(float(hw.get("ra", 0)), 1)
    media_status = media.get("media_status", "PAUSED")

    ct = int(hw.get("ct", 0))
    gt = int(hw.get("gt", 0))
    cl = int(hw.get("cl", 0))
    gl = int(hw.get("gl", 0))
    gv = int(hw.get("gv", 0))

    raw_hdd = hw.get("hdd", [])
    drive_letters = ("C", "D", "E", "F")
    hdd_list = []
    for i in range(4):
        if i < len(raw_hdd):
            e = raw_hdd[i]
            n = e.get("n") or drive_letters[i]
            u = e.get("u", 0.0)
            tot = e.get("tot", 0.0)
            t = e.get("t", 0)
            hdd_list.append({"n": n if isinstance(n, str) else drive_letters[i], "u": round(float(u), 1), "tot": round(float(tot), 1), "t": int(t)})
        else:
            hdd_list.append({"n": drive_letters[i], "u": 0.0, "tot": 0.0, "t": 0})

    payload = {
        "ct": ct, "gt": gt,
        "cl": cl, "gl": gl,
        "pw": int(hw.get("pw", 0)), "cc": int(hw.get("cc", 0)),
        "gh": int(hw.get("gh", 0)), "gv": gv,
        "gclock": int(hw.get("gclock", 0)), "vclock": int(hw.get("vclock", 0)),
        "gtdp": int(hw.get("gtdp", 0)),
        "ru": ram_used_f, "ra": ram_total_f,
        "nd": net[1], "nu": net[0], "pg": ping_ms,
        "cf": int(hw.get("cf", 0)), "s1": int(hw.get("s1", 0)), "s2": int(hw.get("s2", 0)), "gf": int(hw.get("gf", 0)),
        "fans": hw.get("fans", [0, 0, 0, 0]),
        "fan_controls": hw.get("fan_controls", [0, 0, 0, 0]),
        "hdd": hdd_list,
        "vu": round(float(hw.get("vu", 0)), 1), "vt": round(float(hw.get("vt", 0)), 1),
        "ch": int(hw.get("ch", 0)),
        "mb_sys": int(hw.get("mb_sys", 0)), "mb_vsoc": int(hw.get("mb_vsoc", 0)),
        "mb_vrm": int(hw.get("mb_vrm", 0)), "mb_chipset": int(hw.get("mb_chipset", 0)),
        "dr": disk[0], "dw": disk[1],
        "wt": wt_val, "wd": wd_val, "wi": int(w.get("icon", 0)),
        "tp": top_procs, "tr": top_procs_ram,
        "art": media.get("art", ""), "trk": media.get("trk", ""),
        "mp": media.get("play", False), "idle": media.get("idle", False),
        "media_status": media_status,
    }

    # RED ALERT: threshold with hysteresis; send alert_metric for value blink
    global _last_alert
    ram_pct = int((ram_used_f / ram_total_f * 100) if ram_total_f > 0 else 0)
    new_alert = None
    if ct >= CPU_TEMP_ALERT:
        new_alert = ("CPU", "ct")
    elif gt >= GPU_TEMP_ALERT:
        new_alert = ("GPU", "gt")
    elif cl >= CPU_LOAD_ALERT:
        new_alert = ("CPU", "cl")
    elif gl >= GPU_LOAD_ALERT:
        new_alert = ("GPU", "gl")
    elif gv >= VRAM_LOAD_ALERT:
        new_alert = ("GPU", "gv")
    elif ram_used_f >= RAM_GB_ALERT:
        new_alert = ("RAM", "ram")

    if new_alert:
        _last_alert = new_alert
        payload["alert"] = "CRITICAL"
        payload["target_screen"] = new_alert[0]
        payload["alert_metric"] = new_alert[1]
    else:
        # Clear only when current alert metric is below (threshold - hysteresis)
        if _last_alert:
            target, metric = _last_alert
            clear = False
            if metric == "ct":
                clear = ct < CPU_TEMP_ALERT - CPU_TEMP_HYST
            elif metric == "gt":
                clear = gt < GPU_TEMP_ALERT - GPU_TEMP_HYST
            elif metric == "cl":
                clear = cl < CPU_LOAD_ALERT - LOAD_HYST
            elif metric == "gl":
                clear = gl < GPU_LOAD_ALERT - LOAD_HYST
            elif metric == "gv":
                clear = gv < VRAM_LOAD_ALERT - LOAD_HYST
            elif metric == "ram":
                clear = ram_used_f < RAM_GB_ALERT - RAM_GB_HYST
            else:
                clear = True
            if clear:
                _last_alert = (None, None)
        if not _last_alert[0]:
            payload["alert"] = ""
            payload["target_screen"] = ""
            payload["alert_metric"] = ""
        else:
            payload["alert"] = "CRITICAL"
            payload["target_screen"] = _last_alert[0]
            payload["alert_metric"] = _last_alert[1]
    return payload


def _payload_snapshot(p: Dict) -> Tuple:
    return (p.get("ct", 0), p.get("gt", 0), p.get("cl", 0), p.get("gl", 0),
            p.get("nd", 0), p.get("nu", 0), p.get("ru", 0), p.get("ra", 0))


def should_send_payload(payload: Dict, now: float) -> bool:
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
    if cur[6] != prev[6] or cur[7] != prev[7]:
        return True
    return False


async def send_data_to_client(writer, payload: Dict) -> bool:
    try:
        data = json.dumps(payload, separators=(",", ":")) + "\n"
        writer.write(data.encode("utf-8"))
        await writer.drain()
        return True
    except Exception as e:
        log_debug(f"Send: {e}")
        return False


async def handle_client(reader, writer):
    addr = writer.get_extra_info("peername")
    log_info(f"Client connected: {addr}")
    tcp_clients.append(writer)
    client_screens[writer] = 0
    client_buffers[writer] = ""
    try:
        if global_data_cache["hw"]:
            payload = build_payload(
                global_data_cache["hw"], global_data_cache["media"],
                global_data_cache["weather"], global_data_cache["top_procs"],
                global_data_cache["top_procs_ram"], global_data_cache["net"],
                global_data_cache["disk"], global_data_cache["ping"], time.time()
            )
            await send_data_to_client(writer, payload)
        else:
            await send_data_to_client(writer, {"ct": 0, "gt": 0, "cl": 0, "gl": 0, "ru": 0, "ra": 0})
    except Exception as e:
        log_debug(f"Initial send: {e}")
    try:
        while not stop_requested:
            try:
                data = await asyncio.wait_for(reader.read(256), timeout=1.0)
            except asyncio.TimeoutError:
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
                    if line == "HELO":
                        continue
                    if line.startswith("screen:"):
                        try:
                            client_screens[writer] = int(line.split(":", 1)[1])
                        except (ValueError, IndexError):
                            pass
            except UnicodeDecodeError:
                client_buffers[writer] = ""
    except Exception as e:
        log_debug(f"Client: {e}")
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
        log_info(f"Client disconnected: {addr}")


async def run():
    global executor, last_sent_track_key, top_procs_cache, top_procs_ram_cache
    global last_top_procs_time, ping_latency_ms, global_data_cache
    global _last_sent_snapshot, _last_heartbeat_time

    server = None
    executor = ThreadPoolExecutor(max_workers=6)
    log_info(f"Starting TCP server on {TCP_HOST}:{TCP_PORT}")
    await asyncio.sleep(2)  # allow network/services to be ready (e.g. at autostart)

    try:
        server = await asyncio.start_server(handle_client, TCP_HOST, TCP_PORT)
    except OSError as e:
        log_err(f"Bind failed: {e}")
        raise

    log_info("Server ready.")
    loop = asyncio.get_event_loop()
    last_lhm_time = 0.0
    last_weather_time = 0.0
    last_ping_time = 0.0
    session = aiohttp.ClientSession()

    try:
        global_data_cache["hw"] = await get_lhm_data_async(session)
        global_data_cache["weather"] = await get_weather_async(session)
        global_data_cache["net"] = await loop.run_in_executor(executor, get_network_speed_sync)
        global_data_cache["disk"] = await loop.run_in_executor(executor, get_disk_speed_sync)
        global_data_cache["media"] = await get_media_info(loop)
        top_procs_cache = await loop.run_in_executor(executor, lambda: get_top_processes_cpu_sync(TOP_PROCS_CPU_N))
        top_procs_ram_cache = await loop.run_in_executor(executor, lambda: get_top_processes_ram_sync(TOP_PROCS_RAM_N))
        global_data_cache["top_procs"] = top_procs_cache
        global_data_cache["top_procs_ram"] = top_procs_ram_cache
        last_top_procs_time = time.time()
        ping_latency_ms = await loop.run_in_executor(executor, get_ping_latency_sync)
        global_data_cache["ping"] = ping_latency_ms
    except Exception as e:
        log_err(f"Initial data: {e}")

    try:
        while not stop_requested:
            now = time.time()
            if now - last_lhm_time >= POLL_INTERVAL:
                last_lhm_time = now
                try:
                    hw_data = await get_lhm_data_async(session)
                    if hw_data:
                        global_data_cache["hw"] = hw_data
                    global_data_cache["net"] = await loop.run_in_executor(executor, get_network_speed_sync)
                    global_data_cache["disk"] = await loop.run_in_executor(executor, get_disk_speed_sync)
                except Exception as e:
                    log_debug(f"Poll: {e}")

            if now - last_top_procs_time >= TOP_PROCS_CACHE_TTL:
                try:
                    top_procs_cache = await loop.run_in_executor(executor, lambda: get_top_processes_cpu_sync(TOP_PROCS_CPU_N))
                    top_procs_ram_cache = await loop.run_in_executor(executor, lambda: get_top_processes_ram_sync(TOP_PROCS_RAM_N))
                    last_top_procs_time = now
                    global_data_cache["top_procs"] = top_procs_cache
                    global_data_cache["top_procs_ram"] = top_procs_ram_cache
                except Exception as e:
                    log_debug(f"Procs: {e}")

            if now - last_weather_time >= WEATHER_UPDATE_INTERVAL:
                last_weather_time = now
                try:
                    global_data_cache["weather"] = await get_weather_async(session)
                except Exception as e:
                    log_debug(f"Weather: {e}")

            if now - last_ping_time >= 5.0:
                last_ping_time = now
                try:
                    ping_latency_ms = await loop.run_in_executor(executor, get_ping_latency_sync)
                    global_data_cache["ping"] = ping_latency_ms
                except Exception as e:
                    log_debug(f"Ping: {e}")

            try:
                global_data_cache["media"] = await get_media_info(loop)
            except Exception as e:
                log_debug(f"Media: {e}")

            payload = build_payload(
                global_data_cache["hw"], global_data_cache["media"],
                global_data_cache["weather"], global_data_cache["top_procs"],
                global_data_cache["top_procs_ram"], global_data_cache["net"],
                global_data_cache["disk"], global_data_cache["ping"], now
            )

            if should_send_payload(payload, now):
                _last_heartbeat_time = now
                _last_sent_snapshot = _payload_snapshot(payload)
                dead = []
                for w in list(tcp_clients):
                    if not await send_data_to_client(w, payload):
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

            await asyncio.sleep(0.5)
    finally:
        await session.close()

    if executor:
        executor.shutdown(wait=False)
    for t in asyncio.all_tasks():
        if t is not asyncio.current_task():
            t.cancel()
    if server:
        server.close()
        await server.wait_closed()


def _run_async_worker() -> None:
    """Background thread: runs the asyncio server loop."""
    try:
        asyncio.run(run())
    except Exception as e:
        log_err(f"Server thread: {e}", e)


def _create_tray_image() -> "Image.Image":
    """Programmatic 64x64 icon: black background, green 'W' / dot. No external .ico."""
    from PIL import Image, ImageDraw
    img = Image.new("RGB", (64, 64), (0, 0, 0))
    draw = ImageDraw.Draw(img)
    green = (0, 255, 0)
    # Simple "W" shape (4 strokes): (x0, y0, x1, y1)
    for (ax, ay, bx, by) in [
        (12, 50, 22, 14), (22, 14, 32, 36), (32, 36, 42, 14), (42, 14, 52, 50),
    ]:
        draw.line([(ax, ay), (bx, by)], fill=green, width=3)
    # Center dot as "live" indicator
    draw.ellipse([29, 29, 35, 35], fill=green)
    return img


def _on_restart(icon: "pystray.Icon", item: Any) -> None:
    global stop_requested
    stop_requested = True
    t = _server_thread_holder[0]
    if t and t.is_alive():
        t.join(timeout=5.0)
    stop_requested = False
    _server_thread_holder[0] = threading.Thread(target=_run_async_worker, daemon=True)
    _server_thread_holder[0].start()
    time.sleep(2)
    if not _server_thread_holder[0].is_alive():
        log_err("Restart: server thread died immediately. Check nocturne.log for exception.")
    else:
        log_info("Server restarted.")


def _on_exit(icon: "pystray.Icon", item: Any) -> None:
    global stop_requested
    stop_requested = True
    icon.stop()


def _on_toggle_autostart(icon: "pystray.Icon", item: Any) -> None:
    set_autostart(not is_autostart_enabled())
    try:
        icon.update_menu(_build_tray_menu())
    except Exception:
        pass


def _build_tray_menu() -> "pystray.Menu":
    autostart_label = "Remove from startup" if is_autostart_enabled() else "Add to startup"
    return pystray.Menu(
        pystray.MenuItem("Status: RUNNING", None, enabled=False),
        pystray.Menu.SEPARATOR,
        pystray.MenuItem(autostart_label, _on_toggle_autostart),
        pystray.MenuItem("Restart Server", _on_restart),
        pystray.MenuItem("Close", _on_exit),
    )


def run_with_tray() -> None:
    """Main thread: pystray icon (blocking). Background thread: asyncio server."""
    if not HAS_PYSTRAY or not HAS_PIL:
        log_info("Tray skipped (pystray/Pillow missing); running in console.")
        asyncio.run(run())
        return
    _server_thread_holder[0] = threading.Thread(target=_run_async_worker, daemon=True)
    _server_thread_holder[0].start()
    time.sleep(0.5)  # let server bind
    icon = pystray.Icon("nocturne", _create_tray_image(), "NOCTURNE_OS", _build_tray_menu())
    log_info("NOCTURNE_OS — Server in system tray. Exit via tray -> Close.")
    icon.run()


if __name__ == "__main__":
    use_console = "--no-tray" in sys.argv or "--console" in sys.argv
    _setup_logging(console=use_console)
    log_info("NOCTURNE_OS — PC Monitor Server (media_status only)")
    if not use_console:
        log_info(f"Log file: {_LOG_FILE}")
    try:
        if use_console:
            asyncio.run(run())
        else:
            run_with_tray()
    except KeyboardInterrupt:
        log_info("Shutting down...")
    except Exception as e:
        log_err(f"Fatal: {e}", e)
        sys.exit(1)
    log_info("Goodbye.")
