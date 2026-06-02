#!/usr/bin/env python3
"""
Forest panel aggregation for NOCTURNE_OS.

Duplicates the dashboard.example.com node dashboard onto the device: a compact status
view of several monitored nodes (e.g. Forestserver/Debian, PC/Windows,
Forestrouter/Keenetic). Each node is scraped from its own Prometheus / exporter
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


# Default node roster (matches the dashboard.example.com panel). The scrape URL is
# read by monitor.py; metric extraction per node type lives there. Kept as data
# so it's overridable from config.json ("forest_nodes") without code changes.
DEFAULT_NODES: List[Dict[str, str]] = [
    {"id": "srv", "name": "Forestserver", "type": "prometheus"},
    {"id": "pc", "name": "PC-Rudywolf", "type": "prometheus"},
    {"id": "rtr", "name": "Forestrouter", "type": "keenetic"},
]
