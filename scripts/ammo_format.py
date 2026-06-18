#!/usr/bin/env python3
"""Host-side FORMAT check for Ammo DOS virtual C: — validate FAT16 boot sector and label."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

SECTOR = 512
PART_LBA = 1


def check_volume(img: bytearray) -> tuple[bool, str]:
    off = PART_LBA * SECTOR
    vol = img[off:off + SECTOR]
    if vol[510:512] != b"\x55\xAA":
        return False, "volume boot sector missing 55 AA"
    oem = vol[3:11].decode("ascii", "ignore").strip()
    label = vol[43:54].decode("ascii", "ignore").strip()
    bps = struct.unpack_from("<H", vol, 11)[0]
    if bps != SECTOR:
        return False, f"bytes/sector {bps}"
    if oem != "RTXDOS70":
        return False, f"OEM '{oem}' (expected RTXDOS70)"
    if label != "RTXDOS":
        return False, f"label '{label}' (expected RTXDOS)"
    return True, f"OK: FAT16 volume RTXDOS OEM={oem}"


def repair_label(img: bytearray) -> None:
    off = PART_LBA * SECTOR
    img[off + 3:off + 11] = b"RTXDOS70"
    img[off + 43:off + 54] = b"RTXDOS     "


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    from rtx_dos_brand import HD_IMAGE  # noqa: E402

    path = Path(sys.argv[1] if len(sys.argv) > 1 else root / "assets" / "dos" / HD_IMAGE)
    repair = "--repair" in sys.argv
    if not path.is_file():
        raise SystemExit(f"missing {path}")
    img = bytearray(path.read_bytes())
    ok, msg = check_volume(img)
    print(f"Ammo FORMAT: {path.name}")
    print(f"  {msg}")
    if not ok and repair:
        repair_label(img)
        path.write_bytes(img)
        ok, msg = check_volume(img)
        print(f"  repaired: {msg}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())