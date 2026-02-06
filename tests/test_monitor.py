# Unit tests for Nocturne monitor: config loading and JSON payload building.
# Run from project root: python -m pytest tests/ -v
# Or: PYTHONPATH=src python -m pytest tests/ -v

import json
import os
import sys
import tempfile

# Allow importing monitor from src
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))
import monitor  # noqa: E402


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
        weather = {"temp": 22, "desc": "Clear", "icon": 0}
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
        assert "alert" not in back or back.get("alert") != "CRITICAL"
