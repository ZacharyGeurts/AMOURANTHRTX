#!/usr/bin/env python3
"""Assemble RTXFL field-layer .SYS drivers via DevKit AMMOASM (no Python bytecode stubs)."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BIN = ROOT / "build/bin/Linux"
LIB = ROOT / "build/libx86emu.a"
QA_SRC = ROOT / "scripts/qa_drivers_build.cpp"
QA_BIN = BIN / "qa_drivers_build"


def ensure_qa_built() -> None:
    if not LIB.is_file():
        raise SystemExit("build libx86emu first: cmake --build build")
    BIN.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "g++-14", "-std=c++20", "-O2",
            "-I", str(ROOT / "Navigator/engine"),
            "-I", str(ROOT / "third_party/libx86emu/include"),
            str(QA_SRC), str(LIB), "-o", str(QA_BIN),
        ],
        cwd=ROOT,
        check=True,
    )


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