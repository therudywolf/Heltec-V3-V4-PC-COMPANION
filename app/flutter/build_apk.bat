@echo off
cd /d "%~dp0"
where flutter >nul 2>&1
if %ERRORLEVEL% neq 0 (
  echo Flutter not found in PATH. Install Flutter and add it to PATH: https://flutter.dev
  exit /b 1
)
flutter pub get
flutter build apk
if %ERRORLEVEL% equ 0 (
  echo.
  echo APK: build\app\outputs\flutter-apk\app-release.apk
)
exit /b %ERRORLEVEL%
