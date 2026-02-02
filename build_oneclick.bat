@echo off
cd /d "%~dp0"

if not exist ".venv" (
  echo Creating .venv...
  python -m venv .venv
)

echo Activating venv...
call .venv\Scripts\activate.bat

echo Installing requirements...
pip install -r requirements.txt
pip install pyinstaller

echo Building monitor.exe...
pyinstaller --onefile --console --name monitor monitor.py

if exist "dist\monitor.exe" (
  copy /Y "dist\monitor.exe" "monitor.exe" >nul
  echo.
  echo Done. monitor.exe is in project root.
  echo Put .env next to monitor.exe and run. See AUTOSTART.md for startup.
) else (
  echo Build failed. Check errors above.
)

pause
