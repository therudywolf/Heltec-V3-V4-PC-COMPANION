# Unit tests for the Claude Code usage/limits source (server/claude_usage.py).
# These exercise the PURE helpers with SYNTHETIC data only (present / absent /
# malformed) plus the never-raise graceful-empty contract of read_claude_usage.
# No real ~/.claude data is read or asserted on.
# Run from project root: python -m pytest tests/ -v

import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))

import claude_usage as cu  # noqa: E402


# Synthetic stats-cache.json shaped like the real file but with invented numbers.
SYNTHETIC_STATS = {
    "lastComputedDate": "2024-01-15",
    "dailyActivity": [
        {"date": "2024-01-14", "messageCount": 10, "sessionCount": 1, "toolCallCount": 5},
        {"date": "2024-01-15", "messageCount": 42, "sessionCount": 3, "toolCallCount": 17},
    ],
    "dailyModelTokens": [
        {"date": "2024-01-14", "tokensByModel": {"model-a": 100}},
        {"date": "2024-01-15", "tokensByModel": {"model-a": 1000, "model-b": 234}},
    ],
}

SYNTHETIC_CREDS = {
    "claudeAiOauth": {
        "accessToken": "x",
        "subscriptionType": "Max",
    }
}


# --------------------------------------------------------------------------- #
# parse_stats
# --------------------------------------------------------------------------- #
class TestParseStats:
    def test_present_sums_today_tokens_and_activity(self):
        out = cu.parse_stats(SYNTHETIC_STATS)
        assert out["date"] == "2024-01-15"
        assert out["today_tokens"] == 1234       # 1000 + 234, only for the today entry
        assert out["today_msgs"] == 42
        assert out["today_tools"] == 17

    def test_absent_returns_empty(self):
        assert cu.parse_stats(None) == {}
        assert cu.parse_stats("not a dict") == {}
        assert cu.parse_stats([]) == {}

    def test_empty_dict_returns_empty(self):
        assert cu.parse_stats({}) == {}

    def test_missing_date_yields_no_today_fields(self):
        # No lastComputedDate -> cannot anchor "today".
        stats = {"dailyModelTokens": [{"date": "x", "tokensByModel": {"m": 5}}]}
        assert cu.parse_stats(stats) == {}

    def test_malformed_token_values_skipped(self):
        stats = {
            "lastComputedDate": "d",
            "dailyModelTokens": [
                {"date": "d", "tokensByModel": {"a": "garbage", "b": 50, "c": None}}
            ],
        }
        out = cu.parse_stats(stats)
        assert out["today_tokens"] == 50  # only the numeric value counted

    def test_malformed_token_map_not_a_dict(self):
        stats = {
            "lastComputedDate": "d",
            "dailyModelTokens": [{"date": "d", "tokensByModel": "nope"}],
        }
        out = cu.parse_stats(stats)
        assert "today_tokens" not in out
        assert out["date"] == "d"

    def test_all_token_values_garbage_yields_no_token_field(self):
        stats = {
            "lastComputedDate": "d",
            "dailyModelTokens": [{"date": "d", "tokensByModel": {"a": "x", "b": None}}],
        }
        out = cu.parse_stats(stats)
        assert "today_tokens" not in out

    def test_daily_activity_not_list(self):
        stats = {"lastComputedDate": "d", "dailyActivity": "nope"}
        out = cu.parse_stats(stats)
        assert "today_msgs" not in out


# --------------------------------------------------------------------------- #
# parse_plan
# --------------------------------------------------------------------------- #
class TestParsePlan:
    def test_present_lowercased(self):
        assert cu.parse_plan(SYNTHETIC_CREDS) == "max"

    def test_absent_returns_none(self):
        assert cu.parse_plan(None) is None
        assert cu.parse_plan({}) is None
        assert cu.parse_plan("nope") is None

    def test_malformed_oauth_block(self):
        assert cu.parse_plan({"claudeAiOauth": "x"}) is None
        assert cu.parse_plan({"claudeAiOauth": {}}) is None
        assert cu.parse_plan({"claudeAiOauth": {"subscriptionType": 5}}) is None
        assert cu.parse_plan({"claudeAiOauth": {"subscriptionType": "   "}}) is None


# --------------------------------------------------------------------------- #
# parse_rate_limit  (forward-looking; absent in practice -> graceful)
# --------------------------------------------------------------------------- #
class TestParseRateLimit:
    def test_absent_returns_empty(self):
        assert cu.parse_rate_limit(None) == {}
        assert cu.parse_rate_limit("x") == {}
        assert cu.parse_rate_limit({}) == {}

    def test_full_shape_parsed(self):
        rl = {
            "five_hour": {"utilization": 73, "resets_in_minutes": 142},
            "weekly": {"utilization": 41},
        }
        out = cu.parse_rate_limit(rl)
        assert out["window_pct"] == 73
        assert out["resets_in_min"] == 142
        assert out["weekly_pct"] == 41

    def test_pct_clamped(self):
        out = cu.parse_rate_limit({"five_hour": {"utilization": 250}})
        assert out["window_pct"] == 100
        out = cu.parse_rate_limit({"weekly": {"utilization": -10}})
        assert out["weekly_pct"] == 0

    def test_partial_and_malformed_fields(self):
        out = cu.parse_rate_limit({"five_hour": {"utilization": "x"}})
        assert out == {}
        out = cu.parse_rate_limit({"five_hour": {"resets_in_minutes": -5}})
        assert out["resets_in_min"] == 0  # clamped to >= 0
        assert "window_pct" not in out


# --------------------------------------------------------------------------- #
# build_usage  (pure combiner)
# --------------------------------------------------------------------------- #
class TestBuildUsage:
    def test_all_sources_present(self):
        u = cu.build_usage(stats=SYNTHETIC_STATS, creds=SYNTHETIC_CREDS)
        assert u["available"] is True
        assert u["plan"] == "max"
        assert u["today_tokens"] == 1234
        assert u["today_msgs"] == 42
        assert u["date"] == "2024-01-15"
        # No local window data -> stays None.
        assert u["window_pct"] is None
        assert u["weekly_pct"] is None
        assert u["resets_in_min"] is None

    def test_all_absent_is_graceful_empty(self):
        u = cu.build_usage(stats=None, creds=None, rate_limit=None)
        assert u == cu.EMPTY_USAGE
        assert u["available"] is False
        # Must be a fresh copy, not the shared constant (mutation safety).
        assert u is not cu.EMPTY_USAGE

    def test_all_malformed_is_graceful_empty(self):
        u = cu.build_usage(stats="garbage", creds=12345, rate_limit=[1, 2])
        assert u["available"] is False
        assert u["plan"] is None
        assert u["today_tokens"] is None

    def test_only_plan_present_marks_available(self):
        u = cu.build_usage(stats=None, creds=SYNTHETIC_CREDS)
        assert u["available"] is True
        assert u["plan"] == "max"
        assert u["today_tokens"] is None

    def test_only_stats_present_marks_available(self):
        u = cu.build_usage(stats=SYNTHETIC_STATS, creds=None)
        assert u["available"] is True
        assert u["plan"] is None
        assert u["today_msgs"] == 42

    def test_rate_limit_fills_window_fields(self):
        rl = {"five_hour": {"utilization": 60, "resets_in_minutes": 30}}
        u = cu.build_usage(stats=None, creds=None, rate_limit=rl)
        assert u["available"] is True
        assert u["window_pct"] == 60
        assert u["resets_in_min"] == 30

    def test_result_has_full_contract_keys(self):
        u = cu.build_usage()
        assert set(u.keys()) == set(cu.EMPTY_USAGE.keys())

    def test_result_is_json_serializable(self):
        u = cu.build_usage(stats=SYNTHETIC_STATS, creds=SYNTHETIC_CREDS)
        s = json.dumps(u)
        assert json.loads(s)["plan"] == "max"


# --------------------------------------------------------------------------- #
# read_claude_usage  (filesystem entry point — never raises)
# --------------------------------------------------------------------------- #
class TestReadClaudeUsage:
    def test_missing_dir_returns_empty(self, tmp_path):
        missing = str(tmp_path / "definitely_missing")
        u = cu.read_claude_usage(base_dir=missing)
        assert u["available"] is False
        assert set(u.keys()) == set(cu.EMPTY_USAGE.keys())

    def test_reads_synthetic_files(self, tmp_path):
        (tmp_path / "stats-cache.json").write_text(
            json.dumps(SYNTHETIC_STATS), encoding="utf-8"
        )
        (tmp_path / ".credentials.json").write_text(
            json.dumps(SYNTHETIC_CREDS), encoding="utf-8"
        )
        u = cu.read_claude_usage(base_dir=str(tmp_path))
        assert u["available"] is True
        assert u["plan"] == "max"
        assert u["today_tokens"] == 1234
        assert u["today_msgs"] == 42

    def test_reads_optional_rate_limit_file(self, tmp_path):
        (tmp_path / "rate-limit.json").write_text(
            json.dumps({"five_hour": {"utilization": 80, "resets_in_minutes": 12}}),
            encoding="utf-8",
        )
        u = cu.read_claude_usage(base_dir=str(tmp_path))
        assert u["available"] is True
        assert u["window_pct"] == 80
        assert u["resets_in_min"] == 12

    def test_malformed_json_files_are_graceful(self, tmp_path):
        (tmp_path / "stats-cache.json").write_text("{not valid json", encoding="utf-8")
        (tmp_path / ".credentials.json").write_text("also broken]", encoding="utf-8")
        u = cu.read_claude_usage(base_dir=str(tmp_path))
        assert u["available"] is False
        assert set(u.keys()) == set(cu.EMPTY_USAGE.keys())

    def test_partial_files_present(self, tmp_path):
        # Only credentials present, stats absent -> still available via plan.
        (tmp_path / ".credentials.json").write_text(
            json.dumps(SYNTHETIC_CREDS), encoding="utf-8"
        )
        u = cu.read_claude_usage(base_dir=str(tmp_path))
        assert u["available"] is True
        assert u["plan"] == "max"
        assert u["today_tokens"] is None
