#!/usr/bin/env python3
"""
NOCTURNE_OS — One-Click Installer / Launcher
Cyberpunk-style setup: deps, config, autostart (winreg), launch to tray.
Paths are resolved with os.path.abspath so it works from any folder location.
"""

import os
import subprocess
import sys

# Resolve paths once (works regardless of cwd)
PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(PROJECT_ROOT, "src")
MONITOR_SCRIPT = os.path.join(SRC_DIR, "monitor.py")
REQUIREMENTS = os.path.join(PROJECT_ROOT, "requirements.txt")
CONFIG_JSON = os.path.join(PROJECT_ROOT, "config.json")

CONFIG_TEMPLATE = """{
  "host": "0.0.0.0",
  "port": 8090,
  "lhm_url": "http://localhost:8085/data.json",
  "limits": { "gpu": 80, "cpu": 75 },
  "weather_city": "Moscow"
}
"""

AUTOSTART_NAME = "NOCTURNE_OS"


def log_cyber(msg: str) -> None:
    print(f"[NEURAL_LINK] {msg}")


def check_dependencies() -> None:
    log_cyber("Checking dependencies...")
    if not os.path.isfile(REQUIREMENTS):
        log_cyber("requirements.txt not found; skipping pip install.")
        return
    try:
        subprocess.run(
            [sys.executable, "-m", "pip", "install", "-r", REQUIREMENTS, "-q"],
            check=True,
            cwd=PROJECT_ROOT,
        )
        log_cyber("Dependencies OK.")
    except subprocess.CalledProcessError as e:
        log_cyber(f"pip install failed: {e}. Continue anyway.")


def ensure_config() -> None:
    log_cyber("Config check...")
    if os.path.isfile(CONFIG_JSON):
        log_cyber("config.json present.")
        return
    try:
        with open(CONFIG_JSON, "w", encoding="utf-8") as f:
            f.write(CONFIG_TEMPLATE)
        log_cyber("config.json created from template.")
    except OSError as e:
        log_cyber(f"Could not create config.json: {e}")


def get_pythonw_path() -> str:
    exe = os.path.normpath(sys.executable)
    base = os.path.dirname(exe)
    name = os.path.basename(exe).lower()
    if "pythonw" in name:
        return exe
    if "python.exe" in name:
        pw = os.path.join(base, "pythonw.exe")
        if os.path.isfile(pw):
            return pw
    return exe


def prompt_autostart() -> bool:
    log_cyber("Autostart: add NOCTURNE_OS to Windows startup (Run at logon)?")
    try:
        r = input("> ENABLE AUTOSTART ON BOOT? [Y/N]: ").strip().upper()
        return r == "Y" or r == "YES"
    except (EOFError, KeyboardInterrupt):
        return False


def set_autostart(enable: bool) -> None:
    if sys.platform != "win32":
        log_cyber("Autostart is Windows-only; skipped.")
        return
    try:
        import winreg
    except ImportError:
        log_cyber("winreg not available; autostart skipped.")
        return
    key_path = r"Software\Microsoft\Windows\CurrentVersion\Run"
    pythonw = get_pythonw_path()
    cmd = f'"{pythonw}" "{MONITOR_SCRIPT}"'
    try:
        key = winreg.OpenKey(
            winreg.HKEY_CURRENT_USER,
            key_path,
            0,
            winreg.KEY_SET_VALUE | winreg.KEY_QUERY_VALUE,
        )
        if enable:
            winreg.SetValueEx(key, AUTOSTART_NAME, 0, winreg.REG_SZ, cmd)
            log_cyber("Autostart ENABLED (HKCU Run).")
        else:
            try:
                winreg.DeleteValue(key, AUTOSTART_NAME)
                log_cyber("Autostart REMOVED.")
            except FileNotFoundError:
                log_cyber("Autostart was not set.")
        winreg.CloseKey(key)
    except OSError as e:
        log_cyber(f"Autostart failed: {e}")


def main() -> None:
    os.chdir(PROJECT_ROOT)
    log_cyber("NOCTURNE_OS — Installer")
    check_dependencies()
    ensure_config()
    if prompt_autostart():
        set_autostart(True)
    else:
        set_autostart(False)
    # Do not auto-start monitor; user runs INSTALL_AND_RUN or pythonw src/monitor.py
    log_cyber("Done. Run START_MONITOR.bat or pythonw src/monitor.py to start.")


if __name__ == "__main__":
    main()
