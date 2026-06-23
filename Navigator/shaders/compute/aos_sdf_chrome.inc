// SDF taskbar — wide dark Win31 strip, lifted Start, icon tabs, clock right.

#ifndef AOS_SDF_CHROME_INC
#define AOS_SDF_CHROME_INC

// Standalone helpers — CANVAS/x86 include this without full aos_chrome.inc.
vec2 aosSdfHudTextCellSm(float sc) { return vec2(8.0 * sc, 14.0 * sc); }

vec4 aosTaskbarTimePack(vec2 pix, float x0, float rowY, vec2 cell, uint hr, uint mn, uint sc) {
    vec4 pack = vec4(0.0);
    float x = x0;
    pack = max(pack, fontGlyphPack(48u + (hr / 10u) % 10u, pix, x, rowY, cell));
    pack = max(pack, fontGlyphPack(48u + hr % 10u, pix, x + cell.x, rowY, cell));
    x += cell.x * 2.0 + cell.x * 0.3;
    pack = max(pack, fontGlyphPack(58u, pix, x, rowY, cell));
    x += cell.x;
    pack = max(pack, fontGlyphPack(48u + (mn / 10u) % 10u, pix, x, rowY, cell));
    pack = max(pack, fontGlyphPack(48u + mn % 10u, pix, x + cell.x, rowY, cell));
    x += cell.x * 2.0 + cell.x * 0.3;
    pack = max(pack, fontGlyphPack(58u, pix, x, rowY, cell));
    x += cell.x;
    pack = max(pack, fontGlyphPack(48u + (sc / 10u) % 10u, pix, x, rowY, cell));
    pack = max(pack, fontGlyphPack(48u + sc % 10u, pix, x + cell.x, rowY, cell));
    return pack;
}

vec3 aosSdfTaskbarRail(vec2 pix, AosTbLayout L, ThemePal th) {
    if (!aosTbOnStrip(pix, L)) return vec3(-1.0);
    vec2 b0 = vec2(0.0, L.barY0);
    vec2 b1 = vec2(L.vpW, L.barY1);
    vec3 face = mix(th.pcb, th.panel, 0.22);
    vec3 col = aosSdWin31Raised(pix, b0, b1, face, 2.5 * L.scale);
    float topGloss = smoothstep(L.barY0, L.barY0 + 6.0 * L.scale, pix.y);
    col = mix(col, mix(th.accent * 0.12, th.border, 0.55), topGloss * 0.28);
    return col;
}

vec3 aosTaskbarClock(vec2 pix, vec2 vp, ThemePal th) {
    AosTbLayout L = aosTbLayout(vp);
    if (!aosTbOnStrip(pix, L)) return vec3(-1.0);
    if (pix.x < L.clockX0 - 2.0 * L.scale || pix.x >= L.clockX1) return vec3(-1.0);

    float scale = L.scale;
    vec2 pill0 = vec2(L.clockX0, L.barY0 + 6.0 * scale);
    vec2 pill1 = vec2(L.clockX1, L.barY1 - 6.0 * scale);
    vec3 pillCol = aosSdWin31Sunken(pix, pill0, pill1, mix(th.pcb, th.panel, 0.18), 1.5 * scale);
    if (pillCol.x < 0.0) return pillCol;

    uint clk = field.data_bus[29];
    uint hr = (clk >> 16u) & 0xFFu;
    uint mn = (clk >> 8u) & 0xFFu;
    uint sc = clk & 0xFFu;
    vec2 cell = aosSdfHudTextCellSm(scale);
    float timeX = L.clockX0 + 10.0 * scale;
    float timeY = L.barY0 + 18.0 * scale;
    vec4 timePack = aosTaskbarTimePack(pix, timeX, timeY, cell, hr, mn, sc);
    return mixHudInkLight(pillCol, timePack, 0.0);
}

void aosStartButtonBounds(vec2 vp, out vec2 b0, out vec2 b1) {
    AosTbLayout L = aosTbLayout(vp);
    aosTbStartBounds(L, b0, b1);
}

bool aosInStartButton(vec2 pix, vec2 vp) {
    vec2 b0, b1;
    aosStartButtonBounds(vp, b0, b1);
    return all(greaterThanEqual(pix, b0)) && all(lessThan(pix, b1));
}

void aosQuickLaunchBounds(vec2 vp, int idx, out vec2 b0, out vec2 b1) {
    AosTbLayout L = aosTbLayout(vp);
    aosTbQuickBounds(L, idx, b0, b1);
}

bool aosInQuickLaunch(vec2 pix, vec2 vp, int idx) {
    vec2 b0, b1;
    aosQuickLaunchBounds(vp, idx, b0, b1);
    return all(greaterThanEqual(pix, b0)) && all(lessThan(pix, b1));
}

vec3 aosStartButtonPanel(vec2 pix, vec2 vp, ThemePal th, bool menuOpen) {
    vec2 b0, b1;
    aosStartButtonBounds(vp, b0, b1);
    float scale = aosUiScale(vp);
    float bevel = 2.5 * scale;
    vec3 shadow = aosSdDropShadow(pix, b0, b1, 3.0, vec2(2.0, 6.0) * scale,
        vec3(0.0, 0.0, 0.0), 0.72);
    if (shadow.x >= 0.0 && !aosInStartButton(pix, vp))
        return shadow;
    vec3 face = menuOpen ? mix(th.pcb, th.panel, 0.15) : mix(th.panel, th.border, 0.12);
    vec3 col = aosSdWin31Raised(pix, b0, b1, face, bevel);
    if (col.x < 0.0) return col;
    if (menuOpen)
        col = aosSdWin31Sunken(pix, b0 + vec2(bevel), b1 - vec2(bevel),
            vec3(0.04, 0.05, 0.07), bevel * 0.55);
    float pad = 5.0 * scale;
    float iconSide = min(b1.y - b0.y - pad * 2.0, 40.0 * scale);
    vec2 icon0 = b0 + vec2(pad, pad + max(0.0, (b1.y - b0.y - pad * 2.0 - iconSide) * 0.5));
    vec2 iconSize = vec2(iconSide);
    vec4 logo = sampleAmmoPortraitRgba(pix, icon0, iconSize);
    if (logo.a >= 0.0)
        col = aosBlendIcon(col, logo);

    float labelX = icon0.x + iconSide + 8.0 * scale;
    float labelY = b0.y + (b1.y - b0.y) * 0.5 - 6.0 * scale;
    vec2 cell = aosSdfHudTextCellSm(scale);
    vec4 labelPack = vec4(0.0);
    float lx = labelX;
    for (uint li = 0u; li < 8u; ++li) {
        uint ch = ramByte(AOS_TASKBAR_RAM + AOS_START_LABEL_OFF + li);
        if (ch == 0u || ch < 32u) break;
        labelPack = max(labelPack, fontGlyphPack(ch, pix, lx, labelY, cell));
        lx += cell.x;
    }
    col = mixHudInkLight(col, labelPack, 0.0);
    return max(col, vec3(0.0));
}

vec3 aosTaskbarTab(vec2 pix, AosTbLayout L, vec2 tabB0, vec2 tabB1,
        bool focused, bool hovered, bool minimized, uint icon, ThemePal th) {
    if (!all(greaterThanEqual(pix, tabB0)) || !all(lessThan(pix, tabB1)))
        return vec3(-1.0);
    vec3 face = focused ? vec3(0.12, 0.13, 0.17) : vec3(0.06, 0.07, 0.09);
    vec3 tabCol = focused
        ? aosSdWin31Raised(pix, tabB0, tabB1, face, 2.0 * L.scale)
        : aosSdWin31Sunken(pix, tabB0, tabB1, face, 1.5 * L.scale);
    if (tabCol.x < 0.0) return tabCol;
    if (minimized) tabCol *= 0.55;
    if (hovered && !focused)
        tabCol = mix(tabCol, th.accent * 0.18 + vec3(0.10, 0.11, 0.14), 0.40);
    float iconSz = min(tabB1.y - tabB0.y - 6.0 * L.scale, tabB1.x - tabB0.x - 6.0 * L.scale);
    vec2 icon0 = tabB0 + (tabB1 - tabB0 - vec2(iconSz)) * 0.5;
    vec2 iuv = (pix - icon0) / iconSz;
    if (icon > 0u && all(greaterThanEqual(iuv, vec2(0.0))) && all(lessThan(iuv, vec2(1.0))))
        tabCol = aosBlendIcon(tabCol, sampleAosIconRgba(icon, iuv));
    return max(tabCol, vec3(0.0));
}

vec3 aosTaskbarStrip(vec2 pix, vec2 vp, ThemePal th) {
    AosTbLayout L = aosTbLayout(vp);
    if (!aosTbOnStrip(pix, L)) return vec3(-1.0);
    if (aosInStartButton(pix, vp)) return vec3(-1.0);

    if (pix.x >= L.clockX0 - 2.0 * L.scale) {
        vec3 clk = aosTaskbarClock(pix, vp, th);
        if (clk.x >= 0.0) return clk;
    }

    uint tabCount = uint(ramByte(AOS_TASKBAR_RAM));
    int focusTab = int(ramByte(AOS_TASKBAR_RAM + 1u));
    int hoverTab = int(ramByte(AOS_TASKBAR_RAM + 2u));
    uint minMask = uint(ramByte(AOS_TASKBAR_RAM + 3u));
    uint chrome54 = field.data_bus[54];
    uint busHover = (chrome54 >> 8u) & 0xFFu;
    uint busFocus = (chrome54 >> 24u) & 0xFFu;
    if (hoverTab < 0 || hoverTab >= int(tabCount)) {
        if (busHover < 255u) hoverTab = int(busHover);
    }
    if (focusTab < 0 || focusTab >= int(tabCount)) {
        if (busFocus < 255u) focusTab = int(busFocus);
    }

    uint visibleTab = 0u;
    for (uint ti = 0u; ti < tabCount && ti < 8u; ++ti) {
        uint tabBase = AOS_TASKBAR_RAM + 0x10u + ti * AOS_TAB_STRIDE;
        uint icon = ramByte(tabBase + AOS_TAB_STRIDE - 1u);
        if (icon == 0u) continue;

        vec2 tabB0, tabB1;
        aosTbTabBounds(L, int(visibleTab), tabB0, tabB1);
        visibleTab++;
        if (tabB1.x > L.clockX0 - 8.0 * L.scale) break;

        bool focused = int(ti) == focusTab;
        bool hovered = int(ti) == hoverTab;
        bool minimized = (minMask & (1u << ti)) != 0u;
        vec3 tabCol = aosTaskbarTab(pix, L, tabB0, tabB1, focused, hovered, minimized, icon, th);
        if (tabCol.x >= 0.0) return tabCol;
    }

    vec3 bar = aosSdfTaskbarRail(pix, L, th);
    vec3 clk = aosTaskbarClock(pix, vp, th);
    if (clk.x >= 0.0) bar = mix(bar, clk, 0.92);
    return max(bar, vec3(0.0));
}

vec3 aosQuickLaunchButton(vec2 pix, vec2 b0, vec2 b1, uint iconSlot,
        bool hovered, bool pressed, float scale, ThemePal th) {
    if (!all(greaterThanEqual(pix, b0)) || !all(lessThan(pix, b1)))
        return vec3(-1.0);
    float bevel = 2.0 * scale;
    vec3 face = pressed ? mix(th.pcb, th.panel, 0.12)
        : (hovered ? mix(th.panel, th.accent, 0.18) : mix(th.pcb, th.panel, 0.35));
    vec3 col = pressed
        ? aosSdWin31Sunken(pix, b0, b1, face, bevel)
        : aosSdWin31Raised(pix, b0, b1, face, bevel);
    if (col.x < 0.0) return col;
    float pad = 5.0 * scale;
    vec2 icon0 = b0 + vec2(pad, pad);
    vec2 iconSz = (b1 - b0) - vec2(pad * 2.0);
    vec2 iuv = (pix - icon0) / max(iconSz, vec2(1.0));
    if (iconSlot > 0u && all(greaterThanEqual(iuv, vec2(0.0))) && all(lessThan(iuv, vec2(1.0))))
        col = aosBlendIcon(col, sampleAosIconRgba(iconSlot, iuv));
    return max(col, vec3(0.0));
}

vec3 aosQuickLaunchStrip(vec2 pix, vec2 vp, ThemePal th) {
    if (!aosChromeActive()) return vec3(-1.0);
    AosTbLayout L = aosTbLayout(vp);
    uint bits = field.data_bus[42];
    bool filesOpen = (bits & (1u << 23u)) != 0u;
    for (int qi = 0; qi < AOS_QUICK_LAUNCH_N; ++qi) {
        vec2 b0, b1;
        aosTbQuickBounds(L, qi, b0, b1);
        bool hovered = ramByte(AOS_TASKBAR_RAM + 4u + uint(qi)) != 0u;
        bool pressed = (qi == 0 && filesOpen);
        uint icon = 6u;
        if (qi == 1) icon = 4u;
        else if (qi == 2) icon = 19u;
        else if (qi == 3) {
            icon = uint(ramByte(AOS_TASKBAR_RAM + 8u));
            if (icon == 0u) icon = 17u;
        }
        vec3 btn = aosQuickLaunchButton(pix, b0, b1, icon, hovered, pressed, L.scale, th);
        if (btn.x >= 0.0) return btn;
    }
    return vec3(-1.0);
}

#endif