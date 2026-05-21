"""PlatformIO pre-build script: inject NOCTURNE_VERSION from the VERSION file.

Makes the top-level VERSION file the single source of truth for the firmware
version string (also used by CI and release tags). config.h keeps a fallback
define only for non-PlatformIO/IDE indexing builds.
"""
import os

Import("env")  # noqa: F821 — provided by PlatformIO/SCons

version = "dev"
version_path = os.path.join(env["PROJECT_DIR"], "VERSION")  # noqa: F821
try:
    with open(version_path, "r", encoding="utf-8") as fh:
        version = fh.read().strip() or "dev"
except OSError:
    pass

env.Append(CPPDEFINES=[("NOCTURNE_VERSION", env.StringifyMacro(version))])  # noqa: F821
print("[version] NOCTURNE_VERSION = %s" % version)
