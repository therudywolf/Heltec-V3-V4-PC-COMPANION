# -*- mode: python ; coding: utf-8 -*-
# Сборка: pyinstaller monitor.spec (или build_oneclick.bat)
# Не используем collect_all('winsdk') — тянет почти весь Windows SDK и раздувает сборку.

from PyInstaller.utils.hooks import collect_submodules

# pystray/PIL — скрытые импорты для Windows-трея и иконки
_hidden = ['pystray._win32', 'PIL.Image', 'PIL.ImageDraw']

# winsdk — только то, что реально нужно для медиа-контроля (обложки/плеер)
# monitor.py: GlobalSystemMediaTransportControlsSessionManager, Buffer, InputStreamOptions
_hidden += collect_submodules('winsdk.windows.media.control')
_hidden += collect_submodules('winsdk.windows.storage.streams')
_hidden += ['winsdk', 'winsdk.windows.media', 'winsdk.windows.storage']

a = Analysis(
    ['monitor.py'],
    pathex=[],
    binaries=[],
    datas=[],
    hiddenimports=_hidden,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='monitor',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
