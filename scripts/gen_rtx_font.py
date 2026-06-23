#!/usr/bin/env python3
"""Rasterize a fixed-width terminal TTF into 8x16 VGA bitmap atlas for Field Die shaders.

Default: JetBrains Mono (SIL OFL) — assets/fonts/JetBrainsMono-Regular.ttf
Outputs FONT_ATLAS in CANVAS.comp and syncs to *.comp variants + rtx_font.inc.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CANVAS = ROOT / "Navigator" / "shaders" / "compute" / "CANVAS.comp"
SHADERS = ROOT / "Navigator" / "shaders" / "compute"
DEFAULT_FONT = ROOT / "assets" / "fonts" / "JetBrainsMono-Regular.ttf"
FALLBACK_FONT = ROOT / "assets" / "fonts" / "font.ttf"

FONT_W = 8
FONT_H = 16
GLYPH_COUNT = 95  # ASCII 32..126

FONT_ATLAS_RE = re.compile(
    r"// RTX Terminal.*?\nconst uint FONT_ATLAS\[380\] = uint\[\]\(\n.*?\n\);",
    re.DOTALL,
)

VIEWPORT_CTRL_RE = re.compile(
    r"const uint CTRL_DOS_PANEL_FS = 256u;\n",
)


def rows_to_words(rows: list[int]) -> list[int]:
    rows = (rows + [0] * FONT_H)[:FONT_H]
    words: list[int] = []
    for i in range(0, FONT_H, 4):
        w = 0
        for j in range(4):
            w |= (rows[i + j] & 0xFF) << (j * 8)
        words.append(w)
    return words


def rasterize_glyph(ch: str, font: ImageFont.FreeTypeFont) -> list[int]:
    img = Image.new("L", (FONT_W, FONT_H), 0)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), ch, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (FONT_W - tw) // 2 - bbox[0]
    y = (FONT_H - th) // 2 - bbox[1] - 1
    draw.text((x, y), ch, fill=255, font=font)
    rows: list[int] = []
    for py in range(FONT_H):
        row = 0
        for px in range(FONT_W):
            if img.getpixel((px, py)) > 96:
                row |= 0x80 >> px
        rows.append(row)
    return rows


def find_font_size(path: Path) -> int:
    """Pick pixel size that fits monospace glyphs in 8x16 cells."""
    for size in range(18, 9, -1):
        font = ImageFont.truetype(str(path), size)
        ok = True
        for ch in "MWij@":
            bbox = font.getbbox(ch)
            if bbox[2] - bbox[0] > FONT_W or bbox[3] - bbox[1] > FONT_H:
                ok = False
                break
        if ok:
            return size
    return 12


def build_atlas(font_path: Path) -> list[int]:
    if not font_path.is_file():
        raise SystemExit(f"font not found: {font_path}")
    size = find_font_size(font_path)
    font = ImageFont.truetype(str(font_path), size)
    atlas: list[int] = []
    for code in range(32, 127):
        rows = rasterize_glyph(chr(code), font)
        atlas.extend(rows_to_words(rows))
    if len(atlas) != GLYPH_COUNT * 4:
        raise SystemExit(f"atlas size mismatch: {len(atlas)}")
    print(f"rasterized {font_path.name} @ {size}px → {GLYPH_COUNT} glyphs {FONT_W}x{FONT_H}")
    return atlas


def format_atlas(nums: list[int], font_name: str) -> str:
    lines = [
        f"// RTX Terminal — {font_name} 8x16 fixed-width (TTF raster, Field Die)",
        "const uint FONT_ATLAS[380] = uint[](",
    ]
    for i in range(0, len(nums), 4):
        chunk = nums[i : i + 4]
        suffix = "," if i + 4 < len(nums) else ""
        line = "    " + ", ".join(f"0x{v:08X}u" for v in chunk) + suffix
        lines.append(line)
    lines.append(");")
    return "\n".join(lines)


def patch_canvas(atlas_block: str) -> None:
    text = CANVAS.read_text(encoding="utf-8")
    text = FONT_ATLAS_RE.sub(atlas_block, text, count=1)
    if "CTRL_DOS_PANEL_STRETCH" not in text:
        text = VIEWPORT_CTRL_RE.sub(
            "const uint CTRL_DOS_PANEL_FS = 256u;\nconst uint CTRL_DOS_PANEL_STRETCH = 512u;\n",
            text,
            count=1,
        )
    CANVAS.write_text(text, encoding="utf-8")
    print(f"patched {CANVAS.name} FONT_ATLAS")


def sync_font_to_comp(path: Path, atlas_block: str) -> bool:
    text = path.read_text(encoding="utf-8")
    if "FONT_ATLAS" not in text:
        return False
    new_text = FONT_ATLAS_RE.sub(atlas_block, text, count=1)
    if new_text == text:
        return False
    path.write_text(new_text, encoding="utf-8")
    print(f"synced font → {path.name}")
    return True


def main() -> int:
    font_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_FONT
    if not font_path.is_file() and font_path == DEFAULT_FONT and FALLBACK_FONT.is_file():
        font_path = FALLBACK_FONT
    nums = build_atlas(font_path)
    block = format_atlas(nums, font_path.stem)
    (SHADERS / "rtx_font.inc").write_text(block + "\n", encoding="utf-8")
    patch_canvas(block)
    n = 0
    for path in sorted(SHADERS.glob("*.comp")):
        if path.name == "CANVAS.comp":
            continue
        if sync_font_to_comp(path, block):
            n += 1
    print(f"synced {n} shader(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())