#!/usr/bin/env python3
"""
Fresh Claude usage from session transcripts for NOCTURNE_OS.

``~/.claude/stats-cache.json`` is refreshed lazily by Claude Code and is often
days stale (it caused the device's "Claude" scene to show usage from days ago).
The session transcripts under ``~/.claude/projects/**/*.jsonl`` are written live,
so they are the freshest local signal: every assistant turn records a ``usage``
block (input/output/cache tokens) and an ISO ``timestamp``.

This module sums **input + output** tokens per local date from those transcripts
(cache tokens are excluded: prompt-cache reads dwarf real usage and would peg any
gauge). The result feeds the same budget %/alert path as before, but with TODAY's
real number and an honest "as of" date instead of a stale one.

Split for testing: :func:`tokens_by_date` is the only filesystem touch (and is
mtime-bounded for cheapness); :func:`summarize` is pure and takes the resulting
``{date: tokens}`` map. Both never raise.
"""

import glob
import json
import os
from typing import Any, Dict, Optional


def _local_date_from_ts(ts: str) -> Optional[str]:
    """ISO-8601 timestamp -> local 'YYYY-MM-DD', or None if unparseable."""
    if not isinstance(ts, str) or not ts:
        return None
    try:
        from datetime import datetime
        dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
        return dt.astimezone().date().isoformat()
    except (ValueError, TypeError):
        return None


def _blank_day() -> Dict[str, int]:
    return {"tok": 0, "msg": 0, "tool": 0}


def add_usage_line(by_date: Dict[str, Dict[str, int]], obj: Any) -> None:
    """Fold one parsed transcript record into ``by_date`` (per local date).

    Accumulates, for each assistant record with a ``usage`` dict + ``timestamp``:
      * ``tok``  – input+output tokens (cache excluded),
      * ``msg``  – +1 (one assistant turn),
      * ``tool`` – number of ``tool_use`` blocks in the message content.
    Mutates ``by_date`` in place. Pure aside from that. Never raises.
    """
    if not isinstance(obj, dict) or obj.get("type") != "assistant":
        return
    msg = obj.get("message")
    usage = msg.get("usage") if isinstance(msg, dict) else None
    if not isinstance(usage, dict):
        return
    date = _local_date_from_ts(obj.get("timestamp"))
    if not date:
        return
    try:
        inp = int(usage.get("input_tokens") or 0)
        out = int(usage.get("output_tokens") or 0)
    except (TypeError, ValueError):
        return
    tools = 0
    content = msg.get("content")
    if isinstance(content, list):
        tools = sum(1 for b in content if isinstance(b, dict) and b.get("type") == "tool_use")
    day = by_date.setdefault(date, _blank_day())
    day["tok"] += inp + out
    day["msg"] += 1
    day["tool"] += tools


def tokens_by_date(base_dir: Optional[str] = None, days: int = 8) -> Dict[str, Dict[str, int]]:
    """Aggregate per-local-date activity across recent session transcripts.

    Scans ``<base_dir>/projects/**/*.jsonl`` but only files modified within the
    last ``days`` days (so it stays cheap regardless of total history). Returns
    ``{date: {"tok":, "msg":, "tool":}}`` (possibly empty). Never raises.
    """
    try:
        base = base_dir if base_dir is not None else os.path.join(os.path.expanduser("~"), ".claude")
        proj = os.path.join(base, "projects")
        if not os.path.isdir(proj):
            return {}
        import time
        cutoff = time.time() - days * 86400
        by_date: Dict[str, Dict[str, int]] = {}
        for path in glob.glob(os.path.join(proj, "**", "*.jsonl"), recursive=True):
            try:
                if os.path.getmtime(path) < cutoff:
                    continue
            except OSError:
                continue
            try:
                with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                    for line in fh:
                        # Cheap pre-filter before the JSON parse.
                        if '"usage"' not in line:
                            continue
                        try:
                            obj = json.loads(line)
                        except ValueError:
                            continue
                        add_usage_line(by_date, obj)
            except OSError:
                continue
        return by_date
    except Exception:
        return {}


def summarize(by_date: Dict[str, Dict[str, int]], today: str, weekly_days: int = 7) -> Dict[str, Any]:
    """Reduce a ``{date: {tok,msg,tool}}`` map to fresh usage fields. Never raises.

    Returns ``{today_tokens, today_msgs, today_tools, weekly_tokens, date,
    last_active, days_tracked}``. ``today_*`` are for ``today`` exactly (0 if no
    activity yet today); ``weekly_tokens`` sums the ``weekly_days`` dates up to and
    including today; ``last_active`` is the most recent active date (may predate
    today). Returns an empty dict when there's no data at all.
    """
    if not isinstance(by_date, dict) or not by_date:
        return {}
    try:
        from datetime import date as _date, timedelta
        t = _date.fromisoformat(today)
    except (ValueError, TypeError):
        return {}

    def _tok(d):
        v = by_date.get(d)
        return int(v.get("tok", 0)) if isinstance(v, dict) else 0

    window = {(t - timedelta(days=i)).isoformat() for i in range(max(1, weekly_days))}
    weekly = sum(_tok(d) for d in window)
    td = by_date.get(today) if isinstance(by_date.get(today), dict) else _blank_day()
    dated = [d for d in by_date if isinstance(d, str)]
    last_active = max(dated) if dated else None
    return {
        "today_tokens": int(td.get("tok", 0)),
        "today_msgs": int(td.get("msg", 0)),
        "today_tools": int(td.get("tool", 0)),
        "weekly_tokens": int(weekly),
        "date": today,
        "last_active": last_active,
        "days_tracked": len(by_date),
    }
