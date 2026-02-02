#!/usr/bin/env python3
"""
Heltec PC Monitor Server v2.0
Собирает данные с ПК и отправляет на Heltec ESP32
"""

import asyncio
import base64
import io
import json
import os
import socket
import sys
import threading
import time
import traceback
from typing import Dict, List, Optional, Any

import psutil
import requests
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
PC_IP = os.getenv("PC_IP", "192.168.1.2")

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

COVER_SIZE = 48
COVER_BYTES = (COVER_SIZE * COVER_SIZE) // 8
COVER_INTERVAL = 60  # Send cover max once per minute

# ============================================================================
# GLOBAL STATE
# ============================================================================

stop_requested = False

# Hardware sensor mapping (LibHardwareMonitor IDs)
TARGETS = {
    "ct": "/amdcpu/0/temperature/2",
    "gt": "/gpu-nvidia/0/temperature/0",
    "gth": "/gpu-nvidia/0/temperature/2",
    "vt": "/lpc/it8688e/0/temperature/4",
    "cpu_load": "/amdcpu/0/load/0",
    "cpu_pwr": "/amdcpu/0/power/0",
    "cpu_mhz": "/amdcpu/0/clock/0",
    "gpu_load": "/gpu-nvidia/0/load/0",
    "gpu_pwr": "/gpu-nvidia/0/power/0",
    "p": "/lpc/it8688e/0/fan/0",
    "r": "/lpc/it8688e/0/fan/1",
    "c": "/lpc/it8688e/0/fan/2",
    "gf": "/gpu-nvidia/0/fan/1",
    "gck": "/gpu-nvidia/0/clock/0",
    "ram_u": "/ram/data/0",
    "ram_a": "/ram/data/1",
    "ram_pct": "/ram/load/0",
    "vram_u": "/gpu-nvidia/0/smalldata/1",
    "vram_t": "/gpu-nvidia/0/smalldata/2",
    "vcore": "/lpc/it8688e/0/voltage/0",
    "c1": "/amdcpu/0/clock/3",
    "c2": "/amdcpu/0/clock/5",
    "c3": "/amdcpu/0/clock/7",
    "c4": "/amdcpu/0/clock/9",
    "c5": "/amdcpu/0/clock/11",
    "c6": "/amdcpu/0/clock/13",
    "nvme2_t": "/nvme/2/temperature/0",
    "chipset_t": "/lpc/it8688e/0/temperature/0",
    "d1_t": "/nvme/2/temperature/0",
    "d1_free": "/nvme/2/data/31",
    "d1_total": "/nvme/2/data/32",
    "d2_t": "/nvme/3/temperature/0",
    "d2_free": "/nvme/3/data/31",
    "d2_total": "/nvme/3/data/32",
    "d3_t": "/hdd/0/temperature/0",
    "d3_free": "/hdd/0/data/31",
    "d3_total": "/hdd/0/data/32",
    "d4_t": "/ssd/1/temperature/0",
    "d4_free": "/ssd/1/data/31",
    "d4_total": "/ssd/1/data/32",
}

weather_cache = {
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
last_weather_update = 0.0
_weather_first_ok = False
_last_weather_log_time = 0.0

last_net_bytes = {"sent": 0, "recv": 0, "time": 0.0}
last_disk_bytes = {"read": 0, "write": 0, "time": 0.0}

tcp_clients: List = []
client_screens: Dict = {}  # StreamWriter -> screen number
client_buffers: Dict = {}  # StreamWriter -> buffer for reading

last_track_key = ""
last_cover_sent = 0

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

def log_err(msg: str, exc: Optional[BaseException] = None, trace: bool = True):
    """Print error to stderr with optional traceback."""
    print(f"[ERROR] {msg}", file=sys.stderr, flush=True)
    if exc is not None and trace:
        traceback.print_exc(file=sys.stderr)
        sys.stderr.flush()


def log_info(msg: str):
    """Print info message."""
    print(f"[INFO] {msg}", flush=True)


def clean_val(v: Any) -> float:
    """Clean and convert sensor value to float."""
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
# DATA COLLECTION
# ============================================================================

def get_lhm_data() -> Dict[str, float]:
    """Fetch hardware monitoring data from LibHardwareMonitor."""
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
                    for item in node:
                        walk(item)
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

            if isinstance(data, list):
                walk(data)
            else:
                walk(data)
                
            return results
            
        except Exception as e:
            if attempt == 2:
                log_err(f"LHM failed after 3 attempts: {e}", e, trace=False)
            continue
            
    return {}


def _weather_desc_from_code(code: int) -> str:
    """Convert WMO weather code to description."""
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


def get_weather() -> Dict:
    """Fetch weather data from Open-Meteo API."""
    global weather_cache, last_weather_update, _weather_first_ok
    global _last_weather_log_time
    
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
                last_weather_update = now
                
                if not _weather_first_ok:
                    _weather_first_ok = True
                    log_info(f"Weather: {temp}°C, {desc}")
                    
                return weather_cache
                
        except Exception as e:
            if attempt == 1:
                if now - _last_weather_log_time > 300:  # Log once per 5 min
                    log_err(f"Weather fetch failed: {e}", trace=False)
                    _last_weather_log_time = now
                    
    return weather_cache


def get_network_speed() -> tuple:
    """Calculate network upload/download speed in KB/s."""
    global last_net_bytes
    
    try:
        counters = psutil.net_io_counters()
        now = time.time()
        
        if last_net_bytes["time"] == 0:
            last_net_bytes = {
                "sent": counters.bytes_sent,
                "recv": counters.bytes_recv,
                "time": now,
            }
            return 0, 0
            
        dt = now - last_net_bytes["time"]
        if dt < 0.1:
            return 0, 0
            
        up = int((counters.bytes_sent - last_net_bytes["sent"]) / dt / 1024)
        down = int((counters.bytes_recv - last_net_bytes["recv"]) / dt / 1024)
        
        last_net_bytes = {
            "sent": counters.bytes_sent,
            "recv": counters.bytes_recv,
            "time": now,
        }
        
        return max(0, up), max(0, down)
        
    except Exception:
        return 0, 0


def get_disk_speed() -> tuple:
    """Calculate disk read/write speed in KB/s."""
    global last_disk_bytes
    
    try:
        counters = psutil.disk_io_counters()
        if counters is None:
            return 0, 0
            
        now = time.time()
        
        if last_disk_bytes["time"] == 0:
            last_disk_bytes = {
                "read": counters.read_bytes,
                "write": counters.write_bytes,
                "time": now,
            }
            return 0, 0
            
        dt = now - last_disk_bytes["time"]
        if dt < 0.1:
            return 0, 0
            
        read_speed = int((counters.read_bytes - last_disk_bytes["read"]) / dt / 1024)
        write_speed = int((counters.write_bytes - last_disk_bytes["write"]) / dt / 1024)
        
        last_disk_bytes = {
            "read": counters.read_bytes,
            "write": counters.write_bytes,
            "time": now,
        }
        
        return max(0, read_speed), max(0, write_speed)
        
    except Exception:
        return 0, 0


def get_top_processes_cpu(n: int = 3) -> List[Dict]:
    """Get top N processes by CPU usage."""
    try:
        procs = []
        for p in psutil.process_iter(["name", "cpu_percent"]):
            try:
                info = p.info
                if info["cpu_percent"] and info["cpu_percent"] > 0:
                    procs.append({
                        "n": info["name"][:20],
                        "c": int(info["cpu_percent"]),
                    })
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
                
        procs.sort(key=lambda x: x["c"], reverse=True)
        return procs[:n]
        
    except Exception:
        return []


def get_top_processes_ram(n: int = 2) -> List[Dict]:
    """Get top N processes by memory usage."""
    try:
        procs = []
        for p in psutil.process_iter(["name", "memory_info"]):
            try:
                info = p.info
                if info["memory_info"]:
                    mb = info["memory_info"].rss / (1024 * 1024)
                    if mb > 10:  # Only show processes using >10MB
                        procs.append({
                            "n": info["name"][:20],
                            "r": int(mb),
                        })
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
                
        procs.sort(key=lambda x: x["r"], reverse=True)
        return procs[:n]
        
    except Exception:
        return []


async def get_media_info() -> Dict:
    """Get currently playing media info from Windows."""
    if not HAS_WINSDK:
        return {"art": "", "trk": "", "play": False, "cover_b64": ""}
        
    try:
        manager = await GlobalSystemMediaTransportControlsSessionManager.request_async()
        session = manager.get_current_session()
        
        if not session:
            return {"art": "", "trk": "", "play": False, "cover_b64": ""}
            
        info = await session.try_get_media_properties_async()
        playback = session.get_playback_info()
        
        artist = info.artist if info and info.artist else ""
        track = info.title if info and info.title else ""
        is_playing = (
            playback
            and playback.playback_status == 4  # Playing status
        )
        
        # Get album cover
        cover_b64 = ""
        if HAS_PIL and info:
            try:
                thumb_ref = info.thumbnail
                if thumb_ref:
                    thumb_stream = await thumb_ref.open_read_async()
                    if thumb_stream:
                        buf = Buffer(COVER_BYTES * 20)  # Larger buffer for image data
                        await thumb_stream.read_async(buf, buf.capacity, InputStreamOptions.READ_AHEAD)
                        
                        img_bytes = bytes(buf)
                        img = Image.open(io.BytesIO(img_bytes))
                        
                        # Convert to 48x48 monochrome bitmap
                        img = img.convert("L").resize((COVER_SIZE, COVER_SIZE), Image.Resampling.LANCZOS)
                        threshold = 128
                        img = img.point(lambda p: 255 if p > threshold else 0, mode="1")
                        
                        # Convert to XBM format (1 bit per pixel)
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
                cover_b64 = ""
                
        return {
            "art": artist[:30],
            "trk": track[:30],
            "play": is_playing,
            "cover_b64": cover_b64,
        }
        
    except Exception:
        return {"art": "", "trk": "", "play": False, "cover_b64": ""}


def get_core_loads(num_cores: int = 6) -> List[int]:
    """Get per-core CPU load percentages."""
    try:
        per_cpu = psutil.cpu_percent(interval=0, percpu=True)
        return [int(x) for x in per_cpu[:num_cores]]
    except Exception:
        return [0] * num_cores


# ============================================================================
# PAYLOAD BUILDING
# ============================================================================

def build_payload(
    hw: Dict,
    media: Dict,
    hw_ok: bool,
    include_cover: bool,
    weather: Dict,
    top_procs: List,
    top_procs_ram: List,
    net: tuple,
    disk: tuple,
    core_loads: List,
    screen: int,
) -> Dict:
    """Build JSON payload to send to Heltec."""
    
    payload = {
        # Hardware
        "ct": int(hw.get("ct", 0)),
        "gt": int(hw.get("gt", 0)),
        "gth": int(hw.get("gth", 0)),
        "vt": int(hw.get("vt", 0)),
        "cpu_load": int(hw.get("cpu_load", 0)),
        "cpu_pwr": int(hw.get("cpu_pwr", 0)),
        "cpu_mhz": int(hw.get("cpu_mhz", 0)),
        "gpu_load": int(hw.get("gpu_load", 0)),
        "gpu_pwr": int(hw.get("gpu_pwr", 0)),
        "p": int(hw.get("p", 0)),
        "r": int(hw.get("r", 0)),
        "c": int(hw.get("c", 0)),
        "gf": int(hw.get("gf", 0)),
        "gck": int(hw.get("gck", 0)),
        "ram_u": round(hw.get("ram_u", 0), 1),
        "ram_a": round(hw.get("ram_a", 0), 1),
        "ram_pct": int(hw.get("ram_pct", 0)),
        "vram_u": round(hw.get("vram_u", 0), 1),
        "vram_t": round(hw.get("vram_t", 0), 1),
        "vcore": round(hw.get("vcore", 0), 2),
        "nvme2_t": int(hw.get("nvme2_t", 0)),
        "chipset_t": int(hw.get("chipset_t", 0)),
        
        # CPU Cores
        "c_arr": [
            int(hw.get("c1", 0)),
            int(hw.get("c2", 0)),
            int(hw.get("c3", 0)),
            int(hw.get("c4", 0)),
            int(hw.get("c5", 0)),
            int(hw.get("c6", 0)),
        ],
        "cl_arr": core_loads,
        
        # Disks
        "d1_t": int(hw.get("d1_t", 0)),
        "d1_u": round(hw.get("d1_total", 0) - hw.get("d1_free", 0), 1),
        "d1_f": round(hw.get("d1_free", 0), 1),
        "d2_t": int(hw.get("d2_t", 0)),
        "d2_u": round(hw.get("d2_total", 0) - hw.get("d2_free", 0), 1),
        "d2_f": round(hw.get("d2_free", 0), 1),
        "d3_t": int(hw.get("d3_t", 0)),
        "d3_u": round(hw.get("d3_total", 0) - hw.get("d3_free", 0), 1),
        "d3_f": round(hw.get("d3_free", 0), 1),
        "d4_t": int(hw.get("d4_t", 0)),
        "d4_u": round(hw.get("d4_total", 0) - hw.get("d4_free", 0), 1),
        "d4_f": round(hw.get("d4_free", 0), 1),
        
        # Network & Disk I/O
        "net_up": net[0],
        "net_down": net[1],
        "disk_r": disk[0],
        "disk_w": disk[1],
        
        # Weather
        "wt": weather.get("temp", 0),
        "wd": weather.get("desc", ""),
        "wi": weather.get("icon", 0),
        "w_dh": weather.get("day_high", 0),
        "w_dl": weather.get("day_low", 0),
        "w_dc": weather.get("day_code", 0),
        "w_wh": weather.get("week_high", []),
        "w_wl": weather.get("week_low", []),
        "w_wc": weather.get("week_code", []),
        
        # Processes
        "tp": top_procs,
        "tp_ram": top_procs_ram,
        
        # Media
        "art": media.get("art", ""),
        "trk": media.get("trk", ""),
        "play": media.get("play", False),
    }
    
    # Only include cover if requested
    if include_cover:
        payload["cover_b64"] = media.get("cover_b64", "")
    else:
        payload["cover_b64"] = ""
        
    return payload


# ============================================================================
# TCP SERVER
# ============================================================================

async def handle_client(reader, writer):
    """Handle incoming client connection."""
    addr = writer.get_extra_info("peername")
    log_info(f"Client connected: {addr}")
    
    tcp_clients.append(writer)
    client_screens[writer] = 0
    client_buffers[writer] = ""
    
    try:
        while not stop_requested:
            try:
                data = await asyncio.wait_for(reader.read(256), timeout=1.0)
            except asyncio.TimeoutError:
                continue
                
            if not data:
                break
                
            try:
                text = data.decode("utf-8")
                client_buffers[writer] += text
                
                while "\n" in client_buffers[writer]:
                    line, client_buffers[writer] = client_buffers[writer].split("\n", 1)
                    line = line.strip()
                    
                    if line.startswith("screen:"):
                        try:
                            screen_num = int(line.split(":", 1)[1])
                            client_screens[writer] = screen_num
                        except (ValueError, IndexError):
                            pass
                            
            except UnicodeDecodeError:
                client_buffers[writer] = ""
                
    except Exception as e:
        log_err(f"Client error: {e}", trace=False)
        
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


async def weather_updater():
    """Background task to update weather periodically."""
    while not stop_requested:
        try:
            get_weather()
        except Exception as e:
            log_err(f"Weather updater error: {e}", trace=False)
            
        await asyncio.sleep(WEATHER_UPDATE_INTERVAL)


async def run():
    """Main server loop."""
    global last_track_key, last_cover_sent
    
    log_info(f"Starting TCP server on {TCP_HOST}:{TCP_PORT}")
    
    # Start TCP server
    server = await asyncio.start_server(handle_client, TCP_HOST, TCP_PORT)
    
    # Start background tasks
    asyncio.create_task(weather_updater())
    
    log_info("Server ready. Waiting for clients...")
    
    # Initialize data
    get_weather()  # Initial weather fetch
    
    # Cache for data
    cache = {}
    
    while not stop_requested:
        try:
            now_sec = int(time.time())
            
            # Collect data
            hw = get_lhm_data()
            hw_ok = len(hw) > 5
            
            if not hw_ok:
                hw = {k: 0 for k in TARGETS.keys()}
                
            media = await get_media_info()
            weather = get_weather()
            top_procs = get_top_processes_cpu(3)
            top_procs_ram = get_top_processes_ram(2)
            net = get_network_speed()
            disk = get_disk_speed()
            core_loads = get_core_loads(6)
            
            # Update cache
            cache.update({
                "hw": hw,
                "hw_ok": hw_ok,
                "media": media,
                "weather": weather,
                "top_procs": top_procs,
                "top_procs_ram": top_procs_ram,
                "net": net,
                "disk": disk,
                "core_loads": core_loads,
            })
            
            # Determine if we should send cover
            track_key = f"{media.get('art', '')}|{media.get('trk', '')}"
            include_cover = (
                track_key != last_track_key or
                (now_sec - last_cover_sent >= COVER_INTERVAL)
            )
            
            if include_cover and media.get("cover_b64"):
                last_cover_sent = now_sec
                
            if track_key != last_track_key and last_track_key:
                cov_len = len(media.get("cover_b64", ""))
                log_info(f"Track changed. Cover: {cov_len} bytes")
                
            last_track_key = track_key
            
            # Send to all clients
            dead = []
            for writer in list(tcp_clients):
                screen = client_screens.get(writer, 0)
                
                payload = build_payload(
                    hw=cache["hw"],
                    media=cache["media"],
                    hw_ok=cache["hw_ok"],
                    include_cover=include_cover,
                    weather=cache["weather"],
                    top_procs=cache["top_procs"],
                    top_procs_ram=cache["top_procs_ram"],
                    net=cache["net"],
                    disk=cache["disk"],
                    core_loads=cache["core_loads"],
                    screen=screen,
                )
                
                try:
                    writer.write((json.dumps(payload) + "\n").encode("utf-8"))
                    await writer.drain()
                except Exception:
                    dead.append(writer)
                    
            # Clean up dead connections
            for writer in dead:
                if writer in tcp_clients:
                    tcp_clients.remove(writer)
                client_screens.pop(writer, None)
                client_buffers.pop(writer, None)
                try:
                    writer.close()
                    await writer.wait_closed()
                except Exception:
                    pass
                    
        except Exception as e:
            log_err(f"Main loop error: {e}", e)
            
        await asyncio.sleep(0.5)  # 2 Hz update rate
        
    # Cleanup
    tasks = [t for t in asyncio.all_tasks() if t is not asyncio.current_task()]
    for t in tasks:
        t.cancel()
        
    if tasks:
        await asyncio.gather(*tasks, return_exceptions=True)
        
    if server:
        server.close()
        await server.wait_closed()


# ============================================================================
# SYSTEM TRAY (Windows only)
# ============================================================================

def _make_tray_icon_image():
    """Create tray icon image."""
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
    """Run tray icon in current thread."""
    try:
        icon.run()
    except Exception as e:
        log_err(f"Tray icon error: {e}", trace=False)


def _start_tray():
    """Start tray icon in separate thread."""
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
        
    def on_add_autostart(icon, item):
        if _add_to_autostart():
            log_info("Added to autostart")
        else:
            log_err("Failed to add to autostart", trace=False)
            
    def on_remove_autostart(icon, item):
        if _remove_from_autostart():
            log_info("Removed from autostart")
        else:
            log_err("Failed to remove from autostart", trace=False)
            
    menu = Menu(
        MenuItem("Add to Autostart", on_add_autostart),
        MenuItem("Remove from Autostart", on_remove_autostart),
        MenuItem("Exit", on_quit, default=True),
    )
    
    icon = pystray.Icon("heltec", img, "Heltec Monitor v2.0", menu=menu)
    t = threading.Thread(target=_run_tray_thread, args=(icon,), daemon=True)
    t.start()
    
    return icon


def _is_in_autostart() -> bool:
    """Check if in Windows autostart registry."""
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
    """Add to Windows autostart registry."""
    if sys.platform != "win32":
        return False
        
    try:
        import winreg
        
        if getattr(sys, "frozen", False):
            exe_path = os.path.abspath(sys.executable)
        else:
            script_path = os.path.abspath(__file__)
            exe_path = f'"{sys.executable}" "{script_path}"'
            
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
        log_err(f"Autostart add failed: {e}", trace=False)
        return False


def _remove_from_autostart() -> bool:
    """Remove from Windows autostart registry."""
    if sys.platform != "win32":
        return False
        
    try:
        import winreg
        
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            r"Software\Microsoft\Windows\CurrentVersion\Run",
            0,
            winreg.KEY_SET_VALUE,
        )
        
        try:
            winreg.DeleteValue(key, "HeltecMonitor")
            return True
        except OSError:
            return False
        finally:
            winreg.CloseKey(key)
            
    except Exception as e:
        log_err(f"Autostart remove failed: {e}", trace=False)
        return False


# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    log_info("Heltec PC Monitor Server v2.0")
    log_info("=" * 50)
    
    # Check autostart on first run
    if sys.platform == "win32" and not getattr(sys, "frozen", False):
        if not _is_in_autostart():
            try:
                print("Add to Windows autostart? (Y/N): ", end="", flush=True)
                ans = input().strip().upper()
                if ans in ("Y", "YES"):
                    if _add_to_autostart():
                        log_info("Added to autostart")
                    else:
                        log_err("Failed to add to autostart", trace=False)
            except Exception:
                pass
                
    # Start tray icon
    tray_icon = None
    if sys.platform == "win32":
        tray_icon = _start_tray()
        
    # Run main server
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        log_info("Shutting down...")
    except Exception as e:
        log_err(f"Fatal error: {e}", e)
        
        if getattr(sys, "frozen", False) and sys.platform == "win32":
            try:
                import ctypes
                ctypes.windll.user32.MessageBoxW(None, str(e), "Heltec Monitor", 0x10)
            except Exception:
                pass
                
        sys.exit(1)
        
    log_info("Goodbye!")
