#!/usr/bin/env python3
"""Play through Shareware Episode 1 (S1E1) — retry until success."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "build/libx86emu.a"
SRC = ROOT / "scripts/play_s1e1_test.cpp"
OUT = ROOT / "build/bin/Linux/play_s1e1_test"
MAX_ATTEMPTS = 8


def build() -> None:
    if not LIB.is_file():
        raise SystemExit("Missing build/libx86emu.a — run: cmake --build build")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "g++-14", "-std=c++20", "-O2",
            "-I", str(ROOT / "Navigator/engine"),
            "-I", str(ROOT / "third_party/libx86emu/include"),
            str(SRC), str(LIB), "-o", str(OUT),
        ],
        cwd=ROOT,
        check=True,
    )


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