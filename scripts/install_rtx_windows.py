#!/usr/bin/env python3
"""Stage user-supplied Windows 3.1 / 98 files into RTX-DOS C: image.

Drop install media into:
  assets/dos/incoming/win31/   — Windows 3.1x setup files (WIN.COM, SYSTEM.ini, etc.)
  assets/dos/incoming/win98/   — Windows 98 setup files

We do not redistribute Windows; this script only copies files you provide.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCOMING = ROOT / "assets" / "dos" / "incoming"
AMMO = ROOT / "assets" / "dos" / "ammo"
WINDOWS = AMMO / "WINDOWS"
SCRIPTS = ROOT / "scripts"


def stage_tree(src: Path, dst: Path) -> int:
    if not src.is_dir():
        return 0
    n = 0
    for f in src.rglob("*"):
        if not f.is_file():
            continue
        rel = f.relative_to(src)
        out = dst / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(f, out)
        n += 1
    return n


def main() -> int:
    INCOMING.mkdir(parents=True, exist_ok=True)
    (INCOMING / "win31").mkdir(exist_ok=True)
    (INCOMING / "win98").mkdir(exist_ok=True)
    WINDOWS.mkdir(parents=True, exist_ok=True)

    n31 = stage_tree(INCOMING / "win31", WINDOWS)
    n98 = stage_tree(INCOMING / "win98", WINDOWS)
    total = n31 + n98

    if total == 0:
        print("No Windows files in assets/dos/incoming/win31 or win98")
        print("Copy your licensed Windows setup files there, then re-run.")
        (INCOMING / "PLACE_WINDOWS_HERE.txt").write_text(
            "win31/  - Windows 3.1 or 3.11 (WIN.COM, *.EXE, SYSTEM, PROGRAMS)\n"
            "win98/  - Windows 98 setup files\n"
            "Then: python3 scripts/install_rtx_windows.py && ./linux.sh dos --force\n",
            encoding="ascii",
        )
        return 0

    print(f"Staged {n31} Win31 + {n98} Win98 files → {WINDOWS}")
    from rtx_dos_brand import HD_IMAGE  # noqa: E402

    hd = ROOT / "assets" / "dos" / HD_IMAGE
    if hd.is_file():
        subprocess.run(
            [sys.executable, str(SCRIPTS / "mk_ammo_hd.py"), str(hd)],
            check=True,
            cwd=ROOT,
        )
        print(f"Rebuilt HD image: {hd}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())