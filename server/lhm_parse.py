#!/usr/bin/env python3
"""
LibreHardwareMonitor (LHM) JSON parsing helpers for NOCTURNE_OS.

This module holds the pure, side-effect-free parts of LHM /data.json parsing,
extracted from the former ~157-line ``_parse_lhm_json`` god function in
``monitor.py``. ``monitor.py`` imports these and wires them together; the public
behaviour of ``monitor._parse_lhm_json`` is unchanged.

The LHM tree is a nested ``{Children: [...]}`` structure where leaf sensors carry
a ``SensorId`` path (e.g. ``/amdcpu/0/temperature/2``) and a ``Value`` string.
Parsing works in two stages:

1. :func:`walk_sensors` flattens the tree into ``path_to_val`` and collects a few
   name/type-categorised side lists (GPU memory sensors, IT8688E fans/temps, fan
   control percentages).
2. The ``extract_*`` / ``build_*`` helpers turn that flat map into the canonical
   short-key fields (``vu``/``vt``, ``fans``, ``cf`` ..., motherboard temps and the
   ``hdd`` device list) the firmware expects.
"""

from typing import Any, Dict, List, Optional, Tuple

# Key-name variants LHM has used across versions (case / snake / camel).
SENSOR_ID_KEYS: Tuple[str, ...] = ("SensorId", "SensorID", "sensor_id", "sensorId")
VALUE_KEYS: Tuple[str, ...] = ("Value", "RawValue", "value", "raw_value", "rawValue")
TYPE_KEYS: Tuple[str, ...] = ("Type", "type")
NAME_KEYS: Tuple[str, ...] = ("Name", "Text", "name", "text")

# Storage device id prefixes (first path segment) and the drive letters we map
# the first four discovered devices onto, in sorted order.
STORAGE_PREFIXES: Tuple[str, ...] = ("hdd", "nvme", "ssd", "storage", "drive")
DRIVE_LETTERS: Tuple[str, ...] = ("C", "D", "E", "F")


def clean_val(v: Any) -> float:
    """Coerce an LHM value (e.g. ``"55.0 °C"`` or ``"3,5"``) to a float.

    Returns ``0.0`` for ``None``, empty, or unparseable input. Comma is treated
    as a decimal separator and only the first whitespace-delimited token is used.
    """
    if v is None:
        return 0.0
    try:
        s = str(v).strip().replace(",", ".")
        if not s:
            return 0.0
        return float(s.split()[0] if s.split() else s)
    except (ValueError, TypeError):
        return 0.0


def get_any_key(node: Dict, keys: Tuple[str, ...]) -> Any:
    """Return the first non-empty value among ``keys`` in ``node``.

    Strings that are empty/whitespace-only are skipped so callers can rely on a
    truthy, meaningful value (or ``None``).
    """
    for k in keys:
        v = node.get(k)
        if v is not None and (not isinstance(v, str) or str(v).strip()):
            return v
    return None


def walk_sensors(
    data: Any,
    targets: Dict[str, str],
    targets_alias: Dict[str, str],
    it8688e_prefix: str,
) -> Dict[str, Any]:
    """Recursively flatten the LHM tree, mapping known sensors to short keys.

    Returns a dict with:

    * ``results``   – short-key -> float for sensors matched via ``targets`` /
      ``targets_alias`` (raw values; later post-processing applies unit scaling).
    * ``path_to_val`` – full ``SensorId`` -> float for every leaf sensor.
    * ``gpu_memory_sensors`` – ``[(sid, val, name_lower)]`` candidates for the
      VRAM used/total name-based fallback.
    * ``it8688e_fans`` / ``it8688e_temps`` – ``[(sid, val)]`` from the IT8688E
      super-IO chip (fans/temps), used for the chipset temp.
    * ``fan_controls`` – ``[cpu, pump/sys1, gpu, case/sys2]`` control percentages.

    This mirrors the original nested ``walk()`` closure exactly; it is pure aside
    from building these collections.
    """
    results: Dict[str, Any] = {}
    path_to_val: Dict[str, float] = {}
    targets_set = set(targets.values())
    it8688e_fans: List[Tuple[str, float]] = []
    it8688e_temps: List[Tuple[str, float]] = []
    gpu_memory_sensors: List[Tuple[str, float, str]] = []  # (sid, val, name_lower)
    fan_controls: List[int] = [0, 0, 0, 0]  # CPU, Pump/Sys#1, GPU, Case/Sys#2

    def walk(node: Any) -> None:
        if isinstance(node, list):
            for item in node:
                walk(item)
        elif isinstance(node, dict):
            sid = get_any_key(node, SENSOR_ID_KEYS)
            raw = get_any_key(node, VALUE_KEYS) or ""
            val = clean_val(raw)
            if sid:
                path_to_val[sid] = val
                if sid in targets_set:
                    for k, v in targets.items():
                        if v == sid:
                            results[k] = val
                            break
                elif sid in targets_alias:
                    results[targets_alias[sid]] = val
                elif sid.startswith(it8688e_prefix):
                    stype = str(get_any_key(node, TYPE_KEYS) or "").lower()
                    if "fan" in stype:
                        it8688e_fans.append((sid, val))
                    elif "temperature" in stype or "temp" in stype:
                        it8688e_temps.append((sid, val))
                    elif "control" in stype:
                        # Fan control %: control/0=CPU, /1=Pump/Sys#1, /2=Case/Sys#2
                        if "/control/0" in sid:
                            fan_controls[0] = int(val)
                        elif "/control/1" in sid:
                            fan_controls[1] = int(val)
                        elif "/control/2" in sid:
                            fan_controls[3] = int(val)
                # GPU fan control: /gpu-nvidia/0/control/1
                if sid and "/gpu-nvidia/0/control/1" in sid:
                    stype = str(get_any_key(node, TYPE_KEYS) or "").lower()
                    if "control" in stype:
                        fan_controls[2] = int(val)
                # Collect GPU memory sensors for name-based fallback (vu/vt)
                if ("/nvidiagpu/" in sid or "/gpu-nvidia/" in sid) and val > 0:
                    stype = str(get_any_key(node, TYPE_KEYS) or "").lower()
                    if "data" in stype or "smalldata" in stype:
                        name = str(get_any_key(node, NAME_KEYS) or "").lower()
                        if "memory" in name or "used" in name or "total" in name or "limit" in name:
                            gpu_memory_sensors.append((sid, val, name))
            if "Children" in node:
                walk(node["Children"])
            if "children" in node:
                walk(node["children"])

    walk(data)
    return {
        "results": results,
        "path_to_val": path_to_val,
        "gpu_memory_sensors": gpu_memory_sensors,
        "it8688e_fans": it8688e_fans,
        "it8688e_temps": it8688e_temps,
        "fan_controls": fan_controls,
    }


def apply_vram_fallback(
    results: Dict[str, Any],
    gpu_memory_sensors: List[Tuple[str, float, str]],
) -> None:
    """Fill ``vu``/``vt`` (VRAM used/total) from sensor names if paths missed.

    Mutates ``results`` in place. Values stay in MB here; unit scaling to GB is
    applied later by the caller. No-op once both keys are present.
    """
    if "vu" not in results or "vt" not in results:
        for _sid, val, name in gpu_memory_sensors:
            if "used" in name and "vu" not in results:
                results["vu"] = val
            if ("total" in name or "limit" in name) and "vt" not in results:
                results["vt"] = val


def extract_fans(
    path_to_val: Dict[str, float],
    fan_paths: List[str],
    fan_case_path: str,
) -> Dict[str, Any]:
    """Build the fan RPM list and the ``cf``/``s1``/``gf``/``s2`` short keys.

    Order is [CPU, Pump, GPU, Case]. The GPU entry prefers the LHM
    ``/gpu-nvidia/0/fan/1`` ("GPU Fan") RPM over the configured fan path.
    Returns ``{"fans": [...], "cf":.., "s1":.., "gf":.., "s2":..}``.
    """
    fans: List[int] = [int(path_to_val.get(p, 0)) for p in fan_paths]
    fans.append(int(path_to_val.get(fan_case_path, 0)))
    gf_val = int(path_to_val.get("/gpu-nvidia/0/fan/1", 0)) or (fans[2] if len(fans) > 2 else 0)
    if len(fans) > 2:
        fans[2] = gf_val
    return {
        "fans": fans,
        "cf": fans[0] if len(fans) > 0 else 0,
        "s1": fans[1] if len(fans) > 1 else 0,
        "gf": gf_val,
        "s2": fans[3] if len(fans) > 3 else 0,
    }


def extract_mb_temps(path_to_val: Dict[str, float]) -> Dict[str, int]:
    """Extract motherboard temps (System, VSoC MOS, VRM MOS, Chipset) as ints."""
    return {
        "mb_sys": int(path_to_val.get("/lpc/it8688e/0/temperature/0", 0)),
        "mb_vsoc": int(path_to_val.get("/lpc/it8688e/0/temperature/1", 0)),
        "mb_vrm": int(path_to_val.get("/lpc/it8688e/0/temperature/4", 0)),
        "mb_chipset": int(path_to_val.get("/lpc/it8688e/0/temperature/5", 0)),
    }


def extract_storage_devices(
    path_to_val: Dict[str, float],
) -> List[Tuple[str, int, float, float, float]]:
    """Discover storage devices from the flat sensor map.

    Walks every ``/<prefix>/<n>/...`` sensor where ``<prefix>`` is a known
    storage prefix, dedupes per (prefix, num), and computes used/total GB plus
    temperature. Old LHM exposes Free=data/31, Total=data/32; used = total-free.
    Devices that only report a temperature or used-% (no capacity) are still
    included with zero capacity so the caller can merge psutil capacity later.

    Returns a list of ``(prefix, num, used_gb, total_gb, temp)`` sorted by
    (prefix, num).
    """
    path_lower_to_val: Dict[str, float] = {
        k.strip("/").lower(): v for k, v in path_to_val.items()
    }

    def _storage_val(prefix: str, num: int, subpath: str) -> float:
        v = path_lower_to_val.get(f"{prefix}/{num}/{subpath}".lower())
        return float(v) if v is not None else 0.0

    devices_seen: set = set()
    storage_devices: List[Tuple[str, int, float, float, float]] = []
    for sid, _val in path_to_val.items():
        parts = sid.strip("/").split("/")
        if len(parts) >= 2 and parts[0].lower() in STORAGE_PREFIXES:
            try:
                num = int(parts[1])
                prefix = parts[0].lower()
                key = (prefix, num)
                if key in devices_seen:
                    continue
                devices_seen.add(key)
                free_gb = _storage_val(prefix, num, "data/31")
                total_gb = _storage_val(prefix, num, "data/32")
                temp = _storage_val(prefix, num, "temperature/0")
                load_0 = _storage_val(prefix, num, "load/0")
                if total_gb > 0 or free_gb > 0:
                    used_gb = total_gb - free_gb if total_gb > 0 else 0.0
                    if used_gb < 0:
                        used_gb = 0.0
                    storage_devices.append((prefix, num, used_gb, total_gb, temp))
                elif temp > 0 or load_0 > 0:
                    storage_devices.append((prefix, num, 0.0, 0.0, temp))
            except (ValueError, IndexError):
                pass
    storage_devices.sort(key=lambda x: (x[0], x[1]))
    return storage_devices


def build_hdd_list(
    storage_devices: List[Tuple[str, int, float, float, float]],
) -> List[Dict[str, Any]]:
    """Map up to four discovered storage devices onto C/D/E/F display slots.

    Always returns exactly four entries of the form
    ``{"n": letter, "u": used_gb, "tot": total_gb, "t": temp_int}``; empty slots
    are zero-filled.
    """
    hdd: List[Dict[str, Any]] = []
    for idx in range(4):
        letter = DRIVE_LETTERS[idx] if idx < len(DRIVE_LETTERS) else "?"
        if idx < len(storage_devices):
            _pre, _num, used_gb, total_gb, temp = storage_devices[idx]
            hdd.append({
                "n": letter,
                "u": round(used_gb, 1),
                "tot": round(total_gb, 1),
                "t": int(temp),
            })
        else:
            hdd.append({"n": letter, "u": 0.0, "tot": 0.0, "t": 0})
    return hdd


def finalize_units(results: Dict[str, Any]) -> None:
    """Apply post-walk derived values and unit scaling, in place.

    * RAM total becomes used+available (LHM ``ra`` is "available", not total).
    * VRAM used/total are converted MB -> GB (rounded to 0.1).
    """
    if "ru" in results and "ra" in results:
        results["ra"] = results["ru"] + results["ra"]
    if "vu" in results:
        results["vu"] = round(results["vu"] / 1024.0, 1)
    if "vt" in results:
        results["vt"] = round(results["vt"] / 1024.0, 1)


def parse_lhm_json(
    data: Dict,
    targets: Dict[str, str],
    targets_alias: Dict[str, str],
    it8688e_prefix: str,
    fan_paths: List[str],
    fan_case_path: str,
) -> Dict[str, Any]:
    """Parse the full LHM /data.json tree into the canonical short-key dict.

    This is the orchestration of all helpers above and reproduces the original
    ``monitor._parse_lhm_json`` result exactly (float metric values plus the
    ``fans``/``fan_controls``/``hdd`` arrays and motherboard temps). Sensor-path
    constants are passed in so this module stays config-free.
    """
    walked = walk_sensors(data, targets, targets_alias, it8688e_prefix)
    results: Dict[str, Any] = walked["results"]
    path_to_val: Dict[str, float] = walked["path_to_val"]

    apply_vram_fallback(results, walked["gpu_memory_sensors"])

    results.update(extract_fans(path_to_val, fan_paths, fan_case_path))
    results["fan_controls"] = walked["fan_controls"]

    it8688e_temps = walked["it8688e_temps"]
    it8688e_temps.sort(key=lambda x: x[0])
    if it8688e_temps:
        results["ch"] = it8688e_temps[0][1]

    results.update(extract_mb_temps(path_to_val))
    finalize_units(results)

    storage_devices = extract_storage_devices(path_to_val)
    results["hdd"] = build_hdd_list(storage_devices)
    return results
