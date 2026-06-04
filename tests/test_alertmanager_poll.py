"""Unit tests for server/alertmanager_poll.py (Alertmanager v2 polling)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import alertmanager_poll as amp  # noqa: E402


# A trimmed sample of a real Alertmanager /api/v2/alerts shape.
SAMPLE_V2 = [
    {
        "labels": {"alertname": "DiskWillFill", "severity": "warning"},
        "status": {"state": "active"},
    },
    {
        "labels": {"alertname": "TargetDown", "severity": "critical"},
        "status": {"state": "active"},
    },
    {
        "labels": {"alertname": "Silenced", "severity": "critical"},
        "status": {"state": "suppressed"},
    },
]


class TestNormalizeV2:
    def test_active_maps_to_firing(self):
        out = amp.normalize_am_v2(SAMPLE_V2)
        firing = [a for a in out if a["status"] == "firing"]
        names = {a["name"] for a in firing}
        assert names == {"DiskWillFill", "TargetDown"}

    def test_suppressed_is_resolved(self):
        out = amp.normalize_am_v2(SAMPLE_V2)
        sup = [a for a in out if a["name"] == "Silenced"]
        assert sup and sup[0]["status"] == "resolved"

    def test_severity_preserved_and_normalized(self):
        out = amp.normalize_am_v2([
            {"labels": {"alertname": "X", "severity": "CRITICAL"}, "status": {"state": "active"}},
            {"labels": {"alertname": "Y", "severity": "bogus"}, "status": {"state": "active"}},
        ])
        sev = {a["name"]: a["severity"] for a in out}
        assert sev["X"] == "critical"
        assert sev["Y"] == "none"      # unknown severity -> none

    def test_name_clamped(self):
        out = amp.normalize_am_v2([
            {"labels": {"alertname": "Z" * 50}, "status": {"state": "active"}},
        ])
        assert len(out[0]["name"]) == 20

    def test_missing_fields_default(self):
        out = amp.normalize_am_v2([{"labels": {}}])
        assert out[0]["name"] == "alert"
        assert out[0]["severity"] == "none"
        assert out[0]["status"] == "firing"   # missing state defaults active

    def test_bad_input_returns_empty(self):
        assert amp.normalize_am_v2(None) == []
        assert amp.normalize_am_v2({}) == []
        assert amp.normalize_am_v2("nope") == []

    def test_skips_non_dict_entries(self):
        out = amp.normalize_am_v2([None, 5, {"labels": {"alertname": "OK"}, "status": {"state": "active"}}])
        assert [a["name"] for a in out] == ["OK"]


class TestBuildBlock:
    def test_firing_only_in_block(self):
        block = amp.build_block(SAMPLE_V2)
        # 2 active -> firing; suppressed dropped by build_events_block
        assert block["n"] == 2
        assert block["top"] == "TargetDown"   # critical wins the banner
        assert block["sev"] == "critical"

    def test_empty(self):
        block = amp.build_block([])
        assert block["n"] == 0
