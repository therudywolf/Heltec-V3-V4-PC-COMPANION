#!/usr/bin/env python3
"""
Weather helpers for NOCTURNE_OS.

Pure helpers extracted from ``monitor.py``: the Open-Meteo weather-code -> short
description mapping and the forecast URL builder. The network call
(``get_weather_async``) stays in ``monitor.py`` because it mutates module-level
cache/state, but it delegates the parsing decisions to these functions.
"""


def weather_desc_from_code(code: int) -> str:
    """Map an Open-Meteo WMO weather code to a short (<=20 char) description.

    Ranges mirror the original ``monitor._weather_desc_from_code`` exactly,
    including the quirk that the 80..82 "Showers" branch is unreachable because
    the 51..67 / 71..86 ranges (returning "Rain"/"Snow") are checked first.
    Unknown codes fall back to "Cloudy".
    """
    if code == 0:
        return "Clear"
    if 1 <= code <= 3:
        return "Cloudy"
    if 45 <= code <= 48:
        return "Fog"
    if 51 <= code <= 67:
        return "Rain"
    if 71 <= code <= 86:
        return "Snow"
    if 80 <= code <= 82:
        return "Showers"
    if 95 <= code <= 99:
        return "Storm"
    return "Cloudy"


def build_weather_url(lat: str, lon: str) -> str:
    """Build the Open-Meteo forecast URL for the given latitude/longitude.

    Requests current temperature + weather code and an 8-day daily forecast,
    auto-timezone — identical to the original ``WEATHER_URL`` construction.
    """
    return (
        "https://api.open-meteo.com/v1/forecast?"
        f"latitude={lat}&longitude={lon}"
        "&current=temperature_2m,weather_code"
        "&daily=temperature_2m_max,temperature_2m_min,weather_code"
        "&timezone=auto&forecast_days=8"
    )


def parse_daily_forecast(data: dict, days: int = 5) -> list:
    """Extract a compact daily forecast from an Open-Meteo JSON response.

    Returns up to ``days`` entries, each a 3-int list ``[tmin, tmax, wmocode]``
    (min temp, max temp, WMO weather code) — small enough to ride along in the
    device payload. The order is fixed as ``[tmin, tmax, code]`` so the firmware
    can render it positionally without keys.

    Tolerant of missing/short/ragged arrays: iterates only over the indices
    present in all three daily arrays and skips any entry whose values can't be
    coerced to int. Returns ``[]`` when no usable daily data is present (so the
    caller can send ``[]`` or omit the key).
    """
    daily = data.get("daily") if isinstance(data, dict) else None
    if not isinstance(daily, dict):
        return []
    tmaxs = daily.get("temperature_2m_max") or []
    tmins = daily.get("temperature_2m_min") or []
    codes = daily.get("weather_code") or []
    n = min(len(tmaxs), len(tmins), len(codes), max(0, days))
    out: list = []
    for i in range(n):
        try:
            tmin = int(round(float(tmins[i])))
            tmax = int(round(float(tmaxs[i])))
            code = int(codes[i])
        except (TypeError, ValueError):
            continue
        out.append([tmin, tmax, code])
    return out
