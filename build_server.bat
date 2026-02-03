@echo off
REM NOCTURNE_OS — Единый скрипт сервера: очистка (в т.ч. .pio), установка зависимостей, сборка EXE.
setlocal
cd /d "%~dp0"
if not exist "src\monitor.py" (
    echo [SERVER] ERROR: src\monitor.py not found. Run from project root.
    exit /b 1
)

echo [SERVER] Clean...
if exist ".pio" rmdir /s /q ".pio"
if exist "build" rmdir /s /q "build"
if exist "dist" rmdir /s /q "dist"
for /d /r %%d in (__pycache__) do @if exist "%%d" rmdir /s /q "%%d" 2>nul
del /s /q *.pyc 2>nul
REM .spec не удаляем — PyInstaller перезапишет при сборке; del *.spec при отсутствии файла даёт ошибку
echo [SERVER] Clean done.

echo [SERVER] Python deps...
python -m pip install -r requirements.txt -q
if errorlevel 1 (
    echo [SERVER] pip failed.
    exit /b 1
)
echo [SERVER] Deps OK.

if not exist "config.json" (
    echo [SERVER] Creating config.json...
    echo {"host":"0.0.0.0","port":8090,"lhm_url":"http://localhost:8085/data.json","limits":{"gpu":80,"cpu":75},"weather_city":"Moscow"} > config.json
)

echo [SERVER] Build EXE...
python -m PyInstaller --onefile --noconsole --name NocturneServer src\monitor.py
if errorlevel 1 (
    echo [SERVER] PyInstaller failed.
    exit /b 1
)
if exist "config.json" copy /y "config.json" "dist\config.json" >nul

echo [SERVER] Done. Run: dist\NocturneServer.exe
endlocal
