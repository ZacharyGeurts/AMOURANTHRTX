#!/usr/bin/env python3
"""Generate RTX DevKit TOOLS/*.COM stubs (print banner + INT 21 exit)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "assets" / "dos" / "ammo" / "TOOLS"

TOOLS_MSG = {
    "FIELDC.COM": "RTX FIELDC v4 - use: FIELDC file.fld\r\n$",
    "AMMOASM.COM": "RTX AMMOASM v4 - use: AMMOASM file.asm\r\n$",
    "AMMOCC.COM": "RTX AMMOCC v4 - use: AMMOCC file.c\r\n$",
    "AMMODECOMP.COM": "RTX AMMODECOMP v4 - use: AMMODECOMP file.com\r\n$",
    "AMMOZIP.COM": "RTX AMMOZIP - use: AMMOZIP archive.zip [dest]\r\n$",
    "AMMOLINK.COM": "RTX AMMOLINK - use: AMMOLINK file.obj\r\n$",
    "AMMORUN.COM": "RTX AMMORUN - use: AMMORUN file.com\r\n$",
    "AMMODBG.COM": "RTX AMMODBG - use: AMMODBG file.com\r\n$",
}


def make_com(name: str, message: str) -> bytes:
    label = name.upper().split(".")[0]
    sig = label.encode("ascii") + b"\x00"
    msg = message.encode("ascii")
    # ORG 100h COM: jmp over sig, INT 21 AH=09, INT 21 AH=4C
    body = bytearray()
    body += b"\xEB" + bytes([len(sig) + 1]) + b"\x90"
    body += sig
    body += b"\xB4\x09"
    msg_off = 0x100 + len(body) + 3
    body += b"\xBA" + bytes([msg_off & 0xFF, (msg_off >> 8) & 0xFF])
    body += b"\xCD\x21"
    body += b"\xB4\x4C\xB0\x00\xCD\x21"
    out = bytearray(0x100)
    out.extend(body)
    while len(out) < msg_off:
        out.append(0)
    out.extend(msg)
    return bytes(out)


def main() -> int:
    TOOLS.mkdir(parents=True, exist_ok=True)
    for name, msg in TOOLS_MSG.items():
        data = make_com(name, msg)
        path = TOOLS / name
        path.write_bytes(data)
        print(f"  wrote {path.name} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())