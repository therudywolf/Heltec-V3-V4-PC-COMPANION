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


class TestBuildNodesFromQueries:
    DEFS = [
        {"id": "srv", "name": "Server", "cpu": "Q_CPU", "ram": "Q_RAM", "disk": "Q_DISK"},
        {"id": "pc", "name": "PC", "cpu": "P_CPU", "ram": "P_RAM"},
    ]

    def test_runs_each_expr_and_builds_nodes(self):
        seen = []

        def run(expr):
            seen.append(expr)
            return {"Q_CPU": 23.0, "Q_RAM": 49.0, "Q_DISK": 46.0,
                    "P_CPU": 22.0, "P_RAM": 88.0}.get(expr)

        nodes = fp.build_nodes_from_queries(self.DEFS, run)
        assert [n["id"] for n in nodes] == ["srv", "pc"]
        assert nodes[0]["cpu"] == 23 and nodes[0]["ram"] == 49 and nodes[0]["disk"] == 46
        assert nodes[0]["st"] == "up"
        # PC has no disk expr -> n/a, others present
        assert nodes[1]["cpu"] == 22 and nodes[1]["ram"] == 88 and nodes[1]["disk"] == -1
        # only defined exprs were queried (no None disk for PC)
        assert "P_RAM" in seen and len([e for e in seen if e is None]) == 0

    def test_node_all_none_is_down(self):
        nodes = fp.build_nodes_from_queries(self.DEFS, lambda e: None)
        assert all(n["st"] == "down" for n in nodes)
        assert all(n["cpu"] == -1 for n in nodes)

    def test_partial_metrics_still_reachable(self):
        # Only CPU resolves -> node is reachable (up/warn), ram/disk n/a
        nodes = fp.build_nodes_from_queries(
            [self.DEFS[0]], lambda e: 95.0 if e == "Q_CPU" else None)
        assert nodes[0]["st"] == "warn"   # cpu 95 >= WARN
        assert nodes[0]["ram"] == -1 and nodes[0]["disk"] == -1

    def test_run_query_exception_treated_as_miss(self):
        def boom(expr):
            raise RuntimeError("network down")

        nodes = fp.build_nodes_from_queries(self.DEFS, boom)
        assert all(n["st"] == "down" for n in nodes)

    def test_empty_defs(self):
        assert fp.build_nodes_from_queries([], lambda e: 1.0) == []
        assert fp.build_nodes_from_queries(None, lambda e: 1.0) == []

    def test_default_nodes_have_exprs(self):
        # Guard the shipped roster: every default node must carry a cpu query.
        assert fp.DEFAULT_NODES
        for d in fp.DEFAULT_NODES:
            assert d.get("cpu"), d
