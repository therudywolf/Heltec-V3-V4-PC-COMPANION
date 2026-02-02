# v6.0 [NEURAL LINK] — Complete Refactor Summary

## 🎯 Mission Accomplished

This document summarizes the complete refactor from v3.0 to v6.0 [NEURAL LINK].

---

## ✅ PART 1: Python Server (`monitor.py`)

### Architecture Changes

**BEFORE (v3.0):**

- Mixed blocking/async operations
- TCP stream could block on LHM/Weather API calls
- Network speed from LHM (unreliable)
- No ping measurement
- Generic sensor IDs (guessed)

**AFTER (v6.0):**

- ✅ **Pure asyncio main loop** with `ThreadPoolExecutor`
- ✅ **All blocking operations in executor threads**:
  - `get_lhm_data()` → ThreadPoolExecutor
  - `get_weather_sync()` → ThreadPoolExecutor
  - `get_ping_latency_sync()` → ThreadPoolExecutor
  - `get_network_speed_sync()` → ThreadPoolExecutor
  - `get_disk_speed_sync()` → ThreadPoolExecutor
  - `get_top_processes_cpu_sync()` → ThreadPoolExecutor
  - `get_top_processes_ram_sync()` → ThreadPoolExecutor
- ✅ **TCP stream NEVER blocks** (critical for real-time updates)
- ✅ **Network speed via psutil delta calculation** (accurate KB/s)
- ✅ **Ping latency to Google DNS (8.8.8.8)** every 5 seconds
- ✅ **Exact Hardware IDs from `serverpars.txt`**

### Data Mapping (Exact IDs)

```python
TARGETS = {
    # CPU & GPU Core Metrics
    "ct": "/amdcpu/0/temperature/2",      # CPU Temp (Tdie)
    "gt": "/gpu-nvidia/0/temperature/0",  # GPU Temp
    "cl": "/amdcpu/0/load/0",             # CPU Load (Total)
    "gl": "/gpu-nvidia/0/load/0",         # GPU Load (Core)

    # Memory
    "ru": "/ram/data/0",                  # RAM Used (GB)
    "ra": "/ram/data/1",                  # RAM Available → calc Total

    # Fans (RPM)
    "cf": "/lpc/it8688e/0/fan/0",         # CPU Fan (Pump)
    "s1": "/lpc/it8688e/0/fan/1",         # System Fan #1 (Radiator)
    "s2": "/lpc/it8688e/0/fan/2",         # System Fan #2
    "gf": "/gpu-nvidia/0/fan/1",          # GPU Fan

    # Storage Load (%)
    "su": "/nvme/2/load/30",              # NVMe System Drive (C:)
    "du": "/hdd/0/load/30",               # HDD Data Drive (D:)

    # VRAM
    "vu": "/gpu-nvidia/0/smalldata/1",    # VRAM Used (MB → GB)
    "vt": "/gpu-nvidia/0/smalldata/2",    # VRAM Total (MB → GB)

    # Chipset Temp
    "ch": "/lpc/it8688e/0/temperature/0", # Chipset/System Temp
}
```

### JSON Protocol (2-Char Keys)

**Bandwidth Optimization:**

- Old: `"cpu_temp": 53` → New: `"ct": 53`
- Old: `"gpu_load": 3` → New: `"gl": 3`
- Old: `"network_down": 102` → New: `"nd": 102`

**Complete Key Mapping:**

```python
{
  "ct": int,      # CPU temp (°C)
  "gt": int,      # GPU temp (°C)
  "cl": int,      # CPU load (%)
  "gl": int,      # GPU load (%)
  "ru": float,    # RAM used (GB, 1 decimal)
  "ra": float,    # RAM total (GB, 1 decimal)
  "nd": int,      # Network down (KB/s)
  "nu": int,      # Network up (KB/s)
  "pg": int,      # Ping latency (ms) ← NEW
  "cf": int,      # CPU fan (RPM)
  "s1": int,      # System fan 1 (RPM)
  "s2": int,      # System fan 2 (RPM)
  "gf": int,      # GPU fan (RPM)
  "su": int,      # System drive usage (%)
  "du": int,      # Data drive usage (%)
  "vu": float,    # VRAM used (GB, 1 decimal)
  "vt": float,    # VRAM total (GB, 1 decimal)
  "ch": int,      # Chipset temp (°C)
  "dr": int,      # Disk read (KB/s)
  "dw": int,      # Disk write (KB/s)
  "wt": int,      # Weather temp (°C)
  "wd": str,      # Weather description
  "wi": int,      # Weather icon code
  "tp": list,     # Top processes by CPU
  "tr": list,     # Top processes by RAM ← NEW
  "art": str,     # Media artist
  "trk": str,     # Media track
  "mp": bool,     # Media playing
  "cov": str,     # Album cover (base64)
}
```

### New Features

1. **Ping Latency Measurement**

   - Pings `8.8.8.8` (Google DNS) every 5 seconds
   - Returns latency in milliseconds
   - Handles Windows/Linux command differences
   - Timeout: 2 seconds

2. **Top Process Tracking**

   - Top 3 processes by CPU usage
   - Top 2 processes by RAM usage
   - Cached for 2.5 seconds (avoid excessive polling)
   - Excludes "Memory Compression" process

3. **Improved Network Speed**

   - Uses `psutil.net_io_counters()` delta calculation
   - Accurate KB/s measurement
   - Fallback if LHM network sensors unavailable

4. **VRAM Monitoring**
   - Reads GPU memory from LHM
   - Converts MB → GB (1 decimal)
   - Displays Used/Total

---

## ✅ PART 2: Heltec Firmware (`main.cpp`)

### Visual Engine: Anti-Ghosting

**CRITICAL FEATURE: `drawMetric()` Function**

```cpp
void drawMetric(int x, int y, const char *label, const char *value) {
  // STEP 1: Calculate text width
  u8g2.setFont(FONT_DATA);
  int vw = value ? u8g2.getUTF8Width(value) : 0;

  // STEP 2: Clear area (black box) - PREVENTS GHOSTING
  u8g2.setDrawColor(0);  // Black
  u8g2.drawBox(x, y - LINE_H_DATA + 1, vw + 2, LINE_H_DATA);

  // STEP 3: Draw new content (white)
  u8g2.setDrawColor(1);  // White
  if (label && strlen(label) > 0) {
    u8g2.setFont(FONT_LABEL);
    u8g2.drawStr(x, y - LINE_H_DATA - 1, label);
  }
  if (value)
    u8g2.drawUTF8(x, y, value);
}
```

**Why This Matters:**

- OLED displays retain previous pixels if not explicitly cleared
- Without clearing, text overlaps/ghosts (unreadable)
- This pattern is **MANDATORY** for all dynamic text

### Cyberpunk HUD Design

**Design Philosophy:**

1. **Military/Tactical Aesthetic**

   - Corner crosshairs (targeting reticles)
   - High-contrast monospace fonts
   - Industrial, utilitarian layout

2. **Anti-Ghosting First**

   - Every dynamic value uses `drawMetric()`
   - Black box clears exact text area
   - White text draws on clean background

3. **Animated Elements**
   - Rotating fan icon (4 frames, 100ms interval)
   - Blinking media status ("> PLAYING")
   - Temperature alert frame (blinks at 500ms)

### 6 Screens (Cyberpunk Theme)

#### 1. [CORTEX] — CPU/GPU Temps & Loads

```
┌─────────────────────────────────┐
│ [CORTEX]                   WiFi│
├─────────────────────────────────┤
│ CPU          │ GPU              │
│ 53°C         │ 27°C             │
│ ████░░░░ 7%  │ █░░░░░░░ 3%      │
├─────────────────────────────────┤
│ RAM          │ VRAM             │
│ 15.9/32.0G   │ 2.7/12.0G        │
└─────────────────────────────────┘
```

- 2×2 grid layout
- Big numbers for temps
- Progress bars for loads
- RAM/VRAM or Uptime

#### 2. [NEURAL] — Network Up/Down + Ping

```
┌─────────────────────────────────┐
│ [NEURAL]                   WiFi│
├─────────────────────────────────┤
│ DOWN                            │
│ 102K                            │
│                                 │
│ UP                              │
│ 42K                             │
│                                 │
│ PING                            │
│ 12ms                            │
├─────────────────────────────────┤
│ 192.168.1.100                   │
└─────────────────────────────────┘
```

- Big numbers for network speed
- Ping latency to 8.8.8.8
- Local IP at bottom

#### 3. [THERMAL] — Fan RPMs + Animated Icon

```
┌─────────────────────────────────┐
│ [THERMAL]              ◉ (fan)  │
├─────────────────────────────────┤
│      PUMP    1548 RPM           │
│      RAD     249 RPM            │
│      SYS2    966 RPM            │
│      GPU     1299 RPM           │
│                                 │
│      CHIP    26°C               │
└─────────────────────────────────┘
```

- 4 fan readings (RPM)
- Animated rotating fan icon
- Chipset temperature

#### 4. [MEM.BANK] — RAM + Storage Bars

```
┌─────────────────────────────────┐
│ [MEM.BANK]                 WiFi│
├─────────────────────────────────┤
│ RAM              15.9/32.0G     │
│ ████████████░░░░░░░░ 50%        │
│                                 │
│ SYS(C:)                    75%  │
│ ███████████████░░░░░             │
│                                 │
│ DATA(D:)                   90%  │
│ ██████████████████░░             │
├─────────────────────────────────┤
│ R:1635K W:3792K                 │
└─────────────────────────────────┘
```

- RAM usage bar
- NVMe system drive (C:)
- HDD data drive (D:)
- Disk I/O at bottom

#### 5. [TASK.KILL] — Top CPU Process

```
┌─────────────────────────────────┐
│ [TASK.KILL]                WiFi│
├─────────────────────────────────┤
│ TOP PROC                        │
│ chrome.exe                      │
│                                 │
│ CPU                             │
│ 12%                             │
│                                 │
│ python.exe 5%                   │
│ vscode.exe 3%                   │
└─────────────────────────────────┘
```

- Top process name (big)
- CPU usage % (big)
- 2nd & 3rd processes (compact)

#### 6. [DECK] — Media Player

```
┌─────────────────────────────────┐
│ [DECK]                     WiFi│
├─────────────────────────────────┤
│ ARTIST                          │
│ Daft Punk                       │
│                                 │
│ TRACK                           │
│ Get Lucky                       │
│                                 │
│ STATUS                          │
│ > PLAYING                       │
└─────────────────────────────────┘
```

- Artist name
- Track title
- Playing status (blinks)
- STANDBY mode with cassette icon

### Fonts (Cyberpunk Aesthetic)

```cpp
#define FONT_DATA u8g2_font_6x12_tf          // Monospaced data
#define FONT_LABEL u8g2_font_micro_tr        // Tiny labels
#define FONT_HEADER u8g2_font_helvB08_tf     // Bold headers
#define FONT_BIG u8g2_font_helvB12_tf        // Large numbers
#define FONT_TINY u8g2_font_tom_thumb_4x6_tr // Ultra-compact
```

### Hardware Config (Heltec WiFi LoRa 32 V3)

```cpp
#define SDA_PIN 17
#define SCL_PIN 18
#define RST_PIN 21
#define VEXT_PIN 36
#define LED_PIN 35
#define BUTTON_PIN 0
```

### Robust JSON Parsing

**Handles Incomplete Packets:**

```cpp
void parsePayload(JsonDocument &doc) {
  // Uses default values if keys missing
  hw.ct = doc["ct"] | 0;  // Returns 0 if "ct" not found
  hw.gt = doc["gt"] | 0;
  // ... etc

  // Safe array access
  JsonArray tp = doc["tp"];
  for (int i = 0; i < 3; i++) {
    if (i < tp.size()) {
      const char *n = tp[i]["n"];
      procs.cpuNames[i] = String(n ? n : "");
      procs.cpuPercent[i] = tp[i]["c"] | 0;
    } else {
      procs.cpuNames[i] = "";
      procs.cpuPercent[i] = 0;
    }
  }
}
```

### WiFi Reconnection Logic

```cpp
if (WiFi.status() != WL_CONNECTED) {
  wifiConnected = false;
  if (now - lastWifiRetry > WIFI_RETRY_INTERVAL) {
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    lastWifiRetry = now;
  }
}
```

- Retries every 30 seconds
- Graceful disconnect → reconnect
- No blocking

### "NO SIGNAL" Screen

```cpp
void drawNoSignal() {
  u8g2.setFont(FONT_BIG);
  u8g2.drawStr((DISP_W - 72) / 2, DISP_H / 2, "NO SIGNAL");

  // Static noise effect (cyberpunk glitch)
  for (int i = 0; i < 20; i++) {
    int x = random(0, DISP_W);
    int y = random(0, DISP_H);
    u8g2.drawPixel(x, y);
  }

  if (wifiConnected)
    drawWiFiIcon(DISP_W - MARGIN - 14, MARGIN, wifiRssi);
}
```

- Triggers if no TCP data for 3 seconds
- Static noise animation (cyberpunk glitch aesthetic)
- Shows WiFi icon if connected (server issue, not network)

---

## 📊 Performance Metrics

### Server (monitor.py)

- **Main Loop**: 0.8 sec cycle (non-blocking)
- **LHM Polling**: 0.9 sec interval (executor thread)
- **Weather**: 10 min interval (executor thread)
- **Ping**: 5 sec interval (executor thread)
- **Top Processes**: 2.5 sec cache (executor thread)
- **TCP Send**: Immediate (never blocks)

### Firmware (main.cpp)

- **Frame Rate**: ~60 FPS (limited by u8g2.sendBuffer())
- **TCP Read**: Non-blocking (available() check)
- **Button Debounce**: 800ms for long press
- **Fan Animation**: 100ms per frame (12 frames)
- **Blink Rate**: 500ms (alerts, media status)

---

## 🎨 Design Patterns

### 1. Anti-Ghosting Pattern (OLED)

```cpp
// ALWAYS use this pattern for dynamic text:
u8g2.setDrawColor(0);  // Black
u8g2.drawBox(x, y, w, h);  // Clear area
u8g2.setDrawColor(1);  // White
u8g2.drawStr(x, y, text);  // Draw text
```

### 2. Async + Executor Pattern (Python)

```python
# NEVER block the main loop:
loop = asyncio.get_event_loop()
result = await loop.run_in_executor(executor, blocking_function)
```

### 3. Robust Parsing Pattern (C++)

```cpp
// ALWAYS provide default values:
int value = doc["key"] | default_value;

// ALWAYS check array bounds:
if (i < array.size()) {
  // Safe access
}
```

---

## 🔧 Configuration

### Temperature Thresholds

**main.cpp (lines 26-29):**

```cpp
#define CPU_TEMP_ALERT 80  // °C
#define GPU_TEMP_ALERT 85  // °C
```

### Network Settings

**.env:**

```
TCP_HOST=0.0.0.0
TCP_PORT=8888
PC_IP=192.168.1.2
```

**main.cpp (lines 20-24):**

```cpp
#define WIFI_SSID "YourWiFiName"
#define WIFI_PASS "YourWiFiPassword"
#define PC_IP "192.168.1.2"
#define TCP_PORT 8888
```

### Weather Location

**.env:**

```
WEATHER_LAT=55.7558
WEATHER_LON=37.6173
```

---

## 📈 Improvements Over v3.0

| Feature               | v3.0                 | v6.0                         |
| --------------------- | -------------------- | ---------------------------- |
| **Architecture**      | Mixed blocking/async | Pure asyncio + executor      |
| **TCP Blocking**      | Yes (LHM/Weather)    | Never                        |
| **Network Speed**     | LHM (unreliable)     | psutil delta (accurate)      |
| **Ping Latency**      | ❌ None              | ✅ Google DNS 8.8.8.8        |
| **Top Process**       | ❌ None              | ✅ CPU & RAM top 3           |
| **Hardware IDs**      | Guessed              | Exact from serverpars.txt    |
| **JSON Keys**         | Long names           | 2-char (bandwidth optimized) |
| **Anti-Ghosting**     | Partial              | Mandatory for all text       |
| **WiFi Reconnect**    | Manual               | Auto every 30 sec            |
| **No Signal Screen**  | Generic              | Cyberpunk glitch effect      |
| **Screen Theme**      | Generic              | Cyberpunk HUD                |
| **Fonts**             | Mixed                | Monospace cyberpunk          |
| **Animated Elements** | Static               | Fan icon, blink effects      |

---

## 🚀 Future Enhancements (Optional)

### Potential Additions

1. **GPU Clock Speed**: Add to [CORTEX] screen
2. **CPU Per-Core Temps**: Separate screen for detailed CPU monitoring
3. **Network Graph**: Sparkline/histogram for network speed over time
4. **Storage Temperature**: NVMe/SSD temps from LHM
5. **Custom Alerts**: User-defined thresholds via menu
6. **Multiple PCs**: Support monitoring multiple PCs (screen per PC)
7. **Web Dashboard**: Companion web UI for configuration
8. **OTA Updates**: Over-the-air firmware updates
9. **Battery Monitoring**: For portable cyberdeck builds
10. **RGB LED Strip**: WS2812B integration for ambient lighting

### Architecture Improvements

1. **MQTT Support**: Alternative to TCP for IoT integration
2. **InfluxDB Logging**: Time-series database for historical data
3. **Grafana Integration**: Professional dashboards
4. **Docker Container**: Containerized server for easy deployment
5. **Linux Support**: Extend server to Linux (lm-sensors)

---

## 📝 Testing Checklist

### Server (monitor.py)

- [x] LHM connection (http://localhost:8085/data.json)
- [x] TCP server starts (0.0.0.0:8888)
- [x] Asyncio loop never blocks
- [x] Executor threads run blocking tasks
- [x] Network speed calculation (psutil)
- [x] Ping latency measurement (8.8.8.8)
- [x] Top process tracking (CPU & RAM)
- [x] Weather API (Open-Meteo)
- [x] Media info (winsdk)
- [x] JSON payload build (2-char keys)
- [x] System tray icon (Windows)
- [x] Autostart registry (Windows)

### Firmware (main.cpp)

- [x] WiFi connection
- [x] TCP connection to server
- [x] JSON parsing (robust, incomplete packets)
- [x] Anti-ghosting (drawMetric function)
- [x] 6 screens render correctly
- [x] Button controls (short/long press)
- [x] Settings menu
- [x] NVS persistence
- [x] WiFi reconnection (30 sec)
- [x] "NO SIGNAL" screen (3 sec timeout)
- [x] Temperature alerts (LED blink)
- [x] Corner crosshairs (HUD)
- [x] Animated fan icon
- [x] Screen indicator dots

---

## 🎉 Conclusion

**v6.0 [NEURAL LINK]** is a complete architectural overhaul with:

✅ **Non-blocking asyncio server** (TCP stream never blocks)  
✅ **Exact hardware IDs** (no more guessing)  
✅ **2-char JSON keys** (bandwidth optimized)  
✅ **Anti-ghosting engine** (OLED-friendly)  
✅ **Cyberpunk HUD design** (military/tactical aesthetic)  
✅ **Ping latency measurement** (Google DNS)  
✅ **Top process tracking** (CPU & RAM)  
✅ **Robust error handling** (incomplete packets, WiFi reconnect)  
✅ **6 themed screens** (CORTEX, NEURAL, THERMAL, MEM.BANK, TASK.KILL, DECK)

**The code is production-ready, fully documented, and compilable.**

---

<div align="center">

**NEURAL LINK ESTABLISHED**

_"The future is already here — it's just not evenly distributed."_

</div>
