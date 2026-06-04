"""Unit tests for server/services_panel.py (resource status panel, #18)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import services_panel as sp  # noqa: E402


class TestServiceStatus:
    def test_down_when_unreachable(self):
        assert sp.service_status(False, -1) == "down"

    def test_up_when_fast(self):
        assert sp.service_status(True, 12) == "up"

    def test_warn_when_slow(self):
        assert sp.service_status(True, sp.SLOW_MS + 1) == "warn"


class TestBuildService:
    def test_reachable(self):
        s = sp.build_service("lmstudio", "LM Studio", True, 7.4)
        assert s == {"id": "lmstudio", "name": "LM Studio", "st": "up", "ms": 7}

    def test_unreachable_forces_ms_minus1(self):
        s = sp.build_service("x", "Down Svc", False, 999)
        assert s["st"] == "down" and s["ms"] == -1

    def test_truncates_id_and_name(self):
        s = sp.build_service("a" * 30, "b" * 30, True, 1)
        assert len(s["id"]) == 10 and len(s["name"]) == 16

    def test_bad_ms_is_na(self):
        s = sp.build_service("x", "X", True, None)
        assert s["ms"] == -1


class TestBuildSvcBlock:
    def test_empty(self):
        assert sp.build_svc_block([]) == sp.EMPTY_SVC
        assert sp.build_svc_block(None) == sp.EMPTY_SVC

    def test_counts_up(self):
        svcs = [
            sp.build_service("a", "A", True, 5),
            sp.build_service("b", "B", False, 0),
            sp.build_service("c", "C", True, sp.SLOW_MS + 5),  # warn counts as up
        ]
        block = sp.build_svc_block(svcs)
        assert block["n"] == 3
        assert block["up"] == 2  # A (up) + C (warn); B is down

    def test_caps_at_max(self):
        svcs = [sp.build_service(f"s{i}", f"S{i}", True, 1) for i in range(20)]
        assert len(sp.build_svc_block(svcs)["list"]) == sp.MAX_SERVICES
