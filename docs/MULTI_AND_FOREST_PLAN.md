# Nocturne Multi (4th product) + Forest monitoring — plan

Captures the larger, deferred scope so the common-feature work can land first.
Direction approved by the owner (2026-05-30).

## Build order (decided)

1. **Common features FIRST** — built in the shared `src/` tree, gated by
   `NOCT_FEATURE_*`, so they are available to every product. These are done
   incrementally with a green build at each step (see "Common features" below).
2. **4th product `apps/multi`** — assembled AFTER the common features exist.
   Everything-on personal build. Plan below; not started yet.
3. **Forest monitoring server** — separate component; plan below.

## Common features (do now, shared src)

| # | Feature | Files | Verify |
|---|---------|-------|--------|
| 1 | Network scene + sparkline | SceneManager, config.h scene idx | clean build |
| 2 | Alertmanager webhook → RED ALERT | server/ (new alert_webhook.py), monitor.py | pytest |
| 3 | Prometheus source (hybrid) | server/prometheus_source.py, config.json switch | pytest |
| 4 | OBD2 diagnostics (UART first) | ObdClient, BmwManager, new BMW diag scene | clean build |

OBD2 transport is **abstracted** (owner chose UART + BT + USB):
- **UART** (exists, GPIO 9/10) — implement first, proven.
- **Bluetooth SPP** — ESP32 classic BT to a wireless ELM327. Caveat: memory
  contention with the BLE key on the BMW build; fine on multi if managed.
- **USB-C Host (experimental)** — ESP32-S3 USB-OTG as CDC host to a USB ELM327.
  Risky: occupies the flashing/serial port, S3 USB-host CDC is immature. Build
  as an isolated spike flagged "may not work"; never the default.
Design: `ObdTransport` interface (read/write/available) with `UartObdTransport`
first; BT/USB added behind `config.h` selector. ObdClient talks to the
interface, not Serial2 directly. Extend PIDs (RPM/coolant/oil + speed/load/
throttle/intake) and add **DTC read (mode 03) / clear (mode 04)**.

## 4th product — apps/multi (deferred)

- New `apps/multi/` standalone PlatformIO project (same monorepo pattern as
  bmw/pc/hacker): own platformio.ini + VERSION + README, `src_dir` → shared
  tree, `tools/app_includes.py` pre-script, all four `NOCT_FEATURE_*=1`.
- Adds the multi-only menu sections: Forest panel scenes, full OBD2 diagnostics,
  events/alerts, Prometheus hybrid — everything in one firmware.
- Flash headroom: hacker (all features) ≈ 1.12 MB of 5 MB — multi fits easily.
- Release track `multi-v*` added to `release.yml`; CI `apps` matrix gains multi.
- Menu will be long → may need category paging (design when building).

## Forest panel + separate monitoring server (deferred)

Owner wants to **re-stand-up a dedicated monitoring server** (as existed before)
AND aggregate node status in monitor.py for the device.

- **dashboard.example.com is text/hierarchical/monospace/mono** → maps 1:1 to 128×64.
  3 nodes: Forestserver (Debian) / PC-Rudywolf (Windows) / Forestrouter
  (Keenetic) + VPN handshakes + game servers; status `●/—`, labels left, values
  right.
- **Device side:** new "Forest" scene set (one screen per node + a summary),
  rendered from a compact `forest` payload block (same pattern as `claude`).
- **Server side (two parts):**
  1. `forest_panel.py` in monitor.py: aggregate the 3 nodes (each via its own
     Prometheus / SSH / API) into the `forest` block. Owner chose "aggregate in
     monitor.py".
  2. **Separate monitoring server**: stand up the dedicated dashboard service
     again (the dashboard.example.com-style panel) — its own repo/service, exposing a
     JSON endpoint that both the web panel and monitor.py consume. Scope/host TBD
     with owner (likely Debian node, Prometheus-backed).

## Notes / constraints

- Agent cannot see the OLED or test on hardware → visual feel, OBD2 on a real
  car, BT/USB transports, and the live Windows/Prometheus stack need owner
  verification at milestones.
- Keep the three clean products lean — multi is the kitchen-sink; don't bloat
  bmw/pc/hacker with multi-only code (use feature flags / multi-only guards).
