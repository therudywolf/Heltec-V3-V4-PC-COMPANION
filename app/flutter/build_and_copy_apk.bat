@echo off
setlocal
set "WORKSPACE=%~dp0..\.."
set "FLUTTER_APP=%~dp0"
set "RELEASE_APK=%WORKSPACE%\release apk"
set "FLUTTER_BIN=%WORKSPACE%\flutter_sdk\flutter\bin\flutter.bat"
set "JAVA_HOME=%WORKSPACE%\jdk-17.0.18+8"
set "PATH=%JAVA_HOME%\bin;%PATH%"
set "GRADLE_OPTS=-Dorg.gradle.daemon=false"
if not exist "%FLUTTER_BIN%" (
  echo Flutter not found at %FLUTTER_BIN%. Add Flutter to PATH or restore flutter_sdk.
  exit /b 1
)
cd /d "%FLUTTER_APP%"
echo [1/4] flutter clean
call "%FLUTTER_BIN%" clean
if errorlevel 1 exit /b 1
echo [2/4] flutter pub get
call "%FLUTTER_BIN%" pub get
if errorlevel 1 exit /b 1
echo [3/4] flutter build apk --split-per-abi
call "%FLUTTER_BIN%" build apk --split-per-abi
if errorlevel 1 exit /b 1
echo [4/4] Copy APKs to "release apk"
if not exist "%RELEASE_APK%" mkdir "%RELEASE_APK%"
for %%f in (build\app\outputs\flutter-apk\*-release.apk) do copy /Y "%%f" "%RELEASE_APK%\"
echo.
echo APKs copied to: %RELEASE_APK%
dir /b "%RELEASE_APK%\*.apk" 2>nul
endlocal
