<#
.SYNOPSIS
  Nocturne LHM bridge - a tiny LibreHardwareMonitor web-server replacement.

.DESCRIPTION
  LibreHardwareMonitor's built-in remote web server uses HTTP.sys, which needs an
  admin URL reservation (netsh urlacl) and often refuses to bind. This bridge
  instead loads LibreHardwareMonitorLib.dll directly and serves the SAME
  /data.json shape the Nocturne PC server already parses - but on a plain
  System.Net.HttpListener bound to localhost, which needs NO url reservation.

  Drop-in: the PC server keeps pointing at http://localhost:8085/data.json
  (config "lhm_url"); no server code changes. Run this INSTEAD of the LHM GUI.

  MUST run under Windows PowerShell 5.1 (powershell.exe), NOT PowerShell 7 (pwsh):
  LHM 0.9.x is a .NET Framework build and calls a Mutex(...MutexSecurity)
  constructor that does not exist on .NET Core/5+, so Open() throws under pwsh.

  Run ELEVATED (admin) for full sensors - CPU package/core temps, VRM, super-IO
  fans and voltages come from a ring0 driver that needs admin. Without admin you
  still get GPU (NVML), storage SMART and some others. Use lhm_bridge.bat (which
  self-elevates) or the scheduled task for a no-prompt autostart.

.PARAMETER LhmDir
  Folder containing LibreHardwareMonitorLib.dll and its dependencies.

.PARAMETER Port
  TCP port to serve on (default 8086). NOT 8085: if the old LHM web server left a
  "http://+:8085/" HTTP.sys url reservation, raw sockets on 8085 fail with
  WSAEACCES. The PC server's config.json "lhm_url" must use the same port.

.PARAMETER Bind
  Host to bind. "localhost" (default) needs no urlacl. Only the local PC server
  reads this - the ESP32 never connects here - so localhost is correct.

.PARAMETER Once
  Diagnostic: update once, print the JSON to stdout, and exit (no server).

.PARAMETER LogFile
  Optional path to append status/errors to.
#>
param(
    [string]$LhmDir  = "C:\Users\rudywolf\LibreHardwareMonitor",
    [int]   $Port    = 8086,
    [string]$Bind    = "127.0.0.1",
    [switch]$Once,
    [string]$LogFile = ""
)

$ErrorActionPreference = "Stop"

function Write-Log([string]$msg) {
    $line = "{0} {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
    Write-Host $line
    if ($LogFile) { try { Add-Content -Path $LogFile -Value $line -Encoding UTF8 } catch {} }
}

# --- Load the LHM library + resolve its sibling dependencies from $LhmDir ------
$dll = Join-Path $LhmDir "LibreHardwareMonitorLib.dll"
if (-not (Test-Path $dll)) { Write-Log "FATAL: not found: $dll"; exit 2 }

$script:LhmDir = $LhmDir
$onResolve = [System.ResolveEventHandler] {
    param($s, $e)
    $name = ($e.Name -split ',')[0]
    $p = Join-Path $script:LhmDir "$name.dll"
    if (Test-Path $p) { return [System.Reflection.Assembly]::LoadFrom($p) }
    return $null
}
[System.AppDomain]::CurrentDomain.add_AssemblyResolve($onResolve)
Add-Type -Path $dll
Write-Log "Loaded LibreHardwareMonitorLib (PS $($PSVersionTable.PSVersion), CLR $([System.Environment]::Version))"

# --- Open the hardware monitor (all sensor groups) ----------------------------
$computer = New-Object LibreHardwareMonitor.Hardware.Computer
$computer.IsCpuEnabled         = $true
$computer.IsGpuEnabled         = $true
$computer.IsMemoryEnabled      = $true
$computer.IsMotherboardEnabled = $true
$computer.IsStorageEnabled     = $true
$computer.IsControllerEnabled  = $true
try {
    $computer.Open()
} catch {
    Write-Log "FATAL: Computer.Open() failed: $($_.Exception.Message)"
    Write-Log "Hint: run under powershell.exe (5.1), not pwsh; and as Administrator."
    exit 3
}

# Unit suffix per SensorType (cosmetic only - the server parses the leading
# number; comma/dot and any unit are tolerated).
$unitFor = @{
    "Temperature" = "C";  "Load" = "%";   "Clock" = "MHz"; "Fan" = "RPM";
    "Voltage" = "V";      "Power" = "W";   "Data" = "GB";   "SmallData" = "MB";
    "Control" = "%";      "Level" = "%";   "Current" = "A"; "Throughput" = "B/s";
    "Frequency" = "Hz";   "Energy" = "mWh"
}

function ConvertTo-JsonStringEscape([string]$s) {
    if ($null -eq $s) { return "" }
    return $s.Replace('\', '\\').Replace('"', '\"').Replace("`r", " ").Replace("`n", " ").Replace("`t", " ")
}

# Build the LHM /data.json-compatible payload as a flat leaf list under one root.
# The server's walk_sensors() recurses Children and reads SensorId/Type/Name/Value
# off each leaf, so depth beyond this is unnecessary.
function Build-DataJson {
    foreach ($hw in $computer.Hardware) {
        $hw.Update()
        foreach ($sh in $hw.SubHardware) { $sh.Update() }
    }
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append('{"Children":[{"Text":"Computer","Children":[')
    $first = $true
    foreach ($hw in $computer.Hardware) {
        $all = @($hw.Sensors)
        foreach ($sh in $hw.SubHardware) { $all += @($sh.Sensors) }
        foreach ($s in $all) {
            if ($null -eq $s.Value) { continue }
            $type = "$($s.SensorType)"
            $unit = $unitFor[$type]
            $num  = [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", [double]$s.Value)
            $val  = if ($unit) { "$num $unit" } else { $num }
            $sid  = ConvertTo-JsonStringEscape ("$($s.Identifier)")
            $name = ConvertTo-JsonStringEscape ("$($s.Name)")
            if (-not $first) { [void]$sb.Append(',') }
            $first = $false
            [void]$sb.Append('{"SensorId":"'); [void]$sb.Append($sid)
            [void]$sb.Append('","Type":"');    [void]$sb.Append($type)
            [void]$sb.Append('","Name":"');    [void]$sb.Append($name)
            [void]$sb.Append('","Value":"');   [void]$sb.Append($val)
            [void]$sb.Append('"}')
        }
    }
    [void]$sb.Append(']}]}')
    return $sb.ToString()
}

if ($Once) {
    Write-Log "Hardware: $(@($computer.Hardware | ForEach-Object { $_.Name }) -join '; ')"
    Build-DataJson
    $computer.Close()
    exit 0
}

# --- Serve over a RAW TcpListener (NOT HttpListener) ---------------------------
# HttpListener routes through HTTP.sys, which is exactly what broke LHM's own web
# server (needs a urlacl reservation; conflicts with stale +:8085 / IP:8085
# registrations - we saw PID 4 already holding 10.77.77.2:8085). A raw socket on
# 127.0.0.1 has none of that: no kernel reservation, no admin-for-bind, and it
# coexists with any HTTP.sys binding on a different local address. We speak just
# enough HTTP/1.1 for aiohttp's GET to succeed.
$ipAddr = if ($Bind -eq "localhost" -or $Bind -eq "127.0.0.1") {
    [System.Net.IPAddress]::Loopback
} elseif ($Bind -eq "0.0.0.0" -or $Bind -eq "*") {
    [System.Net.IPAddress]::Any
} else {
    [System.Net.IPAddress]::Parse($Bind)
}
$listener = New-Object System.Net.Sockets.TcpListener($ipAddr, $Port)
try {
    $listener.Start()
} catch {
    Write-Log "FATAL: TcpListener.Start() on ${Bind}:${Port} failed: $($_.Exception.Message)"
    $computer.Close()
    exit 4
}
Write-Log "Serving LHM-compatible data on http://${Bind}:${Port}/data.json  (Ctrl+C to stop)"

# Cache the built JSON briefly so rapid polls don't re-read the hardware every
# time (the server polls ~2x/sec; sensors don't change that fast). Keeps load low.
$cacheTtlMs = 1500
$cacheJson = ""
$cacheAt = [DateTime]::MinValue
$utf8 = New-Object System.Text.UTF8Encoding($false)

try {
    while ($true) {
        $client = $listener.AcceptTcpClient()
        try {
            $stream = $client.GetStream()
            # Read (and discard) the request line + headers. aiohttp sends a plain
            # GET; we serve the same body for any path. Read what's available.
            $reqBuf = New-Object byte[] 2048
            $client.ReceiveTimeout = 2000
            try { [void]$stream.Read($reqBuf, 0, $reqBuf.Length) } catch {}

            $age = ([DateTime]::Now - $cacheAt).TotalMilliseconds
            if ($age -ge $cacheTtlMs -or [string]::IsNullOrEmpty($cacheJson)) {
                $cacheJson = Build-DataJson
                $cacheAt = [DateTime]::Now
            }
            $body = $utf8.GetBytes($cacheJson)
            $header = "HTTP/1.1 200 OK`r`n" +
                      "Content-Type: application/json; charset=utf-8`r`n" +
                      "Content-Length: $($body.Length)`r`n" +
                      "Connection: close`r`n`r`n"
            $hbytes = [System.Text.Encoding]::ASCII.GetBytes($header)
            $stream.Write($hbytes, 0, $hbytes.Length)
            $stream.Write($body, 0, $body.Length)
            $stream.Flush()
        } catch {
            Write-Log "request error: $($_.Exception.Message)"
        } finally {
            try { $client.Close() } catch {}
        }
    }
} finally {
    try { $listener.Stop() } catch {}
    try { $computer.Close() } catch {}
    Write-Log "Stopped."
}
