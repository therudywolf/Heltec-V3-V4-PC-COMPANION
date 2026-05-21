# 🐺 Nocturne OS — Heltec WiFi LoRa 32 V4 Firmware

> Embedded wolf companion stack for Heltec boards, telemetry, and BMW I-Bus work.

![Version](https://img.shields.io/badge/version-0.4.0-4c8bf5)
![Status](https://img.shields.io/badge/status-alpha-ef4444)
[![License](https://img.shields.io/badge/license-AGPL--3.0--only-22c55e)](LICENSE)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-0ea5e9)

Multi-profile firmware for **Heltec WiFi LoRa 32 V4 (ESP32-S3)** with BMW E39 I-Bus integration, PC hardware monitoring, and telemetry features.

## Download & Flash

Easiest path — flash straight from your browser (Chrome / Edge on desktop):

### → [Open the web flasher](https://therudywolf.github.io/Heltec-V3-V4-PC-COMPANION/flash/)

Plug the board in over USB-C, pick a profile, click Install. Or grab the
binaries from the [latest release](https://github.com/therudywolf/Heltec-V3-V4-PC-COMPANION/releases/latest).

### Build profiles

| Build | What you get | For |
|-------|--------------|-----|
| **bmw_only** | BMW E39 I-Bus assistant + BLE proximity key. WiFi off. | A BMW E39 — smallest, lowest-RAM build |
| **pc_companion** | PC hardware monitoring + Forza Horizon telemetry + BMW | Desktop monitoring (pair with the PC server in `server/`) |
| **full** | Everything above + WiFi/BLE research tools | Power users who want it all |

Each release ships, per profile:

- `nocturne-<profile>-factory.bin` — full image; what the web flasher writes
- `nocturne-<profile>-<version>.bin` — application image only (OTA / advanced)

### Flash manually

```bash
esptool --chip esp32s3 write_flash 0x0 nocturne-full-factory.bin
```

Or build & flash from source — see [INSTALLATION.md](INSTALLATION.md).

## Components

- **Firmware** — `src/`, `include/`; built with PlatformIO (`platformio.ini`).
- **PC server** — `server/`; Python backend feeding the `pc_companion` profile
  (hardware stats, weather, media) to the board over TCP.
- **Companion app** — `app/android/`; native Android BMW Assistant (BLE).

## Documentation

- [INSTALLATION.md](INSTALLATION.md) — setup & flashing for all profiles
- [CONTRIBUTING.md](CONTRIBUTING.md) — development guidelines
- [docs/USER_GUIDE.md](docs/USER_GUIDE.md) — using the device

## License

GNU AGPL-3.0-only — see [LICENSE](LICENSE).
