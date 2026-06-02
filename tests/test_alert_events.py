"""Unit tests for server/alert_events.py (Alertmanager webhook ingestion)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import alert_events as ae  # noqa: E402


class TestParseAlertmanager:
    def test_standard_webhook(self):
        body = {
            "alerts": [
                {"labels": {"alertname": "DiskFull", "severity": "critical"}, "status": "firing"},
                {"labels": {"alertname": "VpnDown", "severity": "warning"}, "status": "firing"},
            ]
        }
        out = ae.parse_alertmanager(body)
        assert len(out) == 2
        assert out[0] == {"name": "DiskFull", "severity": "critical", "status": "firing"}

    def test_resolved_status(self):
        body = {"alerts": [{"labels": {"alertname": "X"}, "status": "resolved"}]}
        out = ae.parse_alertmanager(body)
        assert out[0]["status"] == "resolved"

    def test_unknown_severity_becomes_none(self):
        body = {"alerts": [{"labels": {"alertname": "X", "severity": "bogus"}}]}
        assert ae.parse_alertmanager(body)[0]["severity"] == "none"

    def test_name_truncated(self):
        body = {"alerts": [{"labels": {"alertname": "A" * 40}}]}
        assert len(ae.parse_alertmanager(body)[0]["name"]) == 20

    def test_garbage_never_raises(self):
        for bad in (None, {}, {"alerts": "x"}, {"alerts": [None, 1, "y"]}, 42):
            assert ae.parse_alertmanager(bad) == [] or isinstance(ae.parse_alertmanager(bad), list)


class TestBuildEventsBlock:
    def test_empty(self):
        assert ae.build_events_block([]) == ae.EMPTY_EVENTS
        assert ae.build_events_block(None) == ae.EMPTY_EVENTS

    def test_picks_highest_severity_as_top(self):
        alerts = [
            {"name": "Warn", "severity": "warning", "status": "firing"},
            {"name": "Crit", "severity": "critical", "status": "firing"},
            {"name": "Info", "severity": "info", "status": "firing"},
        ]
        block = ae.build_events_block(alerts)
        assert block["n"] == 3
        assert block["top"] == "Crit"
        assert block["sev"] == "critical"

    def test_list_capped_at_four(self):
        alerts = [{"name": f"A{i}", "severity": "info", "status": "firing"} for i in range(6)]
        block = ae.build_events_block(alerts)
        assert len(block["list"]) == 4
        assert block["n"] == 6

    def test_ignores_resolved(self):
        alerts = [{"name": "R", "severity": "critical", "status": "resolved"}]
        assert ae.build_events_block(alerts)["n"] == 0


class TestAlertState:
    def test_ingest_and_snapshot(self):
        s = ae.AlertState()
        s.ingest([{"name": "Disk", "severity": "critical", "status": "firing"}], now=100.0)
        snap = s.snapshot(now=101.0)
        assert snap["n"] == 1
        assert snap["top"] == "Disk"

    def test_resolved_removes(self):
        s = ae.AlertState()
        s.ingest([{"name": "Disk", "severity": "critical", "status": "firing"}], now=100.0)
        s.ingest([{"name": "Disk", "severity": "critical", "status": "resolved"}], now=101.0)
        assert s.snapshot(now=102.0)["n"] == 0

    def test_ttl_expiry(self):
        s = ae.AlertState(ttl_sec=60)
        s.ingest([{"name": "Disk", "severity": "warning", "status": "firing"}], now=100.0)
        assert s.snapshot(now=130.0)["n"] == 1     # within TTL
        assert s.snapshot(now=200.0)["n"] == 0     # expired

    def test_refresh_extends_ttl(self):
        s = ae.AlertState(ttl_sec=60)
        s.ingest([{"name": "D", "severity": "warning", "status": "firing"}], now=100.0)
        s.ingest([{"name": "D", "severity": "warning", "status": "firing"}], now=150.0)
        assert s.snapshot(now=190.0)["n"] == 1     # refreshed at 150, still fresh


class TestAlertStateReplace:
    def test_replace_sets_full_set(self):
        s = ae.AlertState()
        s.replace([
            {"name": "A", "severity": "critical", "status": "firing"},
            {"name": "B", "severity": "warning", "status": "firing"},
        ], now=100.0)
        snap = s.snapshot(now=101.0)
        assert snap["n"] == 2
        assert snap["top"] == "A"     # critical outranks warning

    def test_replace_drops_absent_immediately(self):
        s = ae.AlertState()
        s.replace([{"name": "A", "severity": "critical", "status": "firing"}], now=100.0)
        # next poll no longer lists A -> it's gone at once (no TTL wait)
        s.replace([{"name": "B", "severity": "warning", "status": "firing"}], now=101.0)
        snap = s.snapshot(now=102.0)
        assert snap["n"] == 1 and snap["top"] == "B"

    def test_replace_empty_clears(self):
        s = ae.AlertState()
        s.replace([{"name": "A", "severity": "critical", "status": "firing"}], now=100.0)
        s.replace([], now=101.0)
        assert s.snapshot(now=102.0)["n"] == 0

    def test_replace_ignores_non_firing_and_nameless(self):
        s = ae.AlertState()
        s.replace([
            {"name": "A", "severity": "critical", "status": "resolved"},
            {"name": "", "severity": "critical", "status": "firing"},
            {"severity": "critical", "status": "firing"},
        ], now=100.0)
        assert s.snapshot(now=101.0)["n"] == 0
