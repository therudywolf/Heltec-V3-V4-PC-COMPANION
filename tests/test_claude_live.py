"""Unit tests for server/claude_live.py (real limits via OAuth headers)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import claude_live as cl  # noqa: E402


# A real-shape header set (epochs relative to a fixed 'now' in the tests).
NOW = 1_780_000_000.0


def _headers(**over):
    h = {
        "anthropic-ratelimit-unified-status": "allowed",
        "anthropic-ratelimit-unified-5h-utilization": "0.45",
        "anthropic-ratelimit-unified-5h-reset": str(int(NOW + 3 * 3600)),  # +3h
        "anthropic-ratelimit-unified-7d-utilization": "0.04",
        "anthropic-ratelimit-unified-7d-reset": str(int(NOW + 2 * 86400)),  # +2d
        "content-type": "application/json",
    }
    h.update(over)
    return h


class TestParseUnifiedHeaders:
    def test_maps_real_fields(self):
        out = cl.parse_unified_headers(_headers(), NOW)
        assert out["window_pct"] == 45
        assert out["weekly_pct"] == 4
        assert out["resets_in_min"] == 180          # +3h
        assert out["weekly_resets_in_min"] == 2880  # +2d
        assert out["limit_status"] == "allowed"
        assert out["source"] == "live"
        assert out["available"] is True

    def test_case_insensitive_headers(self):
        h = {"Anthropic-RateLimit-Unified-5h-Utilization": "0.5",
             "ANTHROPIC-RATELIMIT-UNIFIED-5H-RESET": str(int(NOW + 600))}
        out = cl.parse_unified_headers(h, NOW)
        assert out["window_pct"] == 50
        assert out["resets_in_min"] == 10

    def test_utilization_already_percent(self):
        # some responses could give 0..100; clamp + accept
        out = cl.parse_unified_headers(
            {"anthropic-ratelimit-unified-5h-utilization": "73"}, NOW)
        assert out["window_pct"] == 73

    def test_reset_in_past_clamped(self):
        out = cl.parse_unified_headers(
            {"anthropic-ratelimit-unified-5h-reset": str(int(NOW - 99999))}, NOW)
        assert out["resets_in_min"] == 0

    def test_no_unified_headers_empty(self):
        assert cl.parse_unified_headers({"content-type": "application/json"}, NOW) == {}
        assert cl.parse_unified_headers({}, NOW) == {}

    def test_bad_input_never_raises(self):
        assert cl.parse_unified_headers(None, NOW) == {}
        assert cl.parse_unified_headers({"anthropic-ratelimit-unified-5h-utilization": "x"}, NOW) == {}


class TestFetchGuards:
    def test_no_token_returns_empty(self):
        assert cl.fetch_live_usage(None, NOW) == {}
        assert cl.fetch_live_usage("", NOW) == {}
