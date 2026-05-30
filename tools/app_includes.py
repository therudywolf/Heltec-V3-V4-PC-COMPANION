"""PlatformIO pre-build: add the shared monorepo include dirs as absolute paths.

The bmw/pc/hacker apps live in apps/<name>/ but compile the shared src/ tree
(set via src_dir) plus lib/nocturne-core. Several shared headers are included
by bare name across directories (e.g. SceneManager.cpp -> "BmwManager.h"), so
every src/modules/* subdir must be on the include path. Computing these as
absolute paths here is deterministic, unlike ${PROJECT_DIR}/../.. math inside
build_flags -I (which PlatformIO resolves against the platform dir).

Note: SCons-exec'd scripts do NOT get a usable __file__, so derive the repo
root from env["PROJECT_DIR"] (the app dir) by walking up to the dir that
contains lib/nocturne-core.
"""
import os

Import("env")  # noqa: F821 — provided by PlatformIO/SCons

repo_root = env["PROJECT_DIR"]  # noqa: F821
for _ in range(6):
    if os.path.isdir(os.path.join(repo_root, "lib", "nocturne-core")):
        break
    parent = os.path.dirname(repo_root)
    if parent == repo_root:
        break
    repo_root = parent

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

added = 0
for d in include_dirs:
    if os.path.isdir(d):
        env.Append(CPPPATH=[d])  # noqa: F821
        added += 1

print("[app_includes] repo_root=%s, added %d shared include dirs" % (repo_root, added))
