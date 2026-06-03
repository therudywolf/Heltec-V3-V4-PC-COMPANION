@echo off
REM One-click: register the Nocturne PC monitor to launch hidden at every
REM Windows login (HKCU Run, no admin). The server also self-registers on its
REM first run; this is here if you want to (re)enable it explicitly.
cd /d "%~dp0"
where pythonw >nul 2>nul
if %errorlevel%==0 (
  python monitor.py --enable-autostart
) else (
  python monitor.py --enable-autostart
)
echo.
echo The PC monitor will now start automatically at login.
echo (To undo: run  python monitor.py --disable-autostart  or use the tray menu.)
pause
