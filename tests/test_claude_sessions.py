"""Unit tests for server/claude_sessions.py (fresh usage from transcripts)."""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import claude_sessions as cs  # noqa: E402


def _tok_total(by_date):
    return sum(d["tok"] for d in by_date.values())


class TestAddUsageLine:
    def test_sums_input_output_and_counts(self):
        bd = {}
        cs.add_usage_line(bd, {
            "type": "assistant", "timestamp": "2026-06-03T10:00:00+00:00",
            "message": {"usage": {"input_tokens": 100, "output_tokens": 50},
                        "content": [{"type": "text"}, {"type": "tool_use"}, {"type": "tool_use"}]},
        })
        day = list(bd.values())[0]
        assert day["tok"] == 150
        assert day["msg"] == 1
        assert day["tool"] == 2

    def test_excludes_cache_tokens(self):
        bd = {}
        cs.add_usage_line(bd, {
            "type": "assistant", "timestamp": "2026-06-03T10:00:00+00:00",
            "message": {"usage": {
                "input_tokens": 10, "output_tokens": 20,
                "cache_creation_input_tokens": 99999,
                "cache_read_input_tokens": 888888,
            }},
        })
        assert _tok_total(bd) == 30   # cache tokens ignored

    def test_accumulates_across_records_same_day(self):
        bd = {}
        rec = {"type": "assistant", "timestamp": "2026-06-03T10:00:00Z",
               "message": {"usage": {"input_tokens": 1, "output_tokens": 9}}}
        cs.add_usage_line(bd, rec)
        cs.add_usage_line(bd, rec)
        day = list(bd.values())[0]
        assert day["tok"] == 20 and day["msg"] == 2

    def test_ignores_non_assistant(self):
        bd = {}
        cs.add_usage_line(bd, {"type": "user", "timestamp": "2026-06-03T10:00:00Z",
                               "message": {"usage": {"input_tokens": 5, "output_tokens": 5}}})
        assert bd == {}

    def test_ignores_missing_usage_or_ts(self):
        bd = {}
        cs.add_usage_line(bd, {"type": "assistant", "message": {}})
        cs.add_usage_line(bd, {"type": "assistant", "timestamp": "x",
                               "message": {"usage": {"output_tokens": 5}}})
        assert bd == {}

    def test_never_raises_on_garbage(self):
        for bad in (None, 5, {}, {"type": "assistant", "message": 7}):
            cs.add_usage_line({}, bad)


class TestSummarize:
    BY_DATE = {
        "2026-05-30": {"tok": 10_000, "msg": 5, "tool": 2},
        "2026-06-01": {"tok": 20_000, "msg": 8, "tool": 3},
        "2026-06-02": {"tok": 5_000, "msg": 2, "tool": 1},
        "2026-06-03": {"tok": 1_000, "msg": 4, "tool": 7},
    }

    def test_today_and_weekly(self):
        s = cs.summarize(self.BY_DATE, "2026-06-03", weekly_days=7)
        assert s["today_tokens"] == 1_000
        assert s["today_msgs"] == 4
        assert s["today_tools"] == 7
        assert s["weekly_tokens"] == 36_000   # all four within 7 days of 06-03
        assert s["date"] == "2026-06-03"
        assert s["last_active"] == "2026-06-03"
        assert s["days_tracked"] == 4

    def test_weekly_window_excludes_old(self):
        s = cs.summarize(self.BY_DATE, "2026-06-03", weekly_days=2)
        assert s["weekly_tokens"] == 6_000      # 06-03 + 06-02 only

    def test_today_zero_when_no_activity_today(self):
        s = cs.summarize(self.BY_DATE, "2026-06-04")
        assert s["today_tokens"] == 0 and s["today_msgs"] == 0
        assert s["last_active"] == "2026-06-03"

    def test_empty(self):
        assert cs.summarize({}, "2026-06-03") == {}
        assert cs.summarize(None, "2026-06-03") == {}

    def test_bad_today_returns_empty(self):
        assert cs.summarize(self.BY_DATE, "not-a-date") == {}


class TestTokensByDate:
    def _write(self, path, records):
        with open(path, "w", encoding="utf-8") as fh:
            for r in records:
                fh.write(json.dumps(r) + "\n")

    def test_scans_projects_tree(self, tmp_path):
        proj = tmp_path / "projects" / "proj-a"
        proj.mkdir(parents=True)
        self._write(proj / "session.jsonl", [
            {"type": "assistant", "timestamp": "2026-06-03T12:00:00+00:00",
             "message": {"usage": {"input_tokens": 100, "output_tokens": 200}}},
            {"type": "user", "timestamp": "2026-06-03T12:00:01+00:00", "message": {}},
            {"type": "assistant", "timestamp": "2026-06-03T13:00:00+00:00",
             "message": {"usage": {"input_tokens": 1, "output_tokens": 9}}},
        ])
        bd = cs.tokens_by_date(str(tmp_path), days=3650)
        assert _tok_total(bd) == 310

    def test_missing_projects_dir(self, tmp_path):
        assert cs.tokens_by_date(str(tmp_path), days=30) == {}

    def test_skips_corrupt_lines(self, tmp_path):
        proj = tmp_path / "projects"
        proj.mkdir(parents=True)
        p = proj / "s.jsonl"
        with open(p, "w", encoding="utf-8") as fh:
            fh.write('{"bad json\n')
            fh.write(json.dumps({"type": "assistant", "timestamp": "2026-06-03T00:00:00Z",
                                 "message": {"usage": {"input_tokens": 7, "output_tokens": 3}}}) + "\n")
        bd = cs.tokens_by_date(str(tmp_path), days=3650)
        assert _tok_total(bd) == 10
