@echo off
REM NOCTURNE_OS — Full clean build: firmware (.pio) + server exe (dist/NocturneServer.exe).
REM Deletes old build artifacts first to avoid stale/corrupt builds.
setlocal
cd /d "%~dp0"

echo [BUILD] Clean...
if exist ".pio\build" rmdir /s /q ".pio\build"
if exist "build" rmdir /s /q "build"
if exist "dist" rmdir /s /q "dist"

echo [BUILD] Firmware (PlatformIO)...
pio run
if errorlevel 1 (
    echo [BUILD] Firmware failed.
    exit /b 1
)

echo [BUILD] Server exe (PyInstaller)...
python -m PyInstaller --onefile --noconsole --name NocturneServer src\monitor.py
if errorlevel 1 (
    echo [BUILD] PyInstaller failed.
    exit /b 1
)

echo [BUILD] Done. Firmware: .pio\build ; Exe: dist\NocturneServer.exe
