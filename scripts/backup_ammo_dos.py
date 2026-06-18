#!/usr/bin/env python3
"""Backup assets/dos/ammo before RTX-DOS patch — never overwrite without snapshot."""

from __future__ import annotations

import datetime
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AMMO = ROOT / "assets" / "dos" / "ammo"
BACKUP_ROOT = ROOT / "assets" / "dos"


def backup(out_dir: Path | None = None) -> Path:
    src = AMMO
    if not src.is_dir() or not any(src.iterdir()):
        raise SystemExit(f"nothing to backup: {src}")
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    dst = out_dir or (BACKUP_ROOT / f"ammo_backup_{ts}")
    if dst.exists():
        raise SystemExit(f"backup exists: {dst}")
    shutil.copytree(src, dst)
    print(f"backup {src} → {dst}")
    return dst


def main() -> int:
    backup()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())