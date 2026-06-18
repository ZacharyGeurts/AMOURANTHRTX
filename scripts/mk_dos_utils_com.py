#!/usr/bin/env python3
"""Generate MEMORYUP.COM and SCANDISK.COM (RTX-DOS 2026 utilities)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AMMO = ROOT / "assets" / "dos" / "ammo"
DOS = AMMO / "DOS"

UTIL_MSG = {
    "MEMORYUP.COM": "RTX MEMORYUP v7 - MemMaker replacement. Run: MEMORYUP /S\r\n$",
    "SCANDISK.COM": "RTX SCANDISK v7 - Surface scan. Run: SCANDISK C:\r\n$",
    "REGEDIT.COM": "RTX Registry Editor 2026. Run: REGEDIT\r\n$",
}


def make_com(name: str, message: str) -> bytes:
    label = name.upper().split(".")[0]
    sig = label.encode("ascii") + b"\x00"
    msg = message.encode("ascii")
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
    for dest in (AMMO, DOS):
        dest.mkdir(parents=True, exist_ok=True)
        for name, msg in UTIL_MSG.items():
            data = make_com(name, msg)
            path = dest / name
            path.write_bytes(data)
            print(f"  wrote {path} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())