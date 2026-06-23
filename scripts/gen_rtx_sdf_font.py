#!/usr/bin/env python3
"""Rasterize professional fixed-width TTF into a 256-glyph SDF atlas for RTX shaders.

Default font: assets/fonts/JetBrainsMono-Regular.ttf
Outputs:
  assets/textures/rtx_font_sdf.png  — R8 SDF atlas (0.5 = edge)
  Navigator/shaders/compute/rtx_font_meta.inc — layout constants
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "Navigator" / "shaders" / "compute"
DEFAULT_FONT = ROOT / "assets" / "fonts" / "JetBrainsMono-Regular.ttf"
FALLBACK_FONT = ROOT / "assets" / "fonts" / "font.ttf"
OUT_PNG = ROOT / "assets" / "textures" / "rtx_font_sdf.png"
OUT_META = SHADERS / "rtx_font_meta.inc"

GLYPH_W = 8
GLYPH_H = 16
GLYPH_COUNT = 256
GRID_COLS = 16
GRID_ROWS = 16
CELL_W = 32
CELL_H = 48
PAD_X = (CELL_W - GLYPH_W) // 2
PAD_Y = (CELL_H - GLYPH_H) // 2
ATLAS_W = GRID_COLS * CELL_W
ATLAS_H = GRID_ROWS * CELL_H
SDF_SPREAD = 6.0
SUPER = 4


def find_font_size(path: Path) -> int:
    for size in range(18, 9, -1):
        font = ImageFont.truetype(str(path), size)
        ok = True
        for ch in "MWij@":
            bbox = font.getbbox(ch)
            if bbox[2] - bbox[0] > GLYPH_W or bbox[3] - bbox[1] > GLYPH_H:
                ok = False
                break
        if ok:
            return size
    return 12


def rasterize_glyph_hires(ch: str, font: ImageFont.FreeTypeFont, scale: int) -> list[list[bool]]:
    sw, sh = GLYPH_W * scale, GLYPH_H * scale
    img = Image.new("L", (sw, sh), 0)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), ch, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (sw - tw) // 2 - bbox[0]
    y = (sh - th) // 2 - bbox[1] - scale
    draw.text((x, y), ch, fill=255, font=font)
    rows: list[list[bool]] = []
    for py in range(sh):
        row: list[bool] = []
        for px in range(sw):
            row.append(img.getpixel((px, py)) > 112)
        rows.append(row)
    return rows


def downsample_mask(hires: list[list[bool]], scale: int) -> list[list[bool]]:
    h = len(hires)
    w = len(hires[0]) if h else 0
    out: list[list[bool]] = []
    for gy in range(GLYPH_H):
        row: list[bool] = []
        for gx in range(GLYPH_W):
            hits = 0
            total = 0
            for sy in range(scale):
                for sx in range(scale):
                    py = gy * scale + sy
                    px = gx * scale + sx
                    if py < h and px < w:
                        total += 1
                        if hires[py][px]:
                            hits += 1
            row.append(hits * 2 >= total if total else False)
        out.append(row)
    return out


def dt_1d(f: list[float]) -> list[float]:
    n = len(f)
    if n == 0:
        return []
    v = [0] * n
    z = [0.0] * (n + 1)
    k = 0
    v[0] = 0
    z[0] = -math.inf
    z[1] = math.inf
    for q in range(1, n):
        num = (f[q] + q * q) - (f[v[k]] + v[k] * v[k])
        den = 2 * (q - v[k])
        s = num / den if den != 0 else math.inf
        while s <= z[k]:
            k -= 1
            num = (f[q] + q * q) - (f[v[k]] + v[k] * v[k])
            den = 2 * (q - v[k])
            s = num / den if den != 0 else math.inf
        k += 1
        v[k] = q
        z[k] = s
        z[k + 1] = math.inf
    k = 0
    d = [0.0] * n
    for q in range(n):
        while z[k + 1] < q:
            k += 1
        diff = q - v[k]
        d[q] = diff * diff + f[v[k]]
    return d


def edt_2d(mask: list[list[bool]]) -> list[list[float]]:
    h = len(mask)
    w = len(mask[0]) if h else 0
    inf = 1e20
    grid = [[0.0 if mask[y][x] else inf for x in range(w)] for y in range(h)]
    for y in range(h):
        row = [grid[y][x] for x in range(w)]
        dist = dt_1d(row)
        for x in range(w):
            grid[y][x] = dist[x]
    for x in range(w):
        col = [grid[y][x] for y in range(h)]
        dist = dt_1d(col)
        for y in range(h):
            grid[y][x] = math.sqrt(dist[y])
    return grid


def signed_distance_cell(glyph: list[list[bool]]) -> list[list[float]]:
    cell = [[False] * CELL_W for _ in range(CELL_H)]
    for gy in range(GLYPH_H):
        for gx in range(GLYPH_W):
            cell[PAD_Y + gy][PAD_X + gx] = glyph[gy][gx]

    dist_in = edt_2d(cell)
    inv = [[not cell[y][x] for x in range(CELL_W)] for y in range(CELL_H)]
    dist_out = edt_2d(inv)

    sdf: list[list[float]] = []
    for y in range(CELL_H):
        row: list[float] = []
        for x in range(CELL_W):
            if cell[y][x]:
                row.append(-dist_out[y][x])
            else:
                row.append(dist_in[y][x])
        sdf.append(row)
    return sdf


def encode_sdf(sdf: list[list[float]]) -> list[list[int]]:
    out: list[list[int]] = []
    for row in sdf:
        enc: list[int] = []
        for d in row:
            v = 0.5 - d / SDF_SPREAD
            enc.append(int(round(max(0.0, min(1.0, v)) * 255.0)))
        out.append(enc)
    return out


def glyph_char(code: int) -> str:
    if code < 32 or code == 127:
        return " "
    if code > 255:
        return "?"
    try:
        return chr(code)
    except ValueError:
        return "?"


def build_atlas(font_path: Path) -> Image.Image:
    if not font_path.is_file():
        raise SystemExit(f"font not found: {font_path}")
    base_size = find_font_size(font_path)
    hi_size = base_size * SUPER
    hi_font = ImageFont.truetype(str(font_path), hi_size)
    atlas = Image.new("L", (ATLAS_W, ATLAS_H), 0)

    for code in range(GLYPH_COUNT):
        col = code % GRID_COLS
        row = code // GRID_COLS
        ox = col * CELL_W
        oy = row * CELL_H
        hires = rasterize_glyph_hires(glyph_char(code), hi_font, SUPER)
        glyph = downsample_mask(hires, SUPER)
        enc = encode_sdf(signed_distance_cell(glyph))
        for y in range(CELL_H):
            for x in range(CELL_W):
                atlas.putpixel((ox + x, oy + y), enc[y][x])

    print(
        f"SDF atlas {font_path.name} @ {base_size}px ({SUPER}x SSAA) — {GLYPH_COUNT} glyphs "
        f"{GLYPH_W}x{GLYPH_H} in {CELL_W}x{CELL_H} cells → {ATLAS_W}x{ATLAS_H} spread={SDF_SPREAD}"
    )
    return atlas


def write_meta(font_name: str) -> None:
    block = f"""// RTX Terminal SDF — {font_name} 256-glyph atlas (generated, do not hand-edit)
const uint FONT_SDF_GLYPH_W = {GLYPH_W}u;
const uint FONT_SDF_GLYPH_H = {GLYPH_H}u;
const uint FONT_SDF_COLS = {GRID_COLS}u;
const uint FONT_SDF_ROWS = {GRID_ROWS}u;
const uint FONT_SDF_CELL_W = {CELL_W}u;
const uint FONT_SDF_CELL_H = {CELL_H}u;
const uint FONT_SDF_PAD_X = {PAD_X}u;
const uint FONT_SDF_PAD_Y = {PAD_Y}u;
const uint FONT_SDF_ATLAS_W = {ATLAS_W}u;
const uint FONT_SDF_ATLAS_H = {ATLAS_H}u;
const float FONT_SDF_SPREAD = {SDF_SPREAD:.1f};
"""
    OUT_META.write_text(block, encoding="utf-8")
    print(f"wrote {OUT_META.name}")


def main() -> int:
    font_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_FONT
    if not font_path.is_file() and font_path == DEFAULT_FONT and FALLBACK_FONT.is_file():
        font_path = FALLBACK_FONT
    atlas = build_atlas(font_path)
    OUT_PNG.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(OUT_PNG)
    print(f"wrote {OUT_PNG}")
    write_meta(font_path.stem)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())