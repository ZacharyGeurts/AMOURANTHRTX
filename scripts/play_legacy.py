#!/usr/bin/env python3
"""Legacy play harness — Amiga love + Xbox360 + PS1 smoke."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
TARGETS = (
    "qa_amiga_test",
    "qa_xbox360_test",
    "qa_ps1_test",
    "qa_n64_test",
    "qa_dreamcast_test",
    "qa_ps2_test",
    "qa_everything_test",
)


def main() -> int:
    for t in TARGETS:
        subprocess.run(
            ["cmake", "--build", str(BUILD), "--target", t, "-j", "8"],
            cwd=ROOT, check=True,
        )
        exe = BUILD / t
        proc = subprocess.run([str(exe)], cwd=ROOT, check=False)
        if proc.returncode != 0:
            print(f"FAIL {t}", file=sys.stderr)
            return 1
        print(f"OK {t}")
    print("OK play_legacy")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())