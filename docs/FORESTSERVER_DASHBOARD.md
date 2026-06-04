# dashboard.example.com dashboard — what the server can actually query (#5)

This documents what is **real and verified** on `https://dashboard.example.com` as of
2026-06-04, what NOCTURNE_OS now wires into the device payload, and what was
deliberately deferred (with the exact endpoint/metric a future change would need).
Investigated live from the server side; nothing here is invented.

## What is actually on dashboard.example.com

The dashboard is a **custom self-hosted page** ("NODE DASHBOARD",
caddy + docker), NOT a raw Prometheus or Grafana instance. The standard
Prometheus / Alertmanager paths are **404**:

| Probe | Result |
|---|---|
| `GET /` | 200 (the dashboard HTML) |
| `GET /api/v1/query?query=up` | 404 |
| `GET /api/v1/label/__name__/values` | 404 |
| `GET /prometheus/api/v1/query?query=up` | 404 |
| `GET /api/v2/alerts` | 404 |
| `GET /metrics` | 404 |

So `forest_query_url` (which the firmware/forest_panel.py treat as a Prometheus
`/api/v1/query` proxy) is **not** satisfied by this host. The dashboard instead
feeds itself from one JSON endpoint:

### `GET /stats.json` — the real, working feed (HTTP 200, `application/json`)

The page polls this every few seconds. Verified shape (example values):

```json
{
  "server": {
    "up": "1", "cpu": "42.15", "ram": "48.66", "disk": "41.67", "load": "9.72",
    "uptime": "1520327.7",
    "containers": "46", "containers_up": "46",
    "backup_age_s": "57860.2", "backup_size_bytes": "649832067"
  },
  "pc": {
    "up": "1", "cpu": "24.39", "ram": "62.28", "disk": "86.54",
    "gpu_util": "28", "gpu_temp": "38",
    "lmstudio_up": "1", "docker_running": "7"
  },
  "router": {
    "up": "1", "load1": "5.12", "cpu_count": "4",
    "mem_used_kb": "142444", "mem_total_kb": "513828",
    "wifi_clients": "21", "nwg1_clients": "5", "conntrack": "367", "uptime": "320862"
  },
  "vpn": {
    "proton_active_gw": "de", "proton_handshake_age_s": "34", "proton_toggle_on": "1",
    "awg_peer_handshake_age_s": "87", "awg_peers": "5", "awg_peers_active": "4"
  },
  "alerts": "37",
  "ts": 1780593632
}
```

Note: **every number is a JSON string** (`"46"`, not `46`). The parser tolerates
strings, ints and floats.

### Docker / container facts (the heart of #5)

There are **two** independent Docker counts in the feed:

* `server.containers` / `server.containers_up` — the **server's** stack
  (currently **46 / 46**). Has both a *total* and a *running* count, and the
  dashboard flags "warn" when `containers_up < containers`. This is the only one
  that maps cleanly to the requested `{n: total, up: running}`.
* `pc.docker_running` — the **PC's** stack, the "Docker stack of 7 containers"
  from the task (currently **7**). This is a *running* count only — there is **no
  total** field for the PC, so it cannot fill `{n, up}` honestly on its own.

### LM Studio fact

* `pc.lmstudio_up` — `1` = online, `0` = offline. The dashboard renders ✓/✗.

## What NOCTURNE_OS now ships

### `dock` payload block (`{"n": int, "up": int}`)

Sourced from **`server.containers_up` / `server.containers`** via the new
`dashboard_stats_url` config key, parsed by the pure
`payload.parse_dashboard_stats()`. With the live feed this yields
`"dock": {"n": 46, "up": 46}`.

* `dashboard_stats_url` is a **new, separate** config key (NOT `forest_query_url`)
  precisely because this host is not a Prometheus proxy — overloading the forest
  key would have been a lie. Empty/unset ⇒ `dock` ships `{"n": 0, "up": 0}`.
* Poll cadence: `DASHBOARD_UPDATE_INTERVAL = 30 s` (same as forest/services), and
  it is also refreshed on the device's `cmd:status` request.
* `n == 0 && up == 0` should be read by the firmware as **"unknown / no data"**
  (the dashboard has never reported a zero-total stack). A genuinely all-stopped
  stack would still report `n > 0, up == 0`.

To enable, set in `config.json`:

```json
"dashboard_stats_url": "https://dashboard.example.com/stats.json"
```

### LM Studio status → `svc` block, now sourced from `pc.lmstudio_up`

LM Studio is surfaced through the **`services` probe** `svc` block (the
`lmstudio` entry in `config.example.json`). Its up/down status, however, now
comes from the dashboard feed's **`pc.lmstudio_up`** (`"1"` = up, `"0"` = down)
— the **same authoritative source the dashboard shows** — and **overrides** the
local `http://127.0.0.1:1234/v1/models` probe result.

Why the change: the local probe runs from the *server* host and could not always
reach LM Studio on the PC, so the device showed "LM Studio: down" while the
dashboard (reading `pc.lmstudio_up`) showed it up. Making `pc.lmstudio_up` the
source of truth removes that disagreement.

* The same `GET /stats.json` fetch that builds `dock` also extracts
  `pc.lmstudio_up` (`get_dashboard_block_async()` in `monitor.py`), parsed by the
  pure `payload.parse_lmstudio_up()`.
* `payload.merge_lmstudio_status()` then rewrites the `svc` entry whose
  `id == "lmstudio"`: `"1"` → `st="up"` (local-probe latency `ms` preserved),
  `"0"` → `st="down"` (`ms=-1`). The `svc.up` count is recomputed accordingly.
* **Precedence:** `pc.lmstudio_up` (when present) **>** local services probe.
  If `dashboard_stats_url` is unset/unreachable, or `pc.lmstudio_up` is
  missing/garbage, the value is **unknown** and the local probe result is kept
  as the fallback — no fabricated "up". (So enabling this requires
  `dashboard_stats_url` to be set; otherwise the `lmstudio` entry behaves exactly
  as the plain probe did.)
* No new payload key: this only changes the existing `svc.list[].st` (+ `ms`/`up`)
  for the `lmstudio` entry.

## Deferred (real, but intentionally not wired) + exactly what's needed

1. **PC Docker count as `{n, up}`** — deferred. `pc.docker_running` is a running
   count with **no total** in `/stats.json`. To ship the PC stack as `{n, up}`
   the dashboard would need to add e.g. `pc.docker_total` (total containers on
   the PC). Until then, only the server stack (which has both) populates `dock`.
   *Alternative already available:* the PC's running Docker count would also be
   directly measurable locally with `docker ps -q | wc -l` if the owner wants a
   PC-side number instead of the dashboard's.

2. ~~**Remote LM Studio (`pc.lmstudio_up`) as a distinct field**~~ — **DONE**
   (no longer deferred). Rather than a separate payload field, `pc.lmstudio_up`
   now drives the existing `svc` `lmstudio` entry directly (overriding the local
   probe — see "LM Studio status" above). This gives the device the dashboard's
   authoritative status *even when the local probe can't reach LM Studio*, with
   one source of truth (`pc.lmstudio_up`, probe = fallback).

3. **forest block from this host** — `forest_query_url` cannot point at
   dashboard.example.com (no `/api/v1/query`). The dashboard's `/stats.json` *does*
   carry per-node cpu/ram/disk for `server` / `pc` / `router`, so a future
   `forest`-from-stats.json adapter is feasible, but that is a larger change than
   #5 and is out of scope here.

## How to re-verify

```bash
curl -s https://dashboard.example.com/stats.json | python -m json.tool
```

Look for `server.containers` / `server.containers_up` (drive `dock`) and
`pc.lmstudio_up` / `pc.docker_running`.
