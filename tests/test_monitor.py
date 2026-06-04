# Unit tests for Nocturne monitor: config loading and JSON payload building.
# Run from project root: python -m pytest tests/ -v
# Or: PYTHONPATH=src python -m pytest tests/ -v

import json
import os
import sys
import tempfile
from collections import namedtuple

# Allow importing monitor from server
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import monitor  # noqa: E402

# Minimal stand-in for psutil's memory_info object (only .rss is read).
_Mem = namedtuple("_Mem", ["rss"])


def _mem_mb(mb):
    return _Mem(rss=int(mb * 1024 * 1024))


class TestLoadConfig:
    def test_load_config_defaults(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as f:
            f.write("{}")
            path = f.name
        old = monitor.CONFIG_PATH
        monitor.CONFIG_PATH = path
        try:
            cfg = monitor.load_config()
            assert cfg["host"] == "0.0.0.0"
            assert cfg["port"] == 8090
            assert "lhm_url" in cfg
            assert cfg["limits"]["gpu"] == 80
            assert cfg["limits"]["cpu"] == 75
        finally:
            monitor.CONFIG_PATH = old
        os.unlink(path)

    def test_load_config_custom(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as f:
            json.dump({
                "host": "127.0.0.1",
                "port": 9999,
                "lhm_url": "http://test/data.json",
                "limits": {"gpu": 72, "cpu": 85},
                "weather_city": "London",
            }, f)
            path = f.name
        old = monitor.CONFIG_PATH
        monitor.CONFIG_PATH = path
        try:
            cfg = monitor.load_config()
            assert cfg["host"] == "127.0.0.1"
            assert cfg["port"] == 9999
            assert cfg["lhm_url"] == "http://test/data.json"
            assert cfg["limits"]["gpu"] == 72
            assert cfg["limits"]["cpu"] == 85
            assert cfg["weather_city"] == "London"
        finally:
            monitor.CONFIG_PATH = old
        os.unlink(path)


class TestBuildPayload:
    def test_build_payload_structure(self):
        hw = {
            "ct": 50, "gt": 45, "cl": 20, "gl": 10, "ru": 8.0, "ra": 16.0,
            "pw": 65, "cc": 4, "gh": 0, "gv": 0, "gclock": 3600, "vclock": 1800,
            "gtdp": 120, "nd": 0, "nu": 0, "cf": 800, "s1": 0, "s2": 0, "gf": 0,
            "fans": [800, 0, 600, 0], "fan_controls": [40, 0, 50, 0],
            "hdd": [{"n": "C", "u": 100.0, "tot": 500.0, "t": 35}],
            "vu": 2.5, "vt": 8.0, "ch": 40,
            "mb_sys": 35, "mb_vsoc": 0, "mb_vrm": 0, "mb_chipset": 0,
            "dr": 0, "dw": 0,
        }
        media = {"art": "Artist", "trk": "Track", "play": True, "idle": False, "media_status": "PLAYING"}
        weather = {"temp": 22, "desc": "Clear", "icon": 0,
                   "forecast": [[12, 21, 2], [10, 19, 3]]}
        top_procs = [["a", 5], ["b", 3], ["c", 2]]
        top_procs_ram = [["x", 512], ["y", 256]]
        net = (1000, 2000)
        disk = (0, 0)
        import time
        monitor._weather_first_ok = True
        monitor._last_alert = (None, None)
        payload = monitor.build_payload(hw, media, weather, top_procs, top_procs_ram, net, disk, 12, time.time())
        assert payload["ct"] == 50
        assert payload["gt"] == 45
        assert payload["cl"] == 20
        assert payload["gl"] == 10
        assert payload["ru"] == 8.0
        assert payload["ra"] == 16.0
        assert payload["art"] == "Artist"
        assert payload["trk"] == "Track"
        assert payload["mp"] is True
        assert payload["media_status"] == "PLAYING"
        assert payload["wt"] == 22
        assert payload["wd"] == "Clear"
        # Compact daily forecast: list of [tmin, tmax, wmocode] int-triples (<=5).
        assert payload["wf"] == [[12, 21, 2], [10, 19, 3]]
        assert len(payload["wf"]) <= 5
        for entry in payload["wf"]:
            assert len(entry) == 3 and all(isinstance(v, int) for v in entry)
        assert payload["tp"] == top_procs
        assert payload["tr"] == top_procs_ram
        assert "hdd" in payload
        assert len(payload["hdd"]) == 4
        assert payload["hdd"][0]["n"] == "C"
        assert payload["hdd"][0]["t"] == 35

    def test_build_payload_json_serializable(self):
        hw = {"ct": 0, "gt": 0, "cl": 0, "gl": 0, "ru": 0, "ra": 0, "hdd": [], "fans": [0, 0, 0, 0], "fan_controls": [0, 0, 0, 0]}
        media = {"art": "", "trk": "", "play": False, "idle": False, "media_status": "PAUSED"}
        weather = {"temp": 0, "desc": "", "icon": 0}
        import time
        monitor._weather_first_ok = False
        monitor._last_alert = (None, None)
        payload = monitor.build_payload(hw, media, weather, [], [], (0, 0), (0, 0), 0, time.time())
        s = json.dumps(payload, separators=(",", ":"))
        assert len(s) > 0
        back = json.loads(s)
        assert back["ct"] == 0
        # No forecast in the weather dict -> wf defaults to an empty list.
        assert back["wf"] == []
        assert "alert" not in back or back.get("alert") != "CRITICAL"


class TestCollectTopProcesses:
    """The merged single-pass process walk (replaces the two separate iters)."""

    def test_both_lists_from_one_batch(self):
        info = [
            {"name": "chrome.exe", "cpu_percent": 15.0, "memory_info": _mem_mb(800)},
            {"name": "python.exe", "cpu_percent": 30.0, "memory_info": _mem_mb(120)},
            {"name": "idle.exe", "cpu_percent": 0.0, "memory_info": _mem_mb(5)},
            {"name": "svc.exe", "cpu_percent": 2.0, "memory_info": _mem_mb(50)},
        ]
        cpu, ram = monitor._collect_top_processes(info, cpu_n=3, ram_n=2)
        # CPU: cpu_percent > 0 only, sorted desc, top 3.
        assert cpu == [
            {"n": "python.exe", "c": 30},
            {"n": "chrome.exe", "c": 15},
            {"n": "svc.exe", "c": 2},
        ]
        # RAM: rss > 10 MB only, sorted desc, top 2 (idle.exe at 5 MB dropped).
        assert ram == [
            {"n": "chrome.exe", "r": 800},
            {"n": "python.exe", "r": 120},
        ]

    def test_cpu_zero_and_falsy_excluded(self):
        info = [
            {"name": "a", "cpu_percent": 0.0, "memory_info": _mem_mb(100)},
            {"name": "b", "cpu_percent": None, "memory_info": _mem_mb(100)},
            {"name": "c", "cpu_percent": 1.0, "memory_info": _mem_mb(100)},
        ]
        cpu, _ = monitor._collect_top_processes(info, cpu_n=3, ram_n=3)
        assert cpu == [{"n": "c", "c": 1}]

    def test_ram_threshold_and_memcompression_skip(self):
        info = [
            # Windows reports this as "MemCompression" (no space); the substring
            # check is "memcompression" in name.lower().
            {"name": "MemCompression", "cpu_percent": 0.0, "memory_info": _mem_mb(2000)},
            {"name": "keep.exe", "cpu_percent": 0.0, "memory_info": _mem_mb(11)},
            {"name": "drop.exe", "cpu_percent": 0.0, "memory_info": _mem_mb(10)},  # not > 10
            {"name": "nomem.exe", "cpu_percent": 0.0, "memory_info": None},
        ]
        _, ram = monitor._collect_top_processes(info, cpu_n=3, ram_n=5)
        assert ram == [{"n": "keep.exe", "r": 11}]

    def test_name_handling_strip_and_truncate(self):
        long_name = "a" * 40
        info = [{"name": "  " + long_name + "  ", "cpu_percent": 5.0, "memory_info": _mem_mb(100)}]
        cpu, ram = monitor._collect_top_processes(info, cpu_n=1, ram_n=1)
        # CPU keeps the raw (unstripped) name, truncated to 20 chars -> leading
        # spaces are part of the slice.
        assert cpu[0]["n"] == ("  " + long_name)[:20]
        # RAM strips first, then truncates to 20.
        assert ram[0]["n"] == long_name[:20]

    def test_matches_reference_two_pass_logic(self):
        """Merged walk == the original pair of per-list passes, on one input."""
        info = [
            {"name": "p1", "cpu_percent": 7.0, "memory_info": _mem_mb(300)},
            {"name": "p2", "cpu_percent": 0.0, "memory_info": _mem_mb(900)},
            {"name": "p3", "cpu_percent": 12.0, "memory_info": _mem_mb(15)},
            {"name": "memcompression", "cpu_percent": 4.0, "memory_info": _mem_mb(500)},
            {"name": "p5", "cpu_percent": 1.0, "memory_info": _mem_mb(9)},
        ]

        def ref_cpu(n):
            out = []
            for i in info:
                if i.get("cpu_percent") and i["cpu_percent"] > 0:
                    out.append({"n": (i.get("name") or "")[:20], "c": int(i["cpu_percent"])})
            out.sort(key=lambda x: x["c"], reverse=True)
            return out[:n]

        def ref_ram(n):
            out = []
            for i in info:
                name = (i.get("name") or "").strip()
                if "memcompression" in name.lower():
                    continue
                if i.get("memory_info"):
                    mb = i["memory_info"].rss / (1024 * 1024)
                    if mb > 10:
                        out.append({"n": name[:20], "r": int(mb)})
            out.sort(key=lambda x: x["r"], reverse=True)
            return out[:n]

        cpu, ram = monitor._collect_top_processes(info, cpu_n=3, ram_n=2)
        assert cpu == ref_cpu(3)
        assert ram == ref_ram(2)


class TestEncodePayload:
    def test_roundtrip_normal_payload(self):
        payload = {"ct": 42, "gt": 50, "ru": 8.0}
        raw = monitor.encode_payload(payload)
        assert isinstance(raw, bytes)
        assert raw.endswith(b"\n")
        # Compact separators (no spaces) and a faithful round-trip.
        assert b", " not in raw
        assert json.loads(raw.decode("utf-8")) == payload

    def test_oversize_falls_back_to_minimal(self):
        big = {"big": "x" * (monitor.MAX_PAYLOAD_BYTES + 100)}
        raw = monitor.encode_payload(big)
        back = json.loads(raw.decode("utf-8"))
        assert back == {"ct": 0, "gt": 0, "cl": 0, "gl": 0, "ru": 0, "ra": 0}
        assert len(raw) <= monitor.MAX_PAYLOAD_BYTES


class TestParseLhmFromText:
    def test_from_text_equals_dict_parse(self):
        tree = {
            "Children": [
                {
                    "Text": "Computer",
                    "Children": [
                        {"SensorId": "/amdcpu/0/temperature/2", "Value": "62.0 °C"},
                        {"SensorId": "/amdcpu/0/load/0", "Value": "23.5 %"},
                        {"SensorId": "/ram/data/0", "Value": "8.0 GB"},
                        {"SensorId": "/ram/data/1", "Value": "8.0 GB"},
                        {"SensorId": "/nvme/0/data/31", "Value": "200 GB"},
                        {"SensorId": "/nvme/0/data/32", "Value": "500 GB"},
                    ],
                }
            ]
        }
        from_dict = monitor._parse_lhm_json(tree)
        from_text = monitor._parse_lhm_json_from_text(json.dumps(tree))
        assert from_text == from_dict


class TestScalarFromPromResult:
    def test_vector_value(self):
        data = {"status": "success", "data": {"resultType": "vector",
                "result": [{"metric": {}, "value": [1700000000.0, "23.09"]}]}}
        assert monitor._scalar_from_prom_result(data) == 23.09

    def test_scalar_value(self):
        data = {"data": {"resultType": "scalar", "result": [1700000000, "42"]}}
        # scalar result is [ts, "v"] directly (not a dict) — first element used
        # as the "series"; our extractor reads index [1] of that list.
        assert monitor._scalar_from_prom_result(
            {"data": {"result": [{"value": [0, "42"]}]}}) == 42.0

    def test_empty_result_is_none(self):
        assert monitor._scalar_from_prom_result({"data": {"result": []}}) is None

    def test_bad_shapes_are_none(self):
        for bad in (None, {}, {"data": {}}, {"data": {"result": [{"value": [1]}]}},
                    {"data": {"result": [{"value": "x"}]}}):
            assert monitor._scalar_from_prom_result(bad) is None

    def test_non_numeric_value_is_none(self):
        data = {"data": {"result": [{"value": [0, "NaNNN"]}]}}
        assert monitor._scalar_from_prom_result(data) is None
