<#
.SYNOPSIS
  Install the Nocturne LHM bridge as a per-logon scheduled task.

.DESCRIPTION
  Registers a Scheduled Task that runs lhm_bridge.ps1 under Windows PowerShell 5.1
  at every logon (hidden, auto-restart on failure), and starts it now. This is the
  clean "set up once" replacement for the laggy LHM GUI + its broken HTTP.sys web
  server. The PC server reads http://127.0.0.1:<port>/data.json - no code changes.

  Privilege degrades gracefully:
    * Run AS ADMIN  -> task runs with HIGHEST privileges -> FULL sensors, incl.
      CPU/VRM/super-IO temps and CPU/case fan RPM (these need a ring0 driver).
    * Run normally  -> task runs LIMITED (no UAC) -> still GPU temps/fans, CPU
      loads, clocks, RAM and disk usage. Re-run as admin anytime to upgrade to
      full data; it just re-registers the same task.

  Easiest: double-click lhm_bridge.bat (self-elevates for full data). Or run this
  directly; right-click -> "Run as administrator" for the CPU/VRM temps.

.PARAMETER BridgeSource   Path to lhm_bridge.ps1 (defaults to alongside this file).
.PARAMETER InstallDir     Stable folder to copy the bridge into (and log to).
.PARAMETER LhmDir         Folder with LibreHardwareMonitorLib.dll.
.PARAMETER Port           Port to serve on (8086; 8085 is poisoned by a urlacl).
.PARAMETER TaskName       Scheduled Task name.
#>
param(
    [string]$BridgeSource = (Join-Path $PSScriptRoot "lhm_bridge.ps1"),
    [string]$InstallDir   = "$env:USERPROFILE\NocturneServer",
    [string]$LhmDir       = "$env:USERPROFILE\NocturneServer\lhm",
    [int]   $Port         = 8086,   # NOT 8085: a stale http://+:8085/ urlacl poisons it
    [string]$TaskName     = "NocturneLhmBridge"
)

$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not (Test-Path $InstallDir)) { New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null }
$transcript = Join-Path $InstallDir "install-debug.log"
try { Start-Transcript -Path $transcript -Force | Out-Null } catch {}
trap {
    Write-Host "ERROR: $($_.Exception.Message)"
    Write-Host $_.ScriptStackTrace
    try { Stop-Transcript | Out-Null } catch {}
    Start-Sleep -Seconds 8
    exit 1
}

$mode = if ($isAdmin) { "ADMIN (full sensors)" } else { "LIMITED (no CPU/VRM temps - re-run as admin for those)" }
Write-Host "== Installing Nocturne LHM bridge task =="
Write-Host "  privilege: $mode"
Write-Host "  bridge src: $BridgeSource (exists: $(Test-Path $BridgeSource))"

# 1. Copy the bridge to a stable location (so the task survives repo moves).
#    Skip if already there (running this from InstallDir => source == dest).
$bridge = Join-Path $InstallDir "lhm_bridge.ps1"
$srcFull = (Resolve-Path -LiteralPath $BridgeSource -ErrorAction SilentlyContinue).Path
if ($srcFull -and ($srcFull -ne (Join-Path $InstallDir "lhm_bridge.ps1"))) {
    Copy-Item -Path $BridgeSource -Destination $bridge -Force
    Write-Host "  bridge -> $bridge"
} else {
    Write-Host "  bridge already in place: $bridge"
}
$logFile = Join-Path $InstallDir "lhm_bridge.log"

# 2a. Stop any interim/old bridge instance so the task can bind the port.
Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" |
    Where-Object { $_.CommandLine -like '*lhm_bridge.ps1*' } |
    ForEach-Object {
        Write-Host "  stopping existing bridge (PID $($_.ProcessId))"
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    }

# 2b. Stop the LHM GUI (single owner of the super-IO chip; running both risks
#     garbled super-IO readings). Only meaningful when we'll read it ourselves.
if ($isAdmin) {
    $gui = Get-Process LibreHardwareMonitor -ErrorAction SilentlyContinue
    if ($gui) {
        Write-Host "  stopping LibreHardwareMonitor.exe (PID $($gui.Id)) - the bridge replaces it"
        $gui | Stop-Process -Force
        Start-Sleep -Milliseconds 800
    }
}

# 3. Set up autostart. ADMIN -> a Highest-privilege Scheduled Task (full data).
#    NON-ADMIN -> a Startup-folder launcher (Task Scheduler refuses non-elevated
#    writes; the Startup folder is user-writable and gives partial data with no
#    UAC). The two are mutually exclusive: each path removes the other's artifact.
$ps51 = Join-Path $env:WINDIR "System32\WindowsPowerShell\v1.0\powershell.exe"
$argline = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$bridge`" -LhmDir `"$LhmDir`" -Port $Port -LogFile `"$logFile`""
$startupVbs = Join-Path ([Environment]::GetFolderPath('Startup')) "NocturneLhmBridge.vbs"

if ($isAdmin) {
    # Remove the non-elevated Startup launcher so we don't run two bridges.
    if (Test-Path $startupVbs) { Remove-Item $startupVbs -Force; Write-Host "  removed Startup launcher (task supersedes it)" }

    $action  = New-ScheduledTaskAction -Execute $ps51 -Argument $argline
    $trigger = New-ScheduledTaskTrigger -AtLogOn
    $principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" `
        -LogonType Interactive -RunLevel Highest
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
        -StartWhenAvailable -ExecutionTimeLimit ([TimeSpan]::Zero) `
        -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)
    Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
        -Principal $principal -Settings $settings -Force | Out-Null
    Write-Host "  task '$TaskName' registered (AtLogon, RunLevel=Highest, hidden)"
    Start-ScheduledTask -TaskName $TaskName
    Write-Host "  task started; waiting for the listener..."
} else {
    # Drop a hidden-window VBS launcher in the Startup folder (no UAC, persists).
    # VBS string literals escape a double-quote by doubling it, so double every "
    # in the command, then wrap the whole command in quotes for WshShell.Run.
    $cmd = "$ps51 $argline"
    $cmdEsc = $cmd -replace '"', '""'
    $vbs = 'CreateObject("WScript.Shell").Run "' + $cmdEsc + '", 0, False'
    Set-Content -Path $startupVbs -Value $vbs -Encoding ASCII
    Write-Host "  Startup launcher -> $startupVbs"
    # Start the bridge now (detached, hidden) for this session.
    Start-Process $ps51 -ArgumentList $argline -WindowStyle Hidden | Out-Null
    Write-Host "  bridge started (limited mode); waiting for the listener..."
}
$ok = $false
for ($i = 0; $i -lt 12; $i++) {
    Start-Sleep -Seconds 1
    try {
        $r = Invoke-RestMethod -Uri ("http://127.0.0.1:{0}/data.json" -f $Port) -TimeoutSec 3
        if ($r) { $ok = $true; break }
    } catch {}
}
if ($ok) {
    $leaves = $r.Children[0].Children
    $cpu = $leaves | Where-Object { $_.SensorId -like "*cpu*temperature*" -and $_.Value -notlike "0 *" } | Select-Object -First 1
    Write-Host "  OK: serving $($leaves.Count) sensors on 127.0.0.1:$Port"
    if ($cpu) { Write-Host "  CPU temp: $($cpu.Name) = $($cpu.Value)  (elevated read working)" }
    elseif ($isAdmin) { Write-Host "  NOTE: CPU temp still 0 even as admin - check the ring0 driver." }
    else { Write-Host "  NOTE: CPU/VRM temps are 0 (limited mode). Re-run as admin for those." }
    Write-Host ""
    Write-Host "  Point the PC server at:  http://127.0.0.1:$Port/data.json"
    Write-Host "  (config.json lhm_url is already set to this.)"
} else {
    Write-Host "  ERROR: no response on 127.0.0.1:$Port - see $logFile"
}
Write-Host "== Done =="
try { Stop-Transcript | Out-Null } catch {}
