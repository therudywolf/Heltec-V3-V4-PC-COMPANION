#!/usr/bin/env python3
"""
Claude usage -> budget percentages + threshold alert for NOCTURNE_OS.

Claude Code does NOT expose official 5-hour/weekly quota percentages on disk
(see claude_usage.py). The only reliable local signal is token counts from
``stats-cache.json``. So to give the device a real "% of limit" gauge and an
80%-remaining reminder, we measure tokens against CONFIGURABLE budgets:

  * window %  = today's tokens / daily_budget        (proxy for the 5h window)
  * weekly %  = rolling 7-day tokens / weekly_budget

Budgets come from config.json ("claude_daily_budget", "claude_weekly_budget").
When a budget is 0/unset the matching percent stays None (device shows "n/a").

:func:`weekly_tokens` sums the last 7 daily entries from a loaded
``stats-cache.json``; :func:`apply_budget` enriches a claude_usage dict with the
percentages; :func:`alert_for` decides whether to raise a device alert. All pure
and never-raising.
"""

from typing import Any, Dict, List, Optional, Tuple

# Default budgets (tokens). Sized from observed ~2-3.3M tokens/day on a Max plan;
# overridable in config.json. 0 disables that gauge.
DEFAULT_DAILY_BUDGET = 5_000_000
DEFAULT_WEEKLY_BUDGET = 25_000_000

# Raise the reminder when usage reaches this percent of any budget.
DEFAULT_ALERT_PCT = 80


def weekly_tokens(stats: Any, days: int = 7) -> Optional[int]:
    """Sum tokens over the most recent ``days`` entries of dailyModelTokens.

    Returns None if ``stats`` has no usable token series. Entries are summed over
    every model; ordering uses the list tail (Claude appends chronologically).
    Never raises.
    """
    if not isinstance(stats, dict):
        return None
    series = stats.get("dailyModelTokens")
    if not isinstance(series, list) or not series:
        return None
    total = 0
    seen = False
    for entry in series[-days:]:
        if not isinstance(entry, dict):
            continue
        by_model = entry.get("tokensByModel")
        if isinstance(by_model, dict):
            for v in by_model.values():
                if isinstance(v, int):
                    total += v
                    seen = True
    return total if seen else None


def _pct(used: Optional[int], budget: int) -> Optional[int]:
    """used/budget as an int percent (0..100+ clamped to 0..100), None if n/a."""
    if used is None or not budget or budget <= 0:
        return None
    return max(0, min(100, int(round(used * 100.0 / budget))))


def apply_budget(
    usage: Dict[str, Any],
    stats: Any,
    daily_budget: int = DEFAULT_DAILY_BUDGET,
    weekly_budget: int = DEFAULT_WEEKLY_BUDGET,
) -> Dict[str, Any]:
    """Return a copy of ``usage`` with window_pct/weekly_pct filled from budgets.

    Does NOT overwrite a non-None window_pct/weekly_pct already present (so a real
    rate-limit source, if it ever appears, wins over the budget estimate). Adds
    ``daily_budget``/``weekly_budget``/``weekly_tokens`` for transparency. Pure.
    """
    out = dict(usage or {})
    today = out.get("today_tokens")
    wk = weekly_tokens(stats)

    if out.get("window_pct") is None:
        out["window_pct"] = _pct(today, daily_budget)
    if out.get("weekly_pct") is None:
        out["weekly_pct"] = _pct(wk, weekly_budget)

    out["weekly_tokens"] = wk
    out["daily_budget"] = daily_budget
    out["weekly_budget"] = weekly_budget
    # available stays true if it already was, or if we produced any percent.
    if out.get("window_pct") is not None or out.get("weekly_pct") is not None:
        out["available"] = True
    return out


def alert_for(
    usage: Dict[str, Any],
    threshold_pct: int = DEFAULT_ALERT_PCT,
) -> Tuple[bool, str]:
    """Decide whether to raise the Claude reminder alert.

    Fires when window_pct OR weekly_pct >= ``threshold_pct``. Returns
    ``(fire, name)`` where name names the breaching limit for the banner
    (e.g. "Claude 5h 82%" or "Claude wk 88%"). Highest breaching % wins.
    """
    if not isinstance(usage, dict):
        return (False, "")
    candidates: List[Tuple[int, str]] = []
    win = usage.get("window_pct")
    wk = usage.get("weekly_pct")
    if isinstance(win, int) and win >= threshold_pct:
        candidates.append((win, f"Claude 5h {win}%"))
    if isinstance(wk, int) and wk >= threshold_pct:
        candidates.append((wk, f"Claude wk {wk}%"))
    if not candidates:
        return (False, "")
    candidates.sort(reverse=True)
    return (True, candidates[0][1])
