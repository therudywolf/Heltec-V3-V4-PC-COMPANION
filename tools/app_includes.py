"""PlatformIO pre-build: add the shared monorepo include dirs as absolute paths.

The bmw/pc/hacker apps live in apps/<name>/ but compile the shared src/ tree
(set via src_dir) plus lib/nocturne-core. Several shared headers are included
by bare name across directories (e.g. SceneManager.cpp -> "BmwManager.h"), so
every src/modules/* subdir must be on the include path. Computing these as
absolute paths here is deterministic, unlike ${PROJECT_DIR}/../.. math inside
build_flags -I (which PlatformIO resolves inconsistently on Windows).
"""
import os

Import("env")  # noqa: F821 — provided by PlatformIO/SCons

# This script lives in <repo>/tools/, so the repo root is its parent dir.
repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

include_dirs = [
    os.path.join(repo_root, "include"),
    os.path.join(repo_root, "src"),
    os.path.join(repo_root, "src", "modules"),
    os.path.join(repo_root, "src", "modules", "display"),
    os.path.join(repo_root, "src", "modules", "network"),
    os.path.join(repo_root, "src", "modules", "ble"),
    os.path.join(repo_root, "src", "modules", "car"),
    os.path.join(repo_root, "src", "modules", "car", "ibus"),
    os.path.join(repo_root, "src", "modules", "system"),
    os.path.join(repo_root, "lib", "nocturne-core", "src"),
]

for d in include_dirs:
    if os.path.isdir(d):
        env.Append(CPPPATH=[d])  # noqa: F821

print("[app_includes] added %d shared include dirs from %s"
      % (len(include_dirs), repo_root))
