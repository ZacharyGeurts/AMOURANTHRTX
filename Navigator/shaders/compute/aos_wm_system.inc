// Windows-style system menu flyout from the title-bar app icon.

#ifndef AOS_WM_SYSTEM_INC
#define AOS_WM_SYSTEM_INC

float rtxWmSystemMenuInk(vec2 winLocal, float dx, float rowY, vec2 cell, int item) {
    float d = 0.0;
    if (item == 0) {
        d = max(d, dosHudRun(winLocal, dx, rowY, cell, 77u, 111u, 118u, 101u, 32u, 32u, 32u, 32u));
    } else if (item == 13) {
        d = max(d, dosHudRun(winLocal, dx, rowY, cell, 77u, 105u, 110u, 105u, 109u, 46u, 32u, 32u));
    } else if (item == 14) {
        d = max(d, dosHudRun(winLocal, dx, rowY, cell, 77u, 97u, 120u, 105u, 109u, 46u, 32u, 32u));
    } else if (item == 15) {
        d = max(d, dosHudRun(winLocal, dx, rowY, cell, 67u, 108u, 111u, 115u, 101u, 32u, 32u, 32u));
    } else if (item >= 1 && item <= 12) {
        int pct = 100;
        if (item == 1) pct = 10;
        else if (item == 2) pct = 25;
        else if (item == 3) pct = 50;
        else if (item == 4) pct = 75;
        else if (item == 5) pct = 100;
        else if (item == 6) pct = 125;
        else if (item == 7) pct = 150;
        else if (item == 8) pct = 200;
        else if (item == 9) pct = 250;
        else if (item == 10) pct = 300;
        else if (item == 11) pct = 350;
        else if (item == 12) pct = 400;
        d = max(d, dosHudRun(winLocal, dx, rowY, cell, 83u, 99u, 97u, 108u, 101u, 32u, 32u, 32u));
        float px = dx + cell.x * 6.0;
        if (pct >= 100) {
            uint d0 = uint(pct / 100);
            uint d1 = uint((pct / 10) % 10u);
            uint d2 = uint(pct % 10u);
            d = max(d, dosHudRun(winLocal, px, rowY, cell, 48u + d0, 48u + d1, 48u + d2, 37u, 32u, 32u, 32u, 32u));
        } else {
            uint d0 = uint(pct / 10);
            uint d1 = uint(pct % 10u);
            d = max(d, dosHudRun(winLocal, px, rowY, cell, 48u + d0, 48u + d1, 37u, 32u, 32u, 32u, 32u, 32u));
        }
    }
    return d;
}

vec3 rtxWmSystemMenuDropdown(vec2 winLocal, vec2 winSize, float titleH, ThemePal th) {
    uint wmPack = field.data_bus[52];
    uint openMenu = (wmPack >> 8u) & 7u;
    if (openMenu != 6u) return vec3(-1.0);
    float uiSc = max(winSize.x / 1920.0, 0.75) * 1.35;
    vec2 cell = vec2(8.0, 14.0) * uiSc;
    float dropH = 28.0 * uiSc;
    float dropW = 196.0 * uiSc;
    float dx = 10.0 * uiSc;
    int nRows = 9;
    int scroll = int((wmPack >> 17u) & 0xFu);
    int totalItems = 16;
    if (winLocal.y < titleH || winLocal.y >= titleH + dropH * float(nRows)
            || winLocal.x < dx - 4.0 * uiSc || winLocal.x >= dx + dropW)
        return vec3(-1.0);
    vec3 drop = mix(th.panel * 0.62, th.accent * 0.18 + th.panel * 0.42, 0.55);
    float edge = min(min(winLocal.x - (dx - 4.0 * uiSc), dx + dropW - winLocal.x),
                     min(winLocal.y - titleH, titleH + dropH * float(nRows) - winLocal.y));
    drop = mix(drop, th.accent * 0.28 + th.title * 0.12, smoothstep(0.0, 2.5 * uiSc, edge));
    int visRow = int((winLocal.y - titleH) / dropH);
    int hoverItem = int((wmPack >> 11u) & 0xFu);
    int item = scroll + visRow;
    if (item >= 0 && item < totalItems && item == hoverItem) {
        float hy0 = titleH + dropH * float(visRow);
        if (winLocal.y >= hy0 && winLocal.y < hy0 + dropH)
            drop = mix(drop, th.accent * 0.32 + th.gemTint * 0.18, 0.72);
    }
    if (scroll + nRows < totalItems && winLocal.y >= titleH + dropH * float(nRows - 1)
            && winLocal.x >= dx + dropW - 18.0 * uiSc)
        drop = mix(drop, th.accent * 0.20, 0.45);
    if (scroll > 0 && winLocal.y >= titleH && winLocal.y < titleH + dropH
            && winLocal.x >= dx + dropW - 18.0 * uiSc)
        drop = mix(drop, th.accent * 0.20, 0.45);
    float rowY = titleH + dropH * float(visRow) + 5.0;
    float dInk = rtxWmSystemMenuInk(winLocal, dx, rowY, cell, item);
    return mixHudInk(drop, dInk, 0.0, AOS_INK_BLACK);
}

void rtxWmTitleAppIcon(vec2 winLocal, float titleH, float uiSc, ThemePal th, inout vec3 base) {
    float iconSz = titleH * 0.58;
    float iconX = 10.0 * uiSc;
    float iconY = (titleH - iconSz) * 0.5;
    uint wmPack = field.data_bus[52];
    uint openMenu = (wmPack >> 8u) & 7u;
    if (openMenu == 6u && winLocal.x >= iconX - 2.0 * uiSc && winLocal.x < iconX + iconSz + 2.0 * uiSc
            && winLocal.y >= iconY - 2.0 * uiSc && winLocal.y < iconY + iconSz + 2.0 * uiSc)
        base = mix(base, th.accent * 0.22 + th.panel * 0.45, 0.55);
    if (winLocal.x >= iconX && winLocal.x < iconX + iconSz
            && winLocal.y >= iconY && winLocal.y < iconY + iconSz) {
        vec2 il = (winLocal - vec2(iconX, iconY)) / iconSz;
        uint titleTab = (field.data_bus[54] >> 24u) & 0xFFu;
        uint titleBase = AOS_TASKBAR_RAM + 0x10u + titleTab * AOS_TAB_STRIDE;
        uint iconSlot = uint(ramByte(titleBase + AOS_TAB_STRIDE - 1u));
        base = mix(base, aosBlendIcon(base, sampleAosIconRgba(iconSlot, il)), 0.88);
    }
}

#endif