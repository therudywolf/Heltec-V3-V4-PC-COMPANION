# -*- mode: python ; coding: utf-8 -*-
# Сборка: pyinstaller monitor.spec (или build_oneclick.bat)

from PyInstaller.utils.hooks import collect_all

# pystray/PIL — скрытые импорты для Windows-трея и иконки
_hidden = ['pystray._win32', 'PIL.Image', 'PIL.ImageDraw']

# winsdk — динамические импорты; collect_all подтягивает подмодули и данные
_w_datas, _w_binaries, _w_hidden = collect_all('winsdk')
_hidden += _w_hidden

a = Analysis(
    ['monitor.py'],
    pathex=[],
    binaries=_w_binaries,
    datas=_w_datas,
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
