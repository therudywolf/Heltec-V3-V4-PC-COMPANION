@echo off
REM NOCTURNE_OS — One-Click: check Python, run setup_nocturne.py (deps, config, autostart, launch to tray)
setlocal
cd /d "%~dp0"

python --version >nul 2>&1
if errorlevel 1 (
    echo [NEURAL_LINK] Python not found. Install Python and add it to PATH.
    pause
    exit /b 1
)

python "%~dp0setup_nocturne.py"
pause
