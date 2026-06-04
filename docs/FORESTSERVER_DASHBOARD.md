# Dashboard stats feed — `/stats.json` integration (#5)

This documents the shape of the **custom dashboard JSON feed** that NOCTURNE_OS
can wire into the device payload, and what was deliberately deferred (with the
exact endpoint/metric a future change would need). Values below are **example
placeholders**, not anyone's real telemetry. Point `dashboard_stats_url` at your
own feed (see `server/config.example.json`).

## Assumptions about the feed

The feed is a **custom self-hosted status page**, NOT a raw Prometheus or Grafana
instance — so the standard Prometheus / Alertmanager paths are typically absent:

| Probe | Typical result |
|---|---|
| `GET /` | 200 (the dashboard HTML) |
| `GET /api/v1/query?query=up` | 404 |
| `GET /api/v1/label/__name__/values` | 404 |
| `GET /api/v2/alerts` | 404 |
| `GET /metrics` | 404 |

So `forest_query_url` (which `forest_panel.py` treats as a Prometheus
`/api/v1/query` proxy) is **not** satisfied by such a host. The dashboard instead
feeds itself from one JSON endpoint:

### `GET /stats.json` — the feed (HTTP 200, `application/json`)

The page polls this every few seconds. Example shape (placeholder values):

```json
{
  "server": {
    "up": "1", "cpu": "30.0", "ram": "50.0", "disk": "40.0", "load": "1.00",
    "uptime": "100000.0",
    "containers": "12", "containers_up": "12",
    "backup_age_s": "3600.0", "backup_size_bytes": "100000000"
  },
  "pc": {
    "up": "1", "cpu": "20.0", "ram": "60.0", "disk": "70.0",
    "gpu_util": "15", "gpu_temp": "40",
    "lmstudio_up": "1", "docker_running": "3"
  },
  "router": {
    "up": "1", "load1": "1.00", "cpu_count": "4",
    "mem_used_kb": "100000", "mem_total_kb": "500000",
    "wifi_clients": "8", "nwg1_clients": "2", "conntrack": "200", "uptime": "100000"
  },
  "vpn": {
    "proton_active_gw": "xx", "proton_handshake_age_s": "30", "proton_toggle_on": "1",
    "awg_peer_handshake_age_s": "30", "awg_peers": "2", "awg_peers_active": "2"
  },
  "alerts": "0",
  "ts": 1700000000
}
```

Note: **every number is a JSON string** (`"12"`, not `12`). The parser tolerates
strings, ints and floats.

### Docker / container facts (the heart of #5)

There are **two** independent Docker counts in the feed:

* `server.containers` / `server.containers_up` — the **server's** stack. Has both
  a *total* and a *running* count, and the dashboard flags "warn" when
  `containers_up < containers`. This is the only one that maps cleanly to the
  requested `{n: total, up: running}`.
* `pc.docker_running` — the **PC's** stack: a *running* count only — there is **no
  total** field for the PC, so it cannot fill `{n, up}` honestly on its own.

### LM Studio fact

* `pc.lmstudio_up` — `1` = online, `0` = offline. The dashboard renders ✓/✗.

## What NOCTURNE_OS ships

### `dock` payload block (`{"n": int, "up": int}`)

Sourced from **`server.containers_up` / `server.containers`** via the
`dashboard_stats_url` config key, parsed by the pure
`payload.parse_dashboard_stats()`.

* `dashboard_stats_url` is a **separate** config key (NOT `forest_query_url`)
  because this host is not a Prometheus proxy. Empty/unset ⇒ `dock` ships
  `{"n": 0, "up": 0}`.
* Poll cadence: `DASHBOARD_UPDATE_INTERVAL = 30 s`, also refreshed on the device's
  `cmd:status` request.
* `n == 0 && up == 0` should be read by the firmware as **"unknown / no data"**.
  A genuinely all-stopped stack would still report `n > 0, up == 0`.

To enable, set in `config.json`:

```json
"dashboard_stats_url": "https://dashboard.example.com/stats.json"
```

### LM Studio status → `svc` block, sourced from `pc.lmstudio_up`

LM Studio is surfaced through the **`services` probe** `svc` block (the
`lmstudio` entry in `config.example.json`). Its up/down status comes from the
dashboard feed's **`pc.lmstudio_up`** (`"1"` = up, `"0"` = down) and **overrides**
the local `http://127.0.0.1:1234/v1/models` probe result.

Why: the local probe runs from the *server* host and may not reach LM Studio on
the PC, so the device could show "LM Studio: down" while the dashboard showed it
up. Making `pc.lmstudio_up` the source of truth removes that disagreement.

* The same `GET /stats.json` fetch that builds `dock` also extracts
  `pc.lmstudio_up` (`get_dashboard_block_async()` in `monitor.py`), parsed by the
  pure `payload.parse_lmstudio_up()`.
* `payload.merge_lmstudio_status()` rewrites the `svc` entry whose
  `id == "lmstudio"`: `"1"` → `st="up"` (local-probe latency `ms` preserved),
  `"0"` → `st="down"` (`ms=-1`). The `svc.up` count is recomputed.
* **Precedence:** `pc.lmstudio_up` (when present) **>** local services probe.
  If `dashboard_stats_url` is unset/unreachable, or `pc.lmstudio_up` is
  missing/garbage, the value is **unknown** and the local probe result is kept.

## Deferred (intentionally not wired) + exactly what's needed

1. **PC Docker count as `{n, up}`** — deferred. `pc.docker_running` is a running
   count with **no total** in `/stats.json`. To ship the PC stack as `{n, up}`
   the feed would need e.g. `pc.docker_total`. Until then, only the server stack
   (which has both) populates `dock`. *Alternative:* the PC's running count is
   directly measurable locally with `docker ps -q | wc -l`.

2. ~~**Remote LM Studio (`pc.lmstudio_up`) as a distinct field**~~ — **DONE.**
   `pc.lmstudio_up` drives the existing `svc` `lmstudio` entry directly
   (overriding the local probe), one source of truth (probe = fallback).

3. **forest block from this host** — `forest_query_url` cannot point at a
   stats-only host (no `/api/v1/query`). The feed's `/stats.json` *does* carry
   per-node cpu/ram/disk for `server` / `pc` / `router`, so a future
   `forest`-from-stats.json adapter is feasible, but larger than #5.

## How to re-verify

```bash
curl -s https://dashboard.example.com/stats.json | python -m json.tool
```

Look for `server.containers` / `server.containers_up` (drive `dock`) and
`pc.lmstudio_up` / `pc.docker_running`.
