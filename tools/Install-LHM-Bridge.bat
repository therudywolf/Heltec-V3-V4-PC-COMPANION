@echo off
REM ============================================================================
REM  Nocturne LHM bridge - ONE-CLICK full-data setup
REM
REM  Double-click this and click YES on the UAC prompt. That one admin step lets
REM  the bridge read CPU / VRM / motherboard temps and CPU/case fan RPM (these
REM  come from a ring0 driver Windows only allows with admin - the same reason
REM  LibreHardwareMonitor itself asks for admin every launch).
REM
REM  After this, the bridge auto-starts hidden at every logon with NO further
REM  prompts, and you can close the laggy LibreHardwareMonitor GUI for good.
REM
REM  Declining UAC is fine too: the bridge still runs with partial data
REM  (GPU temps/fans, CPU load, RAM, disk) via a no-admin Startup launcher.
REM ============================================================================
setlocal
set "PS1=%~dp0install-lhm-bridge.ps1"

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator rights for full sensor access...
    powershell -NoProfile -Command "Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','%PS1%'"
    exit /b
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%"
echo.
pause
