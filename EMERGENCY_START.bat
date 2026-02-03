@echo off
REM NOCTURNE_OS — Fallback: run monitor in visible console (no tray, no setup). For debugging.
setlocal
cd /d "%~dp0"

python --version >nul 2>&1
if errorlevel 1 (
    echo Python not found. Install Python and add it to PATH.
    pause
    exit /b 1
)

python "%~dp0src\monitor.py" --console
pause
