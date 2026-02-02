# v6.0 [NEURAL LINK] — JSON Protocol Specification

## 📡 Communication Protocol

### Overview

- **Transport**: TCP stream (default port 8888)
- **Format**: JSON (newline-delimited)
- **Direction**: Server → Client (Heltec)
- **Frequency**: ~0.8 seconds per message
- **Encoding**: UTF-8

### Message Structure

```json
{
  "ct": 53,
  "gt": 27,
  "cl": 7,
  "gl": 3,
  "ru": 15.9,
  "ra": 32.0,
  "nd": 102,
  "nu": 42,
  "pg": 12,
  "cf": 1548,
  "s1": 249,
  "s2": 966,
  "gf": 1299,
  "su": 75,
  "du": 90,
  "vu": 2.7,
  "vt": 12.0,
  "ch": 26,
  "dr": 1635,
  "dw": 3792,
  "wt": 5,
  "wd": "Clear",
  "wi": 0,
  "tp": [
    { "n": "chrome.exe", "c": 12 },
    { "n": "python.exe", "c": 5 },
    { "n": "vscode.exe", "c": 3 }
  ],
  "tr": [
    { "n": "chrome.exe", "r": 2048 },
    { "n": "vscode.exe", "r": 1024 }
  ],
  "art": "Daft Punk",
  "trk": "Get Lucky",
  "mp": true,
  "cov": ""
}
```

---

## 🔑 Key Definitions

### Core Metrics

| Key  | Type | Unit | Description            | Range |
| ---- | ---- | ---- | ---------------------- | ----- |
| `ct` | int  | °C   | CPU temperature (Tdie) | 0-100 |
| `gt` | int  | °C   | GPU temperature        | 0-100 |
| `cl` | int  | %    | CPU load (total)       | 0-100 |
| `gl` | int  | %    | GPU load (core)        | 0-100 |

### Memory

| Key  | Type  | Unit | Description | Range     |
| ---- | ----- | ---- | ----------- | --------- |
| `ru` | float | GB   | RAM used    | 0.0-256.0 |
| `ra` | float | GB   | RAM total   | 0.0-256.0 |
| `vu` | float | GB   | VRAM used   | 0.0-24.0  |
| `vt` | float | GB   | VRAM total  | 0.0-24.0  |

### Network

| Key  | Type | Unit | Description            | Range     |
| ---- | ---- | ---- | ---------------------- | --------- |
| `nd` | int  | KB/s | Network download speed | 0-1000000 |
| `nu` | int  | KB/s | Network upload speed   | 0-1000000 |
| `pg` | int  | ms   | Ping latency (8.8.8.8) | 0-1000    |

### Cooling

| Key  | Type | Unit | Description             | Range  |
| ---- | ---- | ---- | ----------------------- | ------ |
| `cf` | int  | RPM  | CPU fan (pump)          | 0-5000 |
| `s1` | int  | RPM  | System fan 1 (radiator) | 0-5000 |
| `s2` | int  | RPM  | System fan 2            | 0-5000 |
| `gf` | int  | RPM  | GPU fan                 | 0-5000 |
| `ch` | int  | °C   | Chipset temperature     | 0-100  |

### Storage

| Key  | Type | Unit | Description               | Range     |
| ---- | ---- | ---- | ------------------------- | --------- |
| `su` | int  | %    | System drive usage (NVMe) | 0-100     |
| `du` | int  | %    | Data drive usage (HDD)    | 0-100     |
| `dr` | int  | KB/s | Disk read speed           | 0-1000000 |
| `dw` | int  | KB/s | Disk write speed          | 0-1000000 |

### Weather

| Key  | Type   | Unit | Description             | Range        |
| ---- | ------ | ---- | ----------------------- | ------------ |
| `wt` | int    | °C   | Weather temperature     | -50-50       |
| `wd` | string | -    | Weather description     | max 20 chars |
| `wi` | int    | -    | Weather icon code (WMO) | 0-99         |

### Processes

| Key       | Type   | Description                  |
| --------- | ------ | ---------------------------- |
| `tp`      | array  | Top 3 processes by CPU usage |
| `tp[i].n` | string | Process name (max 20 chars)  |
| `tp[i].c` | int    | CPU usage (%)                |
| `tr`      | array  | Top 2 processes by RAM usage |
| `tr[i].n` | string | Process name (max 20 chars)  |
| `tr[i].r` | int    | RAM usage (MB)               |

### Media

| Key   | Type   | Description                                   |
| ----- | ------ | --------------------------------------------- |
| `art` | string | Artist name (max 30 chars)                    |
| `trk` | string | Track title (max 30 chars)                    |
| `mp`  | bool   | Media playing status                          |
| `cov` | string | Album cover (base64, 48×48 bitmap, 288 bytes) |

---

## 🔄 Client → Server Messages

### Screen Change Notification

```
screen:0\n
screen:1\n
screen:2\n
...
```

**Format**: `screen:<number>\n`  
**Purpose**: Notify server of current screen (optional, for future use)  
**Range**: 0-5 (6 screens)

---

## 📊 Data Sources

### LibreHardwareMonitor Sensor IDs

```python
TARGETS = {
    "ct": "/amdcpu/0/temperature/2",      # CPU Temp (Tdie)
    "gt": "/gpu-nvidia/0/temperature/0",  # GPU Temp
    "cl": "/amdcpu/0/load/0",             # CPU Load
    "gl": "/gpu-nvidia/0/load/0",         # GPU Load
    "ru": "/ram/data/0",                  # RAM Used
    "ra": "/ram/data/1",                  # RAM Available → Total
    "cf": "/lpc/it8688e/0/fan/0",         # CPU Fan
    "s1": "/lpc/it8688e/0/fan/1",         # System Fan 1
    "s2": "/lpc/it8688e/0/fan/2",         # System Fan 2
    "gf": "/gpu-nvidia/0/fan/1",          # GPU Fan
    "su": "/nvme/2/load/30",              # NVMe Usage
    "du": "/hdd/0/load/30",               # HDD Usage
    "vu": "/gpu-nvidia/0/smalldata/1",    # VRAM Used (MB)
    "vt": "/gpu-nvidia/0/smalldata/2",    # VRAM Total (MB)
    "ch": "/lpc/it8688e/0/temperature/0", # Chipset Temp
}
```

### psutil Calculations

- **Network Speed**: Delta calculation from `psutil.net_io_counters()`
- **Disk I/O**: Delta calculation from `psutil.disk_io_counters()`
- **Top Processes**: `psutil.process_iter()` sorted by CPU/RAM

### External APIs

- **Weather**: Open-Meteo API (https://api.open-meteo.com)
- **Ping**: System `ping` command to 8.8.8.8

### Windows SDK

- **Media Info**: `winsdk.windows.media.control` API
- **Album Cover**: Thumbnail converted to 48×48 monochrome bitmap

---

## 🔧 Parsing Guidelines (Firmware)

### Robust Parsing Pattern

```cpp
// ALWAYS provide default values:
hw.ct = doc["ct"] | 0;
hw.gt = doc["gt"] | 0;
hw.ru = doc["ru"] | 0.0f;
hw.ra = doc["ra"] | 0.0f;

// ALWAYS check array bounds:
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

// ALWAYS check string pointers:
const char *art = doc["art"];
media.artist = String(art ? art : "");
```

### Incomplete Packet Handling

```cpp
// Buffer incoming data:
while (tcpClient.available()) {
  char c = tcpClient.read();
  if (c == '\n') {
    // Parse complete line
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, tcpLineBuffer);
    if (!err) {
      parsePayload(doc);
    }
    tcpLineBuffer = "";
  } else {
    tcpLineBuffer += c;
    if (tcpLineBuffer.length() >= TCP_LINE_MAX) {
      tcpLineBuffer = "";  // Discard if too long
    }
  }
}
```

---

## 📏 Size Constraints

### String Lengths

| Field     | Max Length | Truncation  |
| --------- | ---------- | ----------- |
| `wd`      | 20 chars   | Server-side |
| `tp[i].n` | 20 chars   | Server-side |
| `tr[i].n` | 20 chars   | Server-side |
| `art`     | 30 chars   | Server-side |
| `trk`     | 30 chars   | Server-side |

### Buffer Sizes

| Buffer       | Size       | Purpose                 |
| ------------ | ---------- | ----------------------- |
| TCP Line     | 4096 bytes | Max JSON message size   |
| Album Cover  | 288 bytes  | 48×48 monochrome bitmap |
| Base64 Cover | ~400 chars | Encoded album cover     |

---

## 🔐 Error Handling

### Server-Side

```python
# Never raise exceptions in main loop:
try:
    result = await loop.run_in_executor(executor, blocking_function)
except Exception as e:
    log_err(f"Error: {e}", trace_bt=False)
    result = default_value  # Always provide fallback
```

### Client-Side

```cpp
// Always check deserialization:
DeserializationError err = deserializeJson(doc, json);
if (err) {
  // Keep previous values, don't update
  return;
}

// Always provide defaults:
int value = doc["key"] | default_value;
```

---

## 📈 Performance Metrics

### Message Frequency

| Condition  | Frequency | Reason             |
| ---------- | --------- | ------------------ |
| Normal     | 0.8 sec   | Main loop interval |
| High Load  | 0.8 sec   | Never increases    |
| No Clients | N/A       | No messages sent   |

### Bandwidth Usage

| Scenario              | Bytes/Msg | KB/s (avg) |
| --------------------- | --------- | ---------- |
| Minimal (no media)    | ~300      | 0.4        |
| With media (no cover) | ~400      | 0.5        |
| With album cover      | ~700      | 0.9        |

### Latency

| Metric          | Target | Typical |
| --------------- | ------ | ------- |
| Server → Client | <100ms | 20-50ms |
| Parse Time      | <10ms  | 2-5ms   |
| Display Update  | <50ms  | 10-20ms |

---

## 🧪 Testing Protocol

### Validation Checklist

- [ ] All keys present in message
- [ ] All values within valid ranges
- [ ] String lengths respected
- [ ] Array sizes correct (tp: 3, tr: 2)
- [ ] No null values (use defaults)
- [ ] JSON valid (no syntax errors)
- [ ] Newline-terminated
- [ ] UTF-8 encoded

### Test Messages

**Minimal Message:**

```json
{
  "ct": 0,
  "gt": 0,
  "cl": 0,
  "gl": 0,
  "ru": 0.0,
  "ra": 0.0,
  "nd": 0,
  "nu": 0,
  "pg": 0,
  "cf": 0,
  "s1": 0,
  "s2": 0,
  "gf": 0,
  "su": 0,
  "du": 0,
  "vu": 0.0,
  "vt": 0.0,
  "ch": 0,
  "dr": 0,
  "dw": 0,
  "wt": 0,
  "wd": "",
  "wi": 0,
  "tp": [],
  "tr": [],
  "art": "",
  "trk": "",
  "mp": false,
  "cov": ""
}
```

**Full Message:**

```json
{
  "ct": 53,
  "gt": 27,
  "cl": 7,
  "gl": 3,
  "ru": 15.9,
  "ra": 32.0,
  "nd": 102,
  "nu": 42,
  "pg": 12,
  "cf": 1548,
  "s1": 249,
  "s2": 966,
  "gf": 1299,
  "su": 75,
  "du": 90,
  "vu": 2.7,
  "vt": 12.0,
  "ch": 26,
  "dr": 1635,
  "dw": 3792,
  "wt": 5,
  "wd": "Clear",
  "wi": 0,
  "tp": [
    { "n": "chrome.exe", "c": 12 },
    { "n": "python.exe", "c": 5 },
    { "n": "vscode.exe", "c": 3 }
  ],
  "tr": [
    { "n": "chrome.exe", "r": 2048 },
    { "n": "vscode.exe", "r": 1024 }
  ],
  "art": "Daft Punk",
  "trk": "Get Lucky",
  "mp": true,
  "cov": ""
}
```

---

## 🔄 Version History

### v6.0 [NEURAL LINK]

- ✅ Added `pg` (ping latency)
- ✅ Added `tr` (top RAM processes)
- ✅ Changed all keys to 2-char format
- ✅ Added exact LHM sensor IDs
- ✅ Optimized bandwidth (300-700 bytes/msg)

### v3.0 (Previous)

- Long key names (`cpu_temp`, `gpu_load`, etc.)
- Mixed data sources
- ~1000 bytes/msg

---

## 📝 Notes

### Design Decisions

1. **2-Char Keys**: Reduce bandwidth by ~40%
2. **Integer Temps/Loads**: No decimals needed for display
3. **Float RAM/VRAM**: 1 decimal for precision
4. **Array Limits**: Fixed sizes (tp: 3, tr: 2) for predictable parsing
5. **String Truncation**: Server-side to ensure client buffer safety

### Future Extensions

Potential additions (maintain backward compatibility):

- `cc`: CPU clock speed (MHz)
- `gc`: GPU clock speed (MHz)
- `pw`: CPU package power (W)
- `gp`: GPU power (W)
- `nt`: NVMe temperature (°C)
- `bt`: Battery level (%) for portable builds

---

<div align="center">

**PROTOCOL v6.0 [NEURAL LINK]**

_Optimized for real-time hardware monitoring_

</div>
