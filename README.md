# NOCTURNE OS (v4.2.0)

### High-Performance Cyberdeck Firmware for Heltec WiFi LoRa 32 V4

![License](https://img.shields.io/badge/license-MIT-green) ![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue) ![Status](https://img.shields.io/badge/status-STABLE-brightgreen)

> _"In the silence of the net, the wolf hunts alone."_

**Nocturne OS** is a specialized firmware designed for the **Heltec V4** development board. It transforms the device into a dedicated, high-speed hardware monitor and cyberdeck interface, receiving telemetry from a host PC via TCP.

Built for **visual aesthetics**, **low latency**, and **hardware resilience**.

---

## 🐺 Key Features

### 1. The "Unified Grid" Design System

A strict, pixel-perfect 2x2 grid layout used across CPU, GPU, and Motherboard scenes.

- **Tech Brackets:** Custom drawing primitives that frame data like a HUD.
- **Chamfered Boxes:** Industrial aesthetics for menus and headers.
- **Typography:** Hand-picked `ProFont10` (Data) and `HelvB10` (Headers) for maximum readability on 0.96" OLEDs.

### 2. "Iron Grip" Connectivity

The ESP32-S3 radio is notorious for aggressive power-saving drops. Nocturne OS bypasses this:

- **Direct Driver Access:** Uses `esp_wifi_set_ps(WIFI_PS_NONE)` to lock the radio in high-performance mode.
- **Latency:** Sub-50ms updates for real-time graphs.
- **Self-Healing:** Auto-reconnect logic with configurable grace periods.

### 3. "Stealth Hunter" Alert System

A non-intrusive, tactical alert logic for critical temperatures/loads.

- **Trigger:** Server sends `CRITICAL` state.
- **Double Tap:** The White LED (GPIO 25) blinks **exactly 2 times** (Double Tap) to catch your eye.
- **Silence:** After 2 blinks, the LED goes dark, but the specific metric on the screen freezes/highlights. No infinite annoying flashing.

### 4. Tactical Menu (Overlay)

Long-press interaction model for quick adjustments without rebooting.

- **Carousel:** Auto-cycle screens (5s / 10s / 15s / OFF).
- **Flip:** Rotate screen 180° (for cable management).
- **Persistence:** Settings saved to NVS (Non-Volatile Storage).

---

## 🛠 Hardware Specs

| Component   | Specification                 | Notes                        |
| ----------- | ----------------------------- | ---------------------------- |
| **Board**   | Heltec WiFi LoRa 32 V4        | ESP32-S3R2 MCU               |
| **Display** | 0.96" OLED (SSD1306)          | I2C (SDA 17, SCL 18)         |
| **Clock**   | 240 MHz (CPU) / 800 kHz (I2C) | Overclocked I2C for 60FPS UI |
| **LED**     | GPIO 25 (White)               | Programmable Alert LED       |
| **Button**  | GPIO 0 (PRG)                  | Input (Pull-up)              |

---

## 🖥️ Scenes

1.  **MAIN:** Dashboard summary (CPU/GPU Temp bars + RAM usage).
2.  **CPU:** Detailed Core Temp, Clock, Load, Power.
3.  **GPU:** Core Temp, Clock, Load, VRAM Usage.
4.  **RAM:** Top 2 memory-hogging processes + Total usage.
5.  **DISKS:** 2x2 Grid showing Drive Letter + Temperature.
6.  **MEDIA:** Current Track/Artist (Scrollable).
7.  **FANS:** RPM & % for CPU, Pump, GPU, Case fans.
8.  **MB:** Motherboard sensor array (VRM, Chipset, etc.).
9.  **WEATHER:** XBM Pixel-art icons + Large Temp display.

---

## 🎮 Controls

**Button (GPIO 0):**

| Action                | State     | Result                                 |
| :-------------------- | :-------- | :------------------------------------- |
| **Short Press** (<1s) | Normal    | **Next Scene** (Resets Carousel Timer) |
|                       | Menu Open | **Next Menu Item** (Navigate)          |
| **Long Press** (>1s)  | Normal    | **Open Menu**                          |
|                       | Menu Open | **Interact / Change Value**            |

**Menu Items:**

1.  **AUTO:** Cycle Carousel (5s -> 10s -> 15s -> OFF).
2.  **FLIP:** Rotate Screen 180°.
3.  **EXIT:** Close Menu.

---

## 🚀 Installation

### Prerequisites
1.  **VS Code** with **PlatformIO** (for firmware).
2.  **Python 3.x** (for the Host Monitor script).
3.  **Libre Hardware Monitor** (CRITICAL):
    * This software acts as the telemetry source for Windows.
    * [Download Latest Release](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases)
    * **Setup:**
        1.  Unzip and run `LibreHardwareMonitor.exe` **as Administrator**.
        2.  Go to `Options` -> Enable `Run On Windows Startup`.
        3.  Go to `Options` -> `Remote Web Server` -> **Enable**.
        4.  Ensure the port is **8085** (Default).

### Flashing (Firmware)

1.  Clone this repository.
2.  Open in VS Code.
3.  Copy `include/secrets.h.example` to `include/secrets.h` and set your WiFi and PC IP (do not commit `secrets.h`):
    ```cpp
    #define WIFI_SSID "YourSSID"
    #define WIFI_PASS "YourPass"
    #define PC_IP "192.168.1.100"   // Your PC's IP
    #define TCP_PORT 8888
    ```
4.  Connect Heltec V4 via USB-C.
5.  Run **PlatformIO: Upload**.

### Running (Host Monitor)

1.  From the project root: `pip install -r requirements.txt`
2.  Edit `config.json` if needed (LHM URL, port, weather city, limits).
3.  Run: `python src/monitor.py` (or build a standalone exe with `NocturneServer.spec` + PyInstaller).

---

## 📁 Project Structure

```
├── include/
│   ├── nocturne/         # config.h, Types.h
│   └── secrets.h.example # template → copy to secrets.h (local only)
├── src/
│   ├── main.cpp          # firmware entry
│   ├── monitor.py        # PC-side TCP server (LHM, weather, media)
│   └── modules/          # DisplayEngine, SceneManager, NetManager, …
├── config.json           # monitor: host, port, lhm_url, limits, weather_city
├── platformio.ini        # ESP32 build (Heltec V3 profile for V4 board)
├── requirements.txt      # Python deps for monitor
└── NocturneServer.spec   # PyInstaller spec for one-click exe
```

**Not in the repo (see `.gitignore`):** `secrets.h`, `.env`, `.pio/`, build artifacts, logs.

---

## ⚠️ Credits

- **Concept & Code:** RudyWolf
- **UI Design:** "Nocturne" Cyberpunk System
- **Libraries:** U8g2 (Olikraus), ArduinoJson (Bblanchon)

**License:** [MIT](LICENSE)

---

_End of Transmission._
