#!/usr/bin/env python3
"""Play through Shareware Episode 1 (S1E1) — retry until success."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "build/libx86emu.a"
SRC = ROOT / "scripts/play_s1e1_test.cpp"
OUT = ROOT / "build/bin/Linux/play_s1e1_test"
MAX_ATTEMPTS = 8


def build() -> None:
    subprocess.run(
        [
            "cmake", "--build", str(ROOT / "build"),
            "--target", "play_s1e1_test",
            "-j", str(os.cpu_count() or 4),
        ],
        cwd=ROOT,
        check=True,
    )
    if not OUT.is_file():
        alt = ROOT / "build/play_s1e1_test"
        if alt.is_file():
            return
        raise SystemExit(f"FAIL play_s1e1_test not found ({OUT})")


def run_once() -> tuple[int, str]:
    proc = subprocess.run(
        [str(OUT)], cwd=ROOT, capture_output=True, text=True, timeout=120, check=False,
    )
    return proc.returncode, proc.stdout + proc.stderr


def main() -> int:
    build()
    for attempt in range(1, MAX_ATTEMPTS + 1):
        print(f"--- S1E1 attempt {attempt}/{MAX_ATTEMPTS} ---")
        rc, out = run_once()
        sys.stdout.write(out)
        if rc == 0 and "S1E1 complete" in out:
            print("S1E1 playthrough succeeded")
            return 0
        print(f"attempt {attempt} failed rc={rc}, retrying...")
    raise SystemExit(f"S1E1 failed after {MAX_ATTEMPTS} attempts")


if __name__ == "__main__":
    raise SystemExit(main())