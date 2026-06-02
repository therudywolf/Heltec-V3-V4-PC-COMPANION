#!/usr/bin/env python3
"""
Prometheus / windows_exporter telemetry source for NOCTURNE_OS.

An OPTIONAL, additive source. The device's hot screens (CPU/GPU temps, GPU load,
fan RPM, clocks, power) come from LibreHardwareMonitor — windows_exporter does
NOT expose those. So this module fills only the metrics Prometheus does have well
(CPU load, RAM used/total, disk capacity, network rates) into the same short-key
``hw`` dict LHM produces, and ``monitor.py`` MERGES it (Prometheus fills gaps;
LHM keeps temps/GPU/fans). Selected via ``config.json`` ``"source"``:

    "lhm"        -> LHM only (default, current behaviour)
    "prometheus" -> Prometheus for load/ram/disk/net, LHM still for temps/GPU
    (anything else falls back to "lhm")

Two scrape shapes are supported:

* **windows_exporter** text exposition (``/metrics`` on :9182) — parsed here with
  a tiny, dependency-free line parser (:func:`parse_exposition`).
* **Prometheus instant query API** (``/api/v1/query``) — JSON; map with
  :func:`map_query_results`.

Everything here is pure / side-effect-free and never raises on bad input: the
HTTP fetch lives in monitor.py (async), these helpers just transform text/JSON.
"""

from typing import Dict, List, Optional, Tuple

# windows_exporter metric names -> the contribution they make. We keep this as
# data so it is trivially testable and easy to extend.
#
# Notes on units:
#  * windows_cpu_processor_utility_total / windows_cs_* are counters or bytes;
#    load is derived from idle time, RAM from available vs total bytes.
#  * We compute CPU load as 100 - idle%, RAM used = total - available.

CPU_IDLE_METRIC = "windows_cpu_time_total"           # mode="idle" is the idle slice
CPU_TIME_MODE_LABEL = 'mode="idle"'
MEM_AVAIL_METRIC = "windows_os_physical_memory_free_bytes"
MEM_TOTAL_METRIC = "windows_cs_physical_memory_bytes"
NET_RECV_METRIC = "windows_net_bytes_received_total"
NET_SENT_METRIC = "windows_net_bytes_sent_total"
LOGICAL_FREE_METRIC = "windows_logical_disk_free_bytes"
LOGICAL_SIZE_METRIC = "windows_logical_disk_size_bytes"

BYTES_PER_GB = 1024.0 ** 3


def parse_exposition(text: str) -> List[Tuple[str, Dict[str, str], float]]:
    """Parse Prometheus text exposition format into ``(name, labels, value)``.

    Minimal and forgiving: skips ``#`` comment/HELP/TYPE lines and blank lines,
    tolerates missing labels, ignores unparseable values. Not a full parser
    (no exemplars/timestamps) — enough for windows_exporter scrapes.
    """
    out: List[Tuple[str, Dict[str, str], float]] = []
    if not text:
        return out
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        # Split metric{labels} value  [timestamp]
        brace = line.find("{")
        try:
            if brace >= 0:
                name = line[:brace]
                close = line.rfind("}")
                if close < 0:
                    continue
                label_str = line[brace + 1:close]
                rest = line[close + 1:].strip()
                labels = _parse_labels(label_str)
            else:
                parts = line.split()
                if len(parts) < 2:
                    continue
                name = parts[0]
                labels = {}
                rest = parts[1]
            value_token = rest.split()[0] if rest else ""
            value = float(value_token)
        except (ValueError, IndexError):
            continue
        out.append((name, labels, value))
    return out


def _parse_labels(label_str: str) -> Dict[str, str]:
    """Parse ``a="b",c="d"`` into a dict. Tolerant of empty input."""
    labels: Dict[str, str] = {}
    if not label_str:
        return labels
    # Split on commas that separate key="value" pairs. Values can't contain a
    # literal unescaped comma in windows_exporter, so a simple split is safe.
    for pair in label_str.split(","):
        eq = pair.find("=")
        if eq < 0:
            continue
        k = pair[:eq].strip()
        v = pair[eq + 1:].strip().strip('"')
        if k:
            labels[k] = v
    return labels


def _sum(metrics, name: str, label_filter: Optional[Tuple[str, str]] = None) -> float:
    total = 0.0
    for n, labels, val in metrics:
        if n != name:
            continue
        if label_filter and labels.get(label_filter[0]) != label_filter[1]:
            continue
        total += val
    return total


def build_hw_from_exposition(text: str, drive_letters=("C", "D", "E", "F")) -> Dict:
    """Map a windows_exporter scrape to the subset of ``hw`` keys we can fill.

    Returns a dict with any of: ``cl`` (CPU load %), ``ru``/``ra`` (RAM used/total
    GB), ``hdd`` (capacity-only slots). Network counters are returned under
    ``_net_counters`` (raw cumulative bytes) so monitor.py can run them through
    the same delta-rate logic it uses for psutil. Missing inputs are simply
    omitted (caller merges, so absent keys keep the LHM value).
    """
    metrics = parse_exposition(text)
    hw: Dict = {}

    # RAM: used = total - free, in GB.
    mem_total = _sum(metrics, MEM_TOTAL_METRIC)
    mem_free = _sum(metrics, MEM_AVAIL_METRIC)
    if mem_total > 0:
        used = max(0.0, mem_total - mem_free)
        hw["ru"] = round(used / BYTES_PER_GB, 1)
        hw["ra"] = round(mem_total / BYTES_PER_GB, 1)

    # Disks: per-volume free/size -> used/total GB, mapped onto C/D/E/F by name.
    free_by_vol: Dict[str, float] = {}
    size_by_vol: Dict[str, float] = {}
    for n, labels, val in metrics:
        vol = labels.get("volume") or labels.get("instance")
        if not vol:
            continue
        if n == LOGICAL_FREE_METRIC:
            free_by_vol[vol] = val
        elif n == LOGICAL_SIZE_METRIC:
            size_by_vol[vol] = val
    if size_by_vol:
        hdd = []
        # Prefer the configured drive letters, in order.
        for letter in drive_letters:
            # windows_exporter volume labels look like "C:".
            key = next((v for v in size_by_vol if v.rstrip(":").upper() == letter), None)
            if key is None:
                continue
            size_gb = size_by_vol.get(key, 0.0) / BYTES_PER_GB
            used_gb = max(0.0, (size_by_vol.get(key, 0.0) - free_by_vol.get(key, 0.0))) / BYTES_PER_GB
            hdd.append({"n": letter, "u": round(used_gb, 1), "tot": round(size_gb, 1), "t": 0})
        if hdd:
            hw["hdd"] = hdd

    # Network cumulative counters (summed across interfaces); monitor.py converts
    # to rates via netrates.compute_delta_rate using its own last-sample state.
    recv = _sum(metrics, NET_RECV_METRIC)
    sent = _sum(metrics, NET_SENT_METRIC)
    if recv > 0 or sent > 0:
        hw["_net_counters"] = {"recv": recv, "sent": sent}

    return hw


def map_query_results(results: Dict[str, Optional[float]]) -> Dict:
    """Map a dict of pre-run Prometheus instant-query values to ``hw`` keys.

    For users who prefer querying Prometheus (:9090 ``/api/v1/query``) over
    scraping the exporter directly. ``results`` is ``{key: value}`` where keys are
    our short hw names (``cl``/``ru``/``ra``/...). This is a thin pass-through that
    drops ``None``s and rounds floats — the query wiring lives in monitor.py.
    """
    hw: Dict = {}
    for k, v in (results or {}).items():
        if v is None:
            continue
        if k in ("ru", "ra"):
            hw[k] = round(float(v), 1)
        else:
            try:
                hw[k] = int(v)
            except (TypeError, ValueError):
                continue
    return hw


def merge_hw(base: Dict, overlay: Dict) -> Dict:
    """Merge ``overlay`` onto ``base``, overlay wins for present keys.

    Used as ``merge_hw(lhm_hw, prom_hw)`` so Prometheus fills load/ram/disk/net
    while LHM keeps temps/GPU/fans. ``_net_counters`` is passed through untouched.
    Never mutates inputs.
    """
    out = dict(base or {})
    for k, v in (overlay or {}).items():
        if v is None:
            continue
        out[k] = v
    return out
