#!/usr/bin/env python3
"""
Claude Code usage / limits telemetry source for NOCTURNE_OS.

Reads the *locally available* Claude Code state under ``~/.claude`` and distils
it into a small, stable dict that the PC server folds into its JSON payload (so
the ESP32 can render a "Claude" scene). It is read-only and never raises: on any
missing / unreadable / malformed input it returns ``{"available": False, ...}``
and logs at WARNING.

WHAT IS ACTUALLY AVAILABLE LOCALLY
----------------------------------
Claude Code persists rolling statistics to ``~/.claude/stats-cache.json`` and the
subscription plan to ``~/.claude/.credentials.json``. From those we can derive,
for *today* (``lastComputedDate``): total tokens across all models, message count
and tool-call count, plus lifetime totals and the plan name ("max"/"pro"/...).

WHAT IS **NOT** AVAILABLE LOCALLY
---------------------------------
The 5-hour rolling-window usage, the weekly-limit usage and their reset
timestamps are **not** persisted to disk by Claude Code — they live server-side
and are only surfaced live in the TUI / API response headers. So
``window_pct`` / ``weekly_pct`` / ``resets_in_min`` cannot be computed from real
local data here. They are kept in the contract (always present) but are ``None``
unless a future runtime drops a machine-readable window file that
:func:`parse_rate_limit` can read. This keeps the payload schema stable while
never inventing numbers.

DESIGN
------
The filesystem touch points (:func:`read_claude_usage`) are deliberately thin;
all the decision logic lives in pure helpers that take already-loaded dicts
(:func:`parse_stats`, :func:`parse_plan`, :func:`parse_rate_limit`,
:func:`build_usage`) so they are unit-testable without any filesystem.
"""

import json
import logging
import os
from typing import Any, Dict, Optional

# Public, stable contract. ``read_claude_usage`` always returns a dict with at
# least these keys so downstream payload code can index them unconditionally.
EMPTY_USAGE: Dict[str, Any] = {
    "available": False,   # True only when at least one real local source parsed
    "plan": None,         # subscription plan, e.g. "max" / "pro" (str | None)
    "window_pct": None,   # 5-hour window usage %, 0..100 (int | None) — see module docstring
    "weekly_pct": None,   # weekly limit usage %, 0..100 (int | None) — see module docstring
    "resets_in_min": None,  # minutes until the active window resets (int | None)
    "today_tokens": None,   # total tokens used today across all models (int | None)
    "today_msgs": None,     # messages sent today (int | None)
    "today_tools": None,    # tool calls today (int | None)
    "date": None,           # the date the "today_*" figures apply to (str | None)
}


def _coerce_int(value: Any) -> Optional[int]:
    """Best-effort ``int`` coercion. Returns ``None`` for None/garbage."""
    if value is None or isinstance(value, bool):
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def _clamp_pct(value: Any) -> Optional[int]:
    """Coerce to an int percentage clamped to 0..100, or ``None`` if not numeric."""
    iv = _coerce_int(value)
    if iv is None:
        return None
    return max(0, min(100, iv))


def parse_stats(stats: Any) -> Dict[str, Any]:
    """Distil the *today* figures from a loaded ``stats-cache.json`` dict.

    ``stats`` is whatever ``json.load`` produced (any type — this helper validates
    it). Returns a partial dict with ``date`` / ``today_tokens`` / ``today_msgs`` /
    ``today_tools`` populated where derivable, and an empty dict when ``stats`` is
    not a usable mapping. Never raises.

    "Today" is anchored on ``lastComputedDate`` (the most recent date Claude Code
    rolled up); the matching entries in ``dailyModelTokens`` (summed over every
    model) and ``dailyActivity`` provide the figures.
    """
    if not isinstance(stats, dict):
        return {}

    out: Dict[str, Any] = {}
    date = stats.get("lastComputedDate")
    if isinstance(date, str) and date:
        out["date"] = date

    # Today's tokens: sum tokensByModel for the lastComputedDate entry.
    daily_tokens = stats.get("dailyModelTokens")
    if isinstance(daily_tokens, list) and date:
        for entry in daily_tokens:
            if not isinstance(entry, dict) or entry.get("date") != date:
                continue
            by_model = entry.get("tokensByModel")
            if isinstance(by_model, dict):
                total = 0
                seen = False
                for v in by_model.values():
                    iv = _coerce_int(v)
                    if iv is not None:
                        total += iv
                        seen = True
                if seen:
                    out["today_tokens"] = total
            break

    # Today's message / tool-call counts from dailyActivity.
    daily_activity = stats.get("dailyActivity")
    if isinstance(daily_activity, list) and date:
        for entry in daily_activity:
            if not isinstance(entry, dict) or entry.get("date") != date:
                continue
            msgs = _coerce_int(entry.get("messageCount"))
            if msgs is not None:
                out["today_msgs"] = msgs
            tools = _coerce_int(entry.get("toolCallCount"))
            if tools is not None:
                out["today_tools"] = tools
            break

    return out


def parse_plan(creds: Any) -> Optional[str]:
    """Extract the subscription plan string from a loaded ``.credentials.json``.

    Looks for ``claudeAiOauth.subscriptionType`` (e.g. ``"max"``). Returns the
    plan as a lowercase str, or ``None`` if absent / malformed. Never raises.
    """
    if not isinstance(creds, dict):
        return None
    oauth = creds.get("claudeAiOauth")
    if not isinstance(oauth, dict):
        return None
    plan = oauth.get("subscriptionType")
    if isinstance(plan, str) and plan.strip():
        return plan.strip().lower()
    return None


def parse_rate_limit(rl: Any) -> Dict[str, Any]:
    """Distil 5-hour / weekly window usage from a (hypothetical) rate-limit dict.

    Claude Code does not currently persist this to disk (see module docstring), so
    in practice ``rl`` is ``None`` and this returns ``{}``. It is implemented
    against a forward-looking shape so that if a runtime ever drops a
    machine-readable window file, populating it requires no caller changes:

        {
          "five_hour": {"utilization": 0..100, "resets_in_minutes": int},
          "weekly":    {"utilization": 0..100}
        }

    Returns a partial dict with any of ``window_pct`` / ``weekly_pct`` /
    ``resets_in_min`` that could be derived. Never raises.
    """
    if not isinstance(rl, dict):
        return {}

    out: Dict[str, Any] = {}

    five = rl.get("five_hour")
    if isinstance(five, dict):
        pct = _clamp_pct(five.get("utilization"))
        if pct is not None:
            out["window_pct"] = pct
        resets = _coerce_int(five.get("resets_in_minutes"))
        if resets is not None:
            out["resets_in_min"] = max(0, resets)

    weekly = rl.get("weekly")
    if isinstance(weekly, dict):
        pct = _clamp_pct(weekly.get("utilization"))
        if pct is not None:
            out["weekly_pct"] = pct

    return out


def build_usage(
    stats: Any = None,
    creds: Any = None,
    rate_limit: Any = None,
) -> Dict[str, Any]:
    """Combine already-loaded sources into the stable usage dict.

    Pure: takes the parsed JSON objects (any of which may be ``None``) and returns
    a fresh dict containing every key of :data:`EMPTY_USAGE`. ``available`` is
    ``True`` iff at least one real field was derived from a real source (so a
    completely empty / malformed environment yields the graceful-empty contract).
    Never raises.
    """
    usage: Dict[str, Any] = dict(EMPTY_USAGE)

    plan = parse_plan(creds)
    if plan is not None:
        usage["plan"] = plan

    stats_fields = parse_stats(stats)
    usage.update(stats_fields)

    rl_fields = parse_rate_limit(rate_limit)
    usage.update(rl_fields)

    # "available" reflects whether we have *any* real signal to show.
    usage["available"] = bool(plan is not None or stats_fields or rl_fields)
    return usage


def _default_base_dir() -> str:
    """Return the real ``~/.claude`` directory (expanded user home)."""
    return os.path.join(os.path.expanduser("~"), ".claude")


def _load_json(path: str) -> Any:
    """Read+parse a JSON file. Returns ``None`` (logging at WARNING) on any error."""
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        # Absent file is an expected, non-noisy case — debug only.
        logging.debug("claude_usage: file not found: %s", path)
        return None
    except (OSError, ValueError) as e:
        logging.warning("claude_usage: failed to read %s: %s", path, e)
        return None


def read_claude_usage(base_dir: Optional[str] = None) -> Dict[str, Any]:
    """Read local Claude Code usage/limits into the stable dict.

    ``base_dir`` defaults to the real ``~/.claude`` but is injectable for tests.
    Reads ``stats-cache.json``, ``.credentials.json`` and (if present) an optional
    ``rate-limit.json``, then delegates all decisions to :func:`build_usage`.

    NEVER raises and NEVER returns ``None``: on a missing directory / unreadable /
    malformed files it returns a copy of :data:`EMPTY_USAGE` (``available`` False).
    """
    try:
        base = base_dir if base_dir is not None else _default_base_dir()

        stats = _load_json(os.path.join(base, "stats-cache.json"))
        creds = _load_json(os.path.join(base, ".credentials.json"))
        # Optional / forward-looking: not written by Claude Code today, but if a
        # runtime provides it we pick it up transparently (see parse_rate_limit).
        rate_limit = _load_json(os.path.join(base, "rate-limit.json"))

        return build_usage(stats=stats, creds=creds, rate_limit=rate_limit)
    except Exception as e:  # pragma: no cover - defensive: contract is never-raise
        logging.warning("claude_usage: unexpected error, returning empty: %s", e)
        return dict(EMPTY_USAGE)
