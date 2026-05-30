# 🐺 Nocturne Hacker

> Standalone WiFi/BLE research firmware for Heltec WiFi LoRa 32 V4 (ESP32-S3).

One of the three Nocturne OS products. Built from this monorepo on top of the
shared [`lib/nocturne-core`](../../lib/nocturne-core) library. This is the
"everything" build — the full feature set **plus** the WiFi/BLE research tools.

## What it does

- **WiFi recon** — scan, packet sniffing (promiscuous), probe-request and
  station enumeration, EAPOL-handshake detection, channel/packet stats.
- **Evil-twin honeypot** — clone an SSID, captive portal, credential capture
  (on your own AP, for testing).
- **BLE** — scan/enumerate, advertising spam, AirTag/Flipper/skimmer detectors.

> Several WiFi modes are still stubbed (PINESCAN, MULTISSID, SIGNAL_STRENGTH,
> RAW_CAPTURE, AP_STA alias to AP scan); channel hopping, packet capture/export
> and L7 dissection are not done yet. Completing them is Phase C — see
> [docs/RESTRUCTURE_PLAN.md](../../docs/RESTRUCTURE_PLAN.md).

## Scope & ethics

Passive reconnaissance, capture, and an **own-AP** honeypot only. No active
deauthentication or jamming. Use exclusively on networks and devices you own or
are explicitly authorized to test. You are responsible for complying with local
law.

## Build & flash

```bash
# from the repo root
pio run -d apps/hacker                 # build
pio run -d apps/hacker -t upload       # flash over USB-C
pio device monitor -b 115200           # serial log
```

Set WiFi/host in `include/secrets.h` (copy from `include/secrets.h.example`).

## License

GNU AGPL-3.0-only — see [LICENSE](../../LICENSE).
