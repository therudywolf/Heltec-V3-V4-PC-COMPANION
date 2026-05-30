# 🐺 Nocturne PC

> Standalone PC-companion firmware for Heltec WiFi LoRa 32 V4 (ESP32-S3).

One of the three Nocturne OS products. Built from this monorepo on top of the
shared [`lib/nocturne-core`](../../lib/nocturne-core) library; compiles PC
hardware monitoring + Forza Horizon telemetry + the BMW assistant. No WiFi/BLE
hacker tools.

## What it does

- **PC hardware monitoring** — CPU/GPU/RAM/disk/fan stats streamed from the PC
  over WiFi/TCP and shown across themed OLED scenes.
- **Forza Horizon telemetry** — live RPM / gear / speed dash over UDP (port 5300).
- **BMW assistant** — the I-Bus features are bundled in.

## Data collector (PC side)

The board is the display; the numbers come from a small Python service on the PC.
See [`server/`](../../server) (run `monitor.py`; config in `config.json`).

> Reworking the collector into clean modules + new modes (Claude usage/limits,
> mDNS auto-discovery) is Phase B of the restructure — see
> [docs/RESTRUCTURE_PLAN.md](../../docs/RESTRUCTURE_PLAN.md).

## Build & flash

```bash
# from the repo root
pio run -d apps/pc                 # build
pio run -d apps/pc -t upload       # flash over USB-C
pio device monitor -b 115200       # serial log
```

Set your WiFi/host in `include/secrets.h` (copy from `include/secrets.h.example`).

## License

GNU AGPL-3.0-only — see [LICENSE](../../LICENSE).
