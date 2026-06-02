"""Unit tests for server/forest_panel.py (node aggregation)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import forest_panel as fp  # noqa: E402


class TestNodeStatus:
    def test_down_when_unreachable(self):
        assert fp.node_status(False, 10, 10, 10) == "down"

    def test_up_when_low(self):
        assert fp.node_status(True, 10, 20, 30) == "up"

    def test_warn_when_cpu_high(self):
        assert fp.node_status(True, 95, 10, 10) == "warn"

    def test_warn_when_disk_high(self):
        assert fp.node_status(True, 10, 10, 92) == "warn"

    def test_na_resources_dont_warn(self):
        assert fp.node_status(True, -1, -1, -1) == "up"


class TestBuildNode:
    def test_full_metrics(self):
        n = fp.build_node("srv", "Forestserver", {"cpu": 12.4, "ram": 47, "disk": 63, "extra": "load 0.4"})
        assert n["id"] == "srv"
        assert n["name"] == "Forestserver"
        assert n["st"] == "up"
        assert n["cpu"] == 12   # rounded
        assert n["ram"] == 47
        assert n["disk"] == 63
        assert n["extra"] == "load 0.4"

    def test_unreachable_is_down_with_na(self):
        n = fp.build_node("pc", "PC", None)
        assert n["st"] == "down"
        assert n["cpu"] == -1 and n["ram"] == -1 and n["disk"] == -1

    def test_empty_metrics_is_down(self):
        n = fp.build_node("pc", "PC", {})
        assert n["st"] == "down"

    def test_high_resource_warns(self):
        n = fp.build_node("pc", "PC", {"cpu": 99})
        assert n["st"] == "warn"

    def test_name_and_extra_clamped(self):
        n = fp.build_node("x" * 20, "y" * 40, {"cpu": 1, "extra": "z" * 40})
        assert len(n["id"]) == 8
        assert len(n["name"]) == 16
        assert len(n["extra"]) == 16

    def test_invalid_pct_is_na(self):
        n = fp.build_node("a", "A", {"cpu": "bogus"})
        assert n["cpu"] == -1

    def test_pct_clamped_0_100(self):
        assert fp.build_node("a", "A", {"cpu": 250})["cpu"] == 100
        assert fp.build_node("a", "A", {"cpu": -5})["cpu"] == 0

    def test_never_raises(self):
        for bad in (None, {}, {"cpu": None}, {"extra": 123}):
            fp.build_node("a", "A", bad)


class TestBuildForestBlock:
    def test_empty(self):
        assert fp.build_forest_block([]) == fp.EMPTY_FOREST
        assert fp.build_forest_block(None) == fp.EMPTY_FOREST

    def test_counts_up_and_total(self):
        nodes = [
            fp.build_node("a", "A", {"cpu": 10}),       # up
            fp.build_node("b", "B", {"cpu": 99}),       # warn (counts as up/reachable)
            fp.build_node("c", "C", None),              # down
        ]
        block = fp.build_forest_block(nodes)
        assert block["n"] == 3
        assert block["up"] == 2     # up + warn reachable, down excluded
        assert len(block["nodes"]) == 3

    def test_all_down(self):
        nodes = [fp.build_node("a", "A", None), fp.build_node("b", "B", None)]
        block = fp.build_forest_block(nodes)
        assert block["n"] == 2 and block["up"] == 0
