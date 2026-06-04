#!/usr/bin/env python3
"""
Service-status panel for NOCTURNE_OS.

Probes a roster of named services (HTTP or TCP) and turns the results into a
compact ``svc`` payload block the device renders as an up/down status list
(e.g. "LM Studio", "Monitoring", "Forestserver"). Configure via config.json
``services`` (see SETUP.md).

Pure / side-effect-free here (``service_status`` / ``build_service`` /
``build_svc_block``); the actual HTTP/TCP probing lives in monitor.py (async),
so these helpers are fully unit-testable without a network.

Wire shape (``svc`` block):
    {
      "n": 3,                       # service count
      "up": 2,                      # how many are reachable (up or warn)
      "list": [
        {"id":"lmstudio","name":"LM Studio","st":"up","ms":7},
        {"id":"monitoring","name":"Monitoring","st":"down","ms":-1},
        ...
      ]
    }
``st`` is "up" | "warn" | "down"; ``ms`` is round-trip latency in ms (-1 = n/a).
"""
from typing import Any, Dict, List, Optional

# Reachable but slower than this (ms) is flagged "warn" rather than "up".
SLOW_MS = 1500

EMPTY_SVC: Dict[str, Any] = {"n": 0, "up": 0, "list": []}

# Cap how many services we ship (device renders a short list; keep payload small).
MAX_SERVICES = 8


def service_status(reachable: bool, ms: int, slow_ms: int = SLOW_MS) -> str:
    """down if unreachable, warn if reachable-but-slow, else up."""
    if not reachable:
        return "down"
    if ms >= 0 and ms >= slow_ms:
        return "warn"
    return "up"


def build_service(svc_id: str, name: str, reachable: bool,
                  ms: Optional[float]) -> Dict[str, Any]:
    """Map one probe result to the compact wire entry. Never raises."""
    try:
        ims = int(round(float(ms))) if ms is not None else -1
    except (TypeError, ValueError):
        ims = -1
    if not reachable:
        ims = -1
    return {
        "id": str(svc_id)[:10],
        "name": str(name)[:16],
        "st": service_status(reachable, ims),
        "ms": ims,
    }


def build_svc_block(services: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Assemble the ``svc`` block from already-built wire entries.

    Returns a copy of EMPTY_SVC for an empty list. ``up`` counts reachable
    services (status "up" or "warn"); "down" is not counted.
    """
    wire = [s for s in (services or []) if isinstance(s, dict)][:MAX_SERVICES]
    if not wire:
        return dict(EMPTY_SVC)
    up = sum(1 for s in wire if s.get("st") in ("up", "warn"))
    return {"n": len(wire), "up": up, "list": wire}
