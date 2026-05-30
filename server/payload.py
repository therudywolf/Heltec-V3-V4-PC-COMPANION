#!/usr/bin/env python3
"""
ESP32 payload-building helpers for NOCTURNE_OS.

These are the pure, testable pieces of ``monitor.build_payload``: HDD-slot
normalisation with optional psutil capacity fallback, the RED-ALERT
threshold/hysteresis state machine, and the change-detection snapshot.

``monitor.build_payload`` keeps its public signature and still owns the
module-level ``_last_alert`` / ``_weather_first_ok`` / ``_config`` state; it calls
these helpers and assigns results back. Threshold and hysteresis constants are
passed in (as dataclass-like dicts) so this module imports no project globals.
"""

from typing import Any, Callable, Dict, List, Optional, Tuple

DRIVE_LETTERS: Tuple[str, ...] = ("C", "D", "E", "F")


def normalize_hdd(
    hw_hdd: List[Dict[str, Any]],
    fallback_enabled: bool,
    get_fallback_disks: Optional[Callable[[], List[Dict[str, Any]]]] = None,
) -> List[Dict[str, Any]]:
    """Produce the 4-slot ``hdd`` list for the payload.

    Takes the raw ``hw["hdd"]`` list and, when no LHM capacity is present and
    ``fallback_enabled`` is true, fills capacity from psutil via
    ``get_fallback_disks`` (a zero-arg callable returning the same 4-slot shape).
    Two fallback modes preserve the original behaviour exactly:

    * LHM gave per-device temps but no capacity -> merge psutil capacity into
      slots that lack it, keeping the LHM temperature.
    * LHM gave neither -> replace the whole list with the psutil result.

    Always returns exactly four normalised
    ``{"n", "u", "tot", "t"}`` entries (used/total rounded to 0.1, temp int).
    """
    raw_hdd = [dict(e) for e in (hw_hdd or [])[:4]]
    while len(raw_hdd) < 4:
        raw_hdd.append({"n": DRIVE_LETTERS[len(raw_hdd)], "u": 0.0, "tot": 0.0, "t": 0})

    hdd_has_capacity = any(
        (e.get("tot") or 0) > 0 or (e.get("u") or 0) > 0 for e in raw_hdd[:4]
    )
    hdd_has_temp = any((e.get("t") or 0) > 0 for e in raw_hdd[:4])

    if not hdd_has_capacity and fallback_enabled and get_fallback_disks is not None:
        if hdd_has_temp:
            psutil_disks = get_fallback_disks()
            for i in range(min(4, len(raw_hdd), len(psutil_disks))):
                if (raw_hdd[i].get("tot") or 0) == 0 and (raw_hdd[i].get("u") or 0) == 0:
                    raw_hdd[i] = {
                        "n": raw_hdd[i].get("n") or psutil_disks[i]["n"],
                        "u": psutil_disks[i]["u"],
                        "tot": psutil_disks[i]["tot"],
                        "t": raw_hdd[i].get("t", 0),
                    }
        else:
            raw_hdd = get_fallback_disks()

    hdd_list: List[Dict[str, Any]] = []
    for i in range(4):
        if i < len(raw_hdd):
            e = raw_hdd[i]
            n = e.get("n") or DRIVE_LETTERS[i]
            hdd_list.append({
                "n": n if isinstance(n, str) else DRIVE_LETTERS[i],
                "u": round(float(e.get("u", 0.0)), 1),
                "tot": round(float(e.get("tot", 0.0)), 1),
                "t": int(e.get("t", 0)),
            })
        else:
            hdd_list.append({"n": DRIVE_LETTERS[i], "u": 0.0, "tot": 0.0, "t": 0})
    return hdd_list


def evaluate_alert(
    metrics: Dict[str, float],
    prev_alert: Tuple[Optional[str], Optional[str]],
    thresholds: Dict[str, int],
    hysteresis: Dict[str, int],
) -> Tuple[Dict[str, str], Tuple[Optional[str], Optional[str]]]:
    """Run the RED-ALERT threshold/hysteresis state machine.

    ``metrics`` provides ``ct``/``gt``/``cl``/``gl``/``gv`` (ints) and ``ram`` (the
    used-GB float). ``thresholds`` keys: ``cpu_temp``, ``gpu_temp``, ``cpu_load``,
    ``gpu_load``, ``vram_load``, ``ram_gb``. ``hysteresis`` keys: ``cpu_temp``,
    ``gpu_temp``, ``load``, ``ram_gb``.

    Priority order (CPU temp > GPU temp > CPU load > GPU load > VRAM > RAM)
    matches the original. An active alert is only cleared once its driving metric
    drops below ``threshold - hysteresis``.

    Returns ``(fields, new_alert)`` where ``fields`` is the
    ``{"alert", "target_screen", "alert_metric"}`` dict to merge into the payload
    and ``new_alert`` is the updated ``(target, metric)`` state to store back.
    """
    ct = metrics["ct"]
    gt = metrics["gt"]
    cl = metrics["cl"]
    gl = metrics["gl"]
    gv = metrics["gv"]
    ram = metrics["ram"]

    triggered: Optional[Tuple[str, str]] = None
    if ct >= thresholds["cpu_temp"]:
        triggered = ("CPU", "ct")
    elif gt >= thresholds["gpu_temp"]:
        triggered = ("GPU", "gt")
    elif cl >= thresholds["cpu_load"]:
        triggered = ("CPU", "cl")
    elif gl >= thresholds["gpu_load"]:
        triggered = ("GPU", "gl")
    elif gv >= thresholds["vram_load"]:
        triggered = ("GPU", "gv")
    elif ram >= thresholds["ram_gb"]:
        triggered = ("RAM", "ram")

    if triggered:
        return (
            {"alert": "CRITICAL", "target_screen": triggered[0], "alert_metric": triggered[1]},
            triggered,
        )

    cur_alert = prev_alert
    if cur_alert:
        _target, metric = cur_alert
        if metric == "ct":
            clear = ct < thresholds["cpu_temp"] - hysteresis["cpu_temp"]
        elif metric == "gt":
            clear = gt < thresholds["gpu_temp"] - hysteresis["gpu_temp"]
        elif metric == "cl":
            clear = cl < thresholds["cpu_load"] - hysteresis["load"]
        elif metric == "gl":
            clear = gl < thresholds["gpu_load"] - hysteresis["load"]
        elif metric == "gv":
            clear = gv < thresholds["vram_load"] - hysteresis["load"]
        elif metric == "ram":
            clear = ram < thresholds["ram_gb"] - hysteresis["ram_gb"]
        else:
            clear = True
        if clear:
            cur_alert = (None, None)

    if not cur_alert[0]:
        return ({"alert": "", "target_screen": "", "alert_metric": ""}, (None, None))
    return (
        {"alert": "CRITICAL", "target_screen": cur_alert[0], "alert_metric": cur_alert[1]},
        cur_alert,
    )


def payload_snapshot(p: Dict) -> Tuple:
    """Return the change-detection tuple (ct, gt, cl, gl, nd, nu, ru, ra).

    Used by ``should_send_payload`` to decide whether a new payload differs
    enough from the last-sent one to warrant transmission.
    """
    return (
        p.get("ct", 0), p.get("gt", 0), p.get("cl", 0), p.get("gl", 0),
        p.get("nd", 0), p.get("nu", 0), p.get("ru", 0), p.get("ra", 0),
    )
