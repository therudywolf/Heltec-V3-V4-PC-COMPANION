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

# Extracted submodules (kept in the same server/ dir; flat imports work both when
# tests put server/ on sys.path and when monitor.py is run directly). monitor.py
# remains the public entry point and re-exports the names tests rely on.
import claude_usage as claude_mod
import claude_sessions
import lhm_parse
import netrates
import alert_events
import alertmanager_poll
import claude_budget
import forest_panel
import nvidia_smi
import payload as payload_mod
import prometheus_source
import weather as weather_mod

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
# Logging: when frozen, next to exe; otherwise in project root (parent of server/).
# ---------------------------------------------------------------------------
_SERVER_DIR = os.path.dirname(os.path.abspath(__file__))
_PROJECT_ROOT = os.path.dirname(_SERVER_DIR)
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
    # Prefer config in server/ folder, then project root
    for candidate in [os.path.join(_SERVER_DIR, "config.json"), os.path.join(_PROJECT_ROOT, "config.json")]:
        if os.path.isfile(candidate):
            return candidate
    return os.path.join(_SERVER_DIR, "config.json")

CONFIG_PATH = _get_config_path()

def load_config() -> Dict:
    out = {
        "host": "0.0.0.0",
        "port": 8090,
        "lhm_url": "http://localhost:8085/data.json",
        "limits": {"gpu": 80, "cpu": 75},
        "weather_city": "Moscow",
        "lhm_storage_fallback": "psutil",
        # Telemetry source: "lhm" (default) or "prometheus" (hybrid: Prometheus
        # fills load/ram/disk/net, LHM still provides temps/GPU/fans).
        "source": "lhm",
        "prometheus_url": "http://localhost:9182/metrics",
        # Prometheus Alertmanager webhook receiver port (0 = disabled). POST the
        # Alertmanager webhook JSON to http://<host>:<port>/alert.
        "alert_webhook_port": 0,
        # Claude usage budgets (tokens) for the PC-monitor Claude gauge + the
        # "80% reminder" alert. 0 disables that gauge. window=daily, weekly=7-day.
        "claude_daily_budget": claude_budget.DEFAULT_DAILY_BUDGET,
        "claude_weekly_budget": claude_budget.DEFAULT_WEEKLY_BUDGET,
        "claude_alert_pct": claude_budget.DEFAULT_ALERT_PCT,
        # Overlay live NVIDIA GPU metrics (temp/load/VRAM/power/fan) from
        # nvidia-smi — zero-install, fills what windows_exporter cannot. true/false.
        "gpu_via_nvidia_smi": True,
        # Forest panel: query node status from a Prometheus-compatible URL
        # (e.g. a Grafana datasource proxy). "" disables (scene shows NO NODES).
        "forest_query_url": "",
        # Alertmanager v2 alerts URL to POLL (shows the SAME alerts as your
        # Telegram). "" disables polling (local thresholds + webhook still work).
        "alertmanager_url": "",
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
        if "lhm_storage_fallback" in data:
            out["lhm_storage_fallback"] = data["lhm_storage_fallback"]
        if "source" in data:
            out["source"] = data["source"]
        if "prometheus_url" in data:
            out["prometheus_url"] = data["prometheus_url"]
        if "alert_webhook_port" in data:
            out["alert_webhook_port"] = int(data["alert_webhook_port"])
        if "claude_daily_budget" in data:
            out["claude_daily_budget"] = int(data["claude_daily_budget"])
        if "claude_weekly_budget" in data:
            out["claude_weekly_budget"] = int(data["claude_weekly_budget"])
        if "claude_alert_pct" in data:
            out["claude_alert_pct"] = int(data["claude_alert_pct"])
        if "gpu_via_nvidia_smi" in data:
            out["gpu_via_nvidia_smi"] = bool(data["gpu_via_nvidia_smi"])
        if "forest_query_url" in data:
            out["forest_query_url"] = data["forest_query_url"]
        if "alertmanager_url" in data:
            out["alertmanager_url"] = data["alertmanager_url"]
        if "forest_nodes" in data and isinstance(data["forest_nodes"], list):
            out["forest_nodes"] = data["forest_nodes"]
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
    except Exception as e:
        logging.warning("autostart check failed: %s", e)
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
SOURCE = os.getenv("SOURCE", _config.get("source", "lhm"))
PROMETHEUS_URL = os.getenv("PROMETHEUS_URL", _config.get("prometheus_url", ""))
ALERT_WEBHOOK_PORT = int(os.getenv("ALERT_WEBHOOK_PORT", str(_config.get("alert_webhook_port", 0))))
CLAUDE_DAILY_BUDGET = int(_config.get("claude_daily_budget", claude_budget.DEFAULT_DAILY_BUDGET))
CLAUDE_WEEKLY_BUDGET = int(_config.get("claude_weekly_budget", claude_budget.DEFAULT_WEEKLY_BUDGET))
CLAUDE_ALERT_PCT = int(_config.get("claude_alert_pct", claude_budget.DEFAULT_ALERT_PCT))
GPU_VIA_NVIDIA_SMI = str(os.getenv("GPU_VIA_NVIDIA_SMI", str(_config.get("gpu_via_nvidia_smi", True)))).lower() in ("1", "true", "yes")
FOREST_QUERY_URL = os.getenv("FOREST_QUERY_URL", _config.get("forest_query_url", ""))
ALERTMANAGER_URL = os.getenv("ALERTMANAGER_URL", _config.get("alertmanager_url", ""))
FOREST_NODES = _config.get("forest_nodes") or forest_panel.DEFAULT_NODES
FOREST_UPDATE_INTERVAL = 30.0   # node-status poll cadence (seconds)
ALERTMANAGER_UPDATE_INTERVAL = 20.0  # Alertmanager poll cadence (seconds)
TCP_HOST = os.getenv("TCP_HOST", _config["host"])
TCP_PORT = int(os.getenv("TCP_PORT", str(_config["port"])))
WEATHER_LAT = os.getenv("WEATHER_LAT", "55.7558")
WEATHER_LON = os.getenv("WEATHER_LON", "37.6173")
WEATHER_URL = weather_mod.build_weather_url(WEATHER_LAT, WEATHER_LON)
WEATHER_TIMEOUT = 10
WEATHER_UPDATE_INTERVAL = 10 * 60
PING_TARGET = "8.8.8.8"
PING_TIMEOUT = 2
COVER_SIZE = 64
TOP_PROCS_CPU_N = 3
TOP_PROCS_RAM_N = 2
CLIENT_LINE_MAX = 4096
# Max JSON line size sent to device (must not exceed firmware NOCT_TCP_LINE_MAX)
MAX_PAYLOAD_BYTES = 4096
SERVER_VERSION = "1.0"  # Sent in payload "sv" for client/debug
TOP_PROCS_CACHE_TTL = 2.5
POLL_INTERVAL = 0.5
HEARTBEAT_INTERVAL = 0.5  # Send at least every 0.5s so display updates twice per second
MEDIA_UPDATE_INTERVAL = 1.0  # Update media info every 1 second (reduces CPU load)
CLAUDE_UPDATE_INTERVAL = 30.0  # Claude usage changes slowly; poll its local files every 30s
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
top_procs_cache: List = []
top_procs_ram_cache: List = []
last_top_procs_time: float = 0.0
last_media_time: float = 0.0
_media_manager: Optional[Any] = None  # Cached GlobalSystemMediaTransportControlsSessionManager
last_net_bytes = {"sent": 0, "recv": 0, "time": 0.0}
last_disk_bytes = {"read": 0, "write": 0, "time": 0.0}
ping_latency_ms = 0
global_data_cache: Dict = {
    "hw": {}, "weather": {},
    "media": {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"},
    "top_procs": [], "top_procs_ram": [], "net": (0, 0), "disk": (0, 0), "ping": 0,
    "claude": dict(claude_mod.EMPTY_USAGE),
}
# Guards multi-key access to global_data_cache so the poller's writes and a
# connecting client's read can't interleave into an inconsistent snapshot.
# Created lazily inside run() (a fresh Lock per asyncio.run, so Restart — which
# spins a new event loop — never reuses a Lock bound to a dead loop).
cache_lock: Optional[asyncio.Lock] = None
_last_sent_snapshot: Optional[Tuple] = None
_last_heartbeat_time: float = 0.0


async def cache_set(key: str, value: Any) -> None:
    """Atomically store one ``global_data_cache`` entry under ``cache_lock``."""
    if cache_lock is None:
        global_data_cache[key] = value
        return
    async with cache_lock:
        global_data_cache[key] = value


async def cache_snapshot() -> Dict[str, Any]:
    """Return a shallow copy of ``global_data_cache`` taken under ``cache_lock``.

    A shallow copy is enough: callers only read the top-level entries, and each
    entry is replaced wholesale by the poller (never mutated in place), so the
    snapshot is internally consistent for one ``build_payload`` call.
    """
    if cache_lock is None:
        return dict(global_data_cache)
    async with cache_lock:
        return dict(global_data_cache)


def log_err(msg: str, exc: Optional[BaseException] = None) -> None:
    try:
        logging.error(msg, exc_info=exc is not None)
    except Exception:
        pass


def log_info(msg: str) -> None:
    logging.info(msg)


def log_debug(msg: str) -> None:
    logging.debug(msg)


# clean_val / _get_any_key now live in lhm_parse (single source of truth);
# re-exported here so existing references keep working.
clean_val = lhm_parse.clean_val
_get_any_key = lhm_parse.get_any_key


def _parse_lhm_json(data: Dict) -> Dict[str, Any]:
    """Parse LHM JSON; returns dict with float values and 'hdd' / 'fans' arrays.

    Thin wrapper that wires the focused helpers in :mod:`lhm_parse` together,
    passing the sensor-path constants defined in this module. Behaviour is
    identical to the former in-line implementation, including the DEBUG-level
    storage log.
    """
    walked = lhm_parse.walk_sensors(data, TARGETS, TARGETS_ALIAS, IT8688E_PREFIX)
    results: Dict[str, Any] = walked["results"]
    path_to_val: Dict[str, float] = walked["path_to_val"]

    # VRAM used/total name-based fallback (values still in MB at this point).
    lhm_parse.apply_vram_fallback(results, walked["gpu_memory_sensors"])

    # Fans (CPU/Pump/GPU/Case) and their short keys.
    results.update(lhm_parse.extract_fans(path_to_val, FAN_PATHS, FAN_CASE_PATH))
    results["fan_controls"] = walked["fan_controls"]

    # Chipset temp = lowest IT8688E temperature sensor (sorted by sensor id).
    it8688e_temps = walked["it8688e_temps"]
    it8688e_temps.sort(key=lambda x: x[0])
    if it8688e_temps:
        results["ch"] = it8688e_temps[0][1]

    # Motherboard temps + derived RAM total and VRAM MB->GB scaling.
    results.update(lhm_parse.extract_mb_temps(path_to_val))
    lhm_parse.finalize_units(results)

    # Storage devices -> 4-slot hdd list (psutil capacity merge happens later in
    # build_payload).
    storage_devices = lhm_parse.extract_storage_devices(path_to_val)
    if os.getenv("DEBUG", "0") == "1":
        log_debug(f"LHM storage: {len(storage_devices)} devices: {storage_devices}")
    results["hdd"] = lhm_parse.build_hdd_list(storage_devices)
    return results


def _parse_lhm_json_from_text(text: str) -> Dict[str, Any]:
    """Deserialize raw LHM ``/data.json`` text and parse it into the short-key dict.

    Both ``json.loads`` and the recursive :func:`_parse_lhm_json` tree-walk are
    pure-Python CPU work; bundling them here lets the caller run the whole step in
    the executor so it never blocks the asyncio event loop (see OPTIMIZATION_NOTES
    5.5). Reads only module constants / ``os.getenv`` — safe off-thread. Behaviour
    is identical to the former inline ``_parse_lhm_json(await r.json())``.
    """
    return _parse_lhm_json(json.loads(text))


async def _get_lhm_raw_async(session: aiohttp.ClientSession) -> Dict[str, Any]:
    loop = asyncio.get_event_loop()
    for attempt in range(3):
        try:
            async with session.get(LHM_URL, timeout=aiohttp.ClientTimeout(total=2)) as r:
                if r.status != 200:
                    continue
                text = await r.text()
            # JSON deserialize + tree-walk are CPU-bound; keep them off the loop.
            return await loop.run_in_executor(executor, _parse_lhm_json_from_text, text)
        except Exception as e:
            if attempt == 2:
                log_debug(f"LHM failed: {e}")
    return {}


async def get_lhm_data_async(session: aiohttp.ClientSession) -> Dict[str, Any]:
    """Build the hardware dict from the configured sources, layered cheaply.

    Layering (each layer only fills keys it provides; missing layers no-op):
      * BASE = LibreHardwareMonitor (LHM web server on lhm_url). LHM alone covers
        EVERYTHING — CPU/GPU temps, loads, clocks, fan RPM, VRM/board temps, disk
        temps, RAM. If you run LHM, this is all you need.
      * If ``source=="prometheus"`` (hybrid without LHM, or to offload LHM):
        overlay CPU-load/RAM/disk/net from the Grafana-Agent windows_exporter.
        Skip this if you rely on LHM — it's redundant then.
      * If ``gpu_via_nvidia_smi`` (default on): overlay live GPU metrics from
        nvidia-smi. Redundant if LHM is up (LHM has GPU too) but harmless/cheap;
        set it false to avoid the extra subprocess once LHM is your source.

    Recommended configs:
      * LHM only (least overhead, full data): source="lhm", gpu_via_nvidia_smi=false
      * No-LHM hybrid:                        source="prometheus", gpu_via_nvidia_smi=true
    A down source costs one fast failed fetch, so mixing is safe but wasteful."""
    loop = asyncio.get_event_loop()
    hw = await _get_lhm_raw_async(session)
    if SOURCE == "prometheus":
        overlay = await get_prometheus_overlay_async(session)
        if overlay:
            hw = prometheus_source.merge_hw(hw, overlay)
    if GPU_VIA_NVIDIA_SMI:
        gpu = await loop.run_in_executor(executor, nvidia_smi.query_gpu_sync)
        if gpu:
            hw = prometheus_source.merge_hw(hw, gpu)
    return hw


async def _start_alert_webhook() -> None:
    """Start the Prometheus Alertmanager webhook receiver, if configured.

    Listens on ``alert_webhook_port`` (config / ALERT_WEBHOOK_PORT, default off).
    POST the Alertmanager webhook JSON to ``/alert``; firing alerts then appear
    in the device payload ``events`` block. Best-effort: any failure is logged and
    the rest of the server runs normally. Uses aiohttp.web (aiohttp already a dep).
    """
    port = ALERT_WEBHOOK_PORT
    if not port:
        return
    try:
        from aiohttp import web

        async def handle_alert(request):
            try:
                body = await request.json()
            except Exception:
                return web.json_response({"ok": False, "error": "bad json"}, status=400)
            _alert_state.ingest(alert_events.parse_alertmanager(body))
            return web.json_response({"ok": True})

        app = web.Application()
        app.router.add_post("/alert", handle_alert)
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, "0.0.0.0", port)
        await site.start()
        log_info(f"Alert webhook on :{port}/alert")
    except Exception as e:
        log_err(f"Alert webhook failed to start: {e}")


async def get_prometheus_overlay_async(session: aiohttp.ClientSession) -> Dict[str, Any]:
    """Scrape windows_exporter /metrics and map to the hw-key overlay.

    Returns {} on any failure (so the merge is a no-op and LHM stands alone).
    Pure parsing lives in prometheus_source; this is just the fetch.
    """
    if not PROMETHEUS_URL:
        return {}
    try:
        async with session.get(PROMETHEUS_URL, timeout=aiohttp.ClientTimeout(total=2)) as r:
            if r.status != 200:
                return {}
            text = await r.text()
        return prometheus_source.build_hw_from_exposition(
            text, drive_letters=payload_mod.DRIVE_LETTERS
        )
    except Exception as e:
        log_debug(f"Prometheus failed: {e}")
        return {}


def _scalar_from_prom_result(data: Any) -> Optional[float]:
    """Extract one float from a Prometheus instant-query JSON response.

    Accepts the standard ``{"data":{"result":[{"value":[ts,"<num>"]}]}}`` shape
    (vector or scalar). Returns the first series' value, or None on miss/bad data.
    """
    try:
        result = (data or {}).get("data", {}).get("result", [])
        if not result:
            return None
        first = result[0]
        # vector/matrix -> {"value":[ts, "v"]}; scalar -> {"value":[ts,"v"]} too
        val = first.get("value") if isinstance(first, dict) else first
        if isinstance(val, (list, tuple)) and len(val) >= 2:
            return float(val[1])
    except (ValueError, TypeError, AttributeError, IndexError):
        return None
    return None


async def _prom_query_async(session: aiohttp.ClientSession, expr: str) -> Optional[float]:
    """Run a single PromQL instant query against FOREST_QUERY_URL; scalar or None.

    FOREST_QUERY_URL is a Prometheus-compatible ``/api/v1/query`` endpoint (e.g. a
    Grafana datasource proxy). Best-effort: returns None on any failure.
    """
    if not FOREST_QUERY_URL:
        return None
    try:
        async with session.get(
            FOREST_QUERY_URL, params={"query": expr},
            timeout=aiohttp.ClientTimeout(total=4),
        ) as r:
            if r.status != 200:
                return None
            data = await r.json(content_type=None)
        return _scalar_from_prom_result(data)
    except Exception as e:
        log_debug(f"Forest query failed ({expr[:32]}…): {e}")
        return None


async def get_forest_block_async(session: aiohttp.ClientSession) -> Dict[str, Any]:
    """Poll each forest node's PromQL and build the ``forest`` payload block.

    Runs every node's cpu/ram/disk expressions concurrently through the Grafana
    datasource proxy, then aggregates via forest_panel. Returns EMPTY_FOREST when
    no query URL is configured or every node is unreachable.
    """
    if not FOREST_QUERY_URL:
        return dict(forest_panel.EMPTY_FOREST)

    # Collect the unique expressions across all nodes, query them concurrently,
    # then look each up by expression when building nodes (avoids re-querying a
    # shared expr and keeps build_nodes_from_queries pure/sync).
    exprs = {e for d in FOREST_NODES for e in (d.get("cpu"), d.get("ram"), d.get("disk")) if e}
    expr_list = list(exprs)
    results = await asyncio.gather(*[_prom_query_async(session, e) for e in expr_list])
    by_expr = dict(zip(expr_list, results))
    nodes = forest_panel.build_nodes_from_queries(FOREST_NODES, lambda e: by_expr.get(e))
    return forest_panel.build_forest_block(nodes)


async def poll_alertmanager_async(session: aiohttp.ClientSession) -> None:
    """Poll the Alertmanager v2 API and feed firing alerts into the events block.

    Surfaces the SAME alerts that go to Telegram (dashboard.example.com stack). Replaces
    each poll's view of external alerts via AlertState.replace, so resolved/cleared
    alerts disappear. No-op when ALERTMANAGER_URL is unset. Best-effort.
    """
    if not ALERTMANAGER_URL:
        return
    try:
        async with session.get(
            ALERTMANAGER_URL, timeout=aiohttp.ClientTimeout(total=4),
        ) as r:
            if r.status != 200:
                return
            data = await r.json(content_type=None)
    except Exception as e:
        log_debug(f"Alertmanager poll failed: {e}")
        return
    events = alertmanager_poll.normalize_am_v2(data)
    firing = [e for e in events if e.get("status") == "firing"]
    _alert_state.replace(firing)


# Weather code -> short description now lives in weather.py; re-exported.
_weather_desc_from_code = weather_mod.weather_desc_from_code


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
        # Returns (up_kbps, down_kbps); shared delta-rate logic in netrates.
        return netrates.compute_delta_rate(
            c.bytes_sent, c.bytes_recv, last_net_bytes, time.time()
        )
    except Exception as e:
        log_debug(f"Net speed: {e}")
        return 0, 0


def get_disks_psutil_fallback() -> List[Dict[str, Any]]:
    """Fallback when LHM returns no disk data. Uses psutil for used/total GB (temp=0)."""
    drive_letters = ("C", "D", "E", "F")
    out: List[Dict[str, Any]] = []
    try:
        for part in psutil.disk_partitions(all=False):
            fstype = (part.fstype or "").lower()
            if "cdrom" in fstype:
                continue
            if sys.platform == "win32":
                opts = (part.opts or "").lower()
                if "fixed" not in opts:
                    continue
            mp = part.mountpoint
            letter = drive_letters[len(out)] if len(out) < len(drive_letters) else "?"
            if sys.platform == "win32" and len(mp) >= 2 and mp[1] == ":":
                letter = mp[0].upper()
            try:
                usage = psutil.disk_usage(mp)
                used_gb = round(usage.used / (1024 ** 3), 1)
                total_gb = round(usage.total / (1024 ** 3), 1)
                out.append({"n": letter, "u": used_gb, "tot": total_gb, "t": 0})
            except (PermissionError, OSError):
                pass
            if len(out) >= 4:
                break
    except Exception as e:
        logging.warning("psutil disk fallback failed: %s", e)
    while len(out) < 4:
        out.append({"n": drive_letters[len(out)], "u": 0.0, "tot": 0.0, "t": 0})
    return out[:4]


def get_disk_speed_sync() -> tuple:
    try:
        c = psutil.disk_io_counters()
        if c is None:
            return 0, 0
        # Returns (read_kbps, write_kbps); shared delta-rate logic in netrates.
        return netrates.compute_delta_rate(
            c.read_bytes, c.write_bytes, last_disk_bytes, time.time()
        )
    except Exception as e:
        log_debug(f"Disk speed: {e}")
        return 0, 0


def get_ping_latency_sync() -> int:
    """Blocking single ping -> latency in ms (0 on failure).

    This is intentionally synchronous and MUST stay off the event loop: callers
    invoke it via ``loop.run_in_executor(...)`` so the ~1-3s ``subprocess.run``
    never blocks the asyncio loop. Output parsing lives in
    :func:`netrates.parse_ping_latency`.
    """
    try:
        cmd = ["ping", "-n", "1", "-w", str(PING_TIMEOUT * 1000), PING_TARGET] if platform.system().lower() == "windows" else ["ping", "-c", "1", "-W", str(PING_TIMEOUT), PING_TARGET]
        kw: Dict[str, Any] = {"stdout": subprocess.PIPE, "stderr": subprocess.PIPE, "timeout": PING_TIMEOUT + 1, "text": True}
        if sys.platform == "win32":
            kw["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)
        r = subprocess.run(cmd, **kw)
        if r.returncode != 0:
            return 0
        rtt = netrates.parse_ping_latency(r.stdout or "")
        return rtt if rtt is not None else 0
    except Exception as e:
        log_debug(f"Ping: {e}")
        return 0


def _collect_top_processes(
    procs_info: List[Dict[str, Any]], cpu_n: int, ram_n: int
) -> Tuple[List[Dict], List[Dict]]:
    """Build the top-CPU and top-RAM lists from one batch of process ``info`` dicts.

    Pure (no psutil calls) so it is unit-testable: ``procs_info`` is the list of
    ``Process.info`` mappings (each may carry ``name`` / ``cpu_percent`` /
    ``memory_info``) gathered by a single :func:`psutil.process_iter` pass.

    Filtering/shaping/sorting reproduce the former two separate functions exactly:

    * CPU: keep ``cpu_percent`` truthy and ``> 0`` -> ``{"n": name[:20], "c": int}``
      (name **not** stripped), sorted by ``c`` desc, top ``cpu_n``.
    * RAM: skip ``memcompression`` (case-insensitive on the stripped name), require
      ``memory_info``, keep RSS MB ``> 10`` -> ``{"n": name[:20], "r": int(mb)}``
      (name stripped), sorted by ``r`` desc, top ``ram_n``.
    """
    cpu_procs: List[Dict] = []
    ram_procs: List[Dict] = []
    for i in procs_info:
        # CPU list (uses the raw, unstripped name — identical to the original).
        cpu = i.get("cpu_percent")
        if cpu and cpu > 0:
            cpu_procs.append({"n": (i.get("name") or "")[:20], "c": int(cpu)})
        # RAM list (uses the stripped name and a memcompression skip).
        name = (i.get("name") or "").strip()
        if "memcompression" in name.lower():
            continue
        mem = i.get("memory_info")
        if mem:
            mb = mem.rss / (1024 * 1024)
            if mb > 10:
                ram_procs.append({"n": name[:20], "r": int(mb)})
    cpu_procs.sort(key=lambda x: x["c"], reverse=True)
    ram_procs.sort(key=lambda x: x["r"], reverse=True)
    return cpu_procs[:cpu_n], ram_procs[:ram_n]


def get_top_processes_sync(
    cpu_n: int = 3, ram_n: int = 2
) -> Tuple[List[Dict], List[Dict]]:
    """Single ``process_iter`` pass yielding both the top-CPU and top-RAM lists.

    Replaces the two separate full process-table walks (OPTIMIZATION_NOTES 5.1):
    one enumeration collects ``name`` + ``cpu_percent`` + ``memory_info`` and feeds
    :func:`_collect_top_processes`. Output shapes/sorting match the prior pair of
    functions exactly. MUST stay off the event loop (called via the executor).
    """
    try:
        procs_info: List[Dict[str, Any]] = []
        for p in psutil.process_iter(["name", "cpu_percent", "memory_info"]):
            try:
                procs_info.append(p.info)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        return _collect_top_processes(procs_info, cpu_n, ram_n)
    except Exception as e:
        logging.warning("top processes failed: %s", e)
        return [], []


async def _get_media_info_async_impl() -> Dict:
    """Media: no Base64 art; only status PLAYING|PAUSED for procedural cassette animation."""
    global _media_manager
    if not HAS_WINSDK:
        return {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"}
    try:
        # Reuse cached manager if available, otherwise create new one
        if _media_manager is None:
            _media_manager = await GlobalSystemMediaTransportControlsSessionManager.request_async()
        manager = _media_manager
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
    except Exception as e:
        # If manager becomes invalid, reset it so it will be recreated next time
        logging.warning("media session read failed: %s", e)
        _media_manager = None
        return {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"}


async def get_media_info(_loop: asyncio.AbstractEventLoop) -> Dict:
    """Winsdk runs in asyncio loop to avoid COM/threading issues."""
    try:
        return await _get_media_info_async_impl()
    except Exception as e:
        log_debug(f"Media: {e}")
    return {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"}


def get_claude_usage_sync() -> Dict[str, Any]:
    """Read local Claude Code usage/limits (~/.claude) and apply token budgets so
    window_pct/weekly_pct are populated (Claude exposes no official quota % on
    disk — see claude_budget). Cheap file reads; runs in the executor. Never
    raises: a failure yields the graceful-empty dict."""
    try:
        usage = claude_mod.read_claude_usage()
        import os as _os
        from datetime import date as _date
        stats = claude_mod._load_json(
            _os.path.join(claude_mod._default_base_dir(), "stats-cache.json"))

        # Prefer FRESH usage from session transcripts (written live) over the
        # lazily-refreshed, often-stale stats-cache.json. Sessions give today's
        # real tokens + an accurate "as of" date; stats-cache is the fallback.
        today = _date.today().isoformat()
        fresh = claude_sessions.summarize(claude_sessions.tokens_by_date(), today)
        weekly_override = None
        if fresh:
            usage["today_tokens"] = fresh["today_tokens"]
            usage["today_msgs"] = fresh.get("today_msgs")
            usage["today_tools"] = fresh.get("today_tools")
            usage["date"] = fresh["date"]
            usage["last_active"] = fresh.get("last_active")
            usage["source"] = "sessions"
            usage["available"] = True
            weekly_override = fresh["weekly_tokens"]
        else:
            usage["source"] = "stats-cache"
            usage["last_active"] = usage.get("date")

        # Honest staleness: the data is stale if its date isn't today.
        usage["stale"] = bool(usage.get("date") and usage["date"] != today)

        usage = claude_budget.apply_budget(
            usage, stats,
            daily_budget=CLAUDE_DAILY_BUDGET,
            weekly_budget=CLAUDE_WEEKLY_BUDGET,
            weekly_tokens_override=weekly_override,
        )
        return usage
    except Exception as e:
        log_debug(f"Claude usage: {e}")
        return dict(claude_mod.EMPTY_USAGE)


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

# External events from Prometheus Alertmanager (webhook). Distinct from the local
# threshold RED ALERT above; surfaced to the device as the payload "events" block.
_alert_state = alert_events.AlertState()

# Forest panel: latest aggregated node-status block (built by the forest poll in
# run() and read into every payload). Empty until the first successful scrape.
_forest_block: Dict[str, Any] = dict(forest_panel.EMPTY_FOREST)


def _alert_thresholds() -> Dict[str, int]:
    """Current RED-ALERT thresholds as the dict :func:`payload.evaluate_alert` wants."""
    return {
        "cpu_temp": CPU_TEMP_ALERT,
        "gpu_temp": GPU_TEMP_ALERT,
        "cpu_load": CPU_LOAD_ALERT,
        "gpu_load": GPU_LOAD_ALERT,
        "vram_load": VRAM_LOAD_ALERT,
        "ram_gb": RAM_GB_ALERT,
    }


def _alert_hysteresis() -> Dict[str, int]:
    """Current RED-ALERT hysteresis as the dict :func:`payload.evaluate_alert` wants."""
    return {
        "cpu_temp": CPU_TEMP_HYST,
        "gpu_temp": GPU_TEMP_HYST,
        "load": LOAD_HYST,
        "ram_gb": RAM_GB_HYST,
    }


def _build_claude_block(claude: Optional[Dict]) -> Dict[str, Any]:
    """Map the cached :func:`claude_usage.read_claude_usage` dict to the compact
    wire shape sent to the device.

    Uses short, stable keys in keeping with the rest of the payload (``wt``/``wd``
    etc.). ``ok`` mirrors ``available``; the percentage / reset fields stay
    ``None`` (JSON ``null``) when no real local source exists, so the firmware can
    render "n/a". Defensive: a ``None`` / non-dict input degrades to the
    graceful-empty block rather than raising.
    """
    c = claude if isinstance(claude, dict) else claude_mod.EMPTY_USAGE
    return {
        "ok": bool(c.get("available", False)),
        "plan": c.get("plan"),
        "win": c.get("window_pct"),     # 5-hour window usage %  (null until a runtime provides it)
        "wk": c.get("weekly_pct"),      # weekly limit usage %   (null until a runtime provides it)
        "rst": c.get("resets_in_min"),  # minutes to window reset (null until available)
        "tok": c.get("today_tokens"),   # tokens used today (all models)
        "msg": c.get("today_msgs"),     # messages today
        "tool": c.get("today_tools"),   # tool calls today
        "day": c.get("date"),           # date the today_* figures apply to
        "stale": bool(c.get("stale", False)),  # True if "day" isn't today (data is old)
        "act": c.get("last_active"),    # most recent date with activity (freshness)
    }


def _events_with_claude(claude: Optional[Dict], now: float) -> Dict[str, Any]:
    """Events block = Alertmanager webhook alerts + the Claude 80% reminder.

    The Claude reminder reuses the same on-device events banner. When usage
    crosses the configured threshold it's surfaced as an extra firing event so
    the device toasts e.g. "Claude wk 88%". Non-firing claude => just the
    webhook events.
    """
    block = _alert_state.snapshot(now)
    if isinstance(claude, dict):
        fire, name = claude_budget.alert_for(claude, threshold_pct=CLAUDE_ALERT_PCT)
        if fire:
            existing = list(block.get("list", []))
            if name not in existing:
                existing.insert(0, name)
            block = {
                "n": block.get("n", 0) + 1,
                "top": name,                 # Claude reminder takes the banner
                "sev": "warning",
                "list": existing[:4],
            }
    return block


def build_payload(hw: Dict, media: Dict, weather: Dict, top_procs: List, top_procs_ram: List,
                  net: tuple, disk: tuple, ping_ms: int, now: float,
                  claude: Optional[Dict] = None) -> Dict:
    global _last_alert

    w = weather  # (kept identical to prior weather-cache selection, which was a no-op)
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

    # HDD slots + optional psutil capacity fallback (logic in payload.normalize_hdd).
    hdd_list = payload_mod.normalize_hdd(
        hw.get("hdd", []),
        fallback_enabled=_config.get("lhm_storage_fallback") == "psutil",
        get_fallback_disks=get_disks_psutil_fallback,
    )

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
        "claude": _build_claude_block(claude),
        "events": _events_with_claude(claude, now),
        "forest": _forest_block,
        "sv": SERVER_VERSION,
    }

    # RED ALERT: threshold with hysteresis; send alert_metric for value blink.
    # State machine lives in payload.evaluate_alert; _last_alert holds the
    # cross-call hysteresis state.
    alert_fields, _last_alert = payload_mod.evaluate_alert(
        {"ct": ct, "gt": gt, "cl": cl, "gl": gl, "gv": gv, "ram": ram_used_f},
        _last_alert,
        _alert_thresholds(),
        _alert_hysteresis(),
    )
    payload.update(alert_fields)
    return payload


# Change-detection snapshot tuple now lives in payload.py; re-exported.
_payload_snapshot = payload_mod.payload_snapshot


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


def encode_payload(payload: Dict) -> bytes:
    """Serialize a payload to the newline-terminated UTF-8 bytes sent on the wire.

    Applies the same oversize guard as before: if the encoded frame exceeds
    ``MAX_PAYLOAD_BYTES`` (the firmware line cap) it logs and falls back to a tiny
    minimal frame. Pulled out of :func:`send_data_to_client` so the broadcast loop
    can encode the identical payload once for all clients instead of once per
    client (OPTIMIZATION_NOTES 5.3).
    """
    raw = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
    if len(raw) > MAX_PAYLOAD_BYTES:
        logging.warning("Payload too large (%d bytes), sending minimal", len(raw))
        minimal = {"ct": 0, "gt": 0, "cl": 0, "gl": 0, "ru": 0, "ra": 0}
        raw = (json.dumps(minimal, separators=(",", ":")) + "\n").encode("utf-8")
    return raw


async def send_raw_to_client(writer, raw: bytes) -> bool:
    """Write pre-encoded payload bytes to one client. Returns False on error."""
    try:
        writer.write(raw)
        await writer.drain()
        return True
    except Exception as e:
        log_debug(f"Send: {e}")
        return False


async def send_data_to_client(writer, payload: Dict) -> bool:
    return await send_raw_to_client(writer, encode_payload(payload))


async def handle_client(reader, writer):
    addr = writer.get_extra_info("peername")
    log_info(f"Client connected: {addr}")
    tcp_clients.append(writer)
    client_screens[writer] = 0
    client_buffers[writer] = ""
    try:
        snap = await cache_snapshot()
        # Always send a full payload on connect (even before the first hw poll, or
        # on a machine with no LHM): build_payload tolerates an empty hw dict and
        # the device gets the complete shape immediately — hw zeros plus the
        # claude/forest/events/weather blocks — rather than a stripped stub.
        payload = build_payload(
            snap["hw"], snap["media"],
            snap["weather"], snap["top_procs"],
            snap["top_procs_ram"], snap["net"],
            snap["disk"], snap["ping"], time.time(),
            claude=snap.get("claude"),
        )
        await send_data_to_client(writer, payload)
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
    global last_top_procs_time, last_media_time, ping_latency_ms, global_data_cache
    global _last_sent_snapshot, _last_heartbeat_time, _forest_block

    server = None
    executor = ThreadPoolExecutor(max_workers=6)
    log_info(f"Starting TCP server on {TCP_HOST}:{TCP_PORT}")
    await asyncio.sleep(2)  # allow network/services to be ready (e.g. at autostart)

    try:
        server = await asyncio.start_server(handle_client, TCP_HOST, TCP_PORT)  # noqa: F841
    except OSError as e:
        log_err(f"Bind failed: {e}")
        raise

    log_info("Server ready.")
    await _start_alert_webhook()
    loop = asyncio.get_event_loop()
    last_lhm_time = 0.0
    last_weather_time = 0.0
    last_ping_time = 0.0
    last_media_time = 0.0
    last_claude_time = 0.0
    last_forest_time = 0.0
    last_am_time = 0.0
    session = aiohttp.ClientSession()

    if FOREST_QUERY_URL:
        log_info(f"Forest panel: querying {len(FOREST_NODES)} node(s) via {FOREST_QUERY_URL}")
    if ALERTMANAGER_URL:
        log_info(f"Alertmanager poll: {ALERTMANAGER_URL}")

    try:
        await cache_set("hw", await get_lhm_data_async(session))
        await cache_set("weather", await get_weather_async(session))
        await cache_set("net", await loop.run_in_executor(executor, get_network_speed_sync))
        await cache_set("disk", await loop.run_in_executor(executor, get_disk_speed_sync))
        await cache_set("media", await get_media_info(loop))
        top_procs_cache, top_procs_ram_cache = await loop.run_in_executor(
            executor, lambda: get_top_processes_sync(TOP_PROCS_CPU_N, TOP_PROCS_RAM_N)
        )
        await cache_set("top_procs", top_procs_cache)
        await cache_set("top_procs_ram", top_procs_ram_cache)
        last_top_procs_time = time.time()
        ping_latency_ms = await loop.run_in_executor(executor, get_ping_latency_sync)
        await cache_set("ping", ping_latency_ms)
        await cache_set("claude", await loop.run_in_executor(executor, get_claude_usage_sync))
        last_claude_time = time.time()
        if FOREST_QUERY_URL:
            _forest_block = await get_forest_block_async(session)
            last_forest_time = time.time()
        if ALERTMANAGER_URL:
            await poll_alertmanager_async(session)
            last_am_time = time.time()
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
                        await cache_set("hw", hw_data)
                    await cache_set("net", await loop.run_in_executor(executor, get_network_speed_sync))
                    await cache_set("disk", await loop.run_in_executor(executor, get_disk_speed_sync))
                except Exception as e:
                    log_debug(f"Poll: {e}")

            if now - last_top_procs_time >= TOP_PROCS_CACHE_TTL:
                try:
                    top_procs_cache, top_procs_ram_cache = await loop.run_in_executor(
                        executor, lambda: get_top_processes_sync(TOP_PROCS_CPU_N, TOP_PROCS_RAM_N)
                    )
                    last_top_procs_time = now
                    await cache_set("top_procs", top_procs_cache)
                    await cache_set("top_procs_ram", top_procs_ram_cache)
                except Exception as e:
                    log_debug(f"Procs: {e}")

            if now - last_weather_time >= WEATHER_UPDATE_INTERVAL:
                last_weather_time = now
                try:
                    await cache_set("weather", await get_weather_async(session))
                except Exception as e:
                    log_debug(f"Weather: {e}")

            if now - last_ping_time >= 5.0:
                last_ping_time = now
                try:
                    ping_latency_ms = await loop.run_in_executor(executor, get_ping_latency_sync)
                    await cache_set("ping", ping_latency_ms)
                except Exception as e:
                    log_debug(f"Ping: {e}")

            if now - last_media_time >= MEDIA_UPDATE_INTERVAL:
                last_media_time = now
                try:
                    await cache_set("media", await get_media_info(loop))
                except Exception as e:
                    log_debug(f"Media: {e}")

            if now - last_claude_time >= CLAUDE_UPDATE_INTERVAL:
                last_claude_time = now
                try:
                    await cache_set("claude", await loop.run_in_executor(executor, get_claude_usage_sync))
                except Exception as e:
                    log_debug(f"Claude: {e}")

            if FOREST_QUERY_URL and now - last_forest_time >= FOREST_UPDATE_INTERVAL:
                last_forest_time = now
                try:
                    _forest_block = await get_forest_block_async(session)
                except Exception as e:
                    log_debug(f"Forest: {e}")

            if ALERTMANAGER_URL and now - last_am_time >= ALERTMANAGER_UPDATE_INTERVAL:
                last_am_time = now
                try:
                    await poll_alertmanager_async(session)
                except Exception as e:
                    log_debug(f"Alertmanager: {e}")

            snap = await cache_snapshot()
            payload = build_payload(
                snap["hw"], snap["media"],
                snap["weather"], snap["top_procs"],
                snap["top_procs_ram"], snap["net"],
                snap["disk"], snap["ping"], now,
                claude=snap.get("claude"),
            )

            if should_send_payload(payload, now):
                _last_heartbeat_time = now
                _last_sent_snapshot = _payload_snapshot(payload)
                dead = []
                clients = list(tcp_clients)
                # Encode the identical payload once for all clients (not per-client).
                raw = encode_payload(payload) if clients else b""
                for w in clients:
                    if not await send_raw_to_client(w, raw):
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
