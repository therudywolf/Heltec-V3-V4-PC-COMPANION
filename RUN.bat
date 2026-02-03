@echo off
REM NOCTURNE_OS — Main launcher: install deps + ensure config, then start server in tray.
REM Use EMERGENCY_START.bat for console (debug). Autostart: tray menu after start.
setlocal
cd /d "%~dp0"

python --version >nul 2>&1
if errorlevel 1 (
    echo [NOCTURNE] Python not found. Install Python and add to PATH.
    pause
    exit /b 1
)

REM One-time: install deps and create config if missing
python "%~dp0setup_nocturne.py" --no-launch
pythonw "%~dp0src\monitor.py"
