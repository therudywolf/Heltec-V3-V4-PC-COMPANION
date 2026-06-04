# Nocturne PC Server — setup

The PC-side service that streams hardware + Claude usage + alerts to the board
(`apps/pc` / `apps/multi`). Runs on Windows; talks to the board over TCP 8888.

## Quick start

```powershell
pip install -r requirements.txt
python monitor.py --console      # console + logs (debug)
pythonw monitor.py               # normal: tray icon, no console window
```

### Tray icon + autostart (recommended)
With `pystray`+`Pillow` installed, `pythonw monitor.py` runs as a **system-tray
icon** (green = running) with a right-click menu: **Add to startup** (HKCU Run, no
admin), **Restart Server**, **Close**. "Add to startup" makes it launch hidden at
every login. A desktop shortcut that runs `pythonw monitor.py` gives a one-click
start; keep `START_SERVER.bat` for a console/debug run.

Set the board's `PC_IP` (in firmware `include/secrets.h`) to this PC's LAN IP,
and `TCP_PORT` to match `config.json` `port` (default 8888).

---

## Hardware source — pick ONE (don't run more than you need)

The board shows CPU/GPU temps & loads, clocks, fans, VRM/board temps, disk
usage & temps, RAM, network. Where that data comes from is your choice:

| `source` | What it covers | Extra process? |
|----------|----------------|----------------|
| **`lhm`** (recommended) | **Everything** — temps, loads, clocks, fans, VRM, disk temps, RAM, GPU | LibreHardwareMonitor running with its web server |
| **`prometheus`** | CPU-load, RAM, disk capacity, network only (no temps/fans/GPU) | a running Grafana Agent / windows_exporter |

> **LHM alone is enough.** If you run LibreHardwareMonitor, set `source: "lhm"`
> and `gpu_via_nvidia_smi: false` — one source, full data, least overhead. Don't
> also enable the prometheus source; it's redundant and just polls more.

**`gpu_via_nvidia_smi`** (NVIDIA only): overlays live GPU temp/load/VRAM/power/
clock/fan from `nvidia-smi` (zero-install, ships with the driver). Useful with
`source: "prometheus"` (windows_exporter has no GPU). Turn it **off** when
`source: "lhm"` — LHM already has the GPU.

### Option A — LHM bridge (full data, recommended)
LibreHardwareMonitor's own web server uses Windows HTTP.sys, which needs an admin
URL reservation and frequently won't bind (stale `+:8085` reservations, wrong
bind IP, 503s). Instead of fighting it, run **`tools/lhm_bridge.ps1`** — it loads
`LibreHardwareMonitorLib.dll` directly and serves the same `/data.json` over a
plain socket on `127.0.0.1`, so there's no HTTP.sys, no urlacl, no GUI.

1. Get LibreHardwareMonitor (just the files; the GUI isn't needed). Note the
   folder with `LibreHardwareMonitorLib.dll`.
2. Double-click **`tools/Install-LHM-Bridge.bat`** and click **Yes** on UAC.
   That one admin step lets it read CPU/VRM/super-IO temps + CPU/case fan RPM
   (a ring0 driver Windows only allows with admin). It registers a hidden
   auto-start task and starts the bridge now.
   - Decline UAC and it still runs with **partial** data (GPU temps/fans, CPU
     load, clocks, RAM, disk) via a no-admin Startup launcher.
3. It serves on **port 8086**, not 8085: a leftover `http://+:8085/` HTTP.sys
   reservation poisons 8085 for raw sockets (WSAEACCES). `config.json` is already
   `"lhm_url": "http://127.0.0.1:8086/data.json"`.
4. Verify: open `http://127.0.0.1:8086/data.json`.
5. `config.json`: `"source": "lhm"`, `"gpu_via_nvidia_smi": false`.
6. You can now close the LibreHardwareMonitor GUI — the bridge replaces it.

Run under **Windows PowerShell 5.1** (`powershell.exe`), not PowerShell 7: LHM
0.9.x is a .NET Framework build whose `Open()` calls a `Mutex` overload absent
on .NET Core/5+. The bat/task already use 5.1.

### Option B — already running Grafana Agent / windows_exporter (no install)
If you already run a Grafana Agent with a `windows_exporter` integration (e.g.
for a remote Prometheus), reuse it — no extra monitoring load:
1. Find its windows_exporter metrics URL, e.g.
   `http://127.0.0.1:12345/integrations/windows_exporter/metrics`.
2. `config.json`: `"source": "prometheus"`, `"prometheus_url": "<that URL>"`,
   `"gpu_via_nvidia_smi": true` (for GPU, NVIDIA).
3. Note: **no CPU/GPU temps, no fans, no VRM** — windows_exporter doesn't expose
   them. Use Option A if you want those.

---

## Alerts — same ones your Telegram gets

1. **Local thresholds (always on, no network):** the device shows a RED ALERT
   when CPU/GPU temp or load crosses the `limits` in `config.json`, and a
   **Claude 80% reminder** when window/weekly usage reaches `claude_alert_pct`.
2. **Alertmanager — show the SAME alerts as Telegram.** Two ways; pick one:
   - **Poll (recommended, no inbound port):** set `alertmanager_url` to the
     Alertmanager **v2 API**, e.g.
     `https://monitoring.example.com/alertmanager/api/v2/alerts`. The server
     polls it every ~20 s and shows the firing alerts. Works even when the
     Alertmanager can't reach this PC — *we* call *it*. Active alerts map to
     firing; silenced/suppressed are hidden; resolved disappear on the next poll.
   - **Webhook (push):** set `alert_webhook_port` (e.g. 9099) and point your
     Alertmanager at `http://<this-pc>:9099/alert`. Needs the Alertmanager to be
     able to reach this PC (VPN / port-forward).
     ```yaml
     receivers:
       - name: nocturne-device
         webhook_configs:
           - url: http://<this-pc-ip>:9099/alert
             send_resolved: true
     ```
   Leave both empty to rely on local thresholds only.

---

## Forest panel — node-status scene

A compact CPU/RAM/disk status per monitored host. Populated by querying a
**Prometheus-compatible** `/api/v1/query` endpoint — set `forest_query_url`. A
Grafana **datasource proxy** works unauthenticated and read-only, e.g.
`https://monitoring.example.com/api/datasources/proxy/1/api/v1/query`.

The roster + per-node PromQL ship in `forest_panel.DEFAULT_NODES` (a Linux
`node_exporter` host and a Windows `windows_exporter` host). Override per node
from `config.json`:

```json
"forest_nodes": [
  { "id": "srv", "name": "Server",
    "cpu":  "100-(avg(rate(node_cpu_seconds_total{mode=\"idle\",instance=\"myserver\"}[2m]))*100)",
    "ram":  "100*(1-node_memory_MemAvailable_bytes{instance=\"myserver\"}/node_memory_MemTotal_bytes{instance=\"myserver\"})",
    "disk": "100*(1-node_filesystem_avail_bytes{instance=\"myserver\",mountpoint=\"/\"}/node_filesystem_size_bytes{instance=\"myserver\",mountpoint=\"/\"})" }
]
```

A node turns **warn** at ≥90% on any resource and **down** when its queries
return nothing. Empty `forest_query_url` → the scene shows `NO NODES`. Polled
every ~30 s. *Read-only — it only runs `query`, never writes.*

> Note for Windows hosts scraped via Grafana Agent: their scrape interval is
> longer, so the CPU `rate()` needs a wider window (`[5m]`, not `[2m]`).

---

## Claude usage gauge

Reads `~/.claude/stats-cache.json` (today's tokens). Claude exposes no official
quota %, so the gauge is **% of a configurable token budget**:
`claude_daily_budget` (window) and `claude_weekly_budget` (7-day). Tune to your
plan; `0` disables a gauge. The 80% reminder fires off these.

---

## config.json reference

| key | meaning |
|-----|---------|
| `host` / `port` | TCP bind (board connects here). `0.0.0.0:8888`. |
| `source` | `lhm` or `prometheus` (see above). |
| `lhm_url` | LHM web-server data endpoint. |
| `prometheus_url` | windows_exporter `/metrics` URL. |
| `gpu_via_nvidia_smi` | overlay GPU from nvidia-smi (NVIDIA). |
| `limits.cpu` / `limits.gpu` | temp/load alert thresholds. |
| `alert_webhook_port` | Alertmanager webhook receiver port (`0` = off). |
| `alertmanager_url` | Alertmanager **v2** `/api/v2/alerts` URL to poll (`""` = off). |
| `forest_query_url` | Prometheus `/api/v1/query` URL for the Forest panel (`""` = off). |
| `forest_nodes` | optional per-node roster + PromQL (defaults to `DEFAULT_NODES`). |
| `claude_daily_budget` / `claude_weekly_budget` | token budgets for the gauge. |
| `claude_alert_pct` | Claude reminder threshold (default 80). |
| `weather_city` | weather scene location. |

Logs: `nocturne.log`. Run tests from the repo root: `python -m pytest tests/`.
