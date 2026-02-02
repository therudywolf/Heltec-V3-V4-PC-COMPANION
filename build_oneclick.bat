@echo off
chcp 65001 >nul
cd /d "%~dp0"
setlocal

:: Один скрипт: venv + все зависимости + сборка EXE в корень проекта.
:: Требуется: Python 3.10+ в PATH.

echo [1/6] Проверка Python...
python --version 2>nul
if errorlevel 1 (
    echo Ошибка: Python не найден. Установи Python 3.10+ и добавь в PATH.
    goto :err
)

echo.
echo [2/6] Виртуальное окружение...
if not exist ".venv" (
    python -m venv .venv
    if errorlevel 1 (
        echo Ошибка: не удалось создать .venv
        goto :err
    )
)

call .venv\Scripts\activate.bat
if errorlevel 1 (
    echo Ошибка: не удалось активировать .venv
    goto :err
)

echo.
echo [3/6] Установка зависимостей (requirements.txt)...
pip install --upgrade pip -q
pip install -r requirements.txt
if errorlevel 1 (
    echo Ошибка: pip install -r requirements.txt
    goto :err
)

echo.
echo [4/6] Установка PyInstaller...
pip install pyinstaller
if errorlevel 1 (
    echo Ошибка: не удалось установить PyInstaller
    goto :err
)

echo.
echo [5/6] Сборка monitor.exe (monitor.spec, без консоли, иконка в трее)...
pyinstaller --noconfirm monitor.spec
if errorlevel 1 (
    echo Ошибка: сборка не удалась. Проверь вывод выше.
    goto :err
)

echo.
echo [6/6] Копирование EXE в корень проекта...
if not exist "dist\monitor.exe" (
    echo Ошибка: dist\monitor.exe не найден после сборки.
    goto :err
)
copy /Y "dist\monitor.exe" "monitor.exe" >nul
echo.
echo Готово. monitor.exe — в корне проекта.
echo Положи рядом .env (скопируй из .env.example при необходимости) и запускай.
echo EXE работает без окна консоли, только иконка в трее; выход — правый клик по иконке ^> Выход.
echo.
goto :ok

:err
echo.
echo Сборка прервана. Исправь ошибки и запусти build_oneclick.bat снова.
pause
exit /b 1

:ok
pause
exit /b 0
