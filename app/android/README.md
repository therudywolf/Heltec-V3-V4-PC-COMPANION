# BMW Assistant — Android app

BLE client for the **Nocturne OS BMW E39 Assistant** (Heltec V4). Material Design 3 with **BMW-style theme**: black background, amber primary accent, red for errors and dangerous actions (classic cluster look). **Tabs:** Status, Commands, Media, Cluster, Settings, Bus. Target SDK 35 (Android 15), minSdk 26.

## Protocol

See [../docs/bmw/BMW_ANDROID_APP.md](../docs/bmw/BMW_ANDROID_APP.md) for GATT UUIDs, command table, status (16 bytes), Now Playing, and cluster text. Подключение платы к машине (I-Bus, питание): [HELTEC_V4_WIRING.md](../../docs/bmw/HELTEC_V4_WIRING.md).

## Build (one-click)

- **JDK:** используйте системный JDK 11+ или задайте `JAVA_HOME` (например на путь к JDK 17). Папка `jdk-*` в корне репозитория в .gitignore — не часть исходников.
- **Windows:** run `build-apk.bat` in the `app/android` folder. The script uses **Android Studio JBR** (JDK 17) if `JAVA_HOME` is not set, so AGP 8.x builds succeed. If you use another JDK, set `JAVA_HOME` to JDK 11 or 17 before running. You can also open the project in Android Studio and use **Build → Build Bundle(s) / APK(s) → Build APK(s)**.
- **Linux/macOS:** run `./build-apk.sh` (chmod +x once). Ensure `JAVA_HOME` points to JDK 11+ (e.g. `export JAVA_HOME=/path/to/jdk-17`).
- Output: `app/build/outputs/apk/release/app-release.apk` (signed with debug key for local install; for store use your own signing config in `app/build.gradle.kts`).

## Permissions

- **Android 12+:** `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT` (requested at runtime). If denied, scan is disabled until the user grants permission in system settings.
- **Older:** `ACCESS_FINE_LOCATION`, `BLUETOOTH`, `BLUETOOTH_ADMIN`.

## Usage

1. Enable Bluetooth. Put the Heltec in **BMW Assistant** or **BMW Demo** mode (Menu → BMW → BMW Assistant or BMW Demo, long press).
2. Open the app, tap **Scan** to connect to `BMW E39 Key`. Use the **tabs** to switch between Status, Commands, Media, Cluster, Settings, and Bus.
3. **Status:** connection state, auto-connect switch, live I-Bus/OBD, coolant/oil, RPM, PDC, MFL, doors, lock, ignition, odometer.
4. **Commands:** scenarios, lights, locks, windows, wipers, interior, light show.
5. **Media:** send Track / Artist to the board (OLED + MID). Format: track\\0artist.
6. **Cluster:** send custom text (up to 20 chars) to the instrument cluster (IKE).
7. **Auto-connect:** when enabled, the app will try to reconnect to the last device after disconnect (~2.5 s delay).

Connection triggers automatic unlock (firmware); disconnection triggers lock after ~2.5 s (firmware).
