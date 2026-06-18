#!/usr/bin/env python3
"""Pack RTX-DOS HD boot slice for engine embed (first 32 MiB of C: image)."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SLICE_BYTES = 32 * 1024 * 1024
DEFAULT_HD = ROOT / "assets" / "dos" / "rtx_dos_hd.img"
DEFAULT_OUT = ROOT / "build" / "dos_slice.bin"


def ensure_hd(hd: Path) -> None:
    if hd.is_file():
        return
    installer = ROOT / "scripts" / "install_dos.py"
    print(f"HD image missing — running {installer.name}...", flush=True)
    subprocess.run([sys.executable, str(installer)], check=True, cwd=ROOT)
    if not hd.is_file():
        raise SystemExit(f"HD image still missing after install: {hd}")


def main() -> int:
    hd = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_HD
    out_bin = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_OUT
    ensure_hd(hd)

    data = hd.read_bytes()[:SLICE_BYTES]
    if len(data) < 512:
        raise SystemExit("HD image too small")

    out_bin.parent.mkdir(parents=True, exist_ok=True)
    out_bin.write_bytes(data)
    meta = out_bin.with_suffix(".meta")
    meta.write_text(f"{len(data)}\n{hd.stat().st_size}\n", encoding="ascii")
    print(f"packed {len(data)} bytes ({len(data)//1024//1024} MiB) → {out_bin}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())