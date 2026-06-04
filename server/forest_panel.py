#!/usr/bin/env python3
"""
Forest panel aggregation for NOCTURNE_OS.

A compact node-status dashboard on the device: a status view of several monitored
nodes (e.g. a Linux server, a Windows PC, a router). Each node is scraped from its
own Prometheus / exporter
endpoint; this module turns the scraped metrics into a small ``forest`` payload
block the firmware renders as node-status scenes.

Pure and side-effect-free: :func:`build_node` maps one node's metric dict to the
compact wire shape, :func:`build_forest_block` assembles the list, and
:func:`node_status` derives the ●/— health dot. The HTTP scrape lives in
monitor.py (async); these helpers just transform already-fetched values, so they
are fully unit-testable without a network.

Wire shape (``forest`` block):
    {
      "n": 3,                       # node count
      "up": 2,                      # how many are "up"
      "nodes": [
        {"id":"srv","name":"Forestserver","st":"up",
         "cpu":12,"ram":47,"disk":63,"extra":"load 0.4"},
        ...
      ]
    }
``st`` is "up" | "warn" | "down". cpu/ram/disk are percent ints (-1 = n/a).
"""

from typing import Any, Dict, List, Optional

# Status thresholds (percent). A node is "warn" when any monitored resource is
# high, "down" when unreachable (no metrics), else "up".
WARN_CPU = 90
WARN_RAM = 90
WARN_DISK = 90

EMPTY_FOREST: Dict[str, Any] = {"n": 0, "up": 0, "nodes": []}


def _pct(value: Optional[float]) -> int:
    """Clamp a 0..100 float to an int percent; -1 for None/invalid."""
    if value is None:
        return -1
    try:
        v = int(round(float(value)))
    except (TypeError, ValueError):
        return -1
    return max(0, min(100, v))


def node_status(reachable: bool, cpu: int, ram: int, disk: int) -> str:
    """Derive a node's health: down if unreachable, warn if any resource high."""
    if not reachable:
        return "down"
    if (cpu >= 0 and cpu >= WARN_CPU) or (ram >= 0 and ram >= WARN_RAM) or \
       (disk >= 0 and disk >= WARN_DISK):
        return "warn"
    return "up"


def build_node(node_id: str, name: str, metrics: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    """Map one node's metric dict to the compact wire node.

    ``metrics`` keys (any may be absent → n/a): ``cpu``/``ram``/``disk`` (percent),
    ``extra`` (short free-text, e.g. "load 0.4" / "12 clients"). ``metrics`` is
    None when the node was unreachable → status "down", resources n/a.
    Never raises.
    """
    reachable = isinstance(metrics, dict) and len(metrics) > 0
    m = metrics if reachable else {}
    cpu = _pct(m.get("cpu"))
    ram = _pct(m.get("ram"))
    disk = _pct(m.get("disk"))
    extra = str(m.get("extra", ""))[:16]
    return {
        "id": str(node_id)[:8],
        "name": str(name)[:16],
        "st": node_status(reachable, cpu, ram, disk),
        "cpu": cpu,
        "ram": ram,
        "disk": disk,
        "extra": extra,
    }


def build_forest_block(nodes: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Assemble the ``forest`` block from a list of already-built wire nodes.

    Returns a copy of EMPTY_FOREST for an empty list. ``up`` counts nodes whose
    status is "up" or "warn" (i.e. reachable); "down" is not counted as up.
    """
    wire = [n for n in (nodes or []) if isinstance(n, dict)]
    up = sum(1 for n in wire if n.get("st") in ("up", "warn"))
    if not wire:
        return dict(EMPTY_FOREST)
    return {"n": len(wire), "up": up, "nodes": wire}


# Default node roster — GENERIC EXAMPLE. Put your REAL nodes in config.json
# "forest_nodes" (kept out of the repo). Each node has cpu/ram/disk PromQL run
# against forest_query_url (any Prometheus-compatible /api/v1/query, e.g. a
# Grafana datasource proxy). Example targets:
#   node_exporter     instance="myserver"
#   windows_exporter  instance="MY-PC"
DEFAULT_NODES: List[Dict[str, Any]] = [
    {
        "id": "srv", "name": "Server",
        "cpu": '100-(avg(rate(node_cpu_seconds_total{mode="idle",instance="myserver"}[2m]))*100)',
        "ram": '100*(1-node_memory_MemAvailable_bytes{instance="myserver"}/node_memory_MemTotal_bytes{instance="myserver"})',
        "disk": '100*(1-node_filesystem_avail_bytes{instance="myserver",mountpoint="/"}/node_filesystem_size_bytes{instance="myserver",mountpoint="/"})',
    },
    {
        "id": "pc", "name": "My PC",
        # windows_exporter often scrapes at a longer interval than node_exporter,
        # so the idle rate needs a wider [5m] window than [2m].
        "cpu": '100-(avg(rate(windows_cpu_time_total{mode="idle",instance="MY-PC"}[5m]))*100)',
        "ram": '100*(1-windows_os_physical_memory_free_bytes{instance="MY-PC"}/windows_cs_physical_memory_bytes{instance="MY-PC"})',
        "disk": '100*(1-windows_logical_disk_free_bytes{instance="MY-PC",volume="C:"}/windows_logical_disk_size_bytes{instance="MY-PC",volume="C:"})',
    },
]


def build_nodes_from_queries(node_defs, run_query) -> List[Dict[str, Any]]:
    """Build wire nodes by running each node's cpu/ram/disk PromQL.

    ``run_query`` is a callable(expr)->float|None (a single scalar from the first
    result series; None on miss/error). A node with all-None metrics is treated
    as unreachable ("down"). Pure except for the injected run_query. Never raises.
    """
    nodes: List[Dict[str, Any]] = []
    for d in node_defs or []:
        metrics: Dict[str, Any] = {}
        any_ok = False
        for key in ("cpu", "ram", "disk"):
            expr = d.get(key)
            if not expr:
                continue
            try:
                v = run_query(expr)
            except Exception:
                v = None
            if v is not None:
                metrics[key] = v
                any_ok = True
        nodes.append(build_node(d.get("id", "?"), d.get("name", "?"),
                                metrics if any_ok else None))
    return nodes
