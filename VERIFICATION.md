# v6.0 [NEURAL LINK] — Verification & Testing Guide

## ✅ Pre-Deployment Checklist

### 1. Server Requirements

**Python Environment:**

```bash
python --version  # Must be 3.10+
pip install -r requirements.txt
```

**Required Services:**

- [ ] LibreHardwareMonitor running
- [ ] LHM web server enabled (http://localhost:8085)
- [ ] Port 8888 not blocked by firewall

**Configuration:**

- [ ] `.env` file created from `.env.example`
- [ ] `PC_IP` set to your PC's local IP
- [ ] `TCP_PORT` matches firmware (default 8888)
- [ ] `WEATHER_LAT` and `WEATHER_LON` set (optional)

### 2. Firmware Requirements

**Hardware:**

- [ ] Heltec WiFi LoRa 32 V3 (ESP32-S3)
- [ ] USB cable connected
- [ ] OLED display working

**Configuration (main.cpp lines 20-24):**

- [ ] `WIFI_SSID` set to your WiFi name
- [ ] `WIFI_PASS` set to your WiFi password
- [ ] `PC_IP` matches your PC's local IP
- [ ] `TCP_PORT` matches server (default 8888)

**Build Environment:**

- [ ] PlatformIO installed OR Arduino IDE with ESP32 support
- [ ] Libraries installed: U8g2, ArduinoJson, WiFi, Preferences

---

## 🧪 Testing Procedures

### Test 1: Server Startup

**Command:**

```bash
python monitor.py
```

**Expected Output:**

```
[INFO] ============================================================
[INFO] Heltec PC Monitor Server v6.0 [NEURAL LINK]
[INFO] Cyberpunk Cyberdeck Edition
[INFO] ============================================================
[INFO] Starting TCP server on 0.0.0.0:8888
[INFO] Server ready. Waiting for clients...
```

**Verify:**

- [ ] No errors in console
- [ ] System tray icon appears (Windows)
- [ ] Port 8888 listening: `netstat -an | findstr 8888`

### Test 2: LHM Connection

**Command:**

```bash
# In browser or curl:
curl http://localhost:8085/data.json
```

**Expected:**

- [ ] JSON response with sensor data
- [ ] Contains sensor IDs like `/amdcpu/0/temperature/2`
- [ ] No 404 or connection errors

**Troubleshooting:**

- If 404: Enable LHM web server in settings
- If connection refused: LHM not running
- If wrong port: Check LHM settings, update `.env`

### Test 3: Firmware Upload

**PlatformIO:**

```bash
pio run -t upload
pio device monitor
```

**Arduino IDE:**

1. Open `main.cpp`
2. Select board: "Heltec WiFi LoRa 32 V3"
3. Select port
4. Upload
5. Open Serial Monitor (115200 baud)

**Expected Serial Output:**

```
Connecting to WiFi...
WiFi connected: 192.168.1.100
Connecting to TCP server...
TCP connected
```

**Verify:**

- [ ] WiFi connects (check SSID/password)
- [ ] TCP connects (check PC_IP/port)
- [ ] Splash screen shows on OLED
- [ ] Data appears after 2-3 seconds

### Test 4: Data Flow

**On Server Console:**

```
[INFO] Client connected: ('192.168.1.100', 54321)
```

**On OLED:**

- [ ] [CORTEX] screen shows CPU/GPU temps
- [ ] Values update every ~1 second
- [ ] No "NO SIGNAL" screen
- [ ] WiFi icon in top-right corner

### Test 5: Screen Cycling

**Action:** Short press button on Heltec

**Expected:**

1. [CORTEX] → [NEURAL] → [THERMAL] → [MEM.BANK] → [TASK.KILL] → [DECK] → [CORTEX]
2. Screen indicator dots at bottom update
3. No lag or freeze
4. Data persists across screens

**Verify Each Screen:**

- [ ] [CORTEX]: CPU/GPU temps, loads, RAM, VRAM
- [ ] [NEURAL]: Network up/down, ping latency, IP address
- [ ] [THERMAL]: Fan RPMs, animated fan icon, chipset temp
- [ ] [MEM.BANK]: RAM bar, storage bars (NVMe, HDD), disk I/O
- [ ] [TASK.KILL]: Top process name, CPU %, other processes
- [ ] [DECK]: Artist, track, playing status OR "STANDBY"

### Test 6: Anti-Ghosting

**Action:** Watch [NEURAL] screen for 10 seconds

**Expected:**

- [ ] Network speed numbers update cleanly
- [ ] No overlapping/ghosting text
- [ ] Previous digits fully erased before new ones drawn
- [ ] Text remains readable

**If Ghosting Occurs:**

- Check `drawMetric()` function is used for all dynamic text
- Verify `u8g2.setDrawColor(0)` before clearing
- Verify `u8g2.drawBox()` covers exact text area

### Test 7: WiFi Reconnection

**Action:**

1. Disconnect WiFi on router
2. Wait 30 seconds
3. Reconnect WiFi

**Expected:**

- [ ] "NO SIGNAL" screen appears after 3 sec
- [ ] WiFi icon shows disconnected
- [ ] After reconnect: WiFi icon shows connected
- [ ] Data resumes within 30 sec
- [ ] No manual reset required

### Test 8: Temperature Alerts

**Action:** Stress test CPU/GPU to trigger alerts

**Expected:**

- [ ] When CPU > 80°C OR GPU > 85°C:
  - [ ] Blinking frame around screen
  - [ ] LED blinks (if enabled in menu)
- [ ] When temps drop: alerts stop

**Adjust Thresholds (main.cpp lines 26-29):**

```cpp
#define CPU_TEMP_ALERT 80  // Lower for testing
#define GPU_TEMP_ALERT 85  // Lower for testing
```

### Test 9: Settings Menu

**Action:** Long press button (>800ms)

**Expected:**

- [ ] Menu overlay appears
- [ ] 5 items: LED, Carousel, Contrast, Display, Exit
- [ ] Short press cycles items
- [ ] Long press toggles/changes value
- [ ] Settings persist after reboot (NVS)

**Test Each Setting:**

- [ ] LED: Toggle on/off (check LED during alert)
- [ ] Carousel: Toggle on/off, test auto-cycling
- [ ] Contrast: Cycle 128 → 192 → 255 (visible change)
- [ ] Display: Toggle inverted (black/white swap)
- [ ] Exit: Returns to normal screen

### Test 10: Media Player

**Action:** Play music in Spotify/Windows Media Player

**Expected:**

- [ ] [DECK] screen shows artist name
- [ ] [DECK] screen shows track title
- [ ] "STATUS" shows "> PLAYING" (blinking)
- [ ] When paused: "STANDBY" with cassette icon

**If Not Working:**

- Check `winsdk` installed: `pip show winsdk`
- Try different media player (Spotify, Groove Music)
- VLC/MPC-HC may not support Windows Media API

### Test 11: Ping Latency

**Action:** Watch [NEURAL] screen

**Expected:**

- [ ] "PING" shows latency in ms (typically 5-50ms)
- [ ] Updates every 5 seconds
- [ ] If 0ms: check firewall, ICMP not blocked

**Test Network Conditions:**

- Normal: 5-20ms
- WiFi congestion: 20-50ms
- VPN: 50-200ms
- Offline: 0ms (timeout)

### Test 12: Top Process

**Action:**

1. Open Chrome/Firefox (high CPU)
2. Go to [TASK.KILL] screen

**Expected:**

- [ ] Shows browser process name
- [ ] Shows CPU usage % (updates every 2.5 sec)
- [ ] Shows 2nd and 3rd processes below
- [ ] Process name truncated to 18 chars if long

### Test 13: Storage Monitoring

**Action:**

1. Go to [MEM.BANK] screen
2. Copy large file to trigger disk I/O

**Expected:**

- [ ] NVMe (C:) bar shows usage %
- [ ] HDD (D:) bar shows usage %
- [ ] Bottom shows "R:XXXK W:XXXK" (disk I/O)
- [ ] Values update during file copy

### Test 14: Long-Term Stability

**Action:** Leave running for 24 hours

**Expected:**

- [ ] No crashes or freezes
- [ ] No memory leaks (server RAM stable)
- [ ] WiFi stays connected
- [ ] TCP connection stable
- [ ] Data continues updating
- [ ] No OLED burn-in (anti-ghosting working)

**Monitor Server:**

```bash
# Check memory usage
tasklist | findstr python
# Should stay around 50-100 MB
```

---

## 🐛 Common Issues & Solutions

### Issue: "NO SIGNAL" Screen Persists

**Causes:**

1. Server not running
2. Wrong `PC_IP` in firmware
3. Firewall blocking port 8888
4. WiFi disconnected

**Solutions:**

```bash
# Check server running:
netstat -an | findstr 8888

# Check firewall:
netsh advfirewall firewall add rule name="Heltec Monitor" dir=in action=allow protocol=TCP localport=8888

# Verify PC IP:
ipconfig
# Update main.cpp if IP changed
```

### Issue: Ghosting/Overlapping Text

**Cause:** Not using `drawMetric()` for dynamic text

**Solution:**

```cpp
// WRONG:
u8g2.drawStr(x, y, value);

// CORRECT:
drawMetric(x, y, "LABEL", value);
```

### Issue: WiFi Won't Connect

**Causes:**

1. Wrong SSID/password
2. 5GHz network (ESP32 needs 2.4GHz)
3. Hidden SSID
4. MAC filtering

**Solutions:**

- Double-check SSID/password (case-sensitive)
- Use 2.4GHz network
- Unhide SSID or add MAC to whitelist
- Check serial monitor for error messages

### Issue: Media Not Showing

**Causes:**

1. `winsdk` not installed
2. Media player not compatible
3. No media playing

**Solutions:**

```bash
pip install winsdk
# Restart server
# Try Spotify, Groove Music, or Windows Media Player
```

### Issue: Ping Shows 0ms

**Causes:**

1. Firewall blocking ICMP
2. `ping` command not in PATH
3. VPN blocking ping

**Solutions:**

```bash
# Test ping manually:
ping 8.8.8.8

# Check PATH:
where ping
# Should show C:\Windows\System32\ping.exe

# Temporarily disable firewall to test
```

### Issue: High CPU Usage on Server

**Cause:** Polling too frequently

**Solution:** Adjust intervals in `monitor.py`:

```python
LHM_INTERVAL = 1.5  # Increase from 0.9
PING_INTERVAL = 10.0  # Increase from 5.0
```

### Issue: Firmware Won't Compile

**Causes:**

1. Missing libraries
2. Wrong board selected
3. Syntax errors

**Solutions:**

```bash
# PlatformIO: Install dependencies
pio lib install

# Arduino IDE: Install libraries via Library Manager:
# - U8g2
# - ArduinoJson
# - WiFi (built-in)
# - Preferences (built-in)

# Select correct board:
# - Heltec WiFi LoRa 32 V3
# - Or: ESP32 Dev Module
```

---

## 📊 Performance Benchmarks

### Server (monitor.py)

| Metric            | Target   | Typical  |
| ----------------- | -------- | -------- |
| CPU Usage         | <5%      | 2-3%     |
| RAM Usage         | <100 MB  | 50-80 MB |
| Network Bandwidth | <10 KB/s | 2-5 KB/s |
| Response Time     | <100ms   | 20-50ms  |
| Uptime            | 24h+     | Stable   |

### Firmware (main.cpp)

| Metric       | Target  | Typical  |
| ------------ | ------- | -------- |
| Frame Rate   | 30+ FPS | 60 FPS   |
| RAM Usage    | <50 KB  | 30-40 KB |
| WiFi Latency | <50ms   | 10-30ms  |
| TCP Latency  | <100ms  | 20-50ms  |
| Boot Time    | <5 sec  | 2-3 sec  |

---

## ✅ Final Verification

Before deploying to production:

- [ ] All tests passed
- [ ] No linter errors (Python)
- [ ] Firmware compiles without errors
- [ ] 24-hour stability test completed
- [ ] Documentation reviewed
- [ ] `.env` file configured
- [ ] Autostart configured (optional)
- [ ] Backup of working config created

---

## 📝 Deployment Notes

### Production Deployment

1. **Build EXE** (optional):

   ```bash
   build_oneclick.bat
   ```

2. **Configure Autostart**:

   - Copy `monitor.exe` to startup folder
   - Or use tray menu: "Add to Autostart"

3. **Monitor Logs**:

   - Check console for errors
   - Monitor system tray icon

4. **Backup Config**:
   ```bash
   copy .env .env.backup
   copy main.cpp main.cpp.backup
   ```

### Rollback Procedure

If issues occur:

1. Stop server (tray icon → Exit)
2. Restore backup config
3. Restart server
4. Re-flash firmware if needed

---

## 🎉 Success Criteria

**v6.0 [NEURAL LINK] is verified when:**

✅ Server starts without errors  
✅ LHM data flows correctly  
✅ Firmware connects to WiFi  
✅ TCP connection established  
✅ All 6 screens display data  
✅ Anti-ghosting works (no overlapping text)  
✅ WiFi reconnects automatically  
✅ Temperature alerts trigger correctly  
✅ Settings menu works and persists  
✅ Media player shows current track  
✅ Ping latency displays correctly  
✅ Top process tracking works  
✅ 24-hour stability test passed

**NEURAL LINK ESTABLISHED** 🚀
