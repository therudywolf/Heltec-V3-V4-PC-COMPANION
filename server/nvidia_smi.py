#!/usr/bin/env python3
"""
NVIDIA GPU telemetry via nvidia-smi for NOCTURNE_OS.

Zero-install, zero-lag alternative to LibreHardwareMonitor for GPU metrics:
nvidia-smi ships in System32 with the driver. windows_exporter (Grafana Agent)
covers CPU/RAM/disk/net but NOT the GPU; this fills temp/load/VRAM/power/clock/
fan so the GPU and (partly) MAIN scenes light up without LHM.

:func:`parse_nvidia_smi_csv` is the pure parser (testable offline);
:func:`query_gpu_sync` runs nvidia-smi and returns the hw-key overlay (``gt``/
``gl``/``gv``/``vu``/``vt``/``pw``/``gclock``/``gf``). Never raises: any failure
(no GPU, tool missing, bad output) yields an empty dict so the merge is a no-op.
"""

import logging
import subprocess
from typing import Dict, List, Optional

# Query order MUST match the field parsing in parse_nvidia_smi_csv.
QUERY_FIELDS = (
    "temperature.gpu,utilization.gpu,utilization.memory,"
    "memory.used,memory.total,power.draw,clocks.gr,fan.speed"
)
NVIDIA_SMI_CMD = [
    "nvidia-smi",
    f"--query-gpu={QUERY_FIELDS}",
    "--format=csv,noheader,nounits",
]


def _num(tok: str) -> Optional[float]:
    """Parse one CSV token to float; None for '[N/A]'/blank/garbage."""
    t = (tok or "").strip()
    if not t or t.upper().startswith("[N/A") or t.upper() == "N/A":
        return None
    try:
        return float(t)
    except ValueError:
        return None


def parse_nvidia_smi_csv(text: str) -> Dict:
    """Map the first GPU row of nvidia-smi CSV output to hw-key overlay fields.

    Expects one line per GPU, fields in QUERY_FIELDS order:
        temp, gpu_util, mem_util, mem_used_MiB, mem_total_MiB, power_W, clock_MHz, fan%
    Uses the first GPU. Returns {} if no usable row. Keys produced (only when the
    value is present): gt, gl, gv, vu, vt, pw, gclock, gf, gh (gh mirrors gt for
    the MAIN "hot" reading).
    """
    if not text:
        return {}
    line = next((ln for ln in text.splitlines() if ln.strip()), "")
    if not line:
        return {}
    parts = [p.strip() for p in line.split(",")]
    if len(parts) < 8:
        return {}
    temp = _num(parts[0])
    gl = _num(parts[1])
    gv = _num(parts[2])
    mem_used = _num(parts[3])   # MiB
    mem_total = _num(parts[4])  # MiB
    power = _num(parts[5])
    clock = _num(parts[6])
    fan = _num(parts[7])

    hw: Dict = {}
    if temp is not None:
        hw["gt"] = int(round(temp))
        hw["gh"] = int(round(temp))  # MAIN uses gh for the GPU hot value
    if gl is not None:
        hw["gl"] = int(round(gl))
    if gv is not None:
        hw["gv"] = int(round(gv))
    if mem_used is not None:
        hw["vu"] = round(mem_used / 1024.0, 1)   # GiB
    if mem_total is not None:
        hw["vt"] = round(mem_total / 1024.0, 1)  # GiB
    if power is not None:
        hw["pw"] = int(round(power))
    if clock is not None:
        hw["gclock"] = int(round(clock))
    if fan is not None:
        hw["gf"] = int(round(fan))
    return hw


def query_gpu_sync(cmd: Optional[List[str]] = None, timeout: float = 3.0) -> Dict:
    """Run nvidia-smi and return the hw overlay. {} on any failure (never raises)."""
    try:
        out = subprocess.run(
            cmd or NVIDIA_SMI_CMD,
            capture_output=True, text=True, timeout=timeout,
        )
        if out.returncode != 0:
            logging.debug("nvidia-smi rc=%s: %s", out.returncode, out.stderr.strip()[:120])
            return {}
        return parse_nvidia_smi_csv(out.stdout)
    except FileNotFoundError:
        logging.debug("nvidia-smi not found")
        return {}
    except Exception as e:  # subprocess timeout, etc.
        logging.debug("nvidia-smi failed: %s", e)
        return {}
