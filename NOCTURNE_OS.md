# NOCTURNE_OS — Cyberdeck firmware & server

**Identity Integrity:** the system is built to feel like a living, breathing digital organism.

## Firmware (ESP32-S3 / PlatformIO)

### File structure

```
include/
  nocturne/
    config.h       # Pins, dimensions, timeouts (no secrets)
    Types.h        # HardwareData, WeatherData, MediaData, etc.
    DisplayEngine.h
    RollingGraph.h
    NetLink.h
    DataManager.h
    SceneManager.h
  secrets.h.example   # Copy to include/secrets.h and set WIFI_SSID, WIFI_PASS, PC_IP, TCP_PORT
  secrets.h           # Your credentials (gitignored; create from secrets.h.example)
src/
  main.cpp         # Thin loop: NetLink tick, parse TCP, button, render
  DisplayEngine.cpp
  RollingGraph.cpp
  NetLink.cpp
  DataManager.cpp
  SceneManager.cpp
main.cpp           # Legacy monolithic (not used by PlatformIO; src/main.cpp is)
```

### Modules

- **DisplayEngine** — U8g2, full frame buffer (clear → draw → send). BIOS POST boot, Glitch 2.0 (V-Sync tear/invert on data spikes), RollingGraph sparklines, typography (tom_thumb_4x6 for logs, bold for values).
- **NetLink** — WiFi non-blocking reconnect, TCP stream, SEARCH_MODE (scanning animation when PC disconnects).
- **DataManager** — Parse JSON (2-char keys), fill structs.
- **SceneManager** — Cortex, Neural, Thermal, MemBank, TaskKill, Deck, **Weather** (WMO pixel icons), **Phantom Limb** (WiFi radar / sonar).

### Features

- **Boot:** Fake BIOS POST (MEM CHECK... OK, LINK ESTABLISHED..., WOLF_PROTOCOL INIT).
- **Glitch 2.0:** Horizontal tear or brief invert on CPU/net spikes.
- **RollingGraph:** CPU/Net sparklines at bottom of screen.
- **Weather:** WMO codes → pixel-art icons (sun, cloud, rain, lightning, snow).
- **Phantom Limb:** [PHANTOM] screen with RSSI bar and sweep.
- **Predator mode:** Long press (~2.5 s) → OLED off, LED breathing (saves OLED, shows system alive).

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
