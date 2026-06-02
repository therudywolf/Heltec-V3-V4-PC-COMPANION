"""Unit tests for server/prometheus_source.py (pure parsing/mapping)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import prometheus_source as ps  # noqa: E402


class TestParseExposition:
    def test_skips_comments_and_blanks(self):
        text = "# HELP foo bar\n# TYPE foo gauge\n\nfoo 1.5\n"
        out = ps.parse_exposition(text)
        assert out == [("foo", {}, 1.5)]

    def test_parses_labels(self):
        text = 'windows_net_bytes_received_total{nic="eth0"} 123\n'
        out = ps.parse_exposition(text)
        assert out == [("windows_net_bytes_received_total", {"nic": "eth0"}, 123.0)]

    def test_multiple_labels(self):
        text = 'm{a="1",b="two"} 3.0'
        name, labels, val = ps.parse_exposition(text)[0]
        assert name == "m"
        assert labels == {"a": "1", "b": "two"}
        assert val == 3.0

    def test_ignores_unparseable_value(self):
        text = "broken notanumber\ngood 2"
        out = ps.parse_exposition(text)
        assert ("good", {}, 2.0) in out
        assert all(n != "broken" for n, _, _ in out)

    def test_empty(self):
        assert ps.parse_exposition("") == []
        assert ps.parse_exposition(None) == []


class TestBuildHwFromExposition:
    def test_ram_used_and_total(self):
        # 16 GiB total, 4 GiB free -> 12 GiB used.
        total = 16 * ps.BYTES_PER_GB
        free = 4 * ps.BYTES_PER_GB
        text = (
            f"{ps.MEM_TOTAL_METRIC} {total}\n"
            f"{ps.MEM_AVAIL_METRIC} {free}\n"
        )
        hw = ps.build_hw_from_exposition(text)
        assert hw["ra"] == 16.0
        assert hw["ru"] == 12.0

    def test_disks_mapped_to_letters(self):
        size_c = 500 * ps.BYTES_PER_GB
        free_c = 100 * ps.BYTES_PER_GB
        text = (
            f'{ps.LOGICAL_SIZE_METRIC}{{volume="C:"}} {size_c}\n'
            f'{ps.LOGICAL_FREE_METRIC}{{volume="C:"}} {free_c}\n'
        )
        hw = ps.build_hw_from_exposition(text)
        assert "hdd" in hw
        c = hw["hdd"][0]
        assert c["n"] == "C"
        assert c["tot"] == 500.0
        assert c["u"] == 400.0

    def test_network_counters_passthrough(self):
        text = (
            f'{ps.NET_RECV_METRIC}{{nic="eth0"}} 1000\n'
            f'{ps.NET_SENT_METRIC}{{nic="eth0"}} 2000\n'
        )
        hw = ps.build_hw_from_exposition(text)
        assert hw["_net_counters"] == {"recv": 1000.0, "sent": 2000.0}

    def test_network_summed_across_nics(self):
        text = (
            f'{ps.NET_RECV_METRIC}{{nic="eth0"}} 1000\n'
            f'{ps.NET_RECV_METRIC}{{nic="wifi"}} 500\n'
        )
        hw = ps.build_hw_from_exposition(text)
        assert hw["_net_counters"]["recv"] == 1500.0

    def test_missing_inputs_omit_keys(self):
        hw = ps.build_hw_from_exposition("unrelated_metric 1\n")
        assert "ru" not in hw and "hdd" not in hw and "_net_counters" not in hw

    def test_never_raises_on_garbage(self):
        for bad in ("", "x", "{}{}{}", "a{ 1", "a} 1"):
            ps.build_hw_from_exposition(bad)  # must not raise


class TestMapQueryResults:
    def test_drops_none_and_types(self):
        hw = ps.map_query_results({"cl": 55.0, "gl": None, "ru": 12.34})
        assert hw["cl"] == 55
        assert hw["ru"] == 12.3
        assert "gl" not in hw

    def test_empty(self):
        assert ps.map_query_results({}) == {}
        assert ps.map_query_results(None) == {}


class TestCpuLoadFromIdle:
    def _m(self, idle, busy):
        # one idle-mode series + one busy-mode series => total = idle+busy
        return [
            ("windows_cpu_time_total", {"core": "0,0", "mode": "idle"}, float(idle)),
            ("windows_cpu_time_total", {"core": "0,0", "mode": "user"}, float(busy)),
        ]

    def test_first_sample_none(self):
        st = {"idle": None, "total": None}
        assert ps.cpu_load_from_idle(self._m(100, 0), st) is None

    def test_50pct_load(self):
        st = {"idle": None, "total": None}
        ps.cpu_load_from_idle(self._m(100, 100), st)          # prime
        # next: idle +10, busy +10 => d_idle=10 d_total=20 => load 50%
        load = ps.cpu_load_from_idle(self._m(110, 110), st)
        assert load == 50

    def test_full_idle_zero_load(self):
        st = {"idle": None, "total": None}
        ps.cpu_load_from_idle(self._m(100, 100), st)
        load = ps.cpu_load_from_idle(self._m(120, 100), st)   # only idle grew
        assert load == 0

    def test_no_counter_none(self):
        assert ps.cpu_load_from_idle([], {"idle": None, "total": None}) is None

    def test_clamped(self):
        st = {"idle": None, "total": None}
        ps.cpu_load_from_idle(self._m(100, 100), st)
        load = ps.cpu_load_from_idle(self._m(100, 130), st)   # all busy => 100
        assert load == 100


class TestMergeHw:
    def test_overlay_wins(self):
        base = {"cl": 10, "ct": 60, "gl": 20}
        overlay = {"cl": 80, "ru": 12.0}
        out = ps.merge_hw(base, overlay)
        assert out["cl"] == 80   # overlay wins
        assert out["ct"] == 60   # base kept (LHM temp)
        assert out["ru"] == 12.0

    def test_none_overlay_keeps_base(self):
        out = ps.merge_hw({"cl": 10}, {"cl": None})
        assert out["cl"] == 10

    def test_does_not_mutate(self):
        base = {"cl": 10}
        ps.merge_hw(base, {"cl": 99})
        assert base["cl"] == 10
