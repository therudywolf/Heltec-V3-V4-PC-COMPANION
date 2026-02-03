@echo off
REM Start NOCTURNE_OS monitor (tray). Logs: nocturne.log
cd /d "%~dp0"
start "" pythonw "%~dp0src\monitor.py"
