#!/usr/bin/env python3
"""Sync DOS panel viewport + border fix from CANVAS.comp into all *.comp variants."""

from __future__ import annotations

import re
import sys
from pathlib import Path

SHADERS = Path(__file__).resolve().parents[1] / "Navigator" / "shaders" / "compute"
CANVAS = SHADERS / "CANVAS.comp"
X86 = SHADERS / "x86.comp"

VIEWPORT_RE = re.compile(
    r"struct DoomViewport \{.*?\}\n+bool doomPixelInViewport\(vec2 pix, vec2 img\) \{"
    r".*?return doomViewportMap\(pix, img\)\.inside;\n\}"
    r"(?:\n+bool dosPanelBorderHit\(vec2 pix, DoomViewport v\) \{.*?\n\})?",
    re.DOTALL,
)

RTXDOS_RE = re.compile(
    r"const uint FIELD_LAYER_VGA = 1u;.*?return clamp\(colOut, 0\.0, 1\.0\);\n\}",
    re.DOTALL,
)

RTXDOS_OLD_RE = re.compile(
    r"vec3 rtxDosViewportColor\(vec2 pix, vec2 img, ThemePal th\) \{"
    r".*?return clamp\(colOut, 0\.0, 1\.0\);\n\}",
    re.DOTALL,
)

VGA_ATTR_RE = re.compile(
    r"vec3 vgaAttrColor\(uint attr, ThemePal th\) \{.*?\n\}",
    re.DOTALL,
)

DOOM_COLOR_RE = re.compile(
    r"vec3 doomViewportColor\(vec2 pix, vec2 img, ThemePal th\) \{"
    r".*?return clamp\(doomGpuSampleGame\(v\.gameUv\), 0\.0, 1\.0\);",
    re.DOTALL,
)

DOS_SAMPLE_RE = re.compile(
    r"vec3 dosSampleMode13\(vec2 gameUv\) \{"
    r".*?\n\}\n\nconst uint FIELD_LAYER_VGA",
    re.DOTALL,
)


def extract_block(src: str, pattern: re.Pattern[str], label: str) -> str:
    m = pattern.search(src)
    if not m:
        raise SystemExit(f"{label} block not found in CANVAS.comp")
    return m.group(0)


def extract_optional(src: str, pattern: re.Pattern[str]) -> str | None:
    m = pattern.search(src)
    return m.group(0) if m else None


def patch_viewport(path: Path, block: str) -> bool:
    text = path.read_text(encoding="utf-8")
    m = VIEWPORT_RE.search(text)
    if not m:
        print(f"SKIP {path.name}: no DoomViewport")
        return False
    new_text = text[: m.start()] + block + text[m.end() :]
    if new_text == text:
        return False
    path.write_text(new_text, encoding="utf-8")
    print(f"patched viewport {path.name}")
    return True


def patch_optional(path: Path, pattern: re.Pattern[str], block: str, label: str) -> bool:
    text = path.read_text(encoding="utf-8")
    m = pattern.search(text)
    if not m:
        return False
    new_text = text[: m.start()] + block + text[m.end() :]
    if new_text == text:
        return False
    path.write_text(new_text, encoding="utf-8")
    print(f"patched {label} {path.name}")
    return True


def remove_optional(path: Path, pattern: re.Pattern[str], label: str) -> bool:
    text = path.read_text(encoding="utf-8")
    m = pattern.search(text)
    if not m:
        return False
    new_text = text[: m.start()] + text[m.end() :]
    path.write_text(new_text, encoding="utf-8")
    print(f"removed {label} {path.name}")
    return True


def main() -> int:
    if not CANVAS.is_file():
        raise SystemExit(f"missing {CANVAS}")
    viewport_src = X86.read_text(encoding="utf-8") if X86.is_file() else CANVAS.read_text(encoding="utf-8")
    canvas = CANVAS.read_text(encoding="utf-8")
    viewport = extract_block(viewport_src, VIEWPORT_RE, "viewport")
    rtx_dos = extract_block(canvas, RTXDOS_OLD_RE, "rtxDosViewportColor")
    vga_attr = extract_block(canvas, VGA_ATTR_RE, "vgaAttrColor")
    doom_color = extract_optional(canvas, DOOM_COLOR_RE)
    dos_sample = extract_block(canvas, DOS_SAMPLE_RE, "dosSampleMode13+")
    n = 0
    for path in sorted(SHADERS.glob("*.comp")):
        if path.name in ("CANVAS.comp", "x86.comp"):
            continue
        if patch_viewport(path, viewport):
            n += 1
        if patch_optional(path, RTXDOS_OLD_RE, rtx_dos, "rtxDos"):
            n += 1
        if patch_optional(path, VGA_ATTR_RE, vga_attr, "vgaAttr"):
            n += 1
        if doom_color:
            if patch_optional(path, DOOM_COLOR_RE, doom_color, "doomColor"):
                n += 1
        elif remove_optional(path, DOOM_COLOR_RE, "doomColor"):
            n += 1
        if patch_optional(path, DOS_SAMPLE_RE, dos_sample, "dosSample"):
            n += 1
    print(f"synced {n} shader patch(es)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())