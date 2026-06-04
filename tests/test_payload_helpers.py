# Unit tests for the extracted payload helpers (server/payload.py): HDD slot
# normalisation, the RED-ALERT threshold/hysteresis state machine, and the
# change-detection snapshot.
# Run from project root: python -m pytest tests/ -v

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))

import payload as payload_mod  # noqa: E402


# Threshold/hysteresis dicts mirroring monitor.py's module constants.
THRESHOLDS = {
    "cpu_temp": 87, "gpu_temp": 68, "cpu_load": 90,
    "gpu_load": 100, "vram_load": 95, "ram_gb": 30,
}
HYST = {"cpu_temp": 5, "gpu_temp": 5, "load": 5, "ram_gb": 2}


def _metrics(ct=0, gt=0, cl=0, gl=0, gv=0, ram=0.0):
    return {"ct": ct, "gt": gt, "cl": cl, "gl": gl, "gv": gv, "ram": ram}


# --------------------------------------------------------------------------- #
# normalize_hdd
# --------------------------------------------------------------------------- #
class TestNormalizeHdd:
    def test_pads_and_rounds(self):
        hw_hdd = [{"n": "C", "u": 100.06, "tot": 500.04, "t": 35}]
        out = payload_mod.normalize_hdd(hw_hdd, fallback_enabled=False)
        assert len(out) == 4
        assert out[0] == {"n": "C", "u": 100.1, "tot": 500.0, "t": 35}
        assert out[1]["n"] == "D"
        assert out[3]["n"] == "F"

    def test_fallback_disabled_no_call(self):
        called = {"n": 0}

        def fb():
            called["n"] += 1
            return [{"n": "C", "u": 1.0, "tot": 2.0, "t": 0}] * 4

        out = payload_mod.normalize_hdd([], fallback_enabled=False, get_fallback_disks=fb)
        assert called["n"] == 0
        assert all(e["tot"] == 0.0 for e in out)

    def test_fallback_full_replace_when_no_capacity_no_temp(self):
        def fb():
            return [{"n": "C", "u": 111.0, "tot": 222.0, "t": 0}] + [
                {"n": x, "u": 0.0, "tot": 0.0, "t": 0} for x in ("D", "E", "F")
            ]

        out = payload_mod.normalize_hdd([], fallback_enabled=True, get_fallback_disks=fb)
        assert out[0]["u"] == 111.0 and out[0]["tot"] == 222.0

    def test_fallback_merge_keeps_lhm_temp(self):
        # LHM gave temps but no capacity -> merge psutil capacity, keep temp.
        hw_hdd = [{"n": "C", "u": 0.0, "tot": 0.0, "t": 44}]

        def fb():
            return [{"n": "C", "u": 300.0, "tot": 900.0, "t": 0}] + [
                {"n": x, "u": 0.0, "tot": 0.0, "t": 0} for x in ("D", "E", "F")
            ]

        out = payload_mod.normalize_hdd(hw_hdd, fallback_enabled=True, get_fallback_disks=fb)
        assert out[0]["u"] == 300.0
        assert out[0]["tot"] == 900.0
        assert out[0]["t"] == 44  # LHM temp preserved

    def test_no_fallback_when_capacity_present(self):
        called = {"n": 0}

        def fb():
            called["n"] += 1
            return []

        hw_hdd = [{"n": "C", "u": 10.0, "tot": 20.0, "t": 0}]
        out = payload_mod.normalize_hdd(hw_hdd, fallback_enabled=True, get_fallback_disks=fb)
        assert called["n"] == 0
        assert out[0]["tot"] == 20.0


# --------------------------------------------------------------------------- #
# evaluate_alert
# --------------------------------------------------------------------------- #
class TestEvaluateAlert:
    def test_no_alert_when_below(self):
        fields, new = payload_mod.evaluate_alert(
            _metrics(ct=50, gt=50), (None, None), THRESHOLDS, HYST
        )
        assert fields == {"alert": "", "target_screen": "", "alert_metric": ""}
        assert new == (None, None)

    def test_cpu_temp_trips(self):
        fields, new = payload_mod.evaluate_alert(
            _metrics(ct=90), (None, None), THRESHOLDS, HYST
        )
        assert fields["alert"] == "CRITICAL"
        assert fields["target_screen"] == "CPU"
        assert fields["alert_metric"] == "ct"
        assert new == ("CPU", "ct")

    def test_priority_cpu_temp_over_gpu_temp(self):
        fields, new = payload_mod.evaluate_alert(
            _metrics(ct=90, gt=90), (None, None), THRESHOLDS, HYST
        )
        assert new == ("CPU", "ct")

    def test_ram_gb_trips(self):
        fields, new = payload_mod.evaluate_alert(
            _metrics(ram=31.0), (None, None), THRESHOLDS, HYST
        )
        assert new == ("RAM", "ram")
        assert fields["alert_metric"] == "ram"

    def test_hysteresis_holds_alert_in_band(self):
        # Active CPU-temp alert; value drops to 84 which is within [82, 87) band
        # (threshold 87 - hyst 5 = 82) so the alert must persist.
        fields, new = payload_mod.evaluate_alert(
            _metrics(ct=84), ("CPU", "ct"), THRESHOLDS, HYST
        )
        assert new == ("CPU", "ct")
        assert fields["alert"] == "CRITICAL"

    def test_hysteresis_clears_below_band(self):
        # Drops to 81 (< 82) -> clears.
        fields, new = payload_mod.evaluate_alert(
            _metrics(ct=81), ("CPU", "ct"), THRESHOLDS, HYST
        )
        assert new == (None, None)
        assert fields["alert"] == ""

    def test_vram_load_trips(self):
        fields, new = payload_mod.evaluate_alert(
            _metrics(gv=96), (None, None), THRESHOLDS, HYST
        )
        assert new == ("GPU", "gv")


# --------------------------------------------------------------------------- #
# payload_snapshot
# --------------------------------------------------------------------------- #
class TestPayloadSnapshot:
    def test_tuple_order(self):
        p = {"ct": 1, "gt": 2, "cl": 3, "gl": 4, "nd": 5, "nu": 6, "ru": 7, "ra": 8}
        assert payload_mod.payload_snapshot(p) == (1, 2, 3, 4, 5, 6, 7, 8)

    def test_defaults_zero(self):
        assert payload_mod.payload_snapshot({}) == (0, 0, 0, 0, 0, 0, 0, 0)


# --------------------------------------------------------------------------- #
# parse_dashboard_stats (Docker "dock" block, #5)
# --------------------------------------------------------------------------- #
class TestParseDashboardStats:
    def test_real_feed_shape_string_numbers(self):
        # Mirrors the live dashboard.example.com /stats.json: numbers are JSON strings.
        stats = {
            "server": {"containers": "46", "containers_up": "46", "cpu": "42.1"},
            "pc": {"lmstudio_up": "1", "docker_running": "7"},
            "alerts": "37",
        }
        assert payload_mod.parse_dashboard_stats(stats) == {"n": 46, "up": 46}

    def test_partial_down_counts(self):
        stats = {"server": {"containers": "46", "containers_up": "40"}}
        assert payload_mod.parse_dashboard_stats(stats) == {"n": 46, "up": 40}

    def test_up_clamped_to_total(self):
        stats = {"server": {"containers": "5", "containers_up": "9"}}
        assert payload_mod.parse_dashboard_stats(stats) == {"n": 5, "up": 5}

    def test_int_and_float_values(self):
        stats = {"server": {"containers": 10, "containers_up": 8.0}}
        assert payload_mod.parse_dashboard_stats(stats) == {"n": 10, "up": 8}

    def test_missing_block_is_empty(self):
        assert payload_mod.parse_dashboard_stats({"pc": {}}) == payload_mod.EMPTY_DOCK
        assert payload_mod.parse_dashboard_stats({}) == payload_mod.EMPTY_DOCK

    def test_missing_container_fields_is_empty(self):
        # A server block with no container fields -> "unknown", not a fake zero.
        stats = {"server": {"cpu": "42.1", "ram": "48.6"}}
        assert payload_mod.parse_dashboard_stats(stats) == payload_mod.EMPTY_DOCK

    def test_garbage_and_none_never_raise(self):
        for bad in (None, [], "nope", 123, {"server": "x"},
                    {"server": {"containers": "NaN", "containers_up": None}}):
            out = payload_mod.parse_dashboard_stats(bad)
            assert out == payload_mod.EMPTY_DOCK

    def test_only_up_present_defaults_total_zero(self):
        # up present, total absent -> n=0; up kept (no total to clamp against).
        stats = {"server": {"containers_up": "3"}}
        assert payload_mod.parse_dashboard_stats(stats) == {"n": 0, "up": 3}

    def test_custom_block_and_keys(self):
        # Could point at the PC stack if it ever exposed a total.
        stats = {"pc": {"docker_total": "7", "docker_running": "5"}}
        out = payload_mod.parse_dashboard_stats(
            stats, block="pc", total_key="docker_total", up_key="docker_running")
        assert out == {"n": 7, "up": 5}

    def test_empty_dock_constant(self):
        assert payload_mod.EMPTY_DOCK == {"n": 0, "up": 0}
