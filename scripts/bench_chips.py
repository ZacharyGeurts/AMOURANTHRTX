#!/usr/bin/env python3
"""CHIPS expansion bench — PS1 core + existing emulator QA smoke."""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def run_target(target: str) -> tuple[int, dict[str, str]]:
    import os
    if target == "qa_everything_test":
        os.environ.setdefault("AMOURANTHRTX_EVERYTHING_EVERYWHERE", "1")
        os.environ.setdefault("AMOURANTHRTX_FIELD_PERSIST", "1")
        os.environ.setdefault("AMOURANTHRTX_END_GAME", "1")
    subprocess.run(
        ["cmake", "--build", str(BUILD), "--target", target, "-j", "8"],
        cwd=ROOT, check=True,
    )
    exe = BUILD / target
    if not exe.is_file():
        exe = BUILD / "bin" / "Linux" / target
    proc = subprocess.run(
        [str(exe)], cwd=ROOT, capture_output=True, text=True,
        encoding="utf-8", errors="replace", check=False,
    )
    metrics: dict[str, str] = {}
    for line in (proc.stdout + proc.stderr).splitlines():
        m = re.match(r"METRIC (\w+)=(.+)", line.strip())
        if m:
            metrics[m.group(1)] = m.group(2)
    return proc.returncode, metrics


def main() -> int:
    checks = [
        ("qa_ps1_test", "ps1_gpu_wave", lambda v: v == "1"),
        ("qa_xbox360_test", "xbox360_gpu_wave", lambda v: v == "1"),
        ("qa_amiga_test", "amiga_love_score", lambda v: int(v) >= 10),
        ("qa_n64_test", "n64_gpu_wave", lambda v: v == "1"),
        ("qa_dreamcast_test", "dreamcast_gpu_wave", lambda v: v == "1"),
        ("qa_ps2_test", "ps2_gpu_wave", lambda v: v == "1"),
        ("qa_everything_test", "everything_active", lambda v: v == "1"),
        ("qa_nes_cpu_test", None, None),
        ("qa_snes_test", None, None),
    ]
    for target, metric, pred in checks:
        rc, metrics = run_target(target)
        if rc != 0:
            print(f"FAIL {target} exit={rc}", file=sys.stderr)
            return 1
        if metric and pred and (metric not in metrics or not pred(metrics[metric])):
            print(f"FAIL {target} metric {metric}={metrics.get(metric)}", file=sys.stderr)
            return 1
        print(f"OK {target}")
    print("OK bench_chips")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())