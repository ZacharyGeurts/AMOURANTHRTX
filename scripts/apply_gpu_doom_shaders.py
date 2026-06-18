#!/usr/bin/env python3
"""Restore GPU Doom shader paths stripped by strip_gpu_doom_shaders.py."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "Navigator" / "shaders" / "compute"
X86 = SHADERS / "x86.comp"
DOOM_INC = SHADERS / "doom_gpu.inc"

GPU_CONSTS = """const uint CTRL_GPU_DOOM = 128u;
const uint DOOM_PAL_BYTE = 0x90000u;
const uint DOOM_TITLE_BYTE = 0x90300u;
const uint DOOM_FB_BYTE = 0xA0000u;
const uint DOOM_STATE_BYTE = 0xAFA00u;
"""

CONST_PATCH_X86 = (
    "const uint CTRL_DOS_PANEL_FS = 256u;\n"
    "const uint CTRL_DOS_PANEL_STRETCH = 512u;\n" + GPU_CONSTS
)

CONST_PATCH_CANVAS = (
    "const uint CTRL_DOS_PANEL_STRETCH = 512u;\n"
    "const uint CTRL_HOST_CPU = 8u;\n"
    + GPU_CONSTS
)

INIT_GPU_PATCH = """    } else if (gpuDoom) {
        die.cycle_count = 0u;
        die.video_mode = 0x13u;
        ramWriteByte(0x449u, 0x13u);
    }"""

RENDER_GPU_HOOK = """    if ((field.control & CTRL_GPU_DOOM) != 0u) {
        vec3 doomCol = doomViewportColor(panelPos, vec2(img), theme);
        if (doomCol.x >= 0.0)
            return finishHudColor(doomCol, panelPos, vec2(img), hud.puv, t, theme, hoverSec);
    }
    if ((field.control & CTRL_RTXDOS) != 0u && (field.control & CTRL_GPU_DOOM) == 0u) {"""

MAIN_GPU_HOOK = """        if ((field.control & CTRL_GPU_DOOM) != 0u) {
            doomGpuStep();
        } else if ((field.control & CTRL_HOST_CPU) == 0u) {"""


def extract_doom_helpers(inc: str) -> str:
    start = inc.find("const uint DOOM_MODE_TITLE = 0u;")
    end = inc.find("void doomGpuStep()")
    if start < 0 or end < 0:
        raise SystemExit("doom_gpu.inc markers missing")
    helpers = inc[start:end].rstrip() + "\n\n"
    step_i = inc.find("void doomGpuStep() {")
    if step_i < 0:
        raise SystemExit("doomGpuStep not found in doom_gpu.inc")
    return helpers + inc[step_i:].rstrip() + "\n"


def patch_shader(text: str, helpers: str, *, canvas: bool) -> str:
    if "const uint CTRL_GPU_DOOM = 128u;" not in text:
        if canvas:
            text = text.replace(
                "const uint CTRL_DOS_PANEL_STRETCH = 512u;\nconst uint CTRL_HOST_CPU = 8u;\n",
                CONST_PATCH_CANVAS,
                1,
            )
        else:
            text = text.replace(
                "const uint CTRL_DOS_PANEL_FS = 256u;\nconst uint CTRL_DOS_PANEL_STRETCH = 512u;\n",
                CONST_PATCH_X86,
                1,
            )

    if "uint doomState(uint slot)" not in text:
        anchor = "bool dosPanelBorderHit(vec2 pix, DoomViewport v) {"
        i = text.find(anchor)
        if i < 0:
            raise SystemExit("dosPanelBorderHit anchor missing")
        j = text.find("\n}\n\n", i)
        if j < 0:
            raise SystemExit("dosPanelBorderHit block end missing")
        j += len("\n}\n\n")
        text = text[:j] + helpers + text[j:]

    old_init = """    } else if (gpuDoom) {
        die.cycle_count = 0u;
    }"""
    if old_init in text and INIT_GPU_PATCH not in text:
        text = text.replace(old_init, INIT_GPU_PATCH, 1)

    if "doomViewportColor(panelPos" not in text:
        text = text.replace(
            "    if ((field.control & CTRL_RTXDOS) != 0u) {\n"
            "        vec3 dosCol = rtxDosViewportColor(panelPos, vec2(img), theme);",
            RENDER_GPU_HOOK + "\n"
            "        vec3 dosCol = rtxDosViewportColor(panelPos, vec2(img), theme);",
            1,
        )

    if "doomGpuStep();" not in text:
        text = text.replace(
            "        if ((field.control & CTRL_HOST_CPU) == 0u) {",
            MAIN_GPU_HOOK,
            1,
        )
    return text


def main() -> int:
    if not X86.is_file() or not DOOM_INC.is_file():
        raise SystemExit("missing x86.comp or doom_gpu.inc")
    helpers = extract_doom_helpers(DOOM_INC.read_text(encoding="utf-8"))
    x86_text = patch_shader(X86.read_text(encoding="utf-8"), helpers, canvas=False)
    X86.write_text(x86_text, encoding="utf-8")
    print(f"patched {X86.name}")
    canvas = SHADERS / "CANVAS.comp"
    if canvas.is_file():
        canvas_text = patch_shader(canvas.read_text(encoding="utf-8"), helpers, canvas=True)
        canvas.write_text(canvas_text, encoding="utf-8")
        print(f"patched {canvas.name}")
    sync = SHADERS / "sync_canvas_shaders.py"
    if sync.is_file():
        import subprocess
        subprocess.run([sys.executable, str(sync)], cwd=SHADERS, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())