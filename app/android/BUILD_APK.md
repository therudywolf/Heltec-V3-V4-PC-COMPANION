# BMW Digital Key — Android

**Сборка APK**

- **Android Studio (рекомендуется):** откройте папку `android` как проект, выберите **Build → Build Bundle(s) / APK(s) → Build APK(s)**. Сборка использует встроенный JDK студии.

- **Командная строка:** из папки `android` выполните:
  ```powershell
  $env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
  .\gradlew assembleRelease
  ```
  Если появляется ошибка `JdkImageTransform`/jlink, соберите проект из Android Studio.

**Где лежит APK после успешной сборки**

- Release: `android/app/build/outputs/apk/release/app-release.apk`
- Debug:   `android/app/build/outputs/apk/debug/app-debug.apk`
