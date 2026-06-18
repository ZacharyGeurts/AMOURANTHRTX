#!/usr/bin/env python3
"""Render 80x25 VGA text buffer dumps to readable PNG (9x16 cells)."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    raise SystemExit("pip install pillow")

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build/bin/Linux"
GRAB = ROOT / "build/grabs"
LIB = ROOT / "build/libx86emu.a"


def vga_palette() -> list[tuple[int, int, int]]:
    return [
        (0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
        (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
        (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
        (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255),
    ]


def render_frame(ram: bytes, out: Path, scale: int = 2) -> None:
    cw, ch = 9 * scale, 16 * scale
    w, h = 80 * cw, 25 * ch
    img = Image.new("RGB", (w, h), (0, 0, 0))
    draw = ImageDraw.Draw(img)
    pal = vga_palette()
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 14 * scale)
    except OSError:
        font = ImageFont.load_default()
    base = 0xB8000
    for row in range(25):
        for col in range(80):
            off = base + (row * 80 + col) * 2
            chv = ram[off]
            attr = ram[off + 1]
            fg, bg = pal[attr & 15], pal[(attr >> 4) & 15]
            x, y = col * cw, row * ch
            draw.rectangle([x, y, x + cw - 1, y + ch - 1], fill=bg)
            if 32 <= chv < 127:
                draw.text((x + 1, y + 1), chr(chv), fill=fg, font=font)
    img.save(out)


def dump_raw(path: Path) -> bytes:
    return path.read_bytes()


def main() -> int:
    GRAB.mkdir(parents=True, exist_ok=True)
    src = ROOT / "scripts/qa_dos_grab_test.cpp"
    exe = BIN / "qa_dos_grab_test"
    subprocess.run(
        ["g++-14", "-std=c++20", "-O2", "-I", str(ROOT / "Navigator/engine"),
         "-I", str(ROOT / "third_party/libx86emu/include"), str(src), str(LIB), "-o", str(exe)],
        check=True, cwd=ROOT,
    )
    # Patch grab test to also write raw RAM snapshots — use existing test + separate dumper
    dumper = ROOT / "scripts/qa_dos_grab_raw.cpp"
    if not dumper.is_file():
        raise SystemExit("missing qa_dos_grab_raw.cpp")
    raw_exe = BIN / "qa_dos_grab_raw"
    subprocess.run(
        ["g++-14", "-std=c++20", "-O2", "-I", str(ROOT / "Navigator/engine"),
         "-I", str(ROOT / "third_party/libx86emu/include"), str(dumper), str(LIB), "-o", str(raw_exe)],
        check=True, cwd=ROOT,
    )
    subprocess.run([str(raw_exe), str(GRAB / "raw")], check=True, cwd=ROOT)
    for raw in sorted((GRAB / "raw").glob("*.vga")):
        ram = bytearray(1024 * 1024)
        data = raw.read_bytes()
        ram[0xB8000:0xB8000 + len(data)] = data
        png = GRAB / f"{raw.stem}_screen.png"
        render_frame(bytes(ram), png, scale=2)
        print(f"PNG {png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())