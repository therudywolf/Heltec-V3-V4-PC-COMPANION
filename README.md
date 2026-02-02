# Heltec PC Monitor v6.0 [NEURAL LINK]

**Cyberpunk Cyberdeck Edition** — Real-time PC hardware monitoring on **Heltec WiFi LoRa 32 V3** (ESP32-S3 + SSD1306 OLED 128×64). Data streamed from Windows PC via WiFi TCP.

<div align="center">

```
╔═══════════════════════════════════════════════════════════╗
║  [NEURAL LINK ESTABLISHED]                                ║
║  ┌─────────────────────────────────────────────────────┐  ║
║  │  CORTEX  │  NEURAL  │  THERMAL  │  MEM.BANK  │ ... │  ║
║  └─────────────────────────────────────────────────────┘  ║
║  CPU: 53°C  GPU: 27°C  RAM: 15.9/32.0G  NET: ↓102K ↑42K  ║
╚═══════════════════════════════════════════════════════════╝
```

</div>

---

## 🎯 Features

### 📊 6 Cyberpunk HUD Screens

| Screen          | Description                                                  |
| --------------- | ------------------------------------------------------------ |
| **[CORTEX]**    | CPU/GPU temps & loads (2×2 grid, big numbers, progress bars) |
| **[NEURAL]**    | Network up/down speed + ping latency (Google DNS 8.8.8.8)    |
| **[THERMAL]**   | Fan RPMs (CPU/Pump, Radiator, SYS2, GPU) + animated fan icon |
| **[MEM.BANK]**  | RAM usage + Storage bars (NVMe SYS, HDD DATA) + Disk I/O     |
| **[TASK.KILL]** | Top CPU-consuming process name + usage %                     |
| **[DECK]**      | Media player (Artist, Track, Playing status)                 |

### 🚀 Architecture Highlights

- **Asyncio + ThreadPoolExecutor**: Python server never blocks TCP stream
- **Exact Hardware IDs**: Uses LibreHardwareMonitor sensor paths from `serverpars.txt`
- **2-Char JSON Keys**: Bandwidth-optimized protocol (`ct`, `gl`, `nd`, `pg`, etc.)
- **Anti-Ghosting Engine**: Clears exact text area before redraw (prevents OLED burn)
- **Robust Parsing**: Handles incomplete/corrupted TCP packets gracefully
- **WiFi Reconnection**: Auto-reconnects every 30 seconds if disconnected
- **"NO SIGNAL" Screen**: Shows static noise effect if TCP disconnects > 3 sec

### 🎨 Cyberpunk Design

- **Corner Crosshairs**: Military HUD targeting reticles
- **Monospace Fonts**: High-contrast, industrial aesthetic
- **Animated Elements**: Rotating fan icon, blinking media status
- **Temperature Alerts**: Blinking frame + LED when CPU/GPU exceed thresholds

---

## 📋 Requirements

### PC (Windows)

- **Python 3.10+**
- **[LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor)** with web server enabled at `http://localhost:8085`
- **Dependencies**: `pip install -r requirements.txt`
  - `psutil` (network/disk speed, process monitoring)
  - `requests` (LHM API, weather)
  - `python-dotenv` (config)
  - `winsdk` (Windows media info)
  - `pystray` + `Pillow` (system tray icon, album covers)

### Hardware

- **Heltec WiFi LoRa 32 V3** (ESP32-S3 + SSD1306 128×64 OLED)
- **PlatformIO** or **Arduino IDE**:
  - ESP32 board support
  - Libraries: `U8g2`, `ArduinoJson`, `WiFi`, `Preferences`

---

## 🚀 Quick Start

### 1. Configure PC Server

```bash
# Copy example config
copy .env.example .env

# Edit .env:
# - LHM_URL=http://localhost:8085/data.json
# - TCP_HOST=0.0.0.0
# - TCP_PORT=8888
# - PC_IP=192.168.1.2  (your PC's local IP)
# - WEATHER_LAT=55.7558  (optional)
# - WEATHER_LON=37.6173  (optional)
```

### 2. Install Dependencies & Run Server

```bash
pip install -r requirements.txt
python monitor.py
```

You should see:

```
[INFO] ============================================================
[INFO] Heltec PC Monitor Server v6.0 [NEURAL LINK]
[INFO] Cyberpunk Cyberdeck Edition
[INFO] ============================================================
[INFO] Starting TCP server on 0.0.0.0:8888
[INFO] Server ready. Waiting for clients...
```

On Windows, a system tray icon will appear. Right-click → Exit to quit.

### 3. Flash Firmware to Heltec

1. **Edit `main.cpp`** (lines 20-24):

   ```cpp
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASS "YourWiFiPassword"
   #define PC_IP "192.168.1.2"  // Your PC's IP
   #define TCP_PORT 8888
   ```

2. **Build & Upload**:

   - **PlatformIO**: Open project → Build → Upload
   - **Arduino IDE**: Install ESP32 boards + libraries → Compile → Upload

3. **Boot**: Heltec will show splash screen → connect to WiFi → connect to TCP server → display data.

### 4. Controls

- **Short Press**: Cycle through screens (6 screens)
- **Long Press (>800ms)**: Open settings menu
  - **Menu Items**: LED, Carousel, Contrast, Display, Exit
  - **In Menu**: Short press = next item, Long press = toggle/change value

Settings are saved to NVS (non-volatile storage).

---

## 📡 Data Protocol

### JSON Keys (2-Char for Bandwidth Efficiency)

```python
{
  "ct": 53,      # CPU temp (°C)
  "gt": 27,      # GPU temp (°C)
  "cl": 7,       # CPU load (%)
  "gl": 3,       # GPU load (%)
  "ru": 15.9,    # RAM used (GB)
  "ra": 32.0,    # RAM total (GB)
  "nd": 102,     # Network down (KB/s)
  "nu": 42,      # Network up (KB/s)
  "pg": 12,      # Ping latency (ms)
  "cf": 1548,    # CPU fan (RPM)
  "s1": 249,     # System fan 1 (RPM)
  "s2": 966,     # System fan 2 (RPM)
  "gf": 1299,    # GPU fan (RPM)
  "su": 75,      # System drive usage (%)
  "du": 90,      # Data drive usage (%)
  "vu": 2.7,     # VRAM used (GB)
  "vt": 12.0,    # VRAM total (GB)
  "ch": 26,      # Chipset temp (°C)
  "dr": 1635,    # Disk read (KB/s)
  "dw": 3792,    # Disk write (KB/s)
  "wt": 5,       # Weather temp (°C)
  "wd": "Clear", # Weather description
  "wi": 0,       # Weather icon code
  "tp": [        # Top processes by CPU
    {"n": "chrome.exe", "c": 12},
    {"n": "python.exe", "c": 5}
  ],
  "tr": [        # Top processes by RAM
    {"n": "chrome.exe", "r": 2048},
    {"n": "vscode.exe", "r": 1024}
  ],
  "art": "Artist Name",
  "trk": "Track Title",
  "mp": true,    # Media playing (bool)
  "cov": ""      # Album cover (base64, 48×48 bitmap)
}
```

### Hardware ID Mapping (from `serverpars.txt`)

```
CPU Temp:  /amdcpu/0/temperature/2        (Tdie)
GPU Temp:  /gpu-nvidia/0/temperature/0
CPU Load:  /amdcpu/0/load/0
GPU Load:  /gpu-nvidia/0/load/0
RAM Used:  /ram/data/0
RAM Avail: /ram/data/1                    (Total = Used + Avail)
Fans:
  CPU:     /lpc/it8688e/0/fan/0           (Pump)
  Sys1:    /lpc/it8688e/0/fan/1           (Radiator)
  Sys2:    /lpc/it8688e/0/fan/2
  GPU:     /gpu-nvidia/0/fan/1
Storage:
  NVMe:    /nvme/2/load/30                (System drive C:)
  HDD:     /hdd/0/load/30                 (Data drive D:)
VRAM:
  Used:    /gpu-nvidia/0/smalldata/1      (MB → converted to GB)
  Total:   /gpu-nvidia/0/smalldata/2      (MB → converted to GB)
Chipset:   /lpc/it8688e/0/temperature/0
```

---

## 🛠️ Troubleshooting

### No Data on Display

1. **Check LibreHardwareMonitor**: Open `http://localhost:8085/data.json` in browser

   - Verify sensor IDs match those in `monitor.py` TARGETS dict
   - Enable web server in LHM settings if not running

2. **Check Network**:

   - Verify `PC_IP` in `main.cpp` matches your PC's local IP (`ipconfig`)
   - Verify `TCP_PORT` (default 8888) is not blocked by firewall
   - Check WiFi credentials in `main.cpp`

3. **Check Serial Monitor** (PlatformIO/Arduino IDE):
   - Look for WiFi connection status
   - Look for TCP connection status

### "NO SIGNAL" Screen Appears

- Server not running or crashed
- Network disconnected
- Firewall blocking port 8888
- Wrong `PC_IP` in firmware

### Media Not Showing

- Requires `winsdk` (`pip install winsdk`)
- Only works with Windows Media API-compatible apps (Spotify, Windows Media Player, etc.)
- VLC/MPC-HC may not work (depends on version)

### Ping Shows 0ms

- `ping` command not in PATH (Windows: should be in `C:\Windows\System32`)
- Firewall blocking ICMP packets
- Check `get_ping_latency_sync()` function in `monitor.py`

---

## 📦 Build Standalone EXE

### One-Click Build Script

```bash
build_oneclick.bat
```

This script:

1. Creates Python virtual environment (`.venv`)
2. Installs dependencies from `requirements.txt`
3. Installs PyInstaller
4. Builds `monitor.exe` using `monitor.spec`
5. Copies `monitor.exe` to project root

### Manual Build

```bash
pip install -r requirements.txt
pip install pyinstaller
pyinstaller --noconfirm monitor.spec
copy dist\monitor.exe .
```

### Running EXE

- Place `.env` file next to `monitor.exe`
- Double-click `monitor.exe` (runs in background, system tray icon)
- Right-click tray icon → Exit to quit

### Autostart (Windows)

**Method 1: Startup Folder**

```
Win+R → shell:startup
Create shortcut to monitor.exe or run_monitor.vbs
```

**Method 2: Registry (automatic)**

- Run `monitor.py` from Python (not EXE)
- Answer "Y" when prompted to add to autostart
- Or use tray icon menu: "Add to Autostart"

See [AUTOSTART.md](AUTOSTART.md) for details.

---

## 🎨 Customization

### Temperature Thresholds

Edit `main.cpp` (lines 26-29):

```cpp
#define CPU_TEMP_ALERT 80  // °C
#define GPU_TEMP_ALERT 85  // °C
```

### Screen Order

Modify `TOTAL_SCREENS` and `drawScreen()` switch statement in `main.cpp`.

### Fonts

Change font defines (lines 52-58):

```cpp
#define FONT_DATA u8g2_font_6x12_tf
#define FONT_BIG u8g2_font_helvB12_tf
// etc.
```

Browse available fonts: [U8g2 Font List](https://github.com/olikraus/u8g2/wiki/fntlistall)

### Weather Location

Edit `.env`:

```
WEATHER_LAT=55.7558
WEATHER_LON=37.6173
```

Get coordinates: [latlong.net](https://www.latlong.net/)

---

## 📁 Project Structure

```
Heltec v4/
├── main.cpp                 # ESP32 firmware (6 screens, WiFi, TCP)
├── platformio.ini           # PlatformIO config (board, libraries)
├── monitor.py               # Python server (asyncio, LHM, weather, ping)
├── requirements.txt         # Python dependencies
├── .env.example             # Config template
├── .env                     # Your config (DO NOT COMMIT)
├── serverpars.txt           # LHM sensor IDs (reference)
├── run_monitor.bat          # Launch server (console)
├── run_monitor.vbs          # Launch server (no console)
├── build_oneclick.bat       # One-click EXE builder
├── monitor.spec             # PyInstaller spec (no console, tray)
├── README.md                # This file
├── CHANGELOG.md             # Version history
├── PROTOCOL.md              # JSON protocol spec
├── FIRST_START.md           # Step-by-step first run guide
└── AUTOSTART.md             # Autostart setup guide
```

---

## 🔧 Advanced Configuration

### Change TCP Port

**.env:**

```
TCP_PORT=9999
```

**main.cpp:**

```cpp
#define TCP_PORT 9999
```

### Add Custom Sensor

1. Find sensor ID in LHM: `http://localhost:8085/data.json`
2. Add to `TARGETS` dict in `monitor.py`:
   ```python
   TARGETS = {
       "my": "/path/to/sensor/id",
   }
   ```
3. Add to payload in `build_payload()`:
   ```python
   "my": int(hw.get("my", 0)),
   ```
4. Parse in `main.cpp`:
   ```cpp
   int myValue = doc["my"] | 0;
   ```
5. Display in screen function

### Disable Weather

Remove weather fetch from `run()` in `monitor.py`:

```python
# Comment out:
# if now - last_weather_time >= WEATHER_INTERVAL_F:
#     last_weather_time = now
#     cache["weather"] = await loop.run_in_executor(executor, get_weather_sync)
```

---

## 🐛 Known Issues

1. **Album Cover**: Some media players don't expose thumbnails via Windows Media API
2. **VRAM on AMD GPUs**: May not be available in LHM (NVIDIA only in current mapping)
3. **Ping on Some Networks**: Corporate/VPN networks may block ICMP
4. **Font Rendering**: Some Unicode characters may not render correctly

---

## 📝 License

Free to use and modify. Attribution appreciated but not required.

---

## 🙏 Credits

- **LibreHardwareMonitor**: Hardware monitoring
- **Open-Meteo**: Weather API
- **U8g2**: OLED display library
- **ArduinoJson**: JSON parsing on ESP32
- **Heltec**: ESP32 hardware platform

---

## 📞 Support

- **Issues**: Open an issue on GitHub
- **Discussions**: Use GitHub Discussions
- **Documentation**: See `FIRST_START.md`, `PROTOCOL.md`, `AUTOSTART.md`

---

<div align="center">

**v6.0 [NEURAL LINK]** — Cyberpunk Cyberdeck Edition

_"The future is already here — it's just not evenly distributed."_

</div>
