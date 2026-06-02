# Nocturne OS — Roadmap (sprints)

Source-of-truth backlog, broken into sprints. Each sprint has a goal, scoped
work, the files it touches, how it's **verified by the agent** (build / pytest —
no hardware), and a separate **hardware checkpoint** the owner does on device.

**Working rules (learned the hard way):**
- One source tree (`src/` + `lib/nocturne-core`); products are `apps/<x>` built
  with feature flags via `build_src_filter`. No code duplication.
- After any edit: clean build (`rm -rf apps/<x>/.pio/build && pio run -d apps/<x>`)
  and read the literal `[SUCCESS]`; for server, `python -m pytest tests/`.
  "Compiles" ≠ "works" — grep that new dispatch cases AND method bodies exist.
- Agents (sub-tasks) never run git; the main loop verifies, commits, pushes.
- Commit per coherent change; keep all 6 CI jobs green.

---

## ✅ Done (context)

- **Restructure**: 3 standalone products `apps/{bmw,pc,hacker}` on shared core;
  per-product VERSION/README; CI builds 3 apps + 3 legacy profiles; per-product
  release tracks (`bmw-v*`/`pc-v*`/`hacker-v*`).
- **BMW**: I-Bus frame-length fix (datasheet-verified); OBD stub tidied.
- **PC server**: monitor.py modularized (lhm/weather/netrates/payload/claude).
- **Hacker**: 5 stubbed WiFi modes implemented + channel hopping.
- **Common features**: network scene + sparkline; Prometheus hybrid source;
  Alertmanager events → device banner; Claude usage scene (server + firmware).
- **Docs**: BMW wiring guide; restructure plan; multi+forest plan.

---

## Sprint 1 — BMW OBD2 diagnostics  ⬅ NEXT

**Goal:** real engine diagnostics over OBD-II, transport-agnostic, with fault
codes — not just the RPM/temp stub.

**Scope**
1. `ObdTransport` interface (`begin/available/read/write/end`) — decouple
   `ObdClient` from `Serial2`.
2. `UartObdTransport` (existing GPIO 9/10 path) as the first implementation.
3. Extend PIDs: speed (010D), engine load (0104), throttle (0111), intake temp
   (010F), MAF (0110) — in addition to RPM/coolant/oil.
4. **DTC read (mode 03) + clear (mode 04)**: fetch stored fault codes, decode to
   `P0xxx`/`C/B/U` strings; clear on a long-press.
5. BMW **Diagnostics scene**: live PIDs grid + DTC list/count; "no DTC" state.
6. Config: keep `NOCT_OBD_ENABLED` (off by default); document transports.

**Files:** `src/modules/car/ObdClient.{h,cpp}`, new `ObdTransport.h` +
`UartObdTransport.{h,cpp}`, `BmwManager.{h,cpp}`, `SceneManager.{h,cpp}`,
`config.h`, `apps/bmw/README.md`.

**Agent-verifiable:** clean builds bmw/pc/hacker `[SUCCESS]`; DTC decode unit
logic (host-side parse test if extracted as pure fn).

**Hardware checkpoint (owner):** ELM327 on a real OBD-II port → PIDs read, DTC
list correct, clear works. (Bluetooth/USB-C transports = Sprint 8, deferred.)

**DoD:** Diagnostics scene shows live PIDs + DTCs on bench ELM327; all builds green.

---

## Sprint 2 — PC companion: autodiscovery & polish

**Goal:** zero-config pairing with the PC and a finished Forza mode.

**Scope**
1. **mDNS auto-discovery** — board resolves the server via mDNS
   (`nocturne._tcp`), dropping hardcoded `PC_IP`; server advertises the service.
2. **Forza menu-mode polish** — verify UDP listener lifecycle (port 5300),
   shift-light timing, splash/teardown; clean exit back to menu.
3. **`docs/RELEASING.md`** — the version-bump → tag → release recipe.
4. (optional) move `server/` → `apps/pc/server/` for product locality (update
   CI/test paths in the same commit).

**Files:** `src/main.cpp`, `src/modules/network/NetManager.{h,cpp}`, `secrets.h`
docs, `server/monitor.py` (mDNS advertise), `ForzaManager.*`, `SceneManager.cpp`,
`docs/RELEASING.md`.

**Agent-verifiable:** clean builds; mDNS code compiles; pytest stays green.

**Hardware checkpoint (owner):** board finds PC with no IP set; Forza dash live
in-game; reset-to-menu clean.

**DoD:** fresh flash connects without editing `PC_IP`; Forza polished; releasing
doc published.

---

## Sprint 3 — Hacker depth (apps/hacker)

**Goal:** turn the recon toy into a capture+analysis tool. Boundary held:
passive recon + capture + own-AP honeypot; **no deauth/jammer**.

**Scope**
1. **Packet capture/export** — ring buffer → LittleFS (3 MB partition); pcap
   header; dump over serial; on-device "saved N frames" status.
2. **L7 dissection** — surface HTTP Host, DNS query names, EAPOL handshake
   detail in the sniff scenes.
3. **BLE detectors** — sharpen AirTag/Flipper/Flock/skimmer heuristics; wire
   `cloneDevice()` to a UI pick from scan results; scan logging.

**Files:** `src/modules/network/WifiSniffManager.{h,cpp}`,
`src/modules/ble/BleManager.{h,cpp}`, `SceneManager.cpp`, `config.h` (pcap part),
`apps/hacker/README.md`.

**Agent-verifiable:** clean builds; pcap header bytes / DNS-name parser as pure
unit logic where extractable.

**Hardware checkpoint (owner):** capture a real handshake; pcap opens in
Wireshark; BLE detectors fire on known devices.

**DoD:** capture→LittleFS→pcap works on device; L7 fields shown; clone from UI.

---

## Sprint 4 — apps/multi (4th product) scaffold

**Goal:** the personal kitchen-sink build — everything in one firmware.

**Scope**
1. New `apps/multi/` standalone PIO project (monorepo pattern: shared `src_dir`,
   `tools/app_includes.py`, all `NOCT_FEATURE_*=1`), own `VERSION`/`README`.
2. Release track `multi-v*` in `release.yml`; add `multi` to CI `apps` matrix.
3. **Menu paging** — the combined menu is long; add category paging/scroll so it
   stays usable.
4. Flash-size check (hacker ≈ 1.12 MB of 5 MB → headroom fine).

**Files:** `apps/multi/{platformio.ini,VERSION,README.md}`, `.github/workflows/
{ci,release}.yml`, `MenuHandler.*`, `SceneManager.cpp` (paging).

**Agent-verifiable:** `pio run -d apps/multi` `[SUCCESS]`; all four products + CI
green.

**Hardware checkpoint (owner):** every feature reachable from one firmware; menu
navigable.

**DoD:** apps/multi builds, releases on its own tag, menu paged.

---

## Sprint 5 — Forest panel + monitoring server

**Goal:** duplicate the dashboard.example.com node panel on-device, plus re-stand-up a
dedicated monitoring server.

**Scope**
1. **`forest_panel.py`** in the server: aggregate the 3 nodes (Forestserver /
   PC-Rudywolf / Forestrouter) via each one's Prometheus/SSH/API into a compact
   `forest` payload block; VPN + game-server status.
2. **Device Forest scenes** — one screen per node + a summary; status `●/—`,
   labels left / values right (maps 1:1 to 128×64; multi-only).
3. **Separate monitoring server** — re-stand-up the dashboard service exposing a
   JSON endpoint both the web panel and monitor.py consume (host/scope TBD with
   owner; likely Debian node, Prometheus-backed).

**Files:** new `server/forest_panel.py` (+ tests), `monitor.py` (forest block),
`Types.h` (ForestData), `NetManager.cpp` (parse), `SceneManager.{h,cpp}`,
separate-server repo/dir TBD.

**Agent-verifiable:** forest_panel aggregation as pure functions + tests; device
scenes build green.

**Hardware checkpoint (owner):** real node data renders; endpoints reachable.

**DoD:** Forest scenes show live node status in apps/multi; server endpoint up.

---

## Sprint 6 — Architecture cleanup (SceneProvider)

**Goal:** pay down the deferred core debt so products are truly decoupled.

**Scope**
1. **SceneProvider seam** — core owns primitives + common scenes; each app
   registers its own scene set (no giant `#if`-gated SceneManager).
2. **Purge `#if NOCT_FEATURE_*`** from shared `src/` where the split makes it
   unnecessary; each app compiles only its own scenes/modes.
3. Re-verify all four products byte-for-byte behavior unchanged.

**Files:** `SceneManager.*`, `AppModeManager.*`, `MenuHandler.*`, per-app glue.

**Agent-verifiable:** all four builds green; scene dispatch grep-verified.

**Hardware checkpoint (owner):** spot-check each product's screens unchanged.

**DoD:** no cross-product `#if` in shared scene code; all products green.

---

## Sprint 7 — Release & distribution (Phase D)

**Goal:** ship it cleanly; make updates painless.

**Scope**
1. **OTA over WiFi** — board pulls its product's latest GitHub Release `.bin`
   and self-updates (opt-in, with safety/rollback).
2. **GitHub Pages flasher per product** — enable Pages; ESP Web Tools page each
   for bmw/pc/hacker/multi.
3. **Retire legacy** — remove the root unified `platformio.ini` build + its CI
   `legacy` job; archive the `v0.4.0` tag as unified-legacy.
4. Tag & publish first real releases of all four products.

**Files:** new OTA module, `docs/flash/*`, `.github/workflows/*`, root cleanup.

**Agent-verifiable:** workflows lint/build; OTA module compiles.

**Hardware checkpoint (owner):** web-flash a board end-to-end; OTA update lands.

**DoD:** four products released with flashers; OTA works; legacy retired.

---

## Sprint 8 — OBD2 extra transports (deferred / experimental)

**Goal:** wireless & USB OBD2, behind the Sprint-1 transport interface.

**Scope**
1. **Bluetooth SPP ELM327** (`BtObdTransport`) — ESP32 classic BT to a wireless
   dongle. Caveat: BT/BLE memory contention with the BLE key — manage coexist.
2. **USB-C Host (experimental spike)** (`UsbObdTransport`) — ESP32-S3 USB-OTG as
   CDC host. **Risky:** occupies the flash/serial port, S3 host CDC immature.
   Isolated, flagged "may not work"; never the default.
3. `config.h` transport selector (`uart`/`bt`/`usb`).

**Agent-verifiable:** each transport compiles under its flag.

**Hardware checkpoint (owner):** BT dongle pairs & reads; USB spike evaluated.

**DoD:** BT transport usable; USB spike conclusion documented.

---

## Parallel track — Hardware verification (owner-only, ongoing)

These gate "done" but the agent cannot perform them:
- BMW I-Bus frame fix on a real **E39** (owed since the fix landed).
- New scenes on the **OLED**: NET, CLAUDE, event toast — visual feel/legibility.
- Live **windows_exporter** scrape, live **Alertmanager** POST end-to-end.
- ELM327 diagnostics on a car; hacker RF modes in a real environment.

---

## Suggested order & rationale

1 (OBD2) → 2 (PC autodiscovery/polish) → 3 (hacker depth): finishes the three
shipping products. Then 4 (multi) + 5 (forest) deliver the personal all-in-one
vision. 6 (SceneProvider) pays down debt before 7 (release/OTA) ships everything.
8 is opportunistic once a transport interface exists. The hardware track runs
alongside, owner-driven, at each sprint's checkpoint.
