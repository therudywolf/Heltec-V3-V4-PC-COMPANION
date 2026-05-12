# Installation & Setup Guide

Complete guide for setting up Nocturne OS on Heltec WiFi LoRa 32 V4.

## Hardware Requirements

- Heltec WiFi LoRa 32 V4 board (ESP32-S3)
- USB-C cable for power and flashing
- Computer with Windows, macOS, or Linux
- Optional: BMW E39 vehicle or OBD-II simulator

## Software Requirements

### Step 1: Install PlatformIO

#### Option A: VS Code Extension (Recommended)
1. Install Visual Studio Code
2. Go to Extensions (Ctrl+Shift+X)
3. Search "PlatformIO IDE"
4. Click Install

#### Option B: Command Line
```bash
pip install platformio
```

### Step 2: Install Board Drivers

**Windows**: Download CH340 driver from WCH website
**macOS**: `brew install ch340g`
**Linux**: Usually included in kernel

### Step 3: Clone Repository

```bash
git clone https://github.com/therudywolf/Heltec-V3-V4-PC-COMPANION.git
cd Heltec-V3-V4-PC-COMPANION
cp include/secrets.h.example include/secrets.h
# Edit include/secrets.h with your WiFi credentials
```

### Step 4: Build and Flash

Choose your profile:

```bash
# BMW I-Bus only
pio run -e bmw_only -t upload

# PC Companion (monitoring + BMW + Forza)
pio run -e pc_companion -t upload

# Full (all features + WiFi research)
pio run -e full -t upload
```

### Step 5: Verify

Open serial monitor at 115200 baud:
```bash
pio device monitor
```

You should see boot messages from Nocturne OS.

## Troubleshooting

- **Board not recognized**: Check USB cable is data cable; update drivers
- **Compilation errors**: Run `pio run -e bmw_only --target clean` then rebuild
- **WiFi issues**: Verify WIFI_SSID and WIFI_PASS in secrets.h; check 2.4 GHz support

See INSTALLATION.md for complete guide. License: GNU AGPL v3.0
