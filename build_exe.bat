@echo off
cd /d "%~dp0"
pip install pyinstaller
pyinstaller --onefile --console --name monitor monitor.py
echo EXE: dist\monitor.exe
echo Copy dist\monitor.exe and .env to one folder, then add to Startup or Task Scheduler (see AUTOSTART.md).
