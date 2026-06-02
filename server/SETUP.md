# Nocturne PC Server — setup

The PC-side service that streams hardware + Claude usage + alerts to the board
(`apps/pc` / `apps/multi`). Runs on Windows; talks to the board over TCP 8888.

## Quick start

```powershell
pip install -r requirements.txt
python monitor.py --console      # console + logs; or run for the tray icon
```

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

### Option A — LibreHardwareMonitor (full data, recommended)
1. Download LibreHardwareMonitor, run it (as admin for full sensors).
2. Options → **Remote Web Server → Run** (port 8085). First time, the listener
   needs admin or a URL reservation:
   `netsh http add urlacl url=http://+:8085/ user=Everyone` (run once as admin).
3. Verify: open `http://localhost:8085/data.json`.
4. `config.json`: `"source": "lhm"`, `"gpu_via_nvidia_smi": false`.

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

## Alerts — two independent sources, both on

1. **Local thresholds (always on, no network):** the device shows a RED ALERT
   when CPU/GPU temp or load crosses the `limits` in `config.json`, and a
   **Claude 80% reminder** when window/weekly usage reaches `claude_alert_pct`.
2. **Prometheus Alertmanager webhook (optional, matches your Telegram):** set
   `alert_webhook_port` (e.g. 9099) and point your Alertmanager at
   `http://<this-pc>:9099/alert`. The device then shows the **same** firing
   alerts that go to Telegram.
   ```yaml
   # Alertmanager receiver
   receivers:
     - name: nocturne-device
       webhook_configs:
         - url: http://<this-pc-ip>:9099/alert
           send_resolved: true
   ```
   If your Prometheus/Alertmanager runs remotely (e.g. dashboard.example.com), it must
   be able to reach this PC's IP:port (VPN / port-forward). Otherwise run a small
   local Alertmanager, or stick with the local thresholds.

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
| `claude_daily_budget` / `claude_weekly_budget` | token budgets for the gauge. |
| `claude_alert_pct` | Claude reminder threshold (default 80). |
| `weather_city` | weather scene location. |

Logs: `nocturne.log`. Run tests from the repo root: `python -m pytest tests/`.
