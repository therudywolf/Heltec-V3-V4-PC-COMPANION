# Nocturne OS — Heltec WiFi LoRa 32 V4 Firmware

![License](https://img.shields.io/badge/license-AGPL--3.0-blue)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-green)
![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange)

Multi-profile firmware for **Heltec WiFi LoRa 32 V4 (ESP32-S3)** with BMW E39 I-Bus integration, PC hardware monitoring, Forza Horizon telemetry, and WiFi/BLE research tools.

---

## Features

| Profile | Build Flag | Description |
|---------|-----------|-------------|
| `bmw_only` (default) | `pio run -e bmw_only` | BMW E39 I-Bus assistant, BLE proximity key, demo mode. WiFi off. |
| `pc_companion` | `pio run -e pc_companion` | PC monitoring (WiFi+TCP), Forza telemetry (UDP) + BMW. |
| `full` | `pio run -e full` | All above + WiFi scanner/sniffer/trap + BLE spam/clone. |

### BMW I-Bus Assistant
- BLE proximity key (auto lock/unlock)
- Lights control (goodbye, follow-me, park, hazard, low beam)
- Locks (lock/unlock/trunk)
- Cluster text display
- PDC activation
- Demo mode for testing without vehicle

### PC Companion
- 9 OLED display scenes (CPU, GPU, RAM, Disks, Media, Fans, Motherboard, Weather, Main)
- Rolling graphs on 128x64 OLED
- TCP connection to PC monitoring server

### Forza Telemetry
- Real-time RPM/speed dashboard
- Shift light indicator
- UDP data stream from Forza Horizon

---

## Repository Structure

```
├── platformio.ini          # PlatformIO project config (3 environments)
├── huge_app.csv            # Custom partition table
├── include/
│   ├── nocturne/           # config.h, Types.h
│   └── secrets.h.example   # Template for local secrets
├── src/                    # Firmware source
│   ├── main.cpp
│   ├── AppModeManager.*
│   ├── MenuHandler.*
│   └── modules/
│       ├── ble/            # BLE manager
│       ├── car/            # BMW, I-Bus, BLE key, Forza, OBD
│       ├── display/        # OLED scenes, boot animation
│       ├── network/        # WiFi, sniffer, trap
│       └── system/         # Battery manager
├── app/
│   ├── android/            # Native Android companion (Kotlin)
│   └── flutter/            # Flutter MD3 companion (Dart)
├── docs/                   # User guide, wiring, board docs
├── BMW datasheet/          # I-Bus reference materials
├── shared/DataSheets/      # Board datasheets
├── data/                   # LittleFS filesystem data
├── LICENSE                 # GNU AGPL-3.0
└── .gitignore
```

---

## Getting Started
## Quick Start (3 Steps)

1. **Install**: PlatformIO (VS Code or CLI) + board drivers (CH340)
2. **Clone**: `git clone https://github.com/therudywolf/Heltec-V3-V4-PC-COMPANION.git`
3. **Build**: Choose profile:
   - `pio run -e bmw_only -t upload` — BMW I-Bus assistant
   - `pio run -e pc_companion -t upload` — PC monitoring + BMW + Forza
   - `pio run -e full -t upload` — All features + WiFi research

📖 See [INSTALLATION.md](INSTALLATION.md) for complete setup guide.


### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- Heltec WiFi LoRa 32 V4 board
- USB-C cable

### Build & Upload

1. Clone the repository:
   ```bash
   git clone https://github.com/therudywolf/Heltec-V3-V4-PC-COMPANION.git
   cd Heltec-V3-V4-PC-COMPANION
   ```

2. Copy secrets template:
   ```bash
   cp include/secrets.h.example include/secrets.h
   ```
   Edit `include/secrets.h` with your WiFi credentials (only needed for `pc_companion` / `full` profiles).

3. Build and upload:
   ```bash
   pio run -e bmw_only -t upload
   ```

4. Serial monitor (115200 baud):
   ```bash
   pio device monitor
   ```

### Configuration

All hardware pins, I-Bus settings, OBD, and demo mode options are in `include/nocturne/config.h`.

---

## Demo Mode

For testing without a vehicle:
- **Menu** (double tap) → BMW → "BMW Demo" (long press)
- **Or** hold PRG button during boot (~2.5s) — saved to NVS

See [User Guide](docs/USER_GUIDE.md) and [BMW E39 Assistant](docs/bmw/BMW_E39_Assistant.md) for details.

---

## Companion Apps

| App | Path | Description |
|-----|------|-------------|
| Android (Kotlin) | `app/android/` | Native BLE companion for BMW assistant |
| Flutter (Dart) | `app/flutter/` | Material Design 3 companion app |

See [BMW Android App docs](docs/bmw/BMW_ANDROID_APP.md) for build instructions.

---

## Documentation

| Document | Description |
|----------|-------------|
| [User Guide](docs/USER_GUIDE.md) | Button gestures, modes, settings |
| [Installation Guide](INSTALLATION.md) | Step-by-step setup for all three profiles |
| [Contributing](CONTRIBUTING.md) | Development guidelines and PR process |
| [Board & Config](docs/board/HELTEC_V4_BOARD_AND_CONFIG.md) | Pinout, config.h, build |
| [BMW E39 Assistant](docs/bmw/BMW_E39_Assistant.md) | BLE key, I-Bus, lights, locks, PDC |
| [Wiring](docs/bmw/HELTEC_V4_WIRING.md) | Board → I-Bus transceiver → vehicle |
| [Connecting & Terminal](docs/board/CONNECTING_AND_TERMINAL.md) | USB, serial monitor |

---

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for:

- Development setup and workflow
- Code style guidelines  
- Pull request process
- License compliance (AGPL v3.0)

**Quick checklist:**
- ✅ Test all three profiles compile cleanly
- ✅ Use conventional commits (feat:, fix:, docs:)
- ✅ No secrets or personal data committed
- ✅ Update docs for user-visible changes
- ✅ AGPL v3.0 license compliance

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

---

## License

This project is licensed under the **GNU Affero General Public License v3.0** — see the [LICENSE](LICENSE) file for details.

```
Copyright (C) 2024-2026 RudyWolf

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```

---

## Acknowledgments

- [U8g2](https://github.com/olikraus/U8g2) — OLED graphics library (Olikraus)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) — BLE stack (h2zero)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) — JSON library (Bblanchon)
- BMW I-Bus community documentation
