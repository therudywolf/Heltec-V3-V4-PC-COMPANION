# 🐺 Nocturne BMW

> Standalone BMW E39 companion firmware for Heltec WiFi LoRa 32 V4 (ESP32-S3).

One of the three Nocturne OS products. Built from this monorepo on top of the
shared [`lib/nocturne-core`](../../lib/nocturne-core) library; this app compiles
**only** the BMW path — no WiFi, monitoring, or hacker code.

## What it does

- **BLE proximity key** — phone connects → unlock; disconnects → lock.
- **I-Bus assistant** — lights, locks, windows, wipers, interior light, cluster
  text, MFL steering-wheel buttons, PDC parking sensors.
- **Demo mode** — exercise the UI and the Android app without a car or I-Bus.
- **OBD-II (optional, off by default)** — ELM327 stub for RPM / coolant / oil.

## Build & flash

```bash
# from the repo root
pio run -d apps/bmw                 # build
pio run -d apps/bmw -t upload       # flash over USB-C
pio device monitor -b 115200        # serial log
```

Needs an I-Bus transceiver (MCP2004A / TH3122 / optocoupler) — the bus is not
3.3 V logic. Wiring and pinout: [docs/bmw](../../docs/bmw).

## Companion app

`companion-app/` — native Android "BMW Assistant" (BLE). See
[companion-app/README.md](companion-app/README.md).

## Reference

I-Bus protocol reference lives at the repo root under `BMW datasheet/`
(wilhelm-docs, E46 codes, AVR-IBus). Message codes in the firmware are
cross-checked against it.

## License

GNU AGPL-3.0-only — see [LICENSE](../../LICENSE).
