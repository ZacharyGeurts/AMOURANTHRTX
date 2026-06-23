// Instant-boot shell — procedural desktop + black SDF taskbar (no full menu chrome).
#ifndef AOS_LOAD_SHELL_INC
#define AOS_LOAD_SHELL_INC

struct ThemePal {
    vec3 void_; vec3 panel; vec3 border; vec3 accent; vec3 pcb; vec3 silicon;
};

const float AOS_START_BTN_W = 156.0;
const float AOS_QUICK_BTN_W = 44.0;
const float AOS_CLOCK_W = 200.0;
const int AOS_QUICK_LAUNCH_N = 4;
const float AOS_MENU_W = 196.0;
const float AOS_MENU_ROW_H = 50.0;
const float AOS_MENU_HEADER_H = 28.0;
const float AOS_MENU_PAD = 6.0;
const uint AOS_TASKBAR_RAM = 0x000BA400u;
const uint AOS_START_LABEL_OFF = 0x80u;
const vec3 AOS_INK_TASKBAR = vec3(0.97, 0.98, 1.0);

uint themeIdFromControl() { return field.data_bus[62] & 0xFFu; }

ThemePal loadTheme(uint id) {
    uint pack = field.data_bus[62];
    id = pack & 0xFFu;
    float rose = float((pack >> 8u) & 0xFFu) / 255.0;
    float cyan = float((pack >> 16u) & 0xFFu) / 255.0;
    ThemePal t;
    t.void_ = vec3(0.01, 0.01, 0.012);
    t.panel = vec3(0.06, 0.06, 0.065);
    t.border = vec3(0.12, 0.12, 0.14);
    t.accent = vec3(rose * 0.35 + 0.12, cyan * 0.40 + 0.10, cyan * 0.55 + 0.20);
    t.pcb = vec3(0.02, 0.02, 0.022);
    t.silicon = vec3(0.05, 0.05, 0.055);
    if (length(t.accent) < 0.10)
        t.accent = vec3(0.42, 0.58, 0.88);
    return t;
}

bool aosChromeActive() {
    uint b = field.data_bus[42];
    return (b & 4096u) != 0u || (b & (1u << 29u)) != 0u;
}

vec2 aosViewport(vec2 storageImg) {
    float vw = field.viewport_w;
    float vh = field.viewport_h;
    if (vw >= 2.0 && vh >= 2.0) return vec2(vw, vh);
    return storageImg;
}

const float AOS_UI_REF_W = 3840.0;

float aosUiScale(vec2 img) {
    return max(img.x / AOS_UI_REF_W, 0.75) * 1.35;
}

#include "aos_taskbar_layout.inc"
#include "rtx_font_render.inc"
#include "aos_sdf.inc"
#include "aos_chrome_layers.inc"

uint shellRamByte(uint addr) {
    if (addr >= uint(die.RAM.length()) * 4u) return 0u;
    uint widx = addr >> 2u;
    uint shift = (addr & 3u) << 3u;
    return (die.RAM[widx] >> shift) & 0xFFu;
}

vec2 shellHudCell(float sc) { return vec2(8.0 * sc, 14.0 * sc); }

vec3 shellHudTimePart(vec2 pix, float x0, float rowY, vec2 cell, uint val) {
    vec3 ink = max(fontGlyphGeom(48u + (val / 10u) % 10u, pix, x0, rowY, cell),
                   fontGlyphGeom(48u + val % 10u, pix, x0 + cell.x, rowY, cell));
    return ink;
}

vec3 aosDarkDesktop(vec2 pix, vec2 img, float taskH, ThemePal th) {
    float deskH = max(img.y - taskH, 1.0);
    if (pix.y >= deskH) return vec3(-1.0);
    float y = pix.y / deskH;
    float x = pix.x / max(img.x, 1.0);
    vec3 col = mix(th.pcb, th.silicon, y * 0.38 + x * 0.06);
    float grain = sin(pix.x * 0.11 + pix.y * 0.07) * sin(pix.y * 0.19);
    col += vec3(grain * 0.006);
    float vig = 1.0 - 0.12 * length(vec2(x - 0.5, y - 0.45));
    return clamp(col * vig, 0.0, 1.0);
}

vec3 amouranthOsBackdrop(vec2 pix, vec2 vp, ThemePal th) {
    return vec3(-1.0);
}

vec4 sampleAosIconRgba(uint slot, vec2 uv) {
    ivec2 dim = imageSize(aosStartTex);
    if (dim.x <= 0 || dim.y <= 0) return vec4(0.0);
    const float cols = 8.0;
    const float rows = 4.0;
    vec2 atlasUv = (vec2(mod(float(slot), cols), floor(float(slot) / cols)) + clamp(uv, 0.0, 1.0)) / vec2(cols, rows);
    ivec2 px = clamp(ivec2(atlasUv * vec2(dim)), ivec2(0), dim - ivec2(1));
    return imageLoad(aosStartTex, px);
}

vec3 shellTaskbarRail(vec2 pix, AosTbLayout L, ThemePal th) {
    if (!aosTbOnStrip(pix, L)) return vec3(-1.0);
    vec2 b0 = vec2(0.0, L.barY0);
    vec2 b1 = vec2(L.vpW, L.barY1);
    vec3 face = mix(th.pcb, th.panel, 0.22);
    vec3 col = aosSdWin31Raised(pix, b0, b1, face, 2.5 * L.scale);
    float topGloss = smoothstep(L.barY0, L.barY0 + 6.0 * L.scale, pix.y);
    col = mix(col, mix(th.accent * 0.12, th.border, 0.55), topGloss * 0.28);
    return col;
}

vec3 shellStartButton(vec2 pix, AosTbLayout L, ThemePal th, bool menuOpen) {
    vec2 b0, b1;
    aosTbStartBounds(L, b0, b1);
    float bevel = 2.5 * L.scale;
    vec3 face = menuOpen ? mix(th.pcb, th.panel, 0.15) : mix(th.panel, th.border, 0.12);
    vec3 col = aosSdWin31Raised(pix, b0, b1, face, bevel);
    if (col.x < 0.0) return col;
    if (menuOpen)
        col = aosSdWin31Sunken(pix, b0 + vec2(bevel), b1 - vec2(bevel),
            mix(th.pcb, th.panel, 0.10), bevel * 0.55);
    float pad = 5.0 * L.scale;
    float iconSide = min(b1.y - b0.y - pad * 2.0, 40.0 * L.scale);
    vec2 icon0 = b0 + vec2(pad, pad + max(0.0, (b1.y - b0.y - pad * 2.0 - iconSide) * 0.5));
    vec4 icon = sampleAosIconRgba(0u, (pix - icon0) / vec2(iconSide));
    if (icon.a > 0.04) col = mix(col, icon.rgb, clamp(icon.a, 0.0, 1.0));
    vec2 cell = shellHudCell(L.scale);
    float labelX = icon0.x + iconSide + 8.0 * L.scale;
    float labelY = b0.y + (b1.y - b0.y) * 0.5 - 6.0 * L.scale;
    vec3 labelGeom = vec3(0.0);
    float lx = labelX;
    for (uint li = 0u; li < 8u; ++li) {
        uint ch = shellRamByte(AOS_TASKBAR_RAM + AOS_START_LABEL_OFF + li);
        if (ch == 0u || ch < 32u) break;
        labelGeom = max(labelGeom, fontGlyphGeom(ch, pix, lx, labelY, cell));
        lx += cell.x;
    }
    return mixHudInk(col, labelGeom, 0.0, AOS_INK_TASKBAR);
}

vec3 shellQuickLaunchBtn(vec2 pix, AosTbLayout L, ThemePal th, int idx, bool hover, bool pressed) {
    vec2 b0, b1;
    aosTbQuickBounds(L, idx, b0, b1);
    float bevel = 2.0 * L.scale;
    vec3 face = pressed ? mix(th.pcb, th.panel, 0.12)
        : (hover ? mix(th.panel, th.accent, 0.18) : mix(th.pcb, th.panel, 0.35));
    vec3 col = pressed
        ? aosSdWin31Sunken(pix, b0, b1, face, bevel)
        : aosSdWin31Raised(pix, b0, b1, face, bevel);
    if (col.x < 0.0) return col;
    uint icon = 6u;
    if (idx == 1) icon = 4u;
    else if (idx == 2) icon = 19u;
    else if (idx == 3) icon = max(shellRamByte(AOS_TASKBAR_RAM + 8u), 17u);
    float pad = 5.0 * L.scale;
    vec2 icon0 = b0 + vec2(pad, pad);
    vec2 iconSz = (b1 - b0) - vec2(pad * 2.0);
    vec2 iuv = (pix - icon0) / max(iconSz, vec2(1.0));
    if (all(greaterThanEqual(iuv, vec2(0.0))) && all(lessThan(iuv, vec2(1.0))))
        col = mix(col, sampleAosIconRgba(icon, iuv).rgb, 0.85);
    return col;
}

vec3 shellTaskbarClock(vec2 pix, vec2 vp, ThemePal th) {
    AosTbLayout L = aosTbLayout(vp);
    if (!aosTbOnStrip(pix, L) || pix.x < L.clockX0 - 2.0 * L.scale || pix.x >= L.clockX1)
        return vec3(-1.0);
    float scale = L.scale;
    vec2 pill0 = vec2(L.clockX0, L.barY0 + 6.0 * scale);
    vec2 pill1 = vec2(L.clockX1, L.barY1 - 6.0 * scale);
    vec3 pillCol = aosSdWin31Sunken(pix, pill0, pill1, mix(th.pcb, th.panel, 0.18), 1.5 * scale);
    if (pillCol.x < 0.0) return pillCol;
    uint clk = field.data_bus[29];
    uint hr = (clk >> 16u) & 0xFFu;
    uint mn = (clk >> 8u) & 0xFFu;
    uint sc = clk & 0xFFu;
    vec2 cell = shellHudCell(scale);
    float timeX = L.clockX0 + 10.0 * scale;
    float timeY = L.barY0 + 18.0 * scale;
    vec3 ink = shellHudTimePart(pix, timeX, timeY, cell, hr);
    ink = max(ink, fontGlyphGeom(58u, pix, timeX + cell.x * 2.3, timeY, cell));
    ink = max(ink, shellHudTimePart(pix, timeX + cell.x * 3.3, timeY, cell, mn));
    ink = max(ink, fontGlyphGeom(58u, pix, timeX + cell.x * 5.6, timeY, cell));
    ink = max(ink, shellHudTimePart(pix, timeX + cell.x * 6.6, timeY, cell, sc));
    return mixHudInk(pillCol, ink, 0.0, AOS_INK_TASKBAR);
}

float shellMenuRootHeight(AosTbLayout L, uint rowCount) {
    return AOS_MENU_HEADER_H * L.scale + AOS_MENU_PAD * L.scale * 2.0
        + float(rowCount) * AOS_MENU_ROW_H * L.scale;
}

vec3 shellStartMenu(vec2 pix, vec2 vp, ThemePal th) {
    if ((field.data_bus[42] & (1u << 20u)) == 0u) return vec3(-1.0);
    AosTbLayout L = aosTbLayout(vp);
    vec2 startB0, startB1;
    aosTbStartBounds(L, startB0, startB1);
    uint rowPack = (field.data_bus[54] >> 16u) & 0xFFu;
    uint rowCount = rowPack == 255u ? 8u : max(rowPack, 1u);
    float menuW = AOS_MENU_W * L.scale;
    float menuH = shellMenuRootHeight(L, rowCount);
    float pad = AOS_MENU_PAD * L.scale;
    vec2 b0 = vec2(startB0.x, startB0.y - menuH);
    vec2 b1 = b0 + vec2(menuW + pad * 2.0, menuH);
    vec3 col = aosSdRoundPanel(pix, b0, b1, 4.0 * L.scale,
        mix(th.panel, th.accent, 0.08), th.panel, th.border, 1.2 * L.scale);
    if (col.x < 0.0) return col;
    uint hoverRow = field.data_bus[54] & 0xFFu;
    if (hoverRow != 255u) {
        float headH = AOS_MENU_HEADER_H * L.scale;
        float rowH = AOS_MENU_ROW_H * L.scale;
        float ry0 = b0.y + headH + pad + float(hoverRow) * rowH;
        vec2 rowB0 = vec2(b0.x + pad, ry0);
        vec2 rowB1 = vec2(b1.x - pad, ry0 + rowH);
        vec3 rowCol = aosSdRoundPanel(pix, rowB0, rowB1, 2.0 * L.scale,
            mix(th.accent * 0.25, th.panel, 0.5), th.panel, th.border, 0.8 * L.scale);
        if (rowCol.x >= 0.0) col = rowCol;
    }
    return col;
}

bool shellInStartButton(vec2 pix, vec2 vp) {
    AosTbLayout L = aosTbLayout(vp);
    vec2 b0, b1;
    aosTbStartBounds(L, b0, b1);
    return all(greaterThanEqual(pix, b0)) && all(lessThan(pix, b1));
}

vec3 shellTaskbarStrip(vec2 pix, vec2 vp, ThemePal th) {
    AosTbLayout L = aosTbLayout(vp);
    if (!aosTbOnStrip(pix, L)) return vec3(-1.0);
    if (shellInStartButton(pix, vp)) return vec3(-1.0);

    vec3 col = shellTaskbarRail(pix, L, th);
    vec3 clk = shellTaskbarClock(pix, vp, th);
    if (clk.x >= 0.0) col = clk;
    return col;
}

vec3 shellQuickLaunchStrip(vec2 pix, vec2 vp, ThemePal th) {
    AosTbLayout L = aosTbLayout(vp);
    vec2 mousePix = vec2(field.mouse_x, field.mouse_y) * vp;
    if (field.mouse_x < 0.0) mousePix = vec2(-1000.0);
    bool filesOpen = (field.data_bus[42] & (1u << 23u)) != 0u;
    for (int qi = 0; qi < AOS_QUICK_LAUNCH_N; ++qi) {
        vec2 qb0, qb1;
        aosTbQuickBounds(L, qi, qb0, qb1);
        bool hover = all(greaterThanEqual(mousePix, qb0)) && all(lessThan(mousePix, qb1));
        bool pressed = (qi == 0 && filesOpen);
        vec3 qcol = shellQuickLaunchBtn(pix, L, th, qi, hover, pressed);
        if (qcol.x >= 0.0) return qcol;
    }
    return vec3(-1.0);
}

vec3 aosLayerChrome(vec3 base, vec2 pix, vec2 vp, ThemePal th) {
    AosTbLayout L = aosTbLayout(vp);
    bool menuOpen = (field.data_bus[42] & (1u << 20u)) != 0u;
    return aosChromeComposite(base,
        shellTaskbarStrip(pix, vp, th),
        shellQuickLaunchStrip(pix, vp, th),
        shellStartMenu(pix, vp, th),
        vec3(-1.0),
        vec3(-1.0),
        vec3(-1.0),
        vec3(-1.0),
        shellStartButton(pix, L, th, menuOpen),
        vec3(-1.0));
}

#endif