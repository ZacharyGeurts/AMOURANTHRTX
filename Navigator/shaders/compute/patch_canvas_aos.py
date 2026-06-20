#!/usr/bin/env python3
"""Sync AmouranthOS chrome + WM + master theme from x86.comp into CANVAS.comp."""
from __future__ import annotations

import sys
from pathlib import Path

BASE = Path(__file__).parent
X86 = BASE / "x86.comp"
CANVAS = BASE / "CANVAS.comp"

BIND_MARK = "layout(binding = 11, rgba8) uniform readonly image2D ammoTex;"
BIND_ADD = """
// AmouranthOS — photo wallpapers + Start orb icon
layout(binding = 12, rgba8) uniform readonly image2D aosWallTex;
layout(binding = 13, rgba8) uniform readonly image2D aosStartTex;
"""

THEME_START = "ThemePal loadTheme(uint id) {"
THEME_END = "uint themeIdFromControl()"

AOS_START = "vec2 aosViewport(vec2 storageImg) {"
AOS_END = "void main()"

INK_CONST = (
    "const vec3 AOS_INK_BLACK = vec3(0.02, 0.02, 0.03);\n"
    "const vec3 AOS_INK_TASKBAR = vec3(0.94, 0.90, 0.98);\n\n"
)

WM_START = "vec4 sampleAosIconRgba(uint slot, vec2 uv) {"
WM_END = "vec3 rtxDosViewportColor(vec2 pix"

RENDER_HOOK = """    if (aosChromeActive()) {
        vec3 aos = amouranthOsBackdrop(panelPos, vec2(img), theme);
        if (aos.x >= 0.0)
            return finishHudColor(aos, panelPos, vec2(img), hud.puv, t, theme, hoverSec);
    }
"""

RAM_CONST_OLD = """const uint AOS_MENU_RAM = 0x000B8E00u;
const uint AOS_MENU_FLYOUT_OFF = 0x10u + 24u * 112u;
const uint AOS_MENU_ROW_STRIDE = 112u;
const uint AOS_MENU_LABEL_LEN = 36u;
const uint AOS_MENU_DESC_LEN = 72u;
// Materials.hpp chrome indices
const uint AOS_MAT_FRAME  = 37u;
const uint AOS_MAT_TITLE  = 38u;
const uint AOS_MAT_ACCENT = 1u;
const uint AOS_MAT_PANEL  = 36u;
const uint AOS_TASKBAR_RAM = 0x000B92300u;
const uint AOS_FOOTER_RAM = 0x000B8FA00u;"""

RAM_CONST_NEW = """const uint AOS_MENU_RAM = 0x000B9100u;
const uint AOS_MENU_FLYOUT_OFF = 0x10u + 24u * 112u;
const uint AOS_MENU_ROW_STRIDE = 112u;
const uint AOS_MENU_LABEL_LEN = 36u;
const uint AOS_MENU_DESC_LEN = 72u;
// Materials.hpp chrome indices
const uint AOS_MAT_FRAME  = 37u;
const uint AOS_MAT_TITLE  = 38u;
const uint AOS_MAT_ACCENT = 1u;
const uint AOS_MAT_PANEL  = 36u;
const uint AOS_FILES_MENU_RAM = 0x000BA010u;
const uint AOS_FILES_ROW_STRIDE = 24u;
const uint AOS_FILES_LABEL_LEN = 8u;
const uint AOS_TASKBAR_RAM = 0x000BA400u;
const uint AOS_FOOTER_RAM = 0x000BA600u;"""

RENDER_HOOK_OLD = """    if ((field.control & CTRL_RTXDOS) != 0u) {
        vec3 aos = amouranthOsChrome(panelPos, vec2(img), theme);
        if (aos.x >= 0.0)
            return displayMap(applyRtxFinish(hud.puv, aos, t, theme, hoverSec));
    }
"""

THEME_ID_OLD = "uint themeIdFromControl() { return (field.control >> 8u) & 7u; }"

SDF_INCLUDE = '#include "aos_sdf.inc"'
SDF_ROUND_OLD = """float sdRoundBox(vec2 p, vec2 halfSize, float radius) {
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}"""
SDF_ROUND_NEW = """#include "aos_sdf.inc"
#include "aos_sdf_wm.inc"
float sdRoundBox(vec2 p, vec2 halfSize, float radius) {
    return aosSdRoundBox(p, halfSize, radius);
}"""


def extract(src: str, start: str, end: str) -> str:
    i = src.find(start)
    j = src.find(end, i)
    if i < 0 or j < 0:
        raise SystemExit(f"marker not found: {start!r} .. {end!r}")
    return src[i:j]


def patch_theme(canvas: str, theme_block: str) -> str:
    i = canvas.find(THEME_START)
    j = canvas.find(THEME_END, i)
    if i < 0 or j < 0:
        raise SystemExit("theme block not found in CANVAS")
    return canvas[:i] + theme_block + canvas[j:]


def dedup_pre_rtx_dos(canvas: str) -> str:
    """Remove duplicate FIELD_LAYER + WM blocks accidentally stacked before rtxDos."""
    while True:
        rtx_at = canvas.find("vec3 rtxDosViewportColor(vec2 pix")
        if rtx_at < 0:
            break
        prefix = canvas[:rtx_at]
        first_fl = prefix.find("const uint FIELD_LAYER_VGA = 1u;")
        second_fl = prefix.find("const uint FIELD_LAYER_VGA = 1u;", first_fl + 1)
        if second_fl < 0:
            break
        canvas = canvas[:second_fl] + canvas[rtx_at:]
    return canvas


def ensure_sdf_include(canvas: str) -> str:
    wm_inc = '#include "aos_sdf_wm.inc"'
    if SDF_INCLUDE in canvas and wm_inc not in canvas:
        canvas = canvas.replace(SDF_INCLUDE, SDF_INCLUDE + "\n" + wm_inc, 1)
    if SDF_INCLUDE in canvas:
        return canvas
    if SDF_ROUND_OLD in canvas:
        return canvas.replace(SDF_ROUND_OLD, SDF_ROUND_NEW, 1)
    if "float sdRoundBox(vec2 p, vec2 halfSize, float radius) {" in canvas:
        return canvas.replace(
            "float sdRoundBox(vec2 p, vec2 halfSize, float radius) {",
            SDF_ROUND_NEW + "\n",
            1,
        )
    raise SystemExit("sdRoundBox block not found in CANVAS — cannot inject aos_sdf.inc")


def ensure_doom_title_h(canvas: str) -> str:
    old = "        titleH = 22.0 * max(img.x / 1920.0, 0.65);"
    new = "        titleH = rtxWmTitleH(img);"
    if old in canvas:
        canvas = canvas.replace(old, new, 1)
    marker = "DoomViewport doomViewportMap(vec2 pix, vec2 img) {"
    fwd = "float rtxWmTitleH(vec2 img);\n\n"
    if marker in canvas and fwd not in canvas:
        canvas = canvas.replace(marker, fwd + marker, 1)
    return canvas


def strip_block(canvas: str, start: str, end: str) -> str:
    while start in canvas:
        i = canvas.find(start)
        j = canvas.find(end, i)
        if i < 0 or j < 0:
            break
        canvas = canvas[:i] + canvas[j:]
    return canvas


def main() -> None:
    x86 = X86.read_text()
    canvas = CANVAS.read_text()

    if BIND_ADD.strip() not in canvas:
        canvas = canvas.replace(BIND_MARK, BIND_MARK + BIND_ADD)

    theme = extract(x86, THEME_START, THEME_END)
    canvas = patch_theme(canvas, theme)

    if "AOS_INK_TASKBAR" not in canvas:
        if "AOS_INK_BLACK" in canvas:
            canvas = canvas.replace(
                "const vec3 AOS_INK_BLACK = vec3(0.02, 0.02, 0.03);\n",
                INK_CONST,
                1,
            )
        else:
            canvas = canvas.replace(
                "vec3 mixHudInk(vec3 base, float ink, float border, vec3 fg) {",
                INK_CONST + "vec3 mixHudInk(vec3 base, float ink, float border, vec3 fg) {",
                1,
            )

    aos = extract(x86, AOS_START, AOS_END)
    wm = extract(x86, WM_START, WM_END)

    canvas = strip_block(canvas, AOS_START, "void main()")
    for legacy in ("vec3 amouranthOsChrome(vec2 pix", "vec3 amouranthOsBackdrop(vec2 pix",
                   "vec3 amouranthOsOverlay(vec2 pix", "vec3 aosLayerChrome(vec3 base",
                   "vec3 finishHudColor(vec3 color", "vec3 aosStartButtonPanel(vec2 pix"):
        canvas = strip_block(canvas, legacy, "void main()")
    for wm_mark in (WM_START, "vec3 sampleAosStart(vec2 uv) {", "float rtxWmTitleH(vec2 img) {"):
        canvas = strip_block(canvas, wm_mark, "vec3 rtxDosViewportColor(vec2 pix")

    wm_at = canvas.find("vec3 rtxDosViewportColor(vec2 pix")
    if wm_at < 0:
        raise SystemExit("rtxDosViewportColor not found")
    if WM_START not in canvas[:wm_at]:
        canvas = canvas[:wm_at] + wm + canvas[wm_at:]
    canvas = dedup_pre_rtx_dos(canvas)

    insert_at = canvas.find("void main()")
    if insert_at < 0:
        raise SystemExit("void main() not found in CANVAS")
    canvas = canvas[:insert_at] + aos + canvas[insert_at:]

    FS_OLD = """    float mouse_x;  // framebuffer UV 0..1, <0 = no loupe
    float mouse_y;
} field;"""
    FS_NEW = """    float mouse_x;  // framebuffer UV 0..1, <0 = no loupe
    float mouse_y;
    float viewport_w;  // dispatched framebuffer width (may differ from storage image)
    float viewport_h;  // dispatched framebuffer height — taskbar anchors here
} field;"""
    if FS_OLD in canvas:
        canvas = canvas.replace(FS_OLD, FS_NEW, 1)

    if RENDER_HOOK.strip() not in canvas:
        if RENDER_HOOK_OLD.strip() in canvas:
            canvas = canvas.replace(RENDER_HOOK_OLD, RENDER_HOOK, 1)
        else:
            canvas = canvas.replace(
                "    if ((field.control & CTRL_GPU_DOOM) != 0u) {",
                RENDER_HOOK + "    if ((field.control & CTRL_GPU_DOOM) != 0u) {",
                1,
            )

    old_finish = "    color = applyRtxFinish(hud.puv, color, t, theme, hoverSec);\n    return displayMap(color);"
    new_finish = "    return finishHudColor(color, panelPos, vec2(img), hud.puv, t, theme, hoverSec);"
    if old_finish in canvas:
        canvas = canvas.replace(old_finish, new_finish, 1)

    old_bg_finish = "        return applyRtxFinish(hud.puv, bg, t, theme, hoverSec);"
    new_bg_finish = "        return finishHudColor(bg, panelPos, vec2(img), hud.puv, t, theme, hoverSec);"
    if old_bg_finish in canvas:
        canvas = canvas.replace(old_bg_finish, new_bg_finish, 1)

    for old_doom, new_doom in (
        ("return displayMap(applyRtxFinish(hud.puv, doomCol, t, theme, hoverSec));",
         "return finishHudColor(doomCol, panelPos, vec2(img), hud.puv, t, theme, hoverSec);"),
        ("return displayMap(applyRtxFinish(hud.puv, dosCol, t, theme, hoverSec));",
         "return finishHudColor(dosCol, panelPos, vec2(img), hud.puv, t, theme, hoverSec);"),
    ):
        if old_doom in canvas:
            canvas = canvas.replace(old_doom, new_doom, 1)

    # doomViewportMap title offset
    old_p0 = "    vec2 p0 = winP0;\n    vec2 p1 = winP1 - vec2(0.0, hudH);"
    new_p0 = """    float titleH = 0.0;
    if ((field.data_bus[42] & 4096u) != 0u)
        titleH = 34.0 * max(img.x / 1920.0, 0.75);
    vec2 p0 = winP0 + vec2(0.0, titleH);
    vec2 p1 = winP1 - vec2(0.0, hudH);"""
    if old_p0 in canvas:
        canvas = canvas.replace(old_p0, new_p0)

    # rtxDosViewportColor WM hooks
    old_fd = """    vec2 winLocal = pix - winP0;
    vec2 winSize = winP1 - winP0;
    if (dosWindowBorderHit(winLocal, winSize))"""
    new_fd = """    vec2 winLocal = pix - winP0;
    vec2 winSize = winP1 - winP0;
    float titleH = rtxWmTitleH(vec2(img));
    bool aosWm = (field.data_bus[42] & 4096u) != 0u;
    if (aosWm && titleH > 0.0 && winLocal.y < titleH)
        return rtxWmTitleBar(winLocal, winSize, titleH, th);
    if (aosWm) {
        vec3 grip = rtxWmResizeGrip(winLocal, winSize, titleH, hudH, th);
        if (grip.x >= 0.0) return grip;
    }
    if (dosWindowBorderHit(winLocal, winSize))"""
    if old_fd in canvas:
        canvas = canvas.replace(old_fd, new_fd)

    if THEME_ID_OLD in canvas:
        theme_id = extract(x86, "uint themeIdFromControl()", "bool rtxOverlayOn()")
        canvas = canvas.replace(THEME_ID_OLD, theme_id, 1)

    canvas = ensure_sdf_include(canvas)
    canvas = ensure_doom_title_h(canvas)

    quick_const = (
        "const float AOS_START_BTN_W = 156.0;\n"
        "const float AOS_FOLDER_BTN_W = 44.0;\n"
        "const float AOS_QUICK_BTN_W = 44.0;\n"
        "const int AOS_QUICK_LAUNCH_N = 4;"
    )
    if "AOS_QUICK_LAUNCH_N" not in canvas and "const float AOS_FOLDER_BTN_W = 44.0;" in canvas:
        canvas = canvas.replace(
            "const float AOS_START_BTN_W = 156.0;\nconst float AOS_FOLDER_BTN_W = 44.0;",
            quick_const,
            1,
        )

    if RAM_CONST_OLD in canvas:
        canvas = canvas.replace(RAM_CONST_OLD, RAM_CONST_NEW, 1)
    elif "AOS_FILES_MENU_RAM" not in canvas and "const uint AOS_TASKBAR_RAM = 0x000BA400u;" in canvas:
        canvas = canvas.replace(
            "const uint AOS_TASKBAR_RAM = 0x000BA400u;",
            """const uint AOS_FILES_MENU_RAM = 0x000BA010u;
const uint AOS_FILES_ROW_STRIDE = 24u;
const uint AOS_FILES_LABEL_LEN = 8u;
const uint AOS_TASKBAR_RAM = 0x000BA400u;""",
            1,
        )

    ammo_block = """        if (isAmmoBoxCell(cell_y, cell_x)) {
            vec3 ammo = ammoBoxViz(sampleLp, t, theme);
            if (ammo != vec3(0.0)) bg = ammo;
        } else if (isGfx) {"""
    gfx_only = "        if (isGfx) {"
    if ammo_block in canvas:
        canvas = canvas.replace(ammo_block, gfx_only, 1)

    help_block = """        vec3 helpBg = helpPopupBg(cell_y, cell_x, hoverSec, theme);
        if (helpBg != vec3(0.0)) bg = helpBg;
"""
    if help_block in canvas:
        canvas = canvas.replace(help_block, "", 1)

    CANVAS.write_text(canvas)
    print(f"Patched {CANVAS}")


if __name__ == "__main__":
    try:
        main()
    except SystemExit as exc:
        raise SystemExit(exc.code if exc.code is not None else 1) from exc