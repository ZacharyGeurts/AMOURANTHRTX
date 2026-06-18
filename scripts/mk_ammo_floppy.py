#!/usr/bin/env python3
"""Build 720 KiB RTX-DOS boot floppy — pure Microsoft MS-DOS MIT chain.

Boot: bootsect_720k (MS-DOS 6.30-style loader) → IO.SYS → MSDOS.SYS.
CONFIG.SYS hands off to patched COMMAND.COM on C: (MS-DOS 4.0 MIT → RTX-DOS 7.0).
No FreeDOS / FD13 dependency.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

SECTOR = 512
TOTAL_SECTORS = 1440
SPC = 2
RESERVED = 1
FATS = 2
SPF = 3
DATA_START = RESERVED + FATS * SPF + 7

ROOT = Path(__file__).resolve().parents[1]
AMMO = ROOT / "assets" / "dos" / "ammo"
sys.path.insert(0, str(ROOT / "scripts"))
from rtx_dos_brand import FLOPPY_IMAGE, OEM_ID, VERSION_FULL  # noqa: E402
from rtx_dos.bootsect_720k import emit as emit_bootsect  # noqa: E402


def fat12_set(fat: bytearray, cluster: int, value: int) -> None:
    byte_off = cluster * 3 // 2
    if cluster & 1:
        fat[byte_off] = (fat[byte_off] & 0x0F) | ((value & 0x0F) << 4)
        fat[byte_off + 1] = (value >> 4) & 0xFF
    else:
        fat[byte_off] = value & 0xFF
        fat[byte_off + 1] = (fat[byte_off + 1] & 0xF0) | ((value >> 8) & 0x0F)


def write_dir_entry(name: str, size: int, cluster: int, attr: int = 0x20) -> bytes:
    stem, _, ext = name.upper().partition(".")
    ent = bytearray(32)
    ent[0:8] = f"{stem:<8}"[:8].encode("ascii")
    ent[8:11] = f"{ext:<3}"[:3].encode("ascii")
    ent[11] = attr
    struct.pack_into("<H", ent, 26, cluster)
    struct.pack_into("<I", ent, 28, size)
    return bytes(ent)


def alloc_chain(img: bytearray, fat: bytearray, start: int, data: bytes) -> int:
    clusters = max(1, (len(data) + SPC * SECTOR - 1) // (SPC * SECTOR))
    c = start
    for i in range(clusters):
        off = (DATA_START + (c - 2) * SPC) * SECTOR
        chunk = data[i * SPC * SECTOR:(i + 1) * SPC * SECTOR]
        img[off:off + SPC * SECTOR] = chunk.ljust(SPC * SECTOR, b"\x1a")
        fat12_set(fat, c, 0xFF0 if i == clusters - 1 else c + 1)
        c += 1
    return c


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "assets" / "dos" / FLOPPY_IMAGE)
    io_sys = AMMO / "IO.SYS"
    msdos_sys = AMMO / "MSDOS.SYS"
    if not io_sys.is_file() or not msdos_sys.is_file():
        raise SystemExit(f"Run rtx_dos/patch_msdos63.py first ({AMMO})")

    config = (
        f"REM {VERSION_FULL} - Microsoft MS-DOS MIT boot floppy\r\n"
        "REM Modern DOS: MS-DOS 3.x-6.x + DPMI/XMS program compatible\r\n"
        "LASTDRIVE=Z\r\n"
        "FILES=40\r\n"
        "BUFFERS=20\r\n"
        "SHELL=C:\\COMMAND.COM\r\n"
    ).encode("ascii")
    autoexec = b"@ECHO OFF\r\nC:\r\n"

    img = bytearray(TOTAL_SECTORS * SECTOR)
    boot = bytearray(emit_bootsect())
    boot[3:11] = OEM_ID.encode("ascii")[:8].ljust(8, b" ")
    img[0:512] = boot

    fat = bytearray(SPF * SECTOR)
    fat12_set(fat, 0, 0xFF0)
    fat12_set(fat, 1, 0xFF0)

    io_data = io_sys.read_bytes()
    msdos_data = msdos_sys.read_bytes()
    entries = [
        ("IO.SYS", io_data, 0x27),
        ("MSDOS.SYS", msdos_data, 0x27),
        ("CONFIG.SYS", config, 0x20),
        ("AUTOEXEC.BAT", autoexec, 0x20),
    ]
    next_clus = 2
    dir_ents = []
    for name, data, attr in entries:
        clus = next_clus
        next_clus = alloc_chain(img, fat, next_clus, data)
        dir_ents.append(write_dir_entry(name, len(data), clus, attr))

    root_off = RESERVED * SECTOR + FATS * SPF * SECTOR
    for i, ent in enumerate(dir_ents):
        img[root_off + i * 32:root_off + (i + 1) * 32] = ent

    fat_off = RESERVED * SECTOR
    img[fat_off:fat_off + len(fat)] = fat
    img[fat_off + len(fat):fat_off + 2 * len(fat)] = fat

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(img)
    print(f"wrote {out} ({len(img)} bytes, {VERSION_FULL} MS-DOS MIT boot → C:\\COMMAND.COM)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())