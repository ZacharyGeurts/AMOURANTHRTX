#!/usr/bin/env python3
"""Emit 720 KiB FAT12 boot sector for MS-DOS 6.30 (loads IBMBIO.COM → 0070:0000)."""

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
ROOT_SECTORS = 7
DATA_START = RESERVED + FATS * SPF + ROOT_SECTORS  # 14
MEDIA = 0xF9


def emit() -> bytes:
    boot = bytearray(SECTOR)
    # JMP over BPB
    boot[0:3] = b"\xEB\x3C\x90"
    boot[3:11] = b"MSDOS6.3"
    struct.pack_into("<H", boot, 11, SECTOR)
    boot[13] = SPC
    struct.pack_into("<H", boot, 14, RESERVED)
    boot[16] = FATS
    struct.pack_into("<H", boot, 17, 112)
    struct.pack_into("<H", boot, 19, TOTAL_SECTORS)
    boot[21] = MEDIA
    struct.pack_into("<H", boot, 22, SPF)
    struct.pack_into("<H", boot, 24, 9)
    struct.pack_into("<H", boot, 26, 2)
    struct.pack_into("<H", boot, 28, 0)
    boot[36] = 0
    boot[38] = 0x29
    struct.pack_into("<I", boot, 39, 0x414D4F31)
    boot[43:54] = b"AMMO 6.30  "
    boot[54:62] = b"FAT12   "

    # --- boot code @ 0x3E ---
    p = 0x3E
    code = bytes([
        0xFA,                   # cli
        0x31, 0xC0,             # xor ax,ax
        0x8E, 0xD8,             # mov ds,ax
        0x8E, 0xC0,             # mov es,ax
        0x8E, 0xD0,             # mov ss,ax
        0xBC, 0x00, 0x7C,       # mov sp,7C00h
        0xFB,                   # sti
        0xBE, 0x6E, 0x7C,       # mov si, msg
        0xAC,                   # lodsb
        0x08, 0xC0,             # or al,al
        0x74, 0x06,             # jz done_msg
        0xB4, 0x0E,             # mov ah,0Eh
        0xB7, 0x07,             # mov bh,7
        0xCD, 0x10,             # int 10h
        0xEB, 0xF4,             # jmp short -12
        # load IBMBIO cluster 2 → ES:BX = 0070:0000
        0xB8, 0x00, 0x07,       # mov ax,0700h
        0x8E, 0xC0,             # mov es,ax
        0xBB, 0x00, 0x00,       # mov bx,0
        0xB4, 0x02,             # mov ah,2
        0xB0, 0x1A,             # mov al,26 sectors (~13 KiB IBMBIO)
        0xB5, 0x00,             # mov ch,0
        0xB1, 0x06,             # mov cl,6  (sector 6 head 1 = LBA 14)
        0xB6, 0x01,             # mov dh,1
        0xB2, 0x00,             # mov dl,0
        0xCD, 0x13,             # int 13
        0x72, 0x1C,             # jc error
        0xEA, 0x00, 0x00, 0x00, 0x07,  # jmp 0000:0700 → 0070:0000
        # error:
        0xBE, 0x90, 0x7C,       # mov si,err
        0xAC, 0x08, 0xC0, 0x74, 0x06, 0xB4, 0x0E, 0xB7, 0x07, 0xCD, 0x10, 0xEB, 0xF4,
        0xF4,                   # hlt
    ])
    boot[p:p + len(code)] = code
    msg = b"RTX-DOS 7.0 Modern DOS boot (MS-DOS MIT)...\r\n\x00"
    err = b"Non-System disk or disk error\r\n\x00"
    boot[0x6E:0x6E + len(msg)] = msg
    boot[0x90:0x90 + len(err)] = err
    boot[510:512] = b"\x55\xAA"
    return bytes(boot)


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).parent / "bootsect_720k.bin")
    out.write_bytes(emit())
    print(f"wrote {out} ({SECTOR} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())