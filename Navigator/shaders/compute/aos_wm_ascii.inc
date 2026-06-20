// Title-bar window controls — ASCII _ [ ] X on raised Win31 buttons.

#ifndef AOS_WM_ASCII_INC
#define AOS_WM_ASCII_INC

float rtxWmAsciiInk(vec2 uv, float inkX, float rowY, vec2 cell, uint glyph) {
    float ink = dosHudGlyph(glyph, uv, inkX, rowY, cell);
    ink = max(ink, dosHudGlyph(glyph, uv + vec2(-1.0, 0.0), inkX, rowY, cell));
    ink = max(ink, dosHudGlyph(glyph, uv + vec2(1.0, 0.0), inkX, rowY, cell));
    ink = max(ink, dosHudGlyph(glyph, uv + vec2(0.0, -1.0), inkX, rowY, cell));
    ink = max(ink, dosHudGlyph(glyph, uv + vec2(0.0, 1.0), inkX, rowY, cell));
    return ink;
}

vec3 rtxWmAsciiBtn(vec2 uv, float x0, float y0, float sz, uint glyph,
        ThemePal th, bool hov, bool isClose) {
    vec3 base = mix(th.zebra, th.panel, 0.62);
    vec3 hot = isClose
        ? mix(vec3(0.50, 0.16, 0.20), vec3(0.68, 0.22, 0.26), 0.72)
        : th.accent * 0.40 + th.panel * 0.48;
    vec3 col = aosSdWmButton(uv, vec2(x0, y0), sz, base, hot, hov);
    if (col.x < -1.5) return col;
    vec2 cell = vec2(8.0, 14.0) * (sz / 30.0);
    float rowY = y0 + sz * 0.5 - cell.y * 0.40;
    float inkX = x0 + sz * 0.5 - cell.x * 0.5;
    float ink = rtxWmAsciiInk(uv, inkX, rowY, cell, glyph);
    vec3 inkCol = (isClose && hov) ? vec3(0.97, 0.94, 0.95) : mix(th.title, AOS_INK_BLACK, 0.82);
    return mixHudInk(col, ink, 0.0, inkCol);
}

vec3 rtxWmAsciiBoxBtn(vec2 uv, float x0, float y0, float sz,
        ThemePal th, bool hov) {
    vec3 base = mix(th.zebra, th.panel, 0.62);
    vec3 hot = th.accent * 0.40 + th.panel * 0.48;
    vec3 col = aosSdWmButton(uv, vec2(x0, y0), sz, base, hot, hov);
    if (col.x < -1.5) return col;
    vec2 cell = vec2(8.0, 14.0) * (sz / 30.0);
    float rowY = y0 + sz * 0.5 - cell.y * 0.40;
    float cx = x0 + sz * 0.5;
    float ink = rtxWmAsciiInk(uv, cx - cell.x * 0.55, rowY, cell, 91u);
    ink = max(ink, rtxWmAsciiInk(uv, cx + cell.x * 0.55, rowY, cell, 93u));
    vec3 inkCol = mix(th.title, AOS_INK_BLACK, 0.82);
    return mixHudInk(col, ink, 0.0, inkCol);
}

vec3 rtxWmTitleAsciiControls(vec2 winLocal, vec2 winSize, float titleH,
        uint wmPack, ThemePal th) {
    float btn = clamp(titleH - 6.0, 26.0, 34.0);
    float uiSc = max(winSize.x / 1920.0, 0.75) * 1.35;
    float refW = 1920.0;
    float margin = 8.0 * uiSc;
    float visRight = winSize.x;
    if (winSize.x > refW - margin * 2.0)
        visRight = min(winSize.x, refW - margin * 2.0);
    float bx = visRight - btn * 3.0 - 8.0;
    float by = (titleH - btn) * 0.5;
    uint wm = wmPack;
    vec3 c;
    c = rtxWmAsciiBtn(winLocal, bx, by, btn, 95u, th, (wm & 0xFu) == 3u, false);
    if (c.x > -1.5) return c;
    c = rtxWmAsciiBoxBtn(winLocal, bx + btn, by, btn, th, (wm & 0xFu) == 4u);
    if (c.x > -1.5) return c;
    c = rtxWmAsciiBtn(winLocal, bx + btn * 2.0, by, btn, 88u, th, (wm & 0xFu) == 2u, true);
    if (c.x > -1.5) return c;
    return vec3(-2.0);
}

#endif