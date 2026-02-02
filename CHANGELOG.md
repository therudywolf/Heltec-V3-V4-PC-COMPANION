# CHANGELOG

## v6.0 [NEURAL LINK] - 2026-02-03

### 🚀 MAJOR REFACTOR - Cyberpunk Cyberdeck Edition

**Architecture Overhaul:**

- ✅ Python server now uses `asyncio` with `ThreadPoolExecutor` for blocking operations
- ✅ TCP stream never blocks (LHM, Weather, Ping run in executor threads)
- ✅ Exact Hardware IDs from `serverpars.txt` (no more guessing)
- ✅ 2-char JSON keys for bandwidth efficiency (`ct`, `gl`, `nd`, `pg`, etc.)

**New Features:**

- ✅ **Ping Latency**: Pings Google DNS (8.8.8.8) every 5 seconds, displays in [NEURAL] screen
- ✅ **Top Process Tracking**: Shows highest CPU-consuming process in [TASK.KILL] screen
- ✅ **Improved Network Speed**: Uses `psutil` delta calculation (accurate KB/s)
- ✅ **VRAM Monitoring**: Displays GPU memory usage (Used/Total in GB)

**Firmware (ESP32) Improvements:**

- ✅ **Anti-Ghosting Engine**: `drawMetric()` clears exact text area before redraw (mandatory)
- ✅ **Cyberpunk HUD**: Corner crosshairs, status bar, high-contrast design
- ✅ **6 Screens**:
  1. **[CORTEX]** - CPU/GPU temps & loads (2x2 grid, big numbers)
  2. **[NEURAL]** - Network up/down + ping latency (big numbers)
  3. **[THERMAL]** - Fan RPMs with animated fan icon
  4. **[MEM.BANK]** - RAM + Storage bars (NVMe & HDD)
  5. **[TASK.KILL]** - Top CPU process name + usage %
  6. **[DECK]** - Media player (artist, track, playing status)
- ✅ **Robust JSON Parsing**: Handles incomplete/corrupted packets gracefully
- ✅ **WiFi Reconnection**: Auto-reconnects every 30 sec if disconnected
- ✅ **"NO SIGNAL" Screen**: Shows static noise effect if TCP disconnects > 3 sec

**Data Mapping (Exact IDs):**

```
CPU Temp:  /amdcpu/0/temperature/2 (Tdie)
GPU Temp:  /gpu-nvidia/0/temperature/0
CPU Load:  /amdcpu/0/load/0
GPU Load:  /gpu-nvidia/0/load/0
RAM Used:  /ram/data/0
RAM Total: /ram/data/1 (calculated from Used + Available)
Fans:
  - CPU:   /lpc/it8688e/0/fan/0
  - Sys1:  /lpc/it8688e/0/fan/1
  - Sys2:  /lpc/it8688e/0/fan/2
  - GPU:   /gpu-nvidia/0/fan/1
Storage:
  - NVMe:  /nvme/2/load/30
  - HDD:   /hdd/0/load/30
VRAM:
  - Used:  /gpu-nvidia/0/smalldata/1
  - Total: /gpu-nvidia/0/smalldata/2
Chipset:   /lpc/it8688e/0/temperature/0
```

**Visual Design Philosophy:**

- **Cyberpunk Aesthetic**: Military HUD, industrial, high-contrast
- **Anti-Ghosting**: Black box clears old text before drawing new (prevents OLED burn)
- **Monospace Fonts**: `u8g2_font_6x12_tf` for data, `u8g2_font_helvB12_tf` for big numbers
- **Animated Elements**: Rotating fan icon, blinking media status

**Breaking Changes:**

- JSON key names changed to 2-char format (bandwidth optimization)
- Screen names updated to cyberpunk theme
- Removed weather screen (can be re-added if needed)

---

## v3.0 - Previous Version

- Basic TCP JSON streaming
- 6 screens (SYS.OP, NET.IO, THERMAL, STORAGE, ATMOS, MEDIA)
- Weather integration
- Media player support
