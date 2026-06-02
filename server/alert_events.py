#!/usr/bin/env python3
"""
External event/alert ingestion for NOCTURNE_OS (Prometheus Alertmanager).

Alertmanager can POST its webhook JSON to the server; we distil the firing
alerts into a tiny, stable ``events`` payload block the device shows as a banner
(reusing the existing on-device alert/toast rendering). This is SEPARATE from the
local threshold RED ALERT (payload.evaluate_alert): that watches live hw values;
this surfaces named alerts from your monitoring stack (disk full on a server, VPN
down, backup stale, …).

Pure and side-effect-free: :class:`AlertState` holds the current firing set,
:func:`parse_alertmanager` turns a webhook body into normalized alerts, and
:func:`build_events_block` produces the compact wire shape. The HTTP receiver
(aiohttp) in monitor.py just calls ``state.ingest(parse_alertmanager(body))``.

Wire shape (``events`` block):
    {
      "n": 2,                       # number of firing alerts
      "top": "DiskFull",            # highest-severity alert name (banner text)
      "sev": "critical",            # its severity
      "list": ["DiskFull","VpnDown"]  # up to 4 names, for a list scene
    }
"""

import time
from typing import Any, Dict, List, Optional

# Severity ranking (higher = more urgent) for choosing the banner alert.
SEVERITY_RANK = {"critical": 3, "warning": 2, "info": 1, "none": 0}

EMPTY_EVENTS: Dict[str, Any] = {"n": 0, "top": "", "sev": "", "list": []}

# Firing alerts older than this (no refresh) are dropped, so a missed "resolved"
# webhook can't pin a stale banner forever.
DEFAULT_TTL_SEC = 600


def parse_alertmanager(body: Dict[str, Any]) -> List[Dict[str, str]]:
    """Normalize an Alertmanager webhook body to ``[{name, severity, status}]``.

    Accepts the standard v4 webhook (``{"alerts": [{labels, status, ...}]}``).
    Never raises: malformed entries are skipped, returns ``[]`` on bad input.
    """
    out: List[Dict[str, str]] = []
    if not isinstance(body, dict):
        return out
    alerts = body.get("alerts")
    if not isinstance(alerts, list):
        return out
    for a in alerts:
        if not isinstance(a, dict):
            continue
        labels = a.get("labels") or {}
        if not isinstance(labels, dict):
            labels = {}
        name = labels.get("alertname") or a.get("name") or "alert"
        severity = (labels.get("severity") or "none").lower()
        if severity not in SEVERITY_RANK:
            severity = "none"
        status = (a.get("status") or "firing").lower()
        out.append({"name": str(name)[:20], "severity": severity, "status": status})
    return out


def build_events_block(alerts: List[Dict[str, str]]) -> Dict[str, Any]:
    """Build the compact ``events`` block from a list of firing alerts.

    Picks the highest-severity alert as ``top``; ties broken by input order.
    Only firing alerts should be passed in (AlertState handles status). Returns
    a copy of EMPTY_EVENTS when the list is empty.
    """
    firing = [a for a in (alerts or []) if a.get("status", "firing") == "firing"]
    if not firing:
        return dict(EMPTY_EVENTS)
    top = max(firing, key=lambda a: SEVERITY_RANK.get(a.get("severity", "none"), 0))
    return {
        "n": len(firing),
        "top": top.get("name", "")[:20],
        "sev": top.get("severity", ""),
        "list": [a.get("name", "")[:20] for a in firing[:4]],
    }


class AlertState:
    """Holds the current firing alert set, updated by webhook ingests.

    Keyed by alert name. ``ingest`` applies a batch (firing adds/refreshes,
    resolved removes). ``snapshot`` drops entries older than the TTL and returns
    the ``events`` block. Time is injectable for tests (``now`` arg).
    """

    def __init__(self, ttl_sec: int = DEFAULT_TTL_SEC) -> None:
        self._firing: Dict[str, Dict[str, Any]] = {}
        self._ttl = ttl_sec

    def ingest(self, alerts: List[Dict[str, str]], now: Optional[float] = None) -> None:
        t = now if now is not None else time.time()
        for a in alerts or []:
            name = a.get("name")
            if not name:
                continue
            if a.get("status", "firing") == "firing":
                self._firing[name] = {
                    "name": name,
                    "severity": a.get("severity", "none"),
                    "status": "firing",
                    "ts": t,
                }
            else:
                self._firing.pop(name, None)

    def snapshot(self, now: Optional[float] = None) -> Dict[str, Any]:
        t = now if now is not None else time.time()
        # Drop expired entries.
        fresh = [v for v in self._firing.values() if t - v["ts"] <= self._ttl]
        # Rebuild map without expired (keep state tidy).
        self._firing = {v["name"]: v for v in fresh}
        return build_events_block(fresh)
