#!/usr/bin/env python3
"""
NOCTURNE_OS — LHM disk structure dump utility.

Fetches data.json from LibreHardwareMonitor Remote Web Server and prints
storage-related nodes to help debug disk parsing when LHM format changes.

Usage:
    python dump_lhm_disks.py [--url URL] [--save FILE]
"""
import argparse
import json
import os
import sys

try:
    import urllib.request
except ImportError:
    urllib = None


def _get_config_path() -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    server_dir = os.path.dirname(script_dir)
    project_root = os.path.dirname(server_dir)
    for candidate in [
        os.path.join(server_dir, "config.json"),
        os.path.join(project_root, "config.json"),
    ]:
        if os.path.isfile(candidate):
            return candidate
    return os.path.join(server_dir, "config.json")


def load_config() -> dict:
    out = {"lhm_url": "http://localhost:8085/data.json"}
    try:
        with open(_get_config_path(), "r", encoding="utf-8") as f:
            data = json.load(f)
        if "lhm_url" in data:
            out["lhm_url"] = data["lhm_url"]
    except Exception:
        pass
    return out


# Storage-related prefixes to look for
STORAGE_PREFIXES = ("hdd", "nvme", "ssd", "storage", "drive")

# Keys to try when extracting IDs/values (case variations)
SENSOR_ID_KEYS = ("SensorId", "SensorID", "sensor_id", "sensorId")
HARDWARE_ID_KEYS = ("HardwareId", "hardware_id", "hardwareId")
VALUE_KEYS = ("Value", "RawValue", "value", "raw_value", "rawValue")
TEXT_KEYS = ("Text", "Name", "text", "name")


def _get_any(node: dict, keys: tuple) -> str:
    for k in keys:
        v = node.get(k)
        if v is not None and str(v).strip():
            return str(v).strip()
    return ""


def _collect_paths(node: any, prefix: str = "", path_to_val: dict = None) -> dict:
    if path_to_val is None:
        path_to_val = {}
    if isinstance(node, list):
        for item in node:
            _collect_paths(item, prefix, path_to_val)
    elif isinstance(node, dict):
        sid = _get_any(node, SENSOR_ID_KEYS)
        hw = _get_any(node, HARDWARE_ID_KEYS)
        raw = _get_any(node, VALUE_KEYS)
        text = _get_any(node, TEXT_KEYS)
        if sid and raw:
            path_to_val[sid] = raw
        if hw:
            norm = hw.strip("/").lower()
            if any(norm.startswith(p) for p in STORAGE_PREFIXES):
                path_to_val[f"__hardware:{hw}"] = {"text": text, "hw": hw}
        if "Children" in node:
            _collect_paths(node["Children"], prefix, path_to_val)
        if "children" in node:
            _collect_paths(node["children"], prefix, path_to_val)
    return path_to_val


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url)
    req.add_header("Accept", "application/json")
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read().decode("utf-8"))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Dump LibreHardwareMonitor storage structure from data.json"
    )
    parser.add_argument(
        "--url",
        default=None,
        help="LHM data.json URL (default: from config.json)",
    )
    parser.add_argument(
        "--save",
        metavar="FILE",
        default=None,
        help="Save raw JSON to file",
    )
    args = parser.parse_args()

    url = args.url or load_config()["lhm_url"]

    print(f"Fetching: {url}")
    try:
        data = fetch_json(url)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    if args.save:
        with open(args.save, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        print(f"Saved raw JSON to {args.save}")

    path_to_val = _collect_paths(data)

    # Storage sensors: paths like /hdd/0/data/31, /nvme/2/temperature/0
    storage_paths = {}
    hardware_nodes = {}
    for k, v in path_to_val.items():
        if k.startswith("__hardware:"):
            hardware_nodes[k] = v
        else:
            parts = k.strip("/").split("/")
            if len(parts) >= 2 and parts[0].lower() in STORAGE_PREFIXES:
                storage_paths[k] = v

    print("\n--- Storage hardware nodes ---")
    for k, v in sorted(hardware_nodes.items()):
        print(f"  {k}: {v}")

    print("\n--- Storage sensor paths (prefix in hdd/nvme/ssd/storage/drive) ---")
    for path in sorted(storage_paths.keys()):
        print(f"  {path} -> {storage_paths[path]}")

    # Summary: devices by prefix/number
    devices = set()
    for path in storage_paths:
        parts = path.strip("/").split("/")
        if len(parts) >= 2:
            try:
                num = int(parts[1])
                devices.add((parts[0].lower(), num))
            except ValueError:
                pass
    devices = sorted(devices)

    print("\n--- Detected storage devices ---")
    for prefix, num in devices:
        free_path = f"/{prefix}/{num}/data/31"
        total_path = f"/{prefix}/{num}/data/32"
        temp_path = f"/{prefix}/{num}/temperature/0"
        free_val = storage_paths.get(free_path, "?")
        total_val = storage_paths.get(total_path, "?")
        temp_val = storage_paths.get(temp_path, "?")
        print(f"  {prefix}/{num}: free={free_val}, total={total_val}, temp={temp_val}")


if __name__ == "__main__":
    main()
