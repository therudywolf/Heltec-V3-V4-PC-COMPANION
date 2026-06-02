#!/usr/bin/env python3
"""
REAL Claude usage limits via the Claude Code OAuth token (NOCTURNE_OS).

Anthropic does not persist the live quota to disk, but the Messages API returns
it in ``anthropic-ratelimit-unified-*`` response headers. Claude Code already
authorized an OAuth token (``~/.claude/.credentials.json`` -> claudeAiOauth.
accessToken, scope ``user:inference``); we reuse it to make ONE tiny authed call
(max_tokens=1, ~a dozen tokens) and read those headers. That yields the genuine
5-hour and weekly utilization % + reset times — not a budget proxy.

OPT-IN (config ``claude_live``): it spends a negligible amount of the account's
own quota per poll and uses the Claude Code token outside the CLI, so it's off by
default. :func:`parse_unified_headers` is the pure, offline-testable mapper; the
HTTP call lives in :func:`fetch_live_usage`.
"""

import json
import os
import urllib.error
import urllib.request
from typing import Any, Dict, Optional

API_URL = "https://api.anthropic.com/v1/messages"
OAUTH_BETA = "oauth-2025-04-20"
# OAuth (Pro/Max) tokens are scoped to Claude Code; the API expects the CLI's
# system prompt on the request, otherwise it rejects the token.
SYSTEM_PROMPT = "You are Claude Code, Anthropic's official CLI for Claude."
DEFAULT_MODEL = "claude-haiku-4-5"   # cheapest; falls back if unavailable


def read_oauth_token(base_dir: Optional[str] = None) -> Optional[str]:
    """Read claudeAiOauth.accessToken from ~/.claude/.credentials.json, or None."""
    try:
        base = base_dir if base_dir is not None else os.path.join(os.path.expanduser("~"), ".claude")
        with open(os.path.join(base, ".credentials.json"), "r", encoding="utf-8") as fh:
            creds = json.load(fh)
        oauth = creds.get("claudeAiOauth") or {}
        tok = oauth.get("accessToken")
        return tok if isinstance(tok, str) and tok.strip() else None
    except Exception:
        return None


def _header_get(headers: Dict[str, str], key: str) -> Optional[str]:
    """Case-insensitive header lookup."""
    kl = key.lower()
    for k, v in headers.items():
        if k.lower() == kl:
            return v
    return None


def _pct(v: Any) -> Optional[int]:
    """utilization 0..1 (or 0..100) -> int percent 0..100, or None."""
    try:
        f = float(v)
    except (TypeError, ValueError):
        return None
    if f <= 1.0:
        f *= 100.0
    return max(0, min(100, int(round(f))))


def _mins_until(epoch: Any, now_ts: float) -> Optional[int]:
    """Epoch seconds -> whole minutes from now (clamped >=0), or None."""
    try:
        return max(0, int(round((float(epoch) - now_ts) / 60.0)))
    except (TypeError, ValueError):
        return None


def parse_unified_headers(headers: Dict[str, str], now_ts: float) -> Dict[str, Any]:
    """Map ``anthropic-ratelimit-unified-*`` headers to the usage dict. Pure.

    Produces the REAL fields: ``window_pct`` (5h), ``weekly_pct`` (7d),
    ``resets_in_min`` (5h), ``weekly_resets_in_min`` (7d), ``limit_status``.
    Returns {} if no unified headers are present. Never raises.
    """
    out: Dict[str, Any] = {}
    if not isinstance(headers, dict):
        return out
    u5 = _header_get(headers, "anthropic-ratelimit-unified-5h-utilization")
    u7 = _header_get(headers, "anthropic-ratelimit-unified-7d-utilization")
    r5 = _header_get(headers, "anthropic-ratelimit-unified-5h-reset")
    r7 = _header_get(headers, "anthropic-ratelimit-unified-7d-reset")
    st = _header_get(headers, "anthropic-ratelimit-unified-status")

    if u5 is not None:
        p = _pct(u5)
        if p is not None:
            out["window_pct"] = p
    if u7 is not None:
        p = _pct(u7)
        if p is not None:
            out["weekly_pct"] = p
    if r5 is not None:
        m = _mins_until(r5, now_ts)
        if m is not None:
            out["resets_in_min"] = m
    if r7 is not None:
        m = _mins_until(r7, now_ts)
        if m is not None:
            out["weekly_resets_in_min"] = m
    if st:
        out["limit_status"] = str(st)
    if out:
        out["source"] = "live"
        out["available"] = True
    return out


def fetch_live_usage(token: Optional[str], now_ts: float,
                     model: str = DEFAULT_MODEL, timeout: int = 15) -> Dict[str, Any]:
    """Make one tiny authed Messages call and parse the real rate-limit headers.

    Returns the parsed usage dict, or {} on any failure (caller falls back to the
    transcript-derived estimate). Rate-limit headers are present even on a 429, so
    a throttled account still reports its real status. Never raises.
    """
    if not token:
        return {}
    body = json.dumps({
        "model": model,
        "max_tokens": 1,
        "system": SYSTEM_PROMPT,
        "messages": [{"role": "user", "content": "."}],
    }).encode("utf-8")
    req = urllib.request.Request(API_URL, data=body, method="POST")
    req.add_header("authorization", f"Bearer {token}")
    req.add_header("anthropic-version", "2023-06-01")
    req.add_header("anthropic-beta", OAUTH_BETA)
    req.add_header("content-type", "application/json")
    try:
        resp = urllib.request.urlopen(req, timeout=timeout)
        headers = dict(resp.headers)
        resp.read()
    except urllib.error.HTTPError as e:
        headers = dict(e.headers or {})
    except Exception:
        return {}
    return parse_unified_headers(headers, now_ts)
