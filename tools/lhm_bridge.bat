@echo off
REM ============================================================================
REM  Nocturne LHM bridge — self-elevating launcher (manual run / testing)
REM
REM  Double-click to run the bridge elevated (UAC prompt). It loads
REM  LibreHardwareMonitorLib.dll and serves an LHM-compatible /data.json on
REM  http://localhost:8085/ so the Nocturne PC server gets full hardware data
REM  (CPU/GPU temps, fans, VRM, clocks, disks) WITHOUT the LHM GUI or an HTTP.sys
REM  url reservation.
REM
REM  For a no-prompt autostart at logon, run install-lhm-bridge.ps1 once instead.
REM  Run under Windows PowerShell 5.1 (powershell.exe) — NOT pwsh.
REM ============================================================================
setlocal
set "SCRIPT=%~dp0lhm_bridge.ps1"

REM Re-launch elevated if not already running as admin.
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator rights...
    powershell.exe -NoProfile -Command "Start-Process powershell.exe -Verb RunAs -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','%SCRIPT%'"
    exit /b
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%"
