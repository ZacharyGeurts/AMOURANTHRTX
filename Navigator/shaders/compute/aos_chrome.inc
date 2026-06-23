// AmouranthOS chrome layer for aos_load (HUD font + full taskbar chrome)
#include "rtx_font_render.inc"
#include "aos_sdf.inc"

struct ThemePal {
    vec3 void_; vec3 panel; vec3 zebra; vec3 border; vec3 title;
    vec3 section; vec3 label; vec3 value; vec3 warn; vec3 mem; vec3 rule;
    vec3 copper; vec3 silicon; vec3 pcb; vec3 grin; vec3 accent; vec3 gemTint;
    float heatHue; float ior; float dispersion; float facetMul;
};
uint themeIdFromControl() { return field.data_bus[62] & 0xFFu; }

ThemePal loadTheme(uint id) {
    uint pack = field.data_bus[62];
    id = pack & 0xFFu;
    float rose = float((pack >> 8u) & 0xFFu) / 255.0;
    float cyan = float((pack >> 16u) & 0xFFu) / 255.0;
    ThemePal t;
    t.void_ = vec3(0.06, 0.07, 0.09);
    t.panel = vec3(0.18, 0.19, 0.22);
    t.zebra = vec3(0.14, 0.15, 0.18);
    t.border = vec3(0.42, 0.44, 0.48);
    t.title = vec3(0.88, 0.90, 0.94);
    t.section = vec3(0.55, 0.58, 0.62);
    t.label = vec3(0.72, 0.72, 0.76);
    t.value = vec3(0.85, 0.85, 0.88);
    t.warn = vec3(1.0, 0.60, 0.34);
    t.mem = vec3(0.0, 0.45, 0.82);
    t.rule = vec3(0.28, 0.28, 0.32);
    t.copper = vec3(0.85, 0.55, 0.15);
    t.silicon = vec3(0.12, 0.12, 0.14);
    t.pcb = vec3(0.06, 0.06, 0.08);
    t.grin = vec3(0.55, 0.58, 0.64);
    t.accent = vec3(rose * 0.55, cyan * 0.45, cyan);
    if (length(t.accent) < 0.08)
        t.accent = vec3(0.20, 0.45, 0.72);
    t.gemTint = mix(t.accent, vec3(0.35, 0.55, 0.75), 0.55);
    if (id == 1u) {
        t.accent = vec3(0.25, 0.50, 0.78);
        t.gemTint = vec3(0.30, 0.55, 0.72);
        t.panel = vec3(0.22, 0.23, 0.26);
        t.zebra = vec3(0.16, 0.17, 0.20);
        t.border = vec3(0.48, 0.50, 0.54);
        t.title = vec3(0.92, 0.94, 0.96);
        t.mem = vec3(0.30, 0.55, 0.82);
    }
    if (id == 4u) {
        t.void_ = vec3(0.04, 0.05, 0.08);
        t.panel = vec3(0.11, 0.12, 0.16);
        t.zebra = vec3(0.08, 0.09, 0.13);
        t.border = vec3(0.26, 0.28, 0.34);
        t.title = vec3(0.80, 0.82, 0.88);
        t.section = vec3(0.40, 0.42, 0.48);
        t.label = vec3(0.64, 0.66, 0.72);
        t.value = vec3(0.76, 0.78, 0.84);
        t.accent = vec3(0.34, 0.42, 0.56);
        t.gemTint = vec3(0.28, 0.36, 0.50);
        t.mem = vec3(0.24, 0.36, 0.52);
        t.silicon = vec3(0.09, 0.10, 0.13);
        t.pcb = vec3(0.05, 0.06, 0.09);
    }
    if (id == 7u) {
        t.void_ = vec3(0.01, 0.01, 0.012);
        t.panel = vec3(0.06, 0.06, 0.065);
        t.zebra = vec3(0.04, 0.04, 0.045);
        t.border = vec3(0.12, 0.12, 0.14);
        t.title = vec3(0.90, 0.90, 0.92);
        t.section = vec3(0.35, 0.35, 0.38);
        t.label = vec3(0.58, 0.58, 0.62);
        t.value = vec3(0.74, 0.74, 0.78);
        t.accent = vec3(0.42, 0.58, 0.88);
        t.gemTint = vec3(0.32, 0.45, 0.68);
        t.mem = vec3(0.30, 0.48, 0.82);
        t.silicon = vec3(0.05, 0.05, 0.055);
        t.pcb = vec3(0.02, 0.02, 0.022);
    }
    if (id == 8u) {
        t.void_ = vec3(0.05, 0.05, 0.06);
        t.panel = vec3(0.12, 0.12, 0.13);
        t.zebra = vec3(0.09, 0.09, 0.10);
        t.border = vec3(0.22, 0.22, 0.24);
        t.title = vec3(0.91, 0.91, 0.92);
        t.section = vec3(0.38, 0.38, 0.40);
        t.label = vec3(0.62, 0.62, 0.64);
        t.value = vec3(0.78, 0.78, 0.80);
        t.accent = vec3(0.54, 0.71, 0.97);
        t.gemTint = vec3(0.40, 0.55, 0.78);
        t.mem = vec3(0.35, 0.55, 0.90);
        t.silicon = vec3(0.10, 0.10, 0.11);
        t.pcb = vec3(0.06, 0.06, 0.07);
    }
    t.heatHue = 0.58; t.ior = 1.45; t.dispersion = 0.02; t.facetMul = 1.5;
    return t;
}
uint ramByte(uint addr) {
    if (addr >= uint(die.RAM.length()) * 4u) return 0u;
    uint widx = addr >> 2; uint shift = (addr & 3u) << 3;
    return (die.RAM[widx] >> shift) & 0xFFu;
}
bool aosChromeActive() {
    uint b = field.data_bus[42];
    return (b & 4096u) != 0u || (b & (1u << 29u)) != 0u;
}
const uint AOS_TASKBAR_RAM = 0x000BA400u;
const uint AOS_FOOTER_RAM = 0x000BA600u;
const uint CLOCK_DATE_RAM = 0x000BA700u;
const uint AOS_FOOTER_LEN = 72u;
const uint AOS_SURFACE_RAM = 0x000B9000u;
const uint AOS_TAB_STRIDE = 32u;
const uint AOS_START_LABEL_OFF = 0x80u;
const float AOS_START_BTN_W = 156.0;
const float AOS_FOLDER_BTN_W = 44.0;
const float AOS_QUICK_BTN_W = 44.0;
const float AOS_CLOCK_W = 200.0;
const int AOS_QUICK_LAUNCH_N = 4;

float dosHudGlyph(uint ch, vec2 uv, float x0, float rowY, vec2 cell) {
    return fontGlyphGeom(ch, uv, x0, rowY, cell).x;
}

vec3 dosHudRunGeom(vec2 uv, float x0, float rowY, vec2 cell,
    uint c0, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7) {
    vec3 ink = vec3(0.0);
    float x = x0;
    ink = max(ink, fontGlyphGeom(c0, uv, x, rowY, cell)); x += cell.x;
    ink = max(ink, fontGlyphGeom(c1, uv, x, rowY, cell)); x += cell.x;
    ink = max(ink, fontGlyphGeom(c2, uv, x, rowY, cell)); x += cell.x;
    ink = max(ink, fontGlyphGeom(c3, uv, x, rowY, cell)); x += cell.x;
    ink = max(ink, fontGlyphGeom(c4, uv, x, rowY, cell)); x += cell.x;
    ink = max(ink, fontGlyphGeom(c5, uv, x, rowY, cell)); x += cell.x;
    ink = max(ink, fontGlyphGeom(c6, uv, x, rowY, cell)); x += cell.x;
    ink = max(ink, fontGlyphGeom(c7, uv, x, rowY, cell));
    return ink;
}

float dosHudRun(vec2 uv, float x0, float rowY, vec2 cell,
    uint c0, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7) {
    return dosHudRunGeom(uv, x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7).x;
}

const uint AOS_MENU_RAM = 0x000B9100u;
const uint AOS_MENU_FLYOUT_OFF = 0x10u + 24u * 112u;
const uint AOS_MENU_ROW_STRIDE = 112u;
const uint AOS_MENU_LABEL_LEN = 36u;
const uint AOS_MENU_DESC_LEN = 72u;
const uint AOS_MAT_FRAME  = 37u;
const uint AOS_MAT_TITLE  = 38u;
const uint AOS_MAT_ACCENT = 1u;
const uint AOS_MAT_PANEL  = 36u;
const uint AOS_FILES_MENU_RAM = 0x000BA010u;
const uint AOS_FILES_ROW_STRIDE = 24u;
const uint AOS_FILES_LABEL_LEN = 8u;
const uint AOS_FILE_SEARCH_RAM = 0x000BD000u;
const uint AOS_SEARCH_MAX = 10u;
const uint AOS_SEARCH_QUERY_LEN = 32u;
const uint AOS_SEARCH_NAME_LEN = 28u;
const uint AOS_SEARCH_PATH_LEN = 48u;
const uint AOS_SEARCH_ROW_STRIDE = 80u;
const uint AOS_DND_RAM = 0x000BD500u;
const uint AOS_DND_PATH_LEN = 64u;

vec4 dosHudRamStrPack(vec2 uv, float x0, float rowY, vec2 cell, uint ramBase, uint maxChars) {
    vec4 pack = vec4(0.0);
    float x = x0;
    for (uint i = 0u; i < maxChars; ++i) {
        uint ch = ramByte(ramBase + i);
        if (ch == 0u || ch < 32u) break;
        pack = max(pack, fontGlyphPack(ch, uv, x, rowY, cell));
        x += cell.x;
    }
    return pack;
}

vec3 dosHudRamStrGeom(vec2 uv, float x0, float rowY, vec2 cell, uint ramBase, uint maxChars) {
    return dosHudRamStrPack(uv, x0, rowY, cell, ramBase, maxChars).xyz;
}

float dosHudRamStr(vec2 uv, float x0, float rowY, vec2 cell, uint ramBase, uint maxChars) {
    return dosHudRamStrGeom(uv, x0, rowY, cell, ramBase, maxChars).x;
}

float dosHudRamStrBorder(vec2 uv, float x0, float rowY, vec2 cell, uint ramBase, uint maxChars) {
    float border = 0.0;
    border = max(border, dosHudRamStr(uv + vec2(-1.0, 0.0), x0, rowY, cell, ramBase, maxChars));
    border = max(border, dosHudRamStr(uv + vec2(1.0, 0.0), x0, rowY, cell, ramBase, maxChars));
    border = max(border, dosHudRamStr(uv + vec2(0.0, -1.0), x0, rowY, cell, ramBase, maxChars));
    border = max(border, dosHudRamStr(uv + vec2(0.0, 1.0), x0, rowY, cell, ramBase, maxChars));
    border = max(border, dosHudRamStr(uv + vec2(-1.0, -1.0), x0, rowY, cell, ramBase, maxChars));
    border = max(border, dosHudRamStr(uv + vec2(1.0, -1.0), x0, rowY, cell, ramBase, maxChars));
    border = max(border, dosHudRamStr(uv + vec2(-1.0, 1.0), x0, rowY, cell, ramBase, maxChars));
    border = max(border, dosHudRamStr(uv + vec2(1.0, 1.0), x0, rowY, cell, ramBase, maxChars));
    return border;
}

float dosHudRunBorder(vec2 uv, float x0, float rowY, vec2 cell,
    uint c0, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7) {
    float border = 0.0;
    border = max(border, dosHudRun(uv + vec2(-1.0, 0.0), x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7));
    border = max(border, dosHudRun(uv + vec2(1.0, 0.0), x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7));
    border = max(border, dosHudRun(uv + vec2(0.0, -1.0), x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7));
    border = max(border, dosHudRun(uv + vec2(0.0, 1.0), x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7));
    border = max(border, dosHudRun(uv + vec2(-1.0, -1.0), x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7));
    border = max(border, dosHudRun(uv + vec2(1.0, -1.0), x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7));
    border = max(border, dosHudRun(uv + vec2(-1.0, 1.0), x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7));
    border = max(border, dosHudRun(uv + vec2(1.0, 1.0), x0, rowY, cell, c0, c1, c2, c3, c4, c5, c6, c7));
    return border;
}

const vec3 AOS_INK_BLACK = FONT_INK_LIGHT;
const vec3 AOS_INK_DIM = vec3(0.76, 0.78, 0.84);
const vec3 AOS_INK_TASKBAR = FONT_INK_LIGHT;
const vec3 AOS_INK_PANEL = FONT_INK_DARK;

float vgaGlyphBorder(uint ch, vec2 cell, uint px, uint py) {
    float b = 0.0;
    if (px > 0u) b = max(b, fontOn(ch, px - 1u, py) ? 1.0 : 0.0);
    if (px < 7u) b = max(b, fontOn(ch, px + 1u, py) ? 1.0 : 0.0);
    if (py > 0u) b = max(b, fontOn(ch, px, py - 1u) ? 1.0 : 0.0);
    if (py < 15u) b = max(b, fontOn(ch, px, py + 1u) ? 1.0 : 0.0);
    return b;
}

vec3 dosHudUintInkGeom(vec2 uv, float x0, float rowY, vec2 cell, uint val, uint minDigits) {
    vec3 ink = vec3(0.0);
    uint divv = 1u;
    for (uint i = 1u; i < 10u; ++i) {
        if (val / divv < 10u && i >= minDigits) break;
        divv *= 10u;
    }
    float x = x0;
    for (uint place = divv; place >= 1u; place /= 10u) {
        uint digit = (val / place) % 10u;
        ink = max(ink, fontGlyphGeom(48u + digit, uv, x, rowY, cell));
        x += cell.x;
        if (place == 1u) break;
    }
    return ink;
}

float dosHudUintInk(vec2 uv, float x0, float rowY, vec2 cell, uint val, uint minDigits) {
    return dosHudUintInkGeom(uv, x0, rowY, cell, val, minDigits).x;
}
vec4 sampleAosIconRgba(uint slot, vec2 uv) {
    ivec2 dim = imageSize(aosStartTex);
    if (dim.x <= 0 || dim.y <= 0) return vec4(-1.0);
    const float ICON_CELL = 64.0;
    float cols = float(dim.x) / ICON_CELL;
    float rows = ceil(float(dim.y) / ICON_CELL);
    float c = mod(float(slot), cols);
    float r = floor(float(slot) / cols);
    vec2 atlasUv = (vec2(c, r) + clamp(uv, 0.0, 1.0)) / vec2(cols, rows);
    ivec2 px = clamp(ivec2(atlasUv * vec2(dim)), ivec2(0), dim - ivec2(1));
    return imageLoad(aosStartTex, px);
}

vec3 sampleAosIcon(uint slot, vec2 uv) {
    return sampleAosIconRgba(slot, uv).rgb;
}

vec3 aosBlendIcon(vec3 base, vec4 icon) {
    if (icon.a < 0.04) return base;
    return mix(base, icon.rgb, clamp(icon.a, 0.0, 1.0));
}
vec2 aosViewport(vec2 storageImg) {
    float vw = field.viewport_w;
    float vh = field.viewport_h;
    if (vw >= 2.0 && vh >= 2.0)
        return vec2(vw, vh);
    return storageImg;
}

const float AOS_UI_REF_W = 3840.0;

float aosUiScale(vec2 img) {
    return max(img.x / AOS_UI_REF_W, 0.75) * 1.35;
}

#include "aos_taskbar_layout.inc"

vec3 sampleAosWall(vec2 uv) {
    ivec2 dim = imageSize(aosWallTex);
    if (dim.x <= 0 || dim.y <= 0) return vec3(-1.0);
    ivec2 px = clamp(ivec2(uv * vec2(dim)), ivec2(0), dim - ivec2(1));
    return imageLoad(aosWallTex, px).rgb;
}

vec2 aosCoverUv(vec2 pix, vec2 origin, vec2 size, ivec2 texDim) {
    if (texDim.x <= 0 || texDim.y <= 0) return vec2(-1.0);
    vec2 local = pix - origin;
    vec2 t = vec2(texDim);
    float deskAspect = size.x / max(size.y, 1.0);
    float texAspect = t.x / max(t.y, 1.0);
    vec2 uv = local / size;
    if (texAspect > deskAspect) {
        float sc = deskAspect / texAspect;
        uv.x = (uv.x - 0.5) / sc + 0.5;
    } else {
        float sc = texAspect / deskAspect;
        uv.y = (uv.y - 0.5) / sc + 0.5;
    }
    return uv;
}

vec3 aosDarkDesktop(vec2 pix, vec2 img, float taskH, ThemePal th) {
    float deskH = max(img.y - taskH, 1.0);
    if (pix.y >= deskH) return vec3(-1.0);
    float y = pix.y / deskH;
    float x = pix.x / max(img.x, 1.0);
    vec3 col = mix(th.pcb, th.silicon, y * 0.42 + x * 0.08);
    float grain = sin(pix.x * 0.11 + pix.y * 0.07) * sin(pix.y * 0.19);
    col += vec3(grain * 0.008);
    float vig = 1.0 - 0.10 * length(vec2(x - 0.5, y - 0.45));
    return clamp(col * vig, 0.0, 1.0);
}
vec3 aosHudTimePartGeom(vec2 uv, float x0, float rowY, vec2 cell, uint val) {
    vec3 ink = dosHudUintInkGeom(uv, x0, rowY, cell, val / 10u, 1u);
    ink = max(ink, dosHudUintInkGeom(uv, x0 + cell.x, rowY, cell, val % 10u, 1u));
    return ink;
}

float aosHudTimePart(vec2 uv, float x0, float rowY, vec2 cell, uint val) {
    return aosHudTimePartGeom(uv, x0, rowY, cell, val).x;
}

float aosHudTimePartBorder(vec2 uv, float x0, float rowY, vec2 cell, uint val) {
    float border = aosHudTimePart(uv + vec2(-1.0, 0.0), x0, rowY, cell, val);
    border = max(border, aosHudTimePart(uv + vec2(1.0, 0.0), x0, rowY, cell, val));
    border = max(border, aosHudTimePart(uv + vec2(0.0, -1.0), x0, rowY, cell, val));
    border = max(border, aosHudTimePart(uv + vec2(0.0, 1.0), x0, rowY, cell, val));
    return border;
}

float aosTaskbarClockWidth(float scale) {
    return AOS_CLOCK_W * scale;
}

vec4 sampleAmmoPortraitRgba(vec2 pix, vec2 box0, vec2 boxSize) {
    ivec2 dim = imageSize(ammoTex);
    if (dim.x <= 0 || dim.y <= 0) return vec4(-1.0);
    vec2 local = (pix - box0) / max(boxSize, vec2(1.0));
    if (any(lessThan(local, vec2(0.0))) || any(greaterThan(local, vec2(1.0))))
        return vec4(-1.0);
    float imgAspect = float(dim.x) / float(dim.y);
    float boxAspect = boxSize.x / max(boxSize.y, 1.0);
    vec2 fitFrac = (imgAspect > boxAspect)
        ? vec2(1.0, boxAspect / imgAspect)
        : vec2(imgAspect / boxAspect, 1.0);
    vec2 uv = (local - 0.5) / fitFrac + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
        return vec4(-1.0);
    ivec2 px = clamp(ivec2(uv * vec2(dim)), ivec2(0), dim - ivec2(1));
    return imageLoad(ammoTex, px);
}

vec2 aosHudTextCell(float sc) { return vec2(8.0 * sc, 16.0 * sc); }
vec2 aosHudTextCellSm(float sc) { return vec2(8.0 * sc, 14.0 * sc); }

#include "aos_sdf_chrome.inc"

vec3 aosTaskbarTimeInkGeom(vec2 pix, float x0, float rowY, vec2 cell, uint hr, uint mn, uint sc,
        out float inkB) {
    vec4 pack = aosTaskbarTimePack(pix, x0, rowY, cell, hr, mn, sc);
    inkB = pack.w;
    return pack.xyz;
}

float aosTaskbarTimeInk(vec2 pix, float x0, float rowY, vec2 cell, uint hr, uint mn, uint sc,
        out float inkB) {
    return aosTaskbarTimeInkGeom(pix, x0, rowY, cell, hr, mn, sc, inkB).x;
}

float aosQuickLaunchStripW(float scale) {
    return float(AOS_QUICK_LAUNCH_N) * AOS_QUICK_BTN_W * scale
        + float(AOS_QUICK_LAUNCH_N - 1) * 4.0 * scale;
}

vec3 aosMenuPanelBg(vec2 rel, vec2 panelSize, float scale, ThemePal th, float t) {
    vec3 face = mix(th.void_ * 1.4, th.panel, 0.68);
    return aosSdWin31Panel(rel, vec2(0.0), panelSize, face, 2.5 * scale);
}

vec3 aosMenuDrawRow(vec2 pix, vec3 base, vec2 panelOrigin, float panelW, float rowY0, float rowH,
        float scale, uint rowBase, uint hoverRow, uint rowIdx, uint rowCount, ThemePal th,
        bool showChevron, bool showDesc) {
    if (pix.y < rowY0 || pix.y >= rowY0 + rowH) return base;
    if (pix.x < panelOrigin.x || pix.x >= panelOrigin.x + panelW) return base;

    uint rowType = ramByte(rowBase);
    if (rowType == 0xFFu) return base;
    if (rowType == 2u) {
        float lineY = rowY0 + rowH * 0.5;
        if (abs(pix.y - lineY) < 1.5 * scale)
            return mix(base, vec3(0.32, 0.32, 0.38), 0.9);
        return base;
    }

    vec2 cell = vec2(8.0 * scale, 12.0 * scale);
    vec3 col = base;
    bool hov = hoverRow < rowCount && hoverRow == rowIdx;
    if (hov)
        col = mix(col, vec3(0.20, 0.20, 0.26), 0.88);

    float labelY = showDesc ? rowY0 + 10.0 * scale : rowY0 + rowH * 0.5 - 5.0 * scale;
    float rowMidY = labelY;
    if (showChevron && rowType == 0u) {
        uint flyCat = ramByte(AOS_MENU_RAM + 2u);
        bool open = flyCat == ramByte(rowBase + 3u);
        vec3 arrowGeom = fontGlyphGeom(62u, pix, panelOrigin.x + panelW - 14.0 * scale, rowMidY, cell);
        vec3 arrowCol = open ? vec3(0.55, 0.82, 0.95) : vec3(0.55, 0.55, 0.60);
        col = mixHudInk(col, arrowGeom, 0.0, arrowCol);
    }

    uint icon = ramByte(rowBase + 1u);
    float iconSz = (rowType == 0u) ? 22.0 * scale : 20.0 * scale;
    float iconX = panelOrigin.x + 10.0 * scale;
    vec2 icon0 = vec2(iconX, rowY0 + (rowH - iconSz) * 0.5);
    vec2 iuv = (pix - icon0) / iconSz;
    if (icon > 0u && all(greaterThanEqual(iuv, vec2(0.0))) && all(lessThan(iuv, vec2(1.0))))
        col = aosBlendIcon(col, sampleAosIconRgba(icon, iuv));

    float tx = icon0.x + iconSz + 6.0 * scale;
    vec4 inkPack = dosHudRamStrPack(pix, tx, rowMidY, cell, rowBase + 4u, AOS_MENU_LABEL_LEN);
    if (rowType == 3u)
        col = mixHudInkPolar(col, inkPack, 0.0, mix(th.warn, vec3(0.98, 0.92, 0.88), 0.35), true);
    else
        col = mixHudInkLight(col, inkPack, 0.0);

    if (showDesc && rowType == 1u) {
        float descY = rowY0 + 28.0 * scale;
        vec4 descPack = dosHudRamStrPack(pix, tx, descY, cell,
            rowBase + 4u + AOS_MENU_LABEL_LEN, AOS_MENU_DESC_LEN);
        float descFade = hov ? 0.75 : 0.50;
        descPack.xyz *= descFade;
        col = mixHudInkPolar(col, descPack, 0.0, AOS_INK_DIM, true);
    }
    return max(col, vec3(0.0));
}
vec3 aosStartMenu(vec2 pix, vec2 img, float taskH, ThemePal th) {
    float scale = aosUiScale(img);
    float pad = 6.0 * scale;
    float rootW = 196.0 * scale;
    float flyW = 420.0 * scale;
    float gap = 4.0 * scale;
    float rowH = 50.0 * scale;
    float headH = 28.0 * scale;
    float searchW = 300.0 * scale;
    float searchRowH = 28.0 * scale;

    uint rootRows = (field.data_bus[54] >> 16u) & 0xFFu;
    if (rootRows == 0u) rootRows = max(ramByte(AOS_MENU_RAM), 1u);
    uint flyCat = ramByte(AOS_MENU_RAM + 2u);
    uint flyRows = ramByte(AOS_MENU_RAM + 4u);
    bool searchActive = ramByte(AOS_FILE_SEARCH_RAM + 3u) != 0u;
    uint searchCount = ramByte(AOS_FILE_SEARCH_RAM);
    bool flyOpen = flyCat < 4u && flyRows > 0u && !searchActive;

    vec2 vp = aosViewport(img);
    AosTbLayout tbL = aosTbLayout(vp);
    vec2 startB0, startB1;
    aosTbStartBounds(tbL, startB0, startB1);

    float rootH = headH + rowH * float(rootRows) + pad * 2.0;
    float flyH = rowH * float(flyRows) + pad * 2.0;
    float searchH = searchActive ? pad * 2.0 + searchRowH * float(searchCount) : 0.0;
    float rootTop = startB0.y - rootH;
    float flyTop = startB0.y - flyH;
    float searchTop = startB0.y - searchH;
    float rootLeft = startB0.x;
    float clipTop = rootTop;
    if (flyOpen) clipTop = min(clipTop, flyTop);
    if (searchActive) clipTop = min(clipTop, searchTop);
    float totalW = rootLeft + rootW + (flyOpen ? gap + flyW : 0.0)
        + (searchActive ? gap + searchW : 0.0);

    if (pix.x < rootLeft || pix.x > totalW || pix.y < clipTop || pix.y >= tbL.barY1)
        return vec3(-1.0);

    float t = field.sealed_time;
    vec3 col = vec3(-1.0);

    vec2 rootOrigin = vec2(rootLeft, rootTop);
    vec2 rootRel = pix - rootOrigin;
    if (rootRel.x >= 0.0 && rootRel.x < rootW && rootRel.y >= 0.0 && rootRel.y < rootH) {
        col = aosMenuPanelBg(rootRel, vec2(rootW, rootH), scale, th, t);
        vec2 cell = vec2(8.0 * scale, 12.0 * scale);
        float hx = rootOrigin.x + 12.0 * scale;
        float hy = rootTop + 8.0 * scale;
        uint qLen = ramByte(AOS_FILE_SEARCH_RAM + 1u);
        if (qLen > 0u) {
            col = mixHudInk(col,
                dosHudRamStrGeom(pix, hx, hy, cell, AOS_FILE_SEARCH_RAM + 4u, AOS_SEARCH_QUERY_LEN),
                dosHudRamStrBorder(pix, hx, hy, cell, AOS_FILE_SEARCH_RAM + 4u, AOS_SEARCH_QUERY_LEN),
                AOS_INK_BLACK);
        } else {
            col = mixHudInk(col,
                dosHudRunGeom(pix, hx, hy, cell, 83u, 101u, 97u, 114u, 99u, 104u, 46u, 46u),
                dosHudRunBorder(pix, hx, hy, cell, 83u, 101u, 97u, 114u, 99u, 104u, 46u, 46u),
                AOS_INK_DIM);
        }

        if (rootRel.y >= headH) {
            uint row = uint(clamp(floor((rootRel.y - headH) / rowH), 0.0, float(rootRows) - 1.0));
            uint hoverRow = field.data_bus[54] & 0xFFu;
            uint rowBase = AOS_MENU_RAM + 0x10u + row * AOS_MENU_ROW_STRIDE;
            float rowY0 = rootTop + headH + float(row) * rowH;
            col = aosMenuDrawRow(pix, col, rootOrigin, rootW, rowY0, rowH, scale,
                rowBase, hoverRow, row, rootRows, th, true, false);
        }
    }

    if (flyOpen) {
        vec2 flyOrigin = vec2(rootLeft + rootW + gap, flyTop);
        vec2 flyRel = pix - flyOrigin;
        if (flyRel.x >= 0.0 && flyRel.x < flyW && flyRel.y >= 0.0 && flyRel.y < flyH) {
            col = aosMenuPanelBg(flyRel, vec2(flyW, flyH), scale, th, t);

            uint row = uint(clamp(floor((flyRel.y - pad) / rowH), 0.0, float(flyRows) - 1.0));
            uint flyHover = ramByte(AOS_MENU_RAM + 3u);
            uint rowBase = AOS_MENU_RAM + AOS_MENU_FLYOUT_OFF + row * AOS_MENU_ROW_STRIDE;
            float rowY0 = flyTop + pad + float(row) * rowH;
            col = aosMenuDrawRow(pix, col, flyOrigin, flyW, rowY0, rowH, scale,
                rowBase, flyHover, row, flyRows, th, false, true);
        }
    }

    return col.x < 0.0 ? vec3(-1.0) : max(col, vec3(0.0));
}

vec3 aosFilesMenu(vec2 pix, vec2 img, float taskH, ThemePal th) {
    if ((field.data_bus[42] & (1u << 23u)) == 0u) return vec3(-1.0);

    float scale = aosUiScale(img);
    float pad = 10.0 * scale;
    float headH = 30.0 * scale;
    float cellW = 88.0 * scale;
    float cellH = 96.0 * scale;
    uint itemCount = ramByte(AOS_FILES_MENU_RAM);
    if (itemCount == 0u) itemCount = 12u;
    uint cols = max(ramByte(AOS_FILES_MENU_RAM + 2u), 1u);
    uint rows = (itemCount + cols - 1u) / cols;
    float panelW = pad * 2.0 + cellW * float(cols);
    float panelH = headH + pad * 2.0 + cellH * float(rows);

    vec2 b0, b1;
    aosQuickLaunchBounds(img, 0, b0, b1);
    float panelX = b0.x;
    float panelTop = b0.y - panelH - 6.0 * scale;
    if (pix.x < panelX || pix.x >= panelX + panelW || pix.y < panelTop || pix.y >= panelTop + panelH)
        return vec3(-1.0);

    vec2 rel = pix - vec2(panelX, panelTop);
    float t = field.sealed_time;
    vec3 col = aosMenuPanelBg(rel, vec2(panelW, panelH), scale, th, t);

    vec2 cell = aosHudTextCell(scale);
    float hx = panelX + 14.0 * scale;
    float hy = panelTop + 9.0 * scale;
    col = mixHudInk(col,
        dosHudRunGeom(pix, hx, hy, cell, 70u, 105u, 108u, 101u, 115u, 32u, 32u, 32u),
        dosHudRunBorder(pix, hx, hy, cell, 70u, 105u, 108u, 101u, 114u, 32u, 32u, 32u),
        AOS_INK_BLACK);

    uint hoverIdx = ramByte(AOS_FILES_MENU_RAM + 1u);
    float gridY0 = panelTop + headH + pad;
    for (uint i = 0u; i < itemCount; ++i) {
        uint c = i % cols;
        uint r = i / cols;
        float cx0 = panelX + pad + float(c) * cellW;
        float cy0 = gridY0 + float(r) * cellH;
        if (pix.x < cx0 || pix.x >= cx0 + cellW || pix.y < cy0 || pix.y >= cy0 + cellH)
            continue;

        uint rowBase = AOS_FILES_MENU_RAM + 0x10u + i * AOS_FILES_ROW_STRIDE;
        uint icon = ramByte(rowBase);
        bool hov = hoverIdx < itemCount && hoverIdx == i;
        vec3 tile = hov ? mix(vec3(0.20, 0.20, 0.22), th.accent * 0.42, 0.55) : vec3(0.0);
        col = mix(col, tile, hov ? 0.92 : 0.0);

        float iconSz = 52.0 * scale;
        vec2 icon0 = vec2(cx0 + (cellW - iconSz) * 0.5, cy0 + 6.0 * scale);
        vec2 iuv = (pix - icon0) / iconSz;
        if (all(greaterThanEqual(iuv, vec2(0.0))) && all(lessThan(iuv, vec2(1.0))))
            col = aosBlendIcon(col, sampleAosIconRgba(icon, iuv));

        float labelY = cy0 + iconSz + 10.0 * scale;
        float tx = cx0 + cellW * 0.5 - 20.0 * scale;
        vec2 lcell = aosHudTextCellSm(scale);
        col = mixHudInk(col,
            dosHudRamStrGeom(pix, tx, labelY, lcell, rowBase + 3u, AOS_FILES_LABEL_LEN),
            dosHudRamStrBorder(pix, tx, labelY, lcell, rowBase + 3u, AOS_FILES_LABEL_LEN),
            AOS_INK_BLACK);
        return max(col, vec3(0.0));
    }
    return max(col, vec3(0.0));
}

vec3 aosSearchFlyout(vec2 pix, vec2 img, float taskH, ThemePal th) {
    if ((field.data_bus[42] & (1u << 26u)) == 0u) return vec3(-1.0);
    uint count = ramByte(AOS_FILE_SEARCH_RAM);
    if (count == 0u || count > AOS_SEARCH_MAX) return vec3(-1.0);
    float scale = aosUiScale(img);
    float pad = 6.0 * scale;
    float rootW = 196.0 * scale;
    float gap = 4.0 * scale;
    float panelW = 300.0 * scale;
    float rowH = 28.0 * scale;
    float panelH = pad * 2.0 + rowH * float(count);
    float panelX = pad + rootW + pad + gap;
    float panelTop = img.y - taskH - panelH;
    if (pix.x < panelX || pix.x >= panelX + panelW || pix.y < panelTop || pix.y >= panelTop + panelH)
        return vec3(-1.0);
    vec2 rel = pix - vec2(panelX, panelTop);
    vec3 col = aosMenuPanelBg(rel, vec2(panelW, panelH), scale, th, field.sealed_time);
    vec2 cell = aosHudTextCellSm(scale);
    uint hoverRow = ramByte(AOS_FILE_SEARCH_RAM + 2u);
    float rowY0 = panelTop + pad;
    for (uint i = 0u; i < count; ++i) {
        uint rowBase = AOS_FILE_SEARCH_RAM + 0x40u + i * AOS_SEARCH_ROW_STRIDE;
        if (ramByte(rowBase) == 0xFFu) continue;
        float ry0 = rowY0 + float(i) * rowH;
        if (pix.y < ry0 || pix.y >= ry0 + rowH) continue;
        if (hoverRow < count && hoverRow == i)
            col = mix(col, vec3(0.20, 0.20, 0.26), 0.88);
        uint icon = ramByte(rowBase);
        float iconSz = 18.0 * scale;
        vec2 icon0 = vec2(panelX + 8.0 * scale, ry0 + (rowH - iconSz) * 0.5);
        vec2 iuv = (pix - icon0) / iconSz;
        if (icon > 0u && all(greaterThanEqual(iuv, vec2(0.0))) && all(lessThan(iuv, vec2(1.0))))
            col = aosBlendIcon(col, sampleAosIconRgba(icon, iuv));
        float tx = icon0.x + iconSz + 5.0 * scale;
        float nameY = ry0 + rowH * 0.5 - 8.0 * scale;
        col = mixHudInk(col,
            dosHudRamStrGeom(pix, tx, nameY, cell, rowBase + 8u, AOS_SEARCH_NAME_LEN),
            dosHudRamStrBorder(pix, tx, nameY, cell, rowBase + 8u, AOS_SEARCH_NAME_LEN),
            AOS_INK_BLACK);
        return max(col, vec3(0.0));
    }
    return max(col, vec3(0.0));
}

vec3 aosDnDGhost(vec2 pix, vec2 img, float taskH, ThemePal th) {
    if (ramByte(AOS_DND_RAM) != 1u) return vec3(-1.0);
    uint icon = ramByte(AOS_DND_RAM + 1u);
    if (icon == 0u) return vec3(-1.0);
    float scale = aosUiScale(img);
    float ghostSz = 40.0 * scale;
    uint mxBits = uint(ramByte(AOS_DND_RAM + 68u))
        | (uint(ramByte(AOS_DND_RAM + 69u)) << 8u)
        | (uint(ramByte(AOS_DND_RAM + 70u)) << 16u)
        | (uint(ramByte(AOS_DND_RAM + 71u)) << 24u);
    uint myBits = uint(ramByte(AOS_DND_RAM + 72u))
        | (uint(ramByte(AOS_DND_RAM + 73u)) << 8u)
        | (uint(ramByte(AOS_DND_RAM + 74u)) << 16u)
        | (uint(ramByte(AOS_DND_RAM + 75u)) << 24u);
    vec2 ghost0 = vec2(float(mxBits), float(myBits)) - vec2(ghostSz * 0.5);
    vec2 iuv = (pix - ghost0) / ghostSz;
    if (any(lessThan(iuv, vec2(0.0))) || any(greaterThanEqual(iuv, vec2(1.0))))
        return vec3(-1.0);
    vec3 col = mix(th.panel * 0.35, th.accent * 0.25, 0.4);
    col = aosBlendIcon(col, sampleAosIconRgba(icon, iuv));
    return max(col, vec3(0.0)) * 0.82;
}

vec3 aosExitConfirmModal(vec2 pix, vec2 img, float taskH, ThemePal th) {
    if ((field.data_bus[42] & (1u << 27u)) == 0u) return vec3(-1.0);
    float scale = aosUiScale(img);
    vec2 vp = aosViewport(img);
    float panelW = 420.0 * scale;
    float panelH = 200.0 * scale;
    float panelX = (vp.x - panelW) * 0.5;
    float panelY = (vp.y - taskH - panelH) * 0.45;
    if (pix.x < panelX || pix.x >= panelX + panelW
            || pix.y < panelY || pix.y >= panelY + panelH)
        return vec3(-1.0);

    vec2 rel = pix - vec2(panelX, panelY);
    vec3 col = aosMenuPanelBg(rel, vec2(panelW, panelH), scale, th, field.sealed_time);
    col = mix(col, th.accent * 0.18 + vec3(0.12, 0.12, 0.14), 0.55);

    vec2 cell = vec2(8.0 * scale, 12.0 * scale);
    float titleY = panelY + 28.0 * scale;
    col = mixHudInk(col,
        dosHudRunGeom(pix, panelX + 24.0 * scale, titleY, cell,
            69u, 120u, 105u, 116u, 32u, 65u, 109u, 111u),
        dosHudRunBorder(pix, panelX + 24.0 * scale, titleY, cell,
            69u, 120u, 105u, 116u, 32u, 65u, 109u, 111u),
        AOS_INK_BLACK);

    float bodyY = titleY + 30.0 * scale;
    col = mixHudInk(col,
        dosHudRunGeom(pix, panelX + 24.0 * scale, bodyY, cell,
            83u, 104u, 117u, 116u, 32u, 100u, 111u, 119u),
        dosHudRunBorder(pix, panelX + 24.0 * scale, bodyY, cell,
            83u, 104u, 117u, 116u, 32u, 100u, 111u, 119u),
        AOS_INK_DIM);

    float btnW = 148.0 * scale;
    float btnH = 40.0 * scale;
    float btnY = panelY + panelH - btnH - 24.0 * scale;
    float midX = panelX + panelW * 0.5;
    float gap = 12.0 * scale;
    float yesX = midX - gap - btnW;
    float noX = midX + gap;

    if (pix.x >= yesX && pix.x < yesX + btnW && pix.y >= btnY && pix.y < btnY + btnH) {
        vec3 btnCol = aosSdRoundPanel(pix, vec2(yesX, btnY), vec2(yesX + btnW, btnY + btnH), 8.0 * scale,
            vec3(0.22, 0.22, 0.26), th.accent * 0.55 + vec3(0.30, 0.50, 0.72), th.title * 0.2, 1.5 * scale);
        if (btnCol.x < 0.0) return max(col, vec3(0.0));
        col = mixHudInk(btnCol,
            dosHudRunGeom(pix, yesX + 52.0 * scale, btnY + 12.0 * scale, cell,
                89u, 101u, 115u, 32u, 32u, 32u, 32u, 32u),
            0.0, AOS_INK_TASKBAR);
        return max(col, vec3(0.0));
    }
    if (pix.x >= noX && pix.x < noX + btnW && pix.y >= btnY && pix.y < btnY + btnH) {
        vec3 btnCol = aosSdRoundPanel(pix, vec2(noX, btnY), vec2(noX + btnW, btnY + btnH), 8.0 * scale,
            vec3(0.14, 0.14, 0.18), vec3(0.24, 0.24, 0.30), vec3(0.38, 0.38, 0.42), 1.5 * scale);
        if (btnCol.x < 0.0) return max(col, vec3(0.0));
        col = mixHudInk(btnCol,
            dosHudRunGeom(pix, noX + 58.0 * scale, btnY + 12.0 * scale, cell,
                78u, 111u, 32u, 32u, 32u, 32u, 32u, 32u),
            0.0, AOS_INK_BLACK);
        return max(col, vec3(0.0));
    }
    return max(col, vec3(0.0));
}

bool aosPanelHidden() {
    return (field.data_bus[42] & (1u << 25u)) != 0u;
}

vec2 aosFocusedSurfaceSize() {
    uint packW = field.data_bus[60];
    uint packH = field.data_bus[61];
    float pw = float(packW & 0xFFFFu);
    float ph = float(packH & 0xFFFFu);
    if (pw < 2.0) pw = 960.0;
    if (ph < 2.0) ph = 640.0;
    return vec2(pw, ph);
}

#include "aos_chrome_layers.inc"

vec3 aosLayerChrome(vec3 base, vec2 pix, vec2 vp, ThemePal th) {
    AosTbLayout L = aosTbLayout(vp);
    float taskH = L.taskH;
    uint bits = field.data_bus[42];
    bool menuOpen = (bits & (1u << 20u)) != 0u;
    bool filesOpen = (bits & (1u << 23u)) != 0u;
    vec3 menu = menuOpen ? aosStartMenu(pix, vp, taskH, th) : vec3(-1.0);
    vec3 search = menuOpen ? aosSearchFlyout(pix, vp, taskH, th) : vec3(-1.0);
    vec3 files = filesOpen ? aosFilesMenu(pix, vp, taskH, th) : vec3(-1.0);
    vec3 color = aosChromeComposite(base,
        aosTaskbarStrip(pix, vp, th),
        aosQuickLaunchStrip(pix, vp, th),
        menu,
        search,
        files,
        aosDnDGhost(pix, vp, taskH, th),
        vec3(-1.0),
        aosStartButtonPanel(pix, vp, th, menuOpen),
        aosExitConfirmModal(pix, vp, taskH, th));
    vec3 clk = aosTaskbarClock(pix, vp, th);
    if (clk.x >= 0.0) color = clk;
    return color;
}
