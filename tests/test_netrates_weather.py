# Unit tests for the shared delta-rate / ping parsing helpers
# (server/netrates.py) and the weather helpers (server/weather.py).
# Run from project root: python -m pytest tests/ -v

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))

import netrates  # noqa: E402
import weather  # noqa: E402


# --------------------------------------------------------------------------- #
# compute_delta_rate  (the de-duplicated net/disk rate helper)
# --------------------------------------------------------------------------- #
class TestComputeDeltaRate:
    def test_first_sample_returns_zero_and_seeds(self):
        state = {"sent": 0, "recv": 0, "time": 0.0}
        out = netrates.compute_delta_rate(1000, 2000, state, now=100.0)
        assert out == (0, 0)
        # State seeded for next call.
        assert state["sent"] == 1000 and state["recv"] == 2000
        assert state["time"] == 100.0

    def test_rate_over_interval(self):
        # 1 second elapsed, +1024 bytes -> 1 KB/s.
        state = {"sent": 0, "recv": 0, "time": 100.0}
        out = netrates.compute_delta_rate(1024, 2048, state, now=101.0)
        assert out == (1, 2)

    def test_below_min_dt_returns_zero(self):
        state = {"sent": 0, "recv": 0, "time": 100.0}
        out = netrates.compute_delta_rate(99999, 99999, state, now=100.05)
        assert out == (0, 0)

    def test_negative_delta_clamped(self):
        # Counter reset (cur < prev) -> clamp to 0.
        state = {"read": 5000, "write": 5000, "time": 100.0}
        out = netrates.compute_delta_rate(0, 0, state, now=101.0)
        assert out == (0, 0)

    def test_works_with_disk_key_layout(self):
        # Same helper, different key names (read/write).
        state = {"read": 0, "write": 0, "time": 100.0}
        out = netrates.compute_delta_rate(2048, 4096, state, now=101.0)
        assert out == (2, 4)
        assert state["read"] == 2048 and state["write"] == 4096


# --------------------------------------------------------------------------- #
# parse_ping_latency
# --------------------------------------------------------------------------- #
class TestParsePingLatency:
    def test_windows_style(self):
        out = "Reply from 8.8.8.8: bytes=32 time=12ms TTL=117"
        assert netrates.parse_ping_latency(out) == 12

    def test_unix_style(self):
        out = "64 bytes from 8.8.8.8: icmp_seq=1 ttl=117 time=11.4 ms"
        # token is "time=11.4" -> 11
        assert netrates.parse_ping_latency(out) == 11

    def test_no_time_token(self):
        assert netrates.parse_ping_latency("Request timed out.") is None
        assert netrates.parse_ping_latency("") is None
        assert netrates.parse_ping_latency(None) is None


# --------------------------------------------------------------------------- #
# weather helpers
# --------------------------------------------------------------------------- #
class TestWeatherDesc:
    def test_known_codes(self):
        assert weather.weather_desc_from_code(0) == "Clear"
        assert weather.weather_desc_from_code(2) == "Cloudy"
        assert weather.weather_desc_from_code(45) == "Fog"
        assert weather.weather_desc_from_code(61) == "Rain"
        assert weather.weather_desc_from_code(75) == "Snow"
        assert weather.weather_desc_from_code(95) == "Storm"

    def test_unknown_falls_back_to_cloudy(self):
        assert weather.weather_desc_from_code(999) == "Cloudy"

    def test_build_url_contains_coords(self):
        url = weather.build_weather_url("55.75", "37.61")
        assert "latitude=55.75" in url
        assert "longitude=37.61" in url
        assert "current=temperature_2m,weather_code" in url
        # Daily forecast is requested so parse_daily_forecast has data to read.
        assert "daily=temperature_2m_max,temperature_2m_min,weather_code" in url


# --------------------------------------------------------------------------- #
# parse_daily_forecast  (compact device forecast: [tmin, tmax, wmocode])
# --------------------------------------------------------------------------- #
class TestParseDailyForecast:
    def _data(self, tmax, tmin, code):
        return {"daily": {
            "temperature_2m_max": tmax,
            "temperature_2m_min": tmin,
            "weather_code": code,
        }}

    def test_shape_is_list_of_three_int_lists(self):
        data = self._data([21.4, 19.0], [12.6, 10.0], [2, 3])
        out = weather.parse_daily_forecast(data, days=5)
        # Order is [tmin, tmax, code], rounded to ints.
        assert out == [[13, 21, 2], [10, 19, 3]]
        for entry in out:
            assert isinstance(entry, list) and len(entry) == 3
            assert all(isinstance(v, int) for v in entry)

    def test_clamps_to_days_limit(self):
        data = self._data(
            [20, 21, 22, 23, 24, 25, 26, 27],
            [10, 11, 12, 13, 14, 15, 16, 17],
            [0, 1, 2, 3, 45, 61, 71, 95],
        )
        out = weather.parse_daily_forecast(data, days=5)
        assert len(out) == 5
        assert out[0] == [10, 20, 0]
        assert out[4] == [14, 24, 45]

    def test_ragged_arrays_use_shortest(self):
        # Only two codes -> only two entries even though temps have three.
        data = self._data([20, 21, 22], [10, 11, 12], [0, 1])
        out = weather.parse_daily_forecast(data, days=5)
        assert len(out) == 2

    def test_missing_or_bad_returns_empty_list(self):
        assert weather.parse_daily_forecast({}, days=5) == []
        assert weather.parse_daily_forecast({"daily": None}, days=5) == []
        assert weather.parse_daily_forecast({"daily": {}}, days=5) == []
        assert weather.parse_daily_forecast(None, days=5) == []

    def test_skips_unparsable_entry(self):
        data = self._data([20, None, 22], [10, 11, 12], [0, 1, 2])
        out = weather.parse_daily_forecast(data, days=5)
        # Middle entry (None tmax) is skipped; first and third survive.
        assert out == [[10, 20, 0], [12, 22, 2]]
