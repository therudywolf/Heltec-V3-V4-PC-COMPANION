@echo off
REM NOCTURNE_OS — Main launcher: no console. Setup (deps+config) then start server in tray.
REM Use EMERGENCY_START.bat for console (debug). Autostart: tray menu after start.
setlocal
cd /d "%~dp0"

pythonw --version >nul 2>&1
if errorlevel 1 (
    python --version >nul 2>&1
    if errorlevel 1 (
        echo [NOCTURNE] Python not found. Install Python and add to PATH.
        pause
        exit /b 1
    )
)

REM Setup without console; then start monitor in tray and close this window
pythonw "%~dp0setup_nocturne.py" --no-launch
start "" pythonw "%~dp0src\monitor.py"
exit /b 0
