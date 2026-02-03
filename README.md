# NOCTURNE_OS

Hardware monitor: **ESP32 (Heltec)** + **SSD1306 128×64** OLED, backend — **Windows Python** server (aiohttp, system tray). Data: LHM (Liberty Hardware Monitor) → TCP → device.

---

## Structure

```
Heltec v4/
  config.json          — host, port, lhm_url, limits, weather_city
  requirements.txt     — Python deps (aiohttp, pystray, psutil, …)
  platformio.ini       — ESP32 build config
  RUN.bat              — Main launcher (deps + config, then tray server)
  EMERGENCY_START.bat  — Run server in console (debug)
  build.bat            — Full clean build → firmware + exe (see Build)
  setup_nocturne.py    — One-time: pip install, create config.json
  include/nocturne/
    config.h           — Pins, display size, scene IDs, timeouts
    Types.h            — HardwareData, AppState, etc.
    secrets.h.example  — Copy to secrets.h (WiFi, PC IP)
  src/
    main.cpp           — Firmware entry, loop, button, carousel
    monitor.py         — PC server: LHM poll, TCP, tray (autostart, close)
    modules/
      DisplayEngine.*  — OLED draw, layout blocks, progress bar, graph
      NetManager.*      — WiFi, TCP client, JSON parse
      SceneManager.*   — Per-scene draw (HUB, CPU, GPU, RAM, NET, ATMOS, MEDIA)
      RollingGraph.*    — Ring buffer + draw for graphs
```

Logs: `nocturne.log` in project root.

---

## Run (PC server)

- **RUN.bat** — Install deps and create `config.json` if missing, then start server in **system tray**. Autostart and Exit: use **tray menu** (right-click icon).
- **EMERGENCY_START.bat** — Run server in **console** (no tray) for debugging.

---

## Tray menu (when server is running)

- **Status: RUNNING** — info only
- **Add to startup** / **Remove from startup** — toggle Windows startup (HKCU Run, name `NOCTURNE_OS`)
- **Restart Server** — restart TCP server
- **Close** — exit server and tray

---

## Build

1. **Firmware (ESP32)**  
   Copy `include/secrets.h.example` → `include/secrets.h`, set WiFi and PC IP. Then:

   ```text
   pio run
   pio run --target upload   # optional, with board connected
   ```

2. **Full build (firmware + exe)**  
   Run **build.bat**: cleans `.pio/build` and PyInstaller `dist/`, builds firmware, then packs `src/monitor.py` into **dist/NocturneServer.exe**. No incremental build — clean each time.

---

## Config

Edit `config.json`: `host`, `port` (TCP server for device), `lhm_url` (Liberty Hardware Monitor JSON), `limits` (cpu/gpu alert), `weather_city`.
