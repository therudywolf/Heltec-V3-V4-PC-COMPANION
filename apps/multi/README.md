# 🐺 Nocturne Multi — all-in-one

> The kitchen-sink personal build for Heltec WiFi LoRa 32 V4 (ESP32-S3).

The fourth Nocturne OS product. Built from this monorepo on the shared
[`lib/nocturne-core`](../../lib/nocturne-core) library; compiles **everything**
in one firmware:

- **PC monitoring** — hardware stats over WiFi/TCP + the Network scene + the
  Claude usage scene + Prometheus/Alertmanager events.
- **Forza Horizon** telemetry dash (UDP).
- **BMW** — I-Bus assistant + BLE proximity key + **OBD2 diagnostics**
  (live PIDs + DTC read/clear, enabled by default in this build).
- **Hacker** — WiFi/BLE research tools (sniff/scan/probe/EAPOL, evil-twin
  honeypot, BLE scan/spam). Passive recon + own-AP honeypot only; no
  deauth/jammer.

The three clean products (bmw / pc / hacker) stay lean for people who want one
thing; **multi** is for enthusiasts who want it all on one board.

## Build & flash

```bash
# from the repo root
pio run -d apps/multi                 # build
pio run -d apps/multi -t upload       # flash over USB-C
pio device monitor -b 115200          # serial log
```

Set WiFi/host in `include/secrets.h` (copy `include/secrets.h.example`).
OBD2 needs an ELM327 on a second UART (see [docs/bmw](../../docs/bmw)).

> **Heltec V4 16 MB flash:** this build uses the full-chip partition layout
> (dual 3 MB OTA slots + ~10 MB LittleFS). First flash should be boot-confirmed
> on hardware.

## Menu

The combined menu is long (all categories). Navigation is the same as the other
products: double-tap = menu, short = next, long (~2 s) = select.

## License

GNU AGPL-3.0-only — see [LICENSE](../../LICENSE).
