#!/usr/bin/env python3
"""FAT16 partition boot sector — MS-DOS 6.30 style IO.SYS loader for C: (INT 13h LBA)."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

SECTOR = 512
PART_LBA = 1
SPC = 16
RESERVED = 1
FATS = 2
ROOT_ENTRIES = 512
ROOT_SECTORS = (ROOT_ENTRIES * 32 + SECTOR - 1) // SECTOR
SPT = 63
HEADS = 16


def sectors_per_fat(part_sectors: int) -> int:
    data_area = part_sectors - RESERVED - ROOT_SECTORS
    spf = 1
    for _ in range(16):
        clusters = data_area - FATS * spf
        need = (clusters * 2 + SECTOR - 1) // SECTOR
        if need == spf:
            break
        spf = need
    return max(1, spf)


def emit(part_sectors: int) -> bytes:
    spf = sectors_per_fat(part_sectors)
    data_start = RESERVED + FATS * spf + ROOT_SECTORS
    io_lba = PART_LBA + data_start

    boot = bytearray(SECTOR)
    boot[0:3] = b"\xEB\x3C\x90"
    boot[3:11] = b"RTXDOS70"
    struct.pack_into("<H", boot, 11, SECTOR)
    boot[13] = SPC
    struct.pack_into("<H", boot, 14, RESERVED)
    boot[16] = FATS
    struct.pack_into("<H", boot, 17, ROOT_ENTRIES)
    if part_sectors > 65535:
        struct.pack_into("<H", boot, 19, 0)
        struct.pack_into("<I", boot, 32, part_sectors)
    else:
        struct.pack_into("<H", boot, 19, part_sectors)
    boot[21] = 0xF8
    struct.pack_into("<H", boot, 22, spf)
    struct.pack_into("<H", boot, 24, SPT)
    struct.pack_into("<H", boot, 26, HEADS)
    struct.pack_into("<I", boot, 28, PART_LBA)
    boot[36] = 0x80
    boot[38] = 0x29
    struct.pack_into("<I", boot, 39, 0x414D4F31)
    boot[43:54] = b"RTXDOS     "
    boot[54:62] = b"FAT16   "

    # --- boot code @ 0x3E — INT 13h AH=42 extended read IO.SYS → 0070:0000 ---
    p = 0x3E
    code = bytes([
        0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xC0, 0x8E, 0xD0,
        0xBC, 0x00, 0x7C, 0xFB,
        0xBE, 0x6E, 0x7C, 0xAC, 0x08, 0xC0, 0x74, 0x06,
        0xB4, 0x0E, 0xB7, 0x07, 0xCD, 0x10, 0xEB, 0xF4,
        0xBE, 0x80, 0x7C, 0xB4, 0x42, 0xB2, 0x80, 0x8E, 0xD6,
        0xCD, 0x13, 0x72, 0x1A,
        0xEA, 0x00, 0x00, 0x00, 0x07,
        0xBE, 0xA0, 0x7C, 0xAC, 0x08, 0xC0, 0x74, 0x06,
        0xB4, 0x0E, 0xB7, 0x07, 0xCD, 0x10, 0xEB, 0xF4,
        0xF4,
    ])
    boot[p : p + len(code)] = code

    msg = b"RTX-AMMOS C: boot MS-DOS 6.30...\r\n\x00"
    err = b"Non-System disk or disk error\r\n\x00"
    boot[0x6E : 0x6E + len(msg)] = msg
    boot[0xA0 : 0xA0 + len(err)] = err

    # DAP @ 0x80
    struct.pack_into("<H", boot, 0x80, 0x0010)
    struct.pack_into("<H", boot, 0x82, 0x0028)  # 40 sectors (~20 KiB IO.SYS)
    struct.pack_into("<H", boot, 0x84, 0x0000)
    struct.pack_into("<H", boot, 0x86, 0x0700)
    struct.pack_into("<I", boot, 0x88, io_lba)

    boot[510:512] = b"\x55\xAA"
    return bytes(boot)


def main() -> int:
    try:
        from ammo_platform import HD_SIZE_MB
    except ImportError:
        HD_SIZE_MB = 2048
    part_sectors = (HD_SIZE_MB * 1024 * 1024) // SECTOR - PART_LBA
    out = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).parent / "bootsect_hd.bin")
    out.write_bytes(emit(part_sectors))
    spf = sectors_per_fat(part_sectors)
    ds = RESERVED + FATS * spf + ROOT_SECTORS
    print(f"wrote {out} io_lba={PART_LBA + ds} spf={spf}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())