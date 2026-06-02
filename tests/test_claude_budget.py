"""Unit tests for server/claude_budget.py (budget % + 80% alert)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import claude_budget as cb  # noqa: E402


def _stats(daily_totals):
    return {"dailyModelTokens": [{"date": f"d{i}", "tokensByModel": {"m": t}}
                                 for i, t in enumerate(daily_totals)]}


class TestWeeklyTokens:
    def test_sums_last_7(self):
        s = _stats([100] * 10)  # 10 days of 100
        assert cb.weekly_tokens(s) == 700  # last 7

    def test_fewer_than_7(self):
        assert cb.weekly_tokens(_stats([1000, 2000])) == 3000

    def test_none_on_garbage(self):
        assert cb.weekly_tokens(None) is None
        assert cb.weekly_tokens({}) is None
        assert cb.weekly_tokens({"dailyModelTokens": []}) is None


class TestApplyBudget:
    def test_window_pct_from_today(self):
        u = {"today_tokens": 4_000_000, "window_pct": None, "weekly_pct": None}
        out = cb.apply_budget(u, _stats([0]), daily_budget=5_000_000, weekly_budget=25_000_000)
        assert out["window_pct"] == 80   # 4M / 5M

    def test_weekly_pct_from_series(self):
        u = {"today_tokens": 0, "window_pct": None, "weekly_pct": None}
        out = cb.apply_budget(u, _stats([5_000_000] * 7), daily_budget=5_000_000, weekly_budget=25_000_000)
        # 35M / 25M -> clamped to 100
        assert out["weekly_pct"] == 100

    def test_does_not_override_existing(self):
        u = {"today_tokens": 9_999_999, "window_pct": 42, "weekly_pct": 7}
        out = cb.apply_budget(u, _stats([0]))
        assert out["window_pct"] == 42 and out["weekly_pct"] == 7

    def test_zero_budget_is_na(self):
        u = {"today_tokens": 1_000_000, "window_pct": None, "weekly_pct": None}
        out = cb.apply_budget(u, _stats([0]), daily_budget=0, weekly_budget=0)
        assert out["window_pct"] is None and out["weekly_pct"] is None

    def test_sets_available_when_pct_derived(self):
        u = {"today_tokens": 1_000_000, "window_pct": None, "weekly_pct": None, "available": False}
        out = cb.apply_budget(u, _stats([0]), daily_budget=5_000_000)
        assert out["available"] is True

    def test_weekly_override_bypasses_stats(self):
        # stats series says 35M, but the fresh override (90M) must win.
        u = {"today_tokens": 0, "window_pct": None, "weekly_pct": None}
        out = cb.apply_budget(u, _stats([5_000_000] * 7),
                              daily_budget=30_000_000, weekly_budget=100_000_000,
                              weekly_tokens_override=90_000_000)
        assert out["weekly_pct"] == 90      # 90M / 100M, not 35M-derived
        assert out["weekly_tokens"] == 90_000_000

    def test_weekly_override_zero_is_respected(self):
        u = {"today_tokens": 0, "window_pct": None, "weekly_pct": None}
        out = cb.apply_budget(u, _stats([5_000_000] * 7),
                              weekly_budget=100_000_000, weekly_tokens_override=0)
        assert out["weekly_pct"] == 0       # override 0 used, not the stats series

    def test_never_raises(self):
        cb.apply_budget({}, None)
        cb.apply_budget(None, {})


class TestAlertFor:
    def test_fires_at_threshold_window(self):
        fire, name = cb.alert_for({"window_pct": 80, "weekly_pct": 10})
        assert fire and "5h" in name and "80%" in name

    def test_fires_weekly(self):
        fire, name = cb.alert_for({"window_pct": 10, "weekly_pct": 95})
        assert fire and "wk" in name and "95%" in name

    def test_highest_wins(self):
        fire, name = cb.alert_for({"window_pct": 99, "weekly_pct": 81})
        assert fire and "99%" in name

    def test_no_alert_below(self):
        fire, name = cb.alert_for({"window_pct": 79, "weekly_pct": 50})
        assert not fire and name == ""

    def test_na_does_not_fire(self):
        fire, _ = cb.alert_for({"window_pct": None, "weekly_pct": None})
        assert not fire

    def test_custom_threshold(self):
        fire, _ = cb.alert_for({"window_pct": 60}, threshold_pct=50)
        assert fire
