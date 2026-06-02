#!/usr/bin/env python3
"""
Poll a Prometheus Alertmanager v2 API for firing alerts (NOCTURNE_OS).

Alternative to the inbound webhook (alert_events.py): instead of Alertmanager
POSTing to us (needs it to reach this PC), WE poll its public API and surface the
SAME alerts that go to Telegram. Used for the dashboard.example.com monitoring stack
whose Alertmanager is reachable at
``https://dashboard.example.com/monitoring/alertmanager/api/v2/alerts``.

:func:`normalize_am_v2` is the pure parser (offline-testable); the fetch lives in
monitor.py. Reuses alert_events.build_events_block for the wire shape so the
device renders these exactly like webhook/local alerts.
"""

from typing import Any, Dict, List

import alert_events


def normalize_am_v2(payload: Any) -> List[Dict[str, str]]:
    """Normalize Alertmanager v2 ``/api/v2/alerts`` JSON to the common shape.

    v2 returns a top-level list of alerts, each with ``labels`` and a
    ``status.state`` of "active"|"suppressed"|"unprocessed". We map active ->
    firing and (suppressed/resolved) -> resolved, so silenced alerts don't show.
    Never raises; returns [] on bad input.
    """
    out: List[Dict[str, str]] = []
    if not isinstance(payload, list):
        return out
    for a in payload:
        if not isinstance(a, dict):
            continue
        labels = a.get("labels") or {}
        if not isinstance(labels, dict):
            labels = {}
        name = labels.get("alertname") or "alert"
        severity = (labels.get("severity") or "none").lower()
        if severity not in alert_events.SEVERITY_RANK:
            severity = "none"
        state = ((a.get("status") or {}).get("state") or "active").lower()
        status = "firing" if state == "active" else "resolved"
        out.append({"name": str(name)[:20], "severity": severity, "status": status})
    return out


def build_block(payload: Any) -> Dict[str, Any]:
    """Alertmanager v2 JSON -> the compact ``events`` block (firing only)."""
    return alert_events.build_events_block(normalize_am_v2(payload))
