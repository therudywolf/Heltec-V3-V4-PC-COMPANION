# PHASE 1: REPOSITORY AUDIT — BMW E39 I-Bus Assistant

**Workspace:** `c:\Users\rudywolf\Workspace\Heltec v4`  
**Scope:** ESP32-S3 C++ firmware, Android/Flutter app(s), core documentation.  
**Out of scope (candidates for purge):** PC monitoring, Forza, WiFi/BLE “Hacker” modes, LoRa, and any assets not strictly for the BMW E39 assistant.

---

## 1. CURRENT STRUCTURE (hierarchical tree)

Source and config only; build/cache omitted (`.gradle`, `.pio`, `app/build`, `android/app/build`, `.dart_tool` build artifacts).

```
Heltec v4/
├── .env
├── .gitignore
├── LICENSE
├── platformio.ini
├── README.md
├── huge_app.csv
├── .github/
│   └── workflows/
│       └── ci.yml
├── .vscode/
│   ├── c_cpp_properties.json
│   ├── extensions.json
│   └── launch.json
├── .pytest_cache/
│   ├── .gitignore
│   ├── CACHEDIR.TAG
│   ├── README.md
│   └── v/
│       └── cache/
│           └── nodeids
├── android/
│   ├── build-apk.bat
│   ├── build-apk.sh
│   ├── build.gradle.kts
│   ├── BUILD_APK.md
│   ├── gradle.properties
│   ├── gradlew.bat
│   ├── local.properties
│   ├── README.md
│   ├── settings.gradle.kts
│   ├── gradle/
│   │   └── wrapper/
│   │       ├── gradle-wrapper.jar
│   │       └── gradle-wrapper.properties
│   └── app/
│       ├── build.gradle.kts
│       ├── proguard-rules.pro
│       ├── src/main/
│       │   ├── AndroidManifest.xml
│       │   ├── java/com/nocturne/bmwassistant/
│       │   │   ├── BleAssistantHost.kt
│       │   │   ├── BleAssistantViewModel.kt
│       │   │   ├── BmwAssistantApplication.kt
│       │   │   ├── BmwPagerAdapter.kt
│       │   │   ├── BusFragment.kt
│       │   │   ├── ClusterFragment.kt
│       │   │   ├── CommandsFragment.kt
│       │   │   ├── DashboardFragment.kt
│       │   │   ├── MainActivity.kt
│       │   │   ├── MediaFragment.kt
│       │   │   └── SettingsFragment.kt
│       │   └── res/
│       │       ├── drawable/
│       │       │   ├── ic_bmw_logo.webp
│       │       │   ├── ic_launcher_background.xml
│       │       │   ├── ic_launcher_foreground.xml
│       │       │   ├── ic_launcher_legacy.xml
│       │       │   ├── ic_nav_bus.xml
│       │       │   ├── ic_nav_cluster.xml
│       │       │   ├── ic_nav_commands.xml
│       │       │   ├── ic_nav_dashboard.xml
│       │       │   ├── ic_nav_media.xml
│       │       │   └── ic_nav_settings.xml
│       │       ├── layout/
│       │       │   ├── activity_main.xml
│       │       │   ├── fragment_bus.xml
│       │       │   ├── fragment_cluster.xml
│       │       │   ├── fragment_commands.xml
│       │       │   ├── fragment_dashboard.xml
│       │       │   ├── fragment_media.xml
│       │       │   └── fragment_settings.xml
│       │       └── values/
│       │           ├── colors.xml
│       │           ├── dimens.xml
│       │           ├── shapes.xml
│       │           ├── strings.xml
│       │           ├── themes.xml
│       │           └── values-sw600dp/
│       │               └── dimens.xml
├── app/
│   ├── analysis_options.yaml
│   ├── build_apk.bat
│   ├── pubspec.lock
│   ├── pubspec.yaml
│   ├── README.md
│   ├── assets/
│   │   └── icon_my_bmw.webp
│   ├── lib/
│   │   ├── main.dart
│   │   ├── ble/
│   │   │   ├── bmw_ble_constants.dart
│   │   │   └── bmw_ble_provider.dart
│   │   ├── screens/
│   │   │   ├── hero_screen.dart
│   │   │   ├── media_screen.dart
│   │   │   ├── quick_actions_screen.dart
│   │   │   ├── telemetry_screen.dart
│   │   └── settings/
│   │       └── e39_variant_provider.dart
│   ├── lib/theme/
│   │   └── nocturne_theme.dart
│   └── android/
│       ├── app/
│       │   ├── build.gradle
│       │   ├── src/main/
│       │   │   ├── AndroidManifest.xml
│       │   │   ├── kotlin/com/nocturne/bmw_assistant/
│       │   │   │   └── MainActivity.kt
│       │   │   └── res/
│       │   │       ├── drawable/
│       │   │       │   ├── ic_launcher.webp
│       │   │       │   └── ic_launcher_foreground.xml
│       │   │       └── values/
│       │   │           ├── colors.xml
│       │   │           └── styles.xml
│       ├── build.gradle
│       ├── gradle.properties
│       ├── gradlew
│       ├── gradlew.bat
│       ├── local.properties
│       └── settings.gradle
├── BMW datasheet/
│   ├── arduino-ibustrx-master/
│   │   ├── examples/
│   │   ├── extras/
│   │   ├── src/
│   │   ├── keywords.txt
│   │   ├── library.json
│   │   ├── library.properties
│   │   └── README.md
│   ├── AVR-IBus.public-master/
│   │   ├── Firmware/ (HEX zips)
│   │   ├── Hardware/
│   │   ├── Manual/
│   │   ├── Pics/
│   │   ├── Tools/
│   │   └── README.md
│   ├── Ibus 1/
│   │   ├── Codes/
│   │   ├── Docs/ (BMW BUS Information, HackTheIBus, etc.)
│   │   ├── I-K Bus Library/
│   │   └── Programs/
│   └── wilhelm-docs-master/
│       └── (ike, radio, telephone, lcm, nav, gt, bmbt, etc.)
├── data/
│   └── README.txt
├── docs/
│   ├── USER_GUIDE.md
│   ├── GIT.md
│   ├── bmw/
│   │   ├── BMW_E39_Assistant.md
│   │   ├── BMW_ANDROID_APP.md
│   │   └── HELTEC_V4_WIRING.md
│   ├── board/
│   │   ├── CONNECTING_AND_TERMINAL.md
│   │   └── HELTEC_V4_BOARD_AND_CONFIG.md
│   ├── monitoring/
│   │   └── PC_MONITORING.md
│   └── forza/
│       └── FORZA_SETUP.md
├── include/
│   ├── secrets.h.example
│   └── nocturne/
│       ├── config.h
│       └── Types.h
├── optional/
│   └── radio/
│       ├── LoraManager.cpp
│       ├── LoraManager.h
│       └── README.md
├── server/
│   ├── monitor.py
│   ├── build_server.bat
│   ├── NocturneServer.spec
│   ├── requirements.txt
│   ├── config.json
│   └── tools/
│       └── dump_lhm_disks.py
├── src/
│   ├── main.cpp
│   ├── InputHandler.h
│   ├── AppModeManager.h
│   ├── AppModeManager.cpp
│   ├── MenuHandler.h
│   ├── MenuHandler.cpp
│   └── modules/
│       ├── display/
│       │   ├── DisplayEngine.h
│       │   ├── DisplayEngine.cpp
│       │   ├── SceneManager.h
│       │   ├── SceneManager.cpp
│       │   ├── BootAnim.h
│       │   ├── BootAnim.cpp
│       │   ├── RollingGraph.h
│       │   └── RollingGraph.cpp
│       ├── network/
│       │   ├── NetManager.h
│       │   ├── NetManager.cpp
│       │   ├── TrapManager.h
│       │   ├── TrapManager.cpp
│       │   ├── WifiSniffManager.h
│       │   └── WifiSniffManager.cpp
│       ├── ble/
│       │   ├── BleManager.h
│       │   └── BleManager.cpp
│       ├── car/
│       │   ├── BmwManager.h
│       │   ├── BmwManager.cpp
│       │   ├── BleKeyService.h
│       │   ├── BleKeyService.cpp
│       │   ├── ForzaManager.h
│       │   ├── ForzaManager.cpp
│       │   ├── A2dpSink.h
│       │   ├── A2dpSink.cpp
│       │   ├── ObdClient.h
│       │   ├── ObdClient.cpp
│       │   └── ibus/
│       │       ├── IbusDriver.h
│       │       ├── IbusDriver.cpp
│       │       ├── IbusSerial.h
│       │       ├── IbusSerial.cpp
│       │       ├── IbusDefines.h
│       │       ├── IbusCodes.h
│       │       ├── IbusCodes.cpp
│       │       ├── RingBuffer.h
│       │       └── RingBuffer.cpp
│       └── system/
│           ├── BatteryManager.h
│           └── BatteryManager.cpp
├── tests/
│   └── test_monitor.py
├── tools/
│   ├── README.md
│   └── forza_udp_capture.py
└── DataSheets/           (if present)
    └── WiFi_LoRa_32_V4.2.0.pdf
```

---

## 2. PROPOSED STERILE ARCHITECTURE

Logical separation: firmware, app(s), docs, shared/reference. Single product = BMW E39 I-Bus assistant.

```
Heltec v4/
├── firmware/                 # ESP32-S3 C++ (PlatformIO)
│   ├── platformio.ini
│   ├── huge_app.csv
│   ├── include/
│   │   ├── secrets.h.example
│   │   └── nocturne/
│   │       ├── config.h
│   │       └── Types.h
│   ├── src/
│   │   ├── main.cpp
│   │   ├── InputHandler.h
│   │   ├── AppModeManager.*
│   │   ├── MenuHandler.*
│   │   └── modules/
│   │       ├── display/
│   │       ├── ble/
│   │       ├── car/          # BmwManager, BleKeyService, ibus/ only
│   │       └── system/
│   └── data/
│       └── README.txt
├── app/                      # Primary mobile app (choose one or keep both)
│   ├── android/              # Native Android (Kotlin) — BMW Assistant
│   │   └── ...
│   └── flutter/              # Flutter MD3 app (current app/)
│       └── ...
├── docs/
│   ├── USER_GUIDE.md
│   ├── GIT.md
│   ├── bmw/
│   │   ├── BMW_E39_Assistant.md
│   │   ├── BMW_ANDROID_APP.md
│   │   └── HELTEC_V4_WIRING.md
│   ├── board/
│   │   ├── CONNECTING_AND_TERMINAL.md
│   │   └── HELTEC_V4_BOARD_AND_CONFIG.md
│   └── reference/            # Optional: move BMW datasheet here or keep external
├── shared/                   # Optional: scripts, CI, IDE
│   ├── .github/
│   │   └── workflows/
│   │       └── ci.yml        # Firmware build only
│   ├── .vscode/
│   └── DataSheets/
├── LICENSE
├── README.md
└── .gitignore
```

Alternative: keep a flat layout but purge unrelated content and rename nothing until Phase 2:
- `firmware/` = корень репозитория с `src/`, `include/`, `platformio.ini`, `data/` (нет папки `firmware/`; проект открывать из корня).
- `app/` = single app folder (either `android/` or Flutter `app/` as the canonical one).
- `docs/` = only bmw + board (+ USER_GUIDE, GIT).
- `BMW datasheet/` = keep as reference or move under `docs/reference/`.

---

## 3. FILES / DIRECTORIES TO DELETE (unrelated to BMW E39 I-Bus assistant)

Everything listed here is **not** required for the BMW E39 I-Bus assistant (ESP32-S3 firmware, app, or core docs). No code or files have been modified or removed; this is the purge list for **EXECUTE PHASE 2**.

### 3.1 PC monitoring (server + tests)

- `server/` (entire directory)
  - `server/monitor.py`
  - `server/build_server.bat`
  - `server/NocturneServer.spec`
  - `server/requirements.txt`
  - `server/config.json`
  - `server/tools/dump_lhm_disks.py`
- `tests/` (entire directory)
  - `tests/test_monitor.py`
- `.pytest_cache/` (entire directory)

### 3.2 Forza

- `tools/` (entire directory)
  - `tools/README.md`
  - `tools/forza_udp_capture.py`
- `docs/forza/` (entire directory)
  - `docs/forza/FORZA_SETUP.md`
- Firmware: `src/modules/car/ForzaManager.cpp`
- Firmware: `src/modules/car/ForzaManager.h`

### 3.3 WiFi / BLE “Hacker” modes (Trap, WiFi clone, Infosec)

- `src/modules/network/TrapManager.cpp`
- `src/modules/network/TrapManager.h`
- `src/modules/network/WifiSniffManager.cpp`
- `src/modules/network/WifiSniffManager.h`  
  (Keep `NetManager.*` if still used for general WiFi/connection; remove if only used by Trap/monitor.)

### 3.4 LoRa (optional radio)

- `optional/` (entire directory)
  - `optional/radio/LoraManager.cpp`
  - `optional/radio/LoraManager.h`
  - `optional/radio/README.md`

### 3.5 PC monitoring documentation

- `docs/monitoring/` (entire directory)
  - `docs/monitoring/PC_MONITORING.md`

### 3.6 Dead / unused firmware code (ESP32-S3)

- `src/modules/car/A2dpSink.cpp`
- `src/modules/car/A2dpSink.h`  
  (A2DP not supported on ESP32-S3 per config; safe to remove.)

### 3.7 Optional (review before Phase 2)

- **OBD:** `src/modules/car/ObdClient.cpp`, `src/modules/car/ObdClient.h` — used for optional BMW OBD (NOCT_OBD_ENABLED). Keep if you want OBD support; otherwise can be deleted.
- **CI:** `.github/workflows/ci.yml` — remove the `monitor` job (and server deps) so CI only builds firmware; do not delete the file unless you drop CI entirely.

---

## 4. SUMMARY

| Category              | Action |
|-----------------------|--------|
| **Keep**              | `src/` (minus Forza, Trap, WifiSniff, A2dp), `include/`, `android/`, `app/` (Flutter), `docs/bmw/`, `docs/board/`, `docs/USER_GUIDE.md`, `docs/GIT.md`, `data/`, `BMW datasheet/`, `platformio.ini`, `huge_app.csv`, `LICENSE`, `.gitignore`, `.vscode/` |
| **Delete (dirs)**     | `server/`, `tests/`, `.pytest_cache/`, `tools/`, `docs/forza/`, `docs/monitoring/`, `optional/` |
| **Delete (files)**    | `ForzaManager.*`, `TrapManager.*`, `WifiSniffManager.*`, `A2dpSink.*` |
| **Modify in Phase 2** | `README.md`, `main.cpp`, `MenuHandler.*`, `AppModeManager.*`, `SceneManager.*` (remove Forza/Trap/WiFi/BLE mode paths and menu entries); `.github/workflows/ci.yml` (firmware-only). |

**No code has been changed and no files have been deleted.** Awaiting your explicit command: **"EXECUTE PHASE 2"** to apply the restructure and deletions.
