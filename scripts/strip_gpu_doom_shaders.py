#!/usr/bin/env python3
"""Remove fake GPU Doom paths from all compute shaders (keeps rtxDosViewportColor)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

SHADERS = Path(__file__).resolve().parents[1] / "Navigator" / "shaders" / "compute"

GPU_CONST_RE = re.compile(
    r"// GPU Doom on Field Die panel.*?\n"
    r"const uint CTRL_DOS_PANEL_FS = 256u;\n"
    r"const uint CTRL_DOS_PANEL_STRETCH = 512u;\n"
    r"const uint CTRL_GPU_DOOM = 128u;\n"
    r"const uint DOOM_PAL_BYTE = 0x90000u;\n"
    r"const uint DOOM_TITLE_BYTE = 0x90300u;\n"
    r"const uint DOOM_FB_BYTE = 0xA0000u;\n"
    r"const uint DOOM_STATE_BYTE = 0xAFA00u;\n",
    re.DOTALL,
)

GPU_CONST_CANVAS_RE = re.compile(
    r"// GPU Doom on Field Die panel.*?\n"
    r"const uint CTRL_GPU_DOOM = 128u;\n"
    r"const uint DOOM_PAL_BYTE = 0x90000u;\n"
    r"const uint DOOM_TITLE_BYTE = 0x90300u;\n"
    r"const uint DOOM_FB_BYTE = 0xA0000u;\n"
    r"const uint DOOM_STATE_BYTE = 0xAFA00u;\n",
    re.DOTALL,
)

# GPU-only helpers through doomViewportColor — stop before guest VGA sampling.
GPU_HELPERS_RE = re.compile(
    r"const uint DOOM_MODE_TITLE = 0u;.*?^vec3 doomViewportColor\(vec2 pix, vec2 img, ThemePal th\) \{"
    r".*?return clamp\(doomGpuSampleGame\(v\.gameUv\), 0\.0, 1\.0\);\n\}\n\n",
    re.DOTALL | re.MULTILINE,
)

GPU_STEP_RE = re.compile(
    r"^void doomGpuStep\(\) \{.*?\n\}\n",
    re.DOTALL | re.MULTILINE,
)

RENDER_GPU_RE = re.compile(
    r"    if \(\(field\.control & CTRL_GPU_DOOM\) != 0u\) \{\n"
    r"        vec3 doomCol = doomViewportColor\(panelPos, vec2\(img\), theme\);\n"
    r"        if \(doomCol\.x >= 0\.0\)\n"
    r"            return finishHudColor\(doomCol, panelPos, vec2\(img\), hud\.puv, t, theme, hoverSec\);\n"
    r"    \}\n"
    r"    if \(\(field\.control & CTRL_RTXDOS\) != 0u && \(field\.control & CTRL_GPU_DOOM\) == 0u\) \{\n",
)

MAIN_GPU_RE = re.compile(
    r"        if \(\(field\.control & CTRL_GPU_DOOM\) != 0u\) \{\n"
    r"            doomGpuStep\(\);\n"
    r"        \} else if \(\(field\.control & CTRL_HOST_CPU\) == 0u\) \{\n",
)

GPU_CONST_INLINE_RE = re.compile(
    r"const uint CTRL_GPU_DOOM = 128u;\n"
    r"const uint DOOM_PAL_BYTE = 0x90000u;\n"
    r"const uint DOOM_TITLE_BYTE = 0x90300u;\n"
    r"const uint DOOM_FB_BYTE = 0xA0000u;\n"
    r"const uint DOOM_STATE_BYTE = 0xAFA00u;\n",
)

RENDER_GPU_RTXDOS_RE = re.compile(
    r"    if \(\(field\.control & CTRL_GPU_DOOM\) != 0u\) \{\n"
    r"        vec3 doomCol = rtxDosViewportColor\(panelPos, vec2\(img\), theme\);\n"
    r"        if \(doomCol\.x >= 0\.0\) \{\n"
    r"            DoomViewport v = doomViewportMap\(panelPos, vec2\(img\)\);\n"
    r"            if \(v\.inside && !v\.letterbox && !dosPanelBorderHit\(panelPos, v\)\) \{\n"
    r"                uint vmode = ramByte\(0x449u\);\n"
    r"                if \(vmode == VGA_MODE_13 \|\| die\.video_mode == VGA_MODE_13\)\n"
    r"                    doomCol = clamp\(doomGpuSampleGame\(v\.gameUv\), 0\.0, 1\.0\);\n"
    r"            \}\n"
    r"            return finishHudColor\(doomCol, panelPos, vec2\(img\), hud\.puv, t, theme, hoverSec\);\n"
    r"        \}\n"
    r"    \}\n"
    r"    if \(\(field\.control & CTRL_RTXDOS\) != 0u && \(field\.control & CTRL_GPU_DOOM\) == 0u\) \{\n",
)

RTXDOS_ONLY_RE = re.compile(
    r"if \(\(field\.control & CTRL_RTXDOS\) != 0u && \(field\.control & CTRL_GPU_DOOM\) == 0u\)",
)


def patch_file(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    orig = text

    text = GPU_CONST_RE.sub(
        "const uint CTRL_DOS_PANEL_FS = 256u;\n"
        "const uint CTRL_DOS_PANEL_STRETCH = 512u;\n",
        text,
        count=1,
    )
    text = GPU_CONST_CANVAS_RE.sub("", text, count=1)
    text = GPU_HELPERS_RE.sub("", text, count=1)
    text = GPU_STEP_RE.sub("", text, count=1)
    text = RENDER_GPU_RE.sub(
        "    if ((field.control & CTRL_RTXDOS) != 0u) {\n",
        text,
        count=1,
    )
    text = MAIN_GPU_RE.sub(
        "        if ((field.control & CTRL_HOST_CPU) == 0u) {\n",
        text,
        count=1,
    )
    text = GPU_CONST_INLINE_RE.sub("", text, count=1)
    text = RENDER_GPU_RTXDOS_RE.sub(
        "    if ((field.control & CTRL_RTXDOS) != 0u) {\n",
        text,
        count=1,
    )
    text = RTXDOS_ONLY_RE.sub("if ((field.control & CTRL_RTXDOS) != 0u)", text)

    if text == orig:
        return False
    path.write_text(text, encoding="utf-8")
    print(f"stripped GPU doom: {path.name}")
    return True


def main() -> int:
    changed = 0
    for path in sorted(SHADERS.glob("*.comp")):
        if patch_file(path):
            changed += 1
    print(f"done — {changed} file(s) patched")
    return 0


if __name__ == "__main__":
    sys.exit(main())