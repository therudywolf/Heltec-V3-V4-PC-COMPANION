@echo off
cd /d "%~dp0"
if exist .env goto run
echo Copy .env.example to .env and edit if needed.
if exist .env.example copy .env.example .env
:run
python monitor.py
if errorlevel 1 pause
