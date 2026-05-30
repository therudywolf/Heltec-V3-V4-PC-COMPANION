# 🐺 Nocturne OS — Heltec WiFi LoRa 32 V4

> Embedded wolf companion stack for Heltec boards: BMW I-Bus, PC telemetry, WiFi/BLE research.

[![License](https://img.shields.io/badge/license-AGPL--3.0--only-22c55e)](LICENSE)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-0ea5e9)
![Status](https://img.shields.io/badge/status-alpha-ef4444)

Nocturne OS is being split into **three standalone products**, all built from
this monorepo on a single shared core ([`lib/nocturne-core`](lib/nocturne-core))
— one source tree, no duplication.

## The three products

| Product | Dir | What it is |
|---------|-----|-----------|
| 🚗 **BMW** | [`apps/bmw`](apps/bmw) | BMW E39 I-Bus assistant + BLE proximity key (+ Android app) |
| 🖥️ **PC** | [`apps/pc`](apps/pc) | PC hardware monitoring + Forza telemetry (+ Python data collector) |
| 📡 **Hacker** | [`apps/hacker`](apps/hacker) | WiFi/BLE research toolkit — sniff, scan, honeypot, BLE |

Each product has its own README, version, and (soon) its own release track.

## Build

```bash
pio run -d apps/bmw       # BMW product
pio run -d apps/pc        # PC product
pio run -d apps/hacker    # Hacker product
# add -t upload to flash over USB-C
```

PC and Hacker need WiFi creds in `include/secrets.h` (copy `include/secrets.h.example`).

## Repository layout

```
lib/nocturne-core/    shared core: display engine, menu/input, battery, boot, config
src/                  shared firmware sources (compiled per-product via build_src_filter)
apps/{bmw,pc,hacker}/ one PlatformIO project per product (platformio.ini + VERSION + README)
server/               PC-side data collector for the PC product (Python)
BMW datasheet/        I-Bus protocol reference (wilhelm-docs, E46 codes, AVR-IBus)
docs/                 guides + docs/RESTRUCTURE_PLAN.md (the 3-product roadmap)
```

> **Restructure in progress.** The unified `v0.4.0` build (root `platformio.ini`,
> profiles `bmw_only`/`pc_companion`/`full`) is the legacy single-binary release;
> it still builds and will be retired once the three products ship. Roadmap:
> [docs/RESTRUCTURE_PLAN.md](docs/RESTRUCTURE_PLAN.md).

## Documentation

- [docs/RESTRUCTURE_PLAN.md](docs/RESTRUCTURE_PLAN.md) — the 3-product roadmap
- [INSTALLATION.md](INSTALLATION.md) — toolchain setup & flashing
- [CONTRIBUTING.md](CONTRIBUTING.md) — development guidelines
- [docs/USER_GUIDE.md](docs/USER_GUIDE.md) — using the device

## License

GNU AGPL-3.0-only — see [LICENSE](LICENSE).
