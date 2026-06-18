#!/usr/bin/env python3
"""Host-side FDISK for Ammo DOS virtual HDD — validate/repair MBR partition table."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

SECTOR = 512
PART_LBA = 1
try:
    from ammo_platform import HD_SIZE_MB
except ImportError:
    HD_SIZE_MB = 2048
TOTAL_SECTORS = (HD_SIZE_MB * 1024 * 1024) // SECTOR
PART_SECTORS = TOTAL_SECTORS - PART_LBA
SPT = 63
HEADS = 16


def lba_to_chs(lba: int) -> tuple[int, int, int]:
    cyl = lba // (SPT * HEADS)
    rem = lba % (SPT * HEADS)
    head = rem // SPT
    sector = (rem % SPT) + 1
    return cyl, head, sector


def read_partition(img: bytearray) -> tuple[bool, str]:
    sig = img[510:512]
    if sig != b"\x55\xAA":
        return False, "MBR missing 55 AA signature"
    ent = img[0x1BE:0x1BE + 16]
    if ent[4] != 0x06:
        return False, f"partition type {ent[4]:#x} (expected 0x06 FAT16)"
    lba, sectors = struct.unpack_from("<II", ent, 8)
    if lba != PART_LBA or sectors != PART_SECTORS:
        return False, f"partition LBA={lba} sectors={sectors} (expected {PART_LBA}/{PART_SECTORS})"
    if (ent[0] & 0x80) == 0:
        return False, "partition not marked active"
    return True, f"OK: 1 active FAT16 partition {HD_SIZE_MB} MiB (RTXDOS)"


def repair_mbr(img: bytearray) -> None:
    img[0:2] = b"\xFA\x33"
    img[510:512] = b"\x55\xAA"
    cyl, head, sector = lba_to_chs(PART_LBA)
    end_lba = PART_LBA + PART_SECTORS - 1
    ecyl, ehead, esector = lba_to_chs(end_lba)
    ent = bytearray(16)
    ent[0] = 0x80
    ent[1] = head
    ent[2] = sector | ((cyl & 0x300) >> 2)
    ent[3] = cyl & 0xFF
    ent[4] = 0x06
    ent[5] = ehead
    ent[6] = esector | ((ecyl & 0x300) >> 2)
    ent[7] = ecyl & 0xFF
    struct.pack_into("<I", ent, 8, PART_LBA)
    struct.pack_into("<I", ent, 12, PART_SECTORS)
    img[0x1BE:0x1BE + 16] = ent


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    from rtx_dos_brand import HD_IMAGE  # noqa: E402

    path = Path(sys.argv[1] if len(sys.argv) > 1 else root / "assets" / "dos" / HD_IMAGE)
    repair = "--repair" in sys.argv
    if not path.is_file():
        raise SystemExit(f"missing {path}")
    img = bytearray(path.read_bytes())
    ok, msg = read_partition(img)
    print(f"Ammo FDISK: {path.name}")
    print(f"  {msg}")
    if not ok and repair:
        repair_mbr(img)
        path.write_bytes(img)
        ok, msg = read_partition(img)
        print(f"  repaired: {msg}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())