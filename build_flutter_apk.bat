@echo off
REM Phase 4: Build Flutter APKs and copy to "release apk"
setlocal
set FLUTTER_DIR=%~dp0app\flutter
cd /d "%FLUTTER_DIR%"

echo [1/4] flutter clean
call flutter clean
if errorlevel 1 goto err

echo [2/4] flutter pub get
call flutter pub get
if errorlevel 1 goto err

echo [3/4] Generating launcher icons...
call dart run flutter_launcher_icons
if errorlevel 1 goto err

echo [4/4] Building release APKs (split-per-abi)...
call flutter build apk --split-per-abi
if errorlevel 1 goto err

set RELEASE_ROOT=%~dp0release apk
if not exist "%RELEASE_ROOT%" mkdir "%RELEASE_ROOT%"
echo Copying APKs to "release apk"...
copy /Y build\app\outputs\flutter-apk\*.apk "%RELEASE_ROOT%\"
echo Done. APKs in: %RELEASE_ROOT%
goto end
:err
echo Build failed.
exit /b 1
:end
endlocal
