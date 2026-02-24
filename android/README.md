# BMW Assistant — Android app

BLE client for the **Nocturne OS BMW E39 Assistant** (Heltec V4). Material Design 3 dark theme, tabs: Status, Commands, Media, Cluster. Target SDK 35 (Android 15).

## Protocol

See [../docs/bmw/BMW_ANDROID_APP.md](../docs/bmw/BMW_ANDROID_APP.md) for GATT UUIDs, command table, status (10 bytes), Now Playing, and cluster text.

## Build (one-click)

- **Windows:** run `build-apk.bat` in the `android` folder. Requires [Gradle](https://gradle.org/install/) in PATH to generate the wrapper on first run; or open the project in Android Studio and use **Build → Build Bundle(s) / APK(s) → Build APK(s)**.
- **Linux/macOS:** run `./build-apk.sh` (chmod +x once). Same Gradle requirement for first-time wrapper.
- Output: `app/build/outputs/apk/release/app-release.apk` (signed with debug key for local install; for store use your own signing config in `app/build.gradle.kts`).

## Permissions

- **Android 12+:** `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT` (requested at runtime).
- **Older:** `ACCESS_FINE_LOCATION`, `BLUETOOTH`, `BLUETOOTH_ADMIN`.

## Usage

1. Enable Bluetooth. Put the Heltec in **BMW Assistant** mode (Menu → BMW → BMW Assistant, long press).
2. Open the app, tap **Scan** to connect to `BMW E39 Key`.
3. **Status:** connection state, auto-connect switch, live I-Bus/OBD, coolant/oil, RPM, PDC, MFL.
4. **Commands:** lights (Goodbye, Follow Me Home, Park, Hazard, Low Beams, Off), locks (Unlock/Lock), Trunk, Doors, Cluster NOCT, Light Show On/Off.
5. **Media:** send Track / Artist to the board (OLED + MID). Format: track\0artist.
6. **Cluster:** send custom text (up to 20 chars) to the instrument cluster (IKE).

Connection triggers automatic unlock; disconnection triggers lock after ~2.5 s (firmware).
