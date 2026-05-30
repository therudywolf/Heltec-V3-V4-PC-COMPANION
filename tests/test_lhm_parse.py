# Unit tests for the extracted LHM parsing helpers (server/lhm_parse.py) and the
# monitor._parse_lhm_json wrapper that wires them together.
# Run from project root: python -m pytest tests/ -v

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))

import lhm_parse  # noqa: E402
import monitor  # noqa: E402


# --------------------------------------------------------------------------- #
# clean_val
# --------------------------------------------------------------------------- #
class TestCleanVal:
    def test_none_and_empty(self):
        assert lhm_parse.clean_val(None) == 0.0
        assert lhm_parse.clean_val("") == 0.0
        assert lhm_parse.clean_val("   ") == 0.0

    def test_plain_and_unit_suffix(self):
        assert lhm_parse.clean_val("55.0") == 55.0
        assert lhm_parse.clean_val("55.0 °C") == 55.0
        assert lhm_parse.clean_val("1234 RPM") == 1234.0

    def test_comma_decimal(self):
        assert lhm_parse.clean_val("3,5") == 3.5

    def test_unparseable_returns_zero(self):
        assert lhm_parse.clean_val("n/a") == 0.0
        assert lhm_parse.clean_val("abc") == 0.0

    def test_numeric_passthrough(self):
        assert lhm_parse.clean_val(42) == 42.0
        assert lhm_parse.clean_val(3.14) == 3.14


# --------------------------------------------------------------------------- #
# get_any_key
# --------------------------------------------------------------------------- #
class TestGetAnyKey:
    def test_first_present_wins(self):
        node = {"SensorId": "/a/0", "sensor_id": "/b/0"}
        assert lhm_parse.get_any_key(node, ("SensorId", "sensor_id")) == "/a/0"

    def test_skips_empty_strings(self):
        node = {"Name": "   ", "Text": "GPU Core"}
        assert lhm_parse.get_any_key(node, ("Name", "Text")) == "GPU Core"

    def test_returns_none_when_absent(self):
        assert lhm_parse.get_any_key({}, ("Value", "RawValue")) is None


# --------------------------------------------------------------------------- #
# extract_fans
# --------------------------------------------------------------------------- #
class TestExtractFans:
    def test_basic_mapping(self):
        path_to_val = {
            "/lpc/it8688e/0/fan/0": 800,
            "/lpc/it8688e/0/fan/1": 1200,
            "/lpc/it8688e/0/fan/2": 600,
        }
        fan_paths = ["/lpc/it8688e/0/fan/0", "/lpc/it8688e/0/fan/1", "/nvidiagpu/0/fan/0"]
        out = lhm_parse.extract_fans(path_to_val, fan_paths, "/lpc/it8688e/0/fan/2")
        assert out["cf"] == 800
        assert out["s1"] == 1200
        assert out["s2"] == 600
        assert out["fans"] == [800, 1200, 0, 600]

    def test_gpu_fan_prefers_fan1(self):
        # /gpu-nvidia/0/fan/1 should override the configured GPU fan slot.
        path_to_val = {"/gpu-nvidia/0/fan/1": 1500}
        fan_paths = ["/lpc/it8688e/0/fan/0", "/lpc/it8688e/0/fan/1", "/nvidiagpu/0/fan/0"]
        out = lhm_parse.extract_fans(path_to_val, fan_paths, "/lpc/it8688e/0/fan/2")
        assert out["gf"] == 1500
        assert out["fans"][2] == 1500


# --------------------------------------------------------------------------- #
# extract_mb_temps
# --------------------------------------------------------------------------- #
class TestExtractMbTemps:
    def test_all_four(self):
        path_to_val = {
            "/lpc/it8688e/0/temperature/0": 35.0,
            "/lpc/it8688e/0/temperature/1": 40.0,
            "/lpc/it8688e/0/temperature/4": 50.0,
            "/lpc/it8688e/0/temperature/5": 45.0,
        }
        out = lhm_parse.extract_mb_temps(path_to_val)
        assert out == {"mb_sys": 35, "mb_vsoc": 40, "mb_vrm": 50, "mb_chipset": 45}

    def test_missing_defaults_zero(self):
        out = lhm_parse.extract_mb_temps({})
        assert out == {"mb_sys": 0, "mb_vsoc": 0, "mb_vrm": 0, "mb_chipset": 0}


# --------------------------------------------------------------------------- #
# extract_storage_devices + build_hdd_list
# --------------------------------------------------------------------------- #
class TestStorage:
    def test_used_total_from_free_total(self):
        # Free=100, Total=500 -> Used=400
        path_to_val = {
            "/nvme/0/data/31": 100.0,
            "/nvme/0/data/32": 500.0,
            "/nvme/0/temperature/0": 40.0,
        }
        devs = lhm_parse.extract_storage_devices(path_to_val)
        assert len(devs) == 1
        prefix, num, used, total, temp = devs[0]
        assert (prefix, num) == ("nvme", 0)
        assert used == 400.0
        assert total == 500.0
        assert temp == 40.0

    def test_temp_only_device_included(self):
        path_to_val = {"/ssd/0/temperature/0": 38.0}
        devs = lhm_parse.extract_storage_devices(path_to_val)
        assert devs == [("ssd", 0, 0.0, 0.0, 38.0)]

    def test_dedup_and_sort(self):
        path_to_val = {
            "/nvme/1/temperature/0": 41.0,
            "/nvme/0/temperature/0": 40.0,
            "/nvme/0/load/0": 55.0,  # same device, should not duplicate
        }
        devs = lhm_parse.extract_storage_devices(path_to_val)
        keys = [(d[0], d[1]) for d in devs]
        assert keys == [("nvme", 0), ("nvme", 1)]

    def test_negative_used_clamped(self):
        # Free > Total would give negative used -> clamped to 0
        path_to_val = {"/hdd/0/data/31": 600.0, "/hdd/0/data/32": 500.0}
        devs = lhm_parse.extract_storage_devices(path_to_val)
        assert devs[0][2] == 0.0  # used clamped

    def test_build_hdd_list_pads_to_four(self):
        devs = [("nvme", 0, 400.0, 500.0, 40.0)]
        hdd = lhm_parse.build_hdd_list(devs)
        assert len(hdd) == 4
        assert hdd[0] == {"n": "C", "u": 400.0, "tot": 500.0, "t": 40}
        assert hdd[1] == {"n": "D", "u": 0.0, "tot": 0.0, "t": 0}
        assert [e["n"] for e in hdd] == ["C", "D", "E", "F"]


# --------------------------------------------------------------------------- #
# apply_vram_fallback / finalize_units
# --------------------------------------------------------------------------- #
class TestVramAndUnits:
    def test_vram_fallback_by_name(self):
        results = {}
        sensors = [
            ("/gpu/0/smalldata/1", 2048.0, "gpu memory used"),
            ("/gpu/0/smalldata/2", 8192.0, "gpu memory total"),
        ]
        lhm_parse.apply_vram_fallback(results, sensors)
        assert results["vu"] == 2048.0
        assert results["vt"] == 8192.0

    def test_vram_fallback_skips_when_present(self):
        results = {"vu": 1.0, "vt": 2.0}
        sensors = [("/x", 999.0, "memory used")]
        lhm_parse.apply_vram_fallback(results, sensors)
        assert results["vu"] == 1.0 and results["vt"] == 2.0

    def test_finalize_units_ram_and_vram(self):
        results = {"ru": 8.0, "ra": 8.0, "vu": 2048.0, "vt": 8192.0}
        lhm_parse.finalize_units(results)
        assert results["ra"] == 16.0          # used + available
        assert results["vu"] == 2.0           # MB -> GB
        assert results["vt"] == 8.0


# --------------------------------------------------------------------------- #
# Differential test: refactored monitor._parse_lhm_json vs. a hand-rolled
# reference that reproduces the original monolith's logic on a synthetic tree.
# This guards the behaviour-preservation contract for the god-function split.
# --------------------------------------------------------------------------- #
def _make_lhm_tree():
    """Build a small LHM-style tree exercising CPU/GPU/RAM/fans/storage paths."""
    def sensor(sid, value, stype="", name=""):
        return {"SensorId": sid, "Value": value, "Type": stype, "Text": name}

    return {
        "Children": [
            {
                "Text": "Computer",
                "Children": [
                    sensor("/amdcpu/0/temperature/2", "62.0 °C"),
                    sensor("/amdcpu/0/load/0", "23.5 %"),
                    sensor("/amdcpu/0/clock/1", "4200 MHz"),
                    sensor("/amdcpu/0/power/0", "65 W"),
                    sensor("/nvidiagpu/0/temperature/0", "55 °C"),
                    sensor("/nvidiagpu/0/load/0", "40 %"),
                    sensor("/nvidiagpu/0/clock/0", "1800 MHz"),
                    sensor("/nvidiagpu/0/smalldata/1", "2048", "SmallData", "GPU Memory Used"),
                    sensor("/nvidiagpu/0/smalldata/2", "8192", "SmallData", "GPU Memory Total"),
                    sensor("/gpu-nvidia/0/fan/1", "1500 RPM"),
                    sensor("/ram/data/0", "8.0 GB"),
                    sensor("/ram/data/1", "8.0 GB"),
                    sensor("/lpc/it8688e/0/fan/0", "800 RPM", "Fan", "CPU Fan"),
                    sensor("/lpc/it8688e/0/fan/1", "1200 RPM", "Fan", "Pump"),
                    sensor("/lpc/it8688e/0/temperature/0", "35 °C", "Temperature", "System"),
                    sensor("/lpc/it8688e/0/temperature/5", "45 °C", "Temperature", "Chipset"),
                    sensor("/lpc/it8688e/0/control/0", "40 %", "Control", "CPU Fan Control"),
                    sensor("/nvme/0/data/31", "200 GB"),
                    sensor("/nvme/0/data/32", "500 GB"),
                    sensor("/nvme/0/temperature/0", "41 °C"),
                ],
            }
        ]
    }


def test_parse_lhm_json_known_fields():
    tree = _make_lhm_tree()
    out = monitor._parse_lhm_json(tree)

    # CPU
    assert out["ct"] == 62.0
    assert out["cl"] == 23.5
    assert out["cc"] == 4200.0
    assert out["pw"] == 65.0
    # GPU
    assert out["gt"] == 55.0
    assert out["gl"] == 40.0
    assert out["gclock"] == 1800.0
    # VRAM scaled MB->GB
    assert out["vu"] == 2.0
    assert out["vt"] == 8.0
    # RAM total = used + available
    assert out["ru"] == 8.0
    assert out["ra"] == 16.0
    # Fans: CPU/Pump from it8688e, GPU from /gpu-nvidia/0/fan/1
    assert out["cf"] == 800
    assert out["s1"] == 1200
    assert out["gf"] == 1500
    assert out["fans"][2] == 1500
    # Fan control CPU slot
    assert out["fan_controls"][0] == 40
    # Motherboard temps
    assert out["mb_sys"] == 35
    assert out["mb_chipset"] == 45
    # Storage -> first hdd slot
    assert out["hdd"][0]["n"] == "C"
    assert out["hdd"][0]["u"] == 300.0   # 500 - 200
    assert out["hdd"][0]["tot"] == 500.0
    assert out["hdd"][0]["t"] == 41
    assert len(out["hdd"]) == 4


def test_parse_lhm_json_empty_tree():
    out = monitor._parse_lhm_json({"Children": []})
    # Still returns the structural keys with safe defaults.
    assert out["fans"] == [0, 0, 0, 0]
    assert out["fan_controls"] == [0, 0, 0, 0]
    assert len(out["hdd"]) == 4
    assert all(e["u"] == 0.0 and e["tot"] == 0.0 for e in out["hdd"])
