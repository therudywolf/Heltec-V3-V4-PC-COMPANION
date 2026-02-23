"""
Build Meshtastic for Heltec V4 and copy firmware.bin to data/meshtastic.bin.
Used by PlatformIO custom target "meshtastic".
"""
import os
import shutil
import subprocess
import sys

def _find_platformio():
    pio = os.environ.get("PLATFORMIO_CMD")
    if pio:
        return pio
    for name in ("pio", "platformio"):
        pio = shutil.which(name)
        if pio:
            return pio
    if os.name == "nt":
        default = os.path.join(os.environ.get("USERPROFILE", ""), ".platformio", "penv", "Scripts", "platformio.exe")
        if os.path.isfile(default):
            return default
    return "pio"

def run_meshtastic_build(env):
    proj = env["PROJECT_DIR"]
    meshtastic_root = os.path.join(proj, "firmware-2.7.15.567b8ea")
    if not os.path.isdir(meshtastic_root):
        print("Warning: firmware-2.7.15.567b8ea not found. Put Meshtastic source there or add data/meshtastic.bin manually.")
        return False
    build_dir = os.path.join(meshtastic_root, ".pio", "build", "heltec-v4")
    out_bin = os.path.join(build_dir, "firmware.bin")
    pio = env.get("PLATFORMIO_CMD") or _find_platformio()
    print("Building Meshtastic (env heltec-v4) in %s ..." % meshtastic_root)
    try:
        subprocess.check_call([pio, "run", "-e", "heltec-v4"], cwd=meshtastic_root)
    except subprocess.CalledProcessError as e:
        print("Meshtastic build failed: %s" % e)
        return False
    if not os.path.isfile(out_bin):
        print("Meshtastic build did not produce firmware.bin")
        return False
    data_dir = os.path.join(proj, "data")
    os.makedirs(data_dir, exist_ok=True)
    dest = os.path.join(data_dir, "meshtastic.bin")
    shutil.copy2(out_bin, dest)
    print("Copied to %s" % dest)
    return True

def meshtastic_target(target, source, env):
    run_meshtastic_build(env)

Import("env")

def ensure_meshtastic_bin(target, source, env):
    """Before building/uploading FS: build Meshtastic if data/meshtastic.bin missing."""
    data_bin = os.path.join(env["PROJECT_DIR"], "data", "meshtastic.bin")
    if not os.path.isfile(data_bin):
        run_meshtastic_build(env)

env.AddCustomTarget("meshtastic", None, meshtastic_target, title="Build Meshtastic -> data/meshtastic.bin")
# Before building/uploading FS: ensure data/meshtastic.bin exists (build Meshtastic if needed)
env.AddPreAction("buildfs", ensure_meshtastic_bin)
env.AddPreAction("uploadfs", ensure_meshtastic_bin)
