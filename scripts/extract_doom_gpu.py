#!/usr/bin/env python3
"""Extract GPU-die Doom assets (palette + title pic) from FreeDoom IWAD."""
from __future__ import annotations
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WAD = ROOT / "assets" / "wads" / "doom1.wad"
if not WAD.is_file():
    WAD = ROOT / "assets" / "wads" / "freedoom1.wad"
OUT = ROOT / "assets" / "doom_gpu"


def read_wad(path: Path) -> tuple[bytes, list[tuple[str, int, int]]]:
    data = path.read_bytes()
    if data[:4] != b"IWAD" and data[:4] != b"PWAD":
        raise SystemExit(f"Not a WAD: {path}")
    num, table = struct.unpack_from("<II", data, 4)
    lumps = []
    for i in range(num):
        off = table + i * 16
        pos, size = struct.unpack_from("<II", data, off)
        name = data[off + 8 : off + 16].split(b"\0", 1)[0].decode("ascii", "ignore")
        lumps.append((name, pos, size))
    return data, lumps


def lump(data: bytes, lumps: list, name: str) -> bytes:
    for n, pos, size in lumps:
        if n.upper() == name.upper():
            return data[pos : pos + size]
    raise KeyError(name)


def decode_patch(pix: bytes) -> tuple[int, int, bytes]:
    w, h = struct.unpack_from("<HH", pix, 0)
    out = bytearray(w * h)
    colofs = list(struct.unpack_from(f"<{w}H", pix, 8))
    for x in range(w):
        co = colofs[x]
        while True:
            row = pix[co]
            if row == 0xFF:
                break
            cnt = pix[co + 1]
            co += 2
            for _ in range(cnt):
                if row < h:
                    out[row * w + x] = pix[co]
                row += 1
                co += 1
            co += 1
    return w, h, bytes(out)


def main() -> int:
    if not WAD.exists():
        print("Missing doom1.wad — run: python3 scripts/install_shareware_doom.py", file=sys.stderr)
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    data, lumps = read_wad(WAD)
    pal = lump(data, lumps, "PLAYPAL")[:768]
    title_lump = lump(data, lumps, "TITLEPIC")
    tw, th, title = decode_patch(title_lump)
    if (tw, th) != (320, 200):
        print(f"WARN: TITLEPIC is {tw}x{th}, expected 320x200", file=sys.stderr)
    (OUT / "palette.bin").write_bytes(pal)
    (OUT / "title320x200.bin").write_bytes(title.ljust(320 * 200, b"\x00")[: 320 * 200])
    print(f"Wrote {OUT}/palette.bin ({len(pal)} bytes)")
    print(f"Wrote {OUT}/title320x200.bin ({320 * 200} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())