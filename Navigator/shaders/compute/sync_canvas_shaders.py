#!/usr/bin/env python3
"""Magnificent x86 comp compiler — sync infinite instant field shaders from x86.comp."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
ROOT = HERE.parents[2]
DOS_SYNC = ROOT / "scripts" / "sync_dos_viewport_shaders.py"


def run(script: str, cwd: Path | None = None) -> None:
    path = HERE / script if (HERE / script).is_file() else script
    print(f"sync_x86_compiler: {path.name}")
    subprocess.run([sys.executable, str(path)], cwd=cwd or HERE, check=True)


def main() -> int:
    print("sync_x86_compiler: stage 1 — AOS chrome + WM + theme (x86.comp → CANVAS.comp)")
    run("patch_canvas_aos.py")
    print("sync_x86_compiler: stage 2 — DOS viewport parity (CANVAS.comp → all *.comp)")
    if DOS_SYNC.is_file():
        run(str(DOS_SYNC), cwd=ROOT)
    else:
        print(f"sync_x86_compiler: WARN missing {DOS_SYNC}")
    print("sync_x86_compiler: done — x86 + CANVAS unified (single canvas)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())