# Nocturne OS — Restructure into 3 products

Split the single firmware into **3 separate, polished products** built from a
**monorepo with a shared core library**. Each product gets its own release
track, version, flasher, README and identity.

> Status: PLAN (approved direction). Not yet executed.

## Locked decisions

- **Model B** — one repo: `lib/nocturne-core` + `apps/{bmw,pc,hacker}`.
  Not 3 branches (drift trap), not 3 repos.
- **3 release tracks** — tag prefixes `bmw-v*`, `pc-v*`, `hacker-v*`; each
  produces its own GitHub Release + ESP Web Tools flasher.
- **Archive v0.4** — the unified `v0.4.0` build becomes "unified legacy"; the
  3 products replace it.
- **Datasheets** stay at repo root as shared BMW reference; no history rewrite.
- **Hacker boundary** — passive recon + capture + own-AP honeypot only; no
  active deauth/jammer.

## The 3 products

| Product | Dir | Was | "Perfect" means |
|---|---|---|---|
| Hacking toy | `apps/hacker` | `full`/hacker flags | all WiFi/BLE modes real & working |
| PC companion | `apps/pc` | `pc_companion` | monitoring + Forza + reworked server + new modes |
| BMW companion | `apps/bmw` | `bmw_only` | I-Bus verified on car + Android app |

## Target layout

```
/                         monorepo hub (top README links the 3 products)
├── lib/nocturne-core/    shared core (library.json)
│   └── src/
│       ├── display/      DisplayEngine, DisplayManager, BootAnim, RollingGraph,
│       │                 SceneManager BASE (primitives + common scenes)
│       ├── input/        InputHandler, MenuHandler engine
│       ├── app/          AppModeManager framework
│       ├── system/       BatteryManager
│       └── nocturne/     config (core subset), Types.h, strings (core subset)
├── apps/
│   ├── bmw/   platformio.ini, VERSION, src/ (car/*, ibus/*, BMW scenes),
│   │          companion-app/ (native Android), README, docs, flash/
│   ├── pc/    platformio.ini, VERSION, src/ (network/NetManager, car/ForzaManager,
│   │          monitoring scenes), server/ (Python collector), README, docs, flash/
│   └── hacker/ platformio.ini, VERSION, src/ (network/Trap+Sniff, ble/BleManager,
│              hacker scenes), README, docs, flash/
├── BMW datasheet/        shared reference (unchanged)
└── .github/workflows/    ci + release (tag-prefix aware)
```

## The hard part — core extraction

Most code is genuinely shared (display/menu/input/battery/boot). The crux is
**SceneManager.cpp (~2327 lines)**: today one file with every product's scenes
gated by `#if`. Target:

- **Core** owns rendering primitives (header, footer, chamfer boxes, grid cells,
  transitions, fonts) + truly common scenes (menu, NO DATA, NO SIGNAL, charge,
  screensaver, boot).
- **Each app** supplies its own scene set via a `SceneProvider` interface (or
  registered draw callbacks) — design decision to nail in Phase 0.
- Same pattern for `MenuHandler` (engine in core, category/item tables per app —
  already partly data-driven after the strings.h refactor) and `AppModeManager`
  (framework in core, mode set per app).
- `config.h` splits: core (display geometry, battery, timings, button) vs
  per-app (BMW pins/codes, network, hacker).

**Big win:** every `#if NOCT_FEATURE_*` gate disappears — each app compiles only
its own code. That alone is a major quality jump.

## Phased plan (green build at every step)

### Phase 0 — Foundation (highest risk, do once)  ✅ DONE
1. ✅ Create `lib/nocturne-core` + `library.json`.
2. ✅ Move the cleanly-shared leaf modules into core: `DisplayEngine`,
   `BootAnim`, `RollingGraph`, `BatteryManager`, `InputHandler`, plus the
   foundational headers `nocturne/{config,Types,strings}.h`.
3. ✅ Unified root build consumes the core lib; **all 3 profiles green** —
   boundary proven.
4. Deferred (by design): the SceneManager/Menu/Mode `SceneProvider` seam is
   designed during the **first app peel (Phase A)** — you need a concrete second
   consumer to get the interface right, and it keeps Phase 0 low-risk. The
   product-coupled modules (`SceneManager`, `DisplayManager`, `MenuHandler`,
   `AppModeManager`, config split) stay app-side until then.

**Mechanism note:** PlatformIO finds core headers via the `-I lib/nocturne-core/src*`
entries in the env `build_flags` (these reach both app and library compilation).
A clean build (`rm -rf .pio/build`) is required after moving files — stale
incremental state gives misleading "header not found" errors.

### Phase A — BMW app (first standalone; smallest, already audited)  ✅ DONE
- `apps/bmw` is a standalone PlatformIO project on the shared core: own
  `platformio.ini` + `VERSION` (0.5.0) + README; native Android app moved to
  `apps/bmw/companion-app`. Scoped via `build_src_filter` + `NOCT_FEATURE_*`.
- The monorepo pattern (used by all 3 apps): app `platformio.ini` points
  `src_dir`/`include_dir`/`lib_dir` at the shared root tree; shared include dirs
  are added as absolute paths by `tools/app_includes.py` (a pre-script — NOT
  `-I ${PROJECT_DIR}/../..`, which resolves wrong, and NOT `__file__`, which
  SCons doesn't provide). `main.cpp` `#if` purge deferred (see below).
- ✅ Builds green (local + CI). ⏳ **hardware checkpoint (real E39)** still owed
  for the I-Bus frame-length fix.

> Also done alongside A: **apps/pc** and **apps/hacker** were stood up the same
> way (own platformio.ini/VERSION/README), so all three products build green on
> CI now. Their *feature* work is still Phases B and C below.

### Phase B — PC app
- `apps/pc` on core: `NetManager`, `ForzaManager`, monitoring scenes.
- **Server rework** ("доработка сборщика данных"): `monitor.py` (1177 lines) →
  modules `lhm/weather/media/payload/server/tray`; split god-function
  `_parse_lhm_json`; fix `global_data_cache` race, blocking ping off the event
  loop, `except: pass`, dead `last_sent_track_key`.
- **Forza** as a polished menu mode; **Claude usage/limits** mode (server reader
  + scene); **mDNS** auto-discovery (drop hardcoded `PC_IP`).

### Phase C — Hacker app ("лютая" = maximize)
- `apps/hacker` on core: `TrapManager`, `WifiSniffManager`, `BleManager` + scenes.
- Complete the 5 dead WiFi modes (PINESCAN/MULTISSID/SIGNAL_STRENGTH/RAW_CAPTURE/
  AP_STA currently alias to AP scan); channel hopping; packet capture/export to
  LittleFS; L7 dissection (HTTP host, DNS, EAPOL); better BLE detectors +
  cloneDevice UI wiring + scan logging.
- Boundary held: no deauth/jammer.

### Phase D — Release machinery + retire legacy
- Per-product release workflow (tag prefix) → factory `.bin` + manifest + Release.
- 3 flash pages/sections; 3 READMEs; top-level README as a hub.
- Retire the unified root build; archive `v0.4.0` as unified-legacy.

## Release & versioning
- Each app: own `VERSION` (suggest start `0.5.0` to signal continuity, or fresh
  `1.0.0` per product — owner's call).
- Tags: `bmw-v0.5.0`, `pc-v0.5.0`, `hacker-v0.5.0`.

## Risks & checkpoints
- SceneManager/config decoupling = main regression risk; **OLED not visible to
  the agent** → owner verifies on device at each app milestone.
- I-Bus frame fix needs **real-car verification** (Phase A checkpoint).
- Core change re-verifies all 3 apps (the cost of a single source of truth).
- 3 versions/tracks = more release overhead (accepted).

## Migration of work already on `main`
- BMW I-Bus fix (2ed0e75) → `apps/bmw`.
- strings.h centralization (81fda2c) → core + per-app split.
- Restored `server/` → `apps/pc/server`.
Nothing is lost.
