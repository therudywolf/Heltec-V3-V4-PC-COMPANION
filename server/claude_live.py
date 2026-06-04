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
TOKEN_URL = "https://console.anthropic.com/v1/oauth/token"
OAUTH_BETA = "oauth-2025-04-20"
# OAuth (Pro/Max) tokens are scoped to Claude Code; the API expects the CLI's
# system prompt on the request, otherwise it rejects the token.
SYSTEM_PROMPT = "You are Claude Code, Anthropic's official CLI for Claude."
DEFAULT_MODEL = "claude-haiku-4-5-20251001"   # cheapest; matches the reference Telegram bot
CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"   # Claude Code OAuth client
USER_AGENT = "claude-code/2.1.5"
REFRESH_SKEW = 120   # refresh if the token expires within this many seconds


def _creds_path(base_dir: Optional[str]) -> str:
    base = base_dir if base_dir is not None else os.path.join(os.path.expanduser("~"), ".claude")
    return os.path.join(base, ".credentials.json")


def read_oauth_token(base_dir: Optional[str] = None) -> Optional[str]:
    """Read claudeAiOauth.accessToken from ~/.claude/.credentials.json, or None."""
    try:
        with open(_creds_path(base_dir), "r", encoding="utf-8") as fh:
            creds = json.load(fh)
        oauth = creds.get("claudeAiOauth") or {}
        tok = oauth.get("accessToken")
        return tok if isinstance(tok, str) and tok.strip() else None
    except Exception:
        return None


def _refresh_access_token(refresh_token: str, timeout: int = 30) -> Optional[Dict[str, Any]]:
    """Exchange a refresh_token for a fresh access token (same flow as Claude Code
    and the reference Telegram bot). Returns the token JSON or None. Never raises."""
    if not refresh_token:
        return None
    body = json.dumps({
        "grant_type": "refresh_token",
        "refresh_token": refresh_token,
        "client_id": CLIENT_ID,
    }).encode("utf-8")
    req = urllib.request.Request(TOKEN_URL, data=body, method="POST")
    req.add_header("content-type", "application/json")
    req.add_header("accept", "application/json")
    req.add_header("User-Agent", USER_AGENT)
    try:
        resp = urllib.request.urlopen(req, timeout=timeout)
        return json.loads(resp.read().decode("utf-8"))
    except Exception:
        return None


def get_token(base_dir: Optional[str] = None) -> Optional[str]:
    """Return a VALID access token, refreshing (and writing back) if it's expired
    or within REFRESH_SKEW of expiry — so the device's Claude never goes stale the
    way a static token would. Mirrors the reference Telegram bot's claude_meter. Falls
    back to the stored token on any refresh failure. Never raises.
    """
    import time
    try:
        path = _creds_path(base_dir)
        with open(path, "r", encoding="utf-8") as fh:
            cfg = json.load(fh)
        o = cfg.get("claudeAiOauth") or {}
        access = o.get("accessToken")
        expires_at = float(o.get("expiresAt", 0)) / 1000.0
        if access and (expires_at - time.time()) > REFRESH_SKEW:
            return access                      # still fresh
        rt = o.get("refreshToken")
        if not rt:
            return access                      # nothing to refresh with
        tok = _refresh_access_token(rt)
        if not tok or not tok.get("access_token"):
            return access                      # refresh blocked/failed -> stale token
        o["accessToken"] = tok["access_token"]
        if tok.get("refresh_token"):
            o["refreshToken"] = tok["refresh_token"]
        if tok.get("expires_in"):
            o["expiresAt"] = int((time.time() + float(tok["expires_in"])) * 1000)
        cfg["claudeAiOauth"] = o
        try:                                   # atomic write-back, preserve perms
            tmp = path + ".tmp"
            with open(tmp, "w", encoding="utf-8") as fh:
                json.dump(cfg, fh)
            os.replace(tmp, path)
        except Exception:
            pass
        return o["accessToken"]
    except Exception:
        return read_oauth_token(base_dir)


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
    req.add_header("User-Agent", USER_AGENT)
    try:
        resp = urllib.request.urlopen(req, timeout=timeout)
        headers = dict(resp.headers)
        resp.read()
    except urllib.error.HTTPError as e:
        headers = dict(e.headers or {})
    except Exception:
        return {}
    return parse_unified_headers(headers, now_ts)
