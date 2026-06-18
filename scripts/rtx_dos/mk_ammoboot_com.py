#!/usr/bin/env python3
"""Minimal AMMOBOOT.COM — sets C: and execs MS-DOS COMMAND.COM (host INT 21)."""

from __future__ import annotations

import sys
from pathlib import Path

# ORG 100h — loaded as .COM at PSP:100
BODY = bytes([
    0xB4, 0x0E,             # mov ah,0Eh
    0xB0, 0x03,             # mov al,3  (C:)
    0xCD, 0x21,             # int 21h
    0xB8, 0x00, 0x4B,       # mov ax,4B00h
    0xBA, 0x1A, 0x01,       # mov dx,offset cmd (at 100h+1A)
    0xCD, 0x21,             # int 21h
    0xEB, 0xFE,             # jmp $
    0x43, 0x3A, 0x5C, 0x43, 0x4F, 0x4D, 0x4D, 0x41, 0x4E, 0x44, 0x2E, 0x43, 0x4F, 0x4D, 0x00,
])


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    out = Path(sys.argv[1] if len(sys.argv) > 1 else root / "assets/dos/ammo/AMMOBOOT.COM")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(BODY)
    print(f"wrote {out} ({len(BODY)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())