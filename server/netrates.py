#!/usr/bin/env python3
"""
Network / disk throughput and ping helpers for NOCTURNE_OS.

The original ``monitor.py`` had two near-identical blocks computing a KB/s rate
from a pair of monotonic byte counters (one for network sent/recv, one for disk
read/write). :func:`compute_delta_rate` is the shared implementation both now
use. :func:`parse_ping_latency` is the pure parser for Windows/Unix ``ping``
output, separated from the blocking ``subprocess`` call so it is unit-testable.
"""

from typing import Dict, Optional, Tuple


def compute_delta_rate(
    cur_a: int,
    cur_b: int,
    state: Dict[str, float],
    now: float,
    min_dt: float = 0.1,
) -> Tuple[int, int]:
    """Compute a per-second KB/s rate for two monotonic byte counters.

    ``state`` is a mutable dict carrying the previous sample. It uses the legacy
    key layout from ``monitor.py``:

    * ``last_net_bytes`` -> keys ``sent``/``recv``/``time``
    * ``last_disk_bytes`` -> keys ``read``/``write``/``time``

    To stay agnostic to those names this helper reads/writes the two value slots
    positionally via whichever non-``time`` keys exist, so callers keep their
    existing state dicts unchanged.

    Returns ``(rate_a, rate_b)`` in KB/s (``int``, clamped to >= 0). On the first
    sample (``time == 0``) or when less than ``min_dt`` seconds have elapsed it
    returns ``(0, 0)`` after refreshing the stored sample.
    """
    value_keys = [k for k in state.keys() if k != "time"]
    key_a, key_b = value_keys[0], value_keys[1]

    if state["time"] == 0:
        state[key_a] = cur_a
        state[key_b] = cur_b
        state["time"] = now
        return 0, 0

    dt = now - state["time"]
    if dt < min_dt:
        return 0, 0

    rate_a = int((cur_a - state[key_a]) / dt / 1024)
    rate_b = int((cur_b - state[key_b]) / dt / 1024)
    state[key_a] = cur_a
    state[key_b] = cur_b
    state["time"] = now
    return max(0, rate_a), max(0, rate_b)


def parse_ping_latency(stdout: str) -> Optional[int]:
    """Parse the round-trip time (ms) from ``ping`` stdout.

    Looks for a ``time=<n>ms`` token (case-insensitive), as emitted by both
    Windows and Unix ``ping``. Returns the integer millisecond value, or ``None``
    if no latency token is present.
    """
    if not stdout:
        return None
    if "time=" in stdout.lower():
        for part in stdout.split():
            if "time=" in part.lower():
                try:
                    return int(float(part.split("=")[1].replace("ms", "").strip()))
                except (ValueError, IndexError):
                    return None
    return None
