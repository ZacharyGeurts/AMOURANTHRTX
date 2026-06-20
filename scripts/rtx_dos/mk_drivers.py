#!/usr/bin/env python3
"""Assemble RTXFL field-layer .SYS drivers via DevKit AMMOASM (no Python bytecode stubs)."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
BIN = BUILD / "bin/Linux"
QA_BIN = BIN / "qa_drivers_build"


def ensure_qa_built() -> None:
    cache = BUILD / "CMakeCache.txt"
    if not cache.is_file():
        raise SystemExit("configure cmake first: ./linux.sh  (or: cmake -S . -B build)")
    subprocess.run(
        ["cmake", "--build", str(BUILD), "--target", "qa_drivers_build"],
        cwd=ROOT,
        check=True,
    )
    if not QA_BIN.is_file():
        raise SystemExit(f"qa_drivers_build missing after build — expected {QA_BIN}")


def build_drivers(out: Path) -> None:
    ensure_qa_built()
    subprocess.run([str(QA_BIN), str(ROOT), str(out)], cwd=ROOT, check=True)


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "assets" / "dos" / "ammo")
    build_drivers(out)
    print(f"drivers: AMMOASM-built RTXFL .SYS → {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())