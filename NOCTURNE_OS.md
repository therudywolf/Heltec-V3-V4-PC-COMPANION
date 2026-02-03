# NOCTURNE_OS — Cyberdeck firmware & server

**Identity Integrity:** the system is built to feel like a living, breathing digital organism.

## Firmware (ESP32-S3 / PlatformIO)

### File structure

```
include/
  nocturne/
    config.h       # Pins, dimensions, timeouts (no secrets)
    Types.h        # HardwareData, WeatherData, MediaData, AppState, etc.
  secrets.h.example   # Copy to include/secrets.h and set WIFI_SSID, WIFI_PASS, PC_IP, TCP_PORT
  secrets.h           # Your credentials (gitignored; create from secrets.h.example)
src/
  main.cpp         # Thin loop: NetManager tick, parse TCP, button, render
  monitor.py       # Python backend (aiohttp, LHM, cover_b64 on track change)
  modules/
    NetManager.h/cpp    # WiFi (setSleep(false) after connect), TCP, JSON parse
    DisplayEngine.h/cpp # U8g2, BIOS POST, glitch, RollingGraph (profont10, helvB10)
    RollingGraph.h/cpp  # Sparkline buffer
    SceneManager.h/cpp  # HUB, CPU, GPU, NET, ATMOS, MEDIA + Search/Menu/NoSignal
```

### Modules

- **NetManager** — WiFi (with `WiFi.setSleep(false)` after connect for &lt;10 ms ping), non-blocking reconnect, TCP stream, newline-delimited JSON parsing into `AppState`. SEARCH_MODE when TCP is down.
- **DisplayEngine** — U8g2, full frame buffer. BIOS POST boot, Glitch (horizontal shift every 60 s), RollingGraph sparklines. Fonts: `u8g2_font_profont10_mr` (small), `u8g2_font_helvB10_tr` (headers/numbers).
- **SceneManager** — Six scenes: HUB (CPU/GPU temps, clock, RAM bar), CPU DETAIL (load graph, power, fan), GPU DETAIL (load graph, hotspot, VRAM), NET (traffic graph, DL/UP, ping), ATMOS (weather + WMO pixel icons), MEDIA (dithered art + track). Plus drawSearchMode, drawMenu, drawNoSignal, drawConnecting.

### Features

- **Boot:** Fake BIOS POST (MEM CHECK... OK, LINK ESTABLISHED..., WOLF_PROTOCOL INIT).
- **Glitch 2.0:** Horizontal tear or brief invert on CPU/net spikes.
- **RollingGraph:** CPU/Net sparklines at bottom of screen.
- **Weather:** WMO codes → pixel-art icons (sun, cloud, rain, lightning, snow).
- **Phantom Limb:** [PHANTOM] screen with RSSI bar and sweep.
- **Predator mode (Sleep Mode):** Long press (~2.5 s) → OLED off, LED breathing (saves OLED, shows system alive). See [Sleep Mode logic](#sleep-mode-predator-mode) below.

### Sleep Mode (Predator Mode) — Implementation Logic

**Trigger:** Button held for ≥ `NOCT_BUTTON_PREDATOR_MS` (2500 ms). On release, `predatorMode` is toggled.

**While active:**

1. **Display:** Each frame we clear the buffer and send it without drawing any content — effectively **screen OFF**. This reduces OLED wear and power when the user is not looking.
2. **LED:** Instead of normal alarm blink or off, the LED runs a **breathing** pattern: `analogWrite(NOCT_LED_PIN, 128 + 127*sin(t))` with `t = (now - predatorEnterTime) / 20`, so a slow pulse. Only applied if `settings.ledEnabled` is true.
3. **Logic:** All other loop logic still runs (WiFi tick, TCP read, button handling, carousel/alert), but the **render branch** is short-circuited: we skip splash, no-signal, scenes, glitch, and dots; we only `clearBuffer()` → `sendBuffer()` then `delay(10)` and return.

**Exit:** Same button long-press again toggles `predatorMode` off; next frame the normal UI is drawn again.

So “Sleep” here means **display sleep** (OLED off + LED breath), not MCU or WiFi sleep. WiFi remains active so that when the user exits Predator mode, data is already flowing.

### Build

1. Copy `include/secrets.h.example` to `include/secrets.h` and set WiFi and PC_IP/TCP_PORT.
2. `pio run` (or PlatformIO IDE Build).

---

## Server (Python)

### Stack

- **aiohttp** for LHM and Open-Meteo (no blocking HTTP).
- **ThreadPoolExecutor** only for ping, psutil, top processes.
- **Smart send:** Full payload only when values change beyond hysteresis (temp ≥2°C, load ≥5%, net ≥100 KB/s, or RAM change), **or** every **2 s** (heartbeat) to keep the link alive.
- **Media:** When session exists but playback is paused → `idle: true` so the deck shows **IDLE** and a sleep icon.

### Run

```bash
pip install -r requirements.txt
python monitor.py
```

### Config (env)

- `LHM_URL`, `TCP_HOST`, `TCP_PORT`, `WEATHER_LAT`, `WEATHER_LON`, `DEBUG=1` (optional).

---

## Protocol

- TCP JSON lines, 2-char keys (unchanged).
- New keys: **idle** (media paused / no session playing).
- Firmware sends `HELO\n` and `screen:N\n`; server sends JSON lines and responds to HELO/screen as before.
