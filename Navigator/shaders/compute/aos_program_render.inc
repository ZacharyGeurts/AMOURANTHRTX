// Program window body — title bars, identity text, RTX widget panels (aos_load + shared).

#ifndef AOS_PROGRAM_RENDER_INC
#define AOS_PROGRAM_RENDER_INC

const uint CTRL_DOS_PANEL_STRETCH = 512u;
const float DOOM_VIEW_W = 640.0;
const float DOOM_VIEW_H = 400.0;

const uint RTX_WIDGET_RAM = 0x000BB000u;
const uint RTX_WIDGET_POOL = 0x000BB400u;
const uint AOS_IDENTITY_RAM = 0x000BB800u;
const uint AOS_IDENTITY_STRIDE = 160u;
const uint AOS_IDENTITY_TAG_LEN = 44u;
const uint AOS_IDENTITY_RECENT1_OFF = 94u;
const uint AOS_IDENTITY_RECENT2_OFF = 126u;
const uint AOS_IDENTITY_SNAP_OFF = 158u;

vec3 rtxPanelBg(ThemePal th) {
    return mix(th.void_ * 1.35, th.panel, 0.72);
}
vec3 rtxPanelEdge(ThemePal th) {
    return mix(th.border, th.zebra, 0.55);
}
const vec3 RTX_WIDGET_DIM = vec3(0.45, 0.45, 0.48);

#include "aos_wm_ascii.inc"

bool aosDesktopActive() {
    return (field.data_bus[42] & 4096u) != 0u;
}

bool aosInsidePanel(vec2 pix, vec2 img) {
    if ((field.data_bus[42] & (1u << 25u)) != 0u) return false;
    if ((field.control & CTRL_DOS_PANEL_STRETCH) != 0u) return true;
    vec2 p0 = vec2(float(field.data_bus[46]), float(field.data_bus[47]));
    vec2 sz = aosFocusedSurfaceSize();
    if (sz.x < 2.0 || sz.y < 2.0)
        sz = vec2(DOOM_VIEW_W, DOOM_VIEW_H + max(float(field.data_bus[58]), 1.0));
    vec2 p1 = p0 + sz;
    return pix.x >= p0.x && pix.y >= p0.y && pix.x < p1.x && pix.y < p1.y;
}

float rtxWmTitleH(vec2 img) {
    uint aosBits = field.data_bus[42];
    if (!aosChromeActive()) return 0.0;
    bool consoleShell = (aosBits & (1u << 29u)) != 0u;
    if ((aosBits & (1u << 25u)) != 0u && !consoleShell) return 0.0;
    return 32.0 * max(img.x / 1920.0, 0.75) * 1.35;
}



bool rtxWidgetsActive() {
    return ramByte(RTX_WIDGET_RAM) > 0u;
}

uint rtxWidgetCount() {
    return min(ramByte(RTX_WIDGET_RAM), 48u);
}

vec2 rtxWidgetContentOrg(vec2 winSize, float titleH, float hudH) {
    return vec2(4.0, titleH + 2.0);
}

vec2 rtxWidgetContentSize(vec2 winSize, float titleH, float hudH) {
    float top = titleH + 2.0;
    float bot = winSize.y - hudH - 2.0;
    float left = 4.0;
    float right = winSize.x - 18.0;
    return max(vec2(right - left, bot - top), vec2(1.0));
}

vec4 rtxWidgetRect(uint idx) {
    uint base = RTX_WIDGET_RAM + 0x10u + idx * 16u;
    float x0 = float(ramByte(base + 2u) | (uint(ramByte(base + 3u)) << 8u)) / 1024.0;
    float y0 = float(ramByte(base + 4u) | (uint(ramByte(base + 5u)) << 8u)) / 1024.0;
    float x1 = float(ramByte(base + 6u) | (uint(ramByte(base + 7u)) << 8u)) / 1024.0;
    float y1 = float(ramByte(base + 8u) | (uint(ramByte(base + 9u)) << 8u)) / 1024.0;
    return vec4(x0, y0, x1, y1);
}

float rtxWidgetRoundBox(vec2 p, vec2 halfSize, float r) {
    vec2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float rtxWidgetRamStr(vec2 pix, vec2 p0pix, vec2 cellPix, uint strOff, uint maxLen, float clipRight) {
    float ink = 0.0;
    float x = p0pix.x;
    for (uint i = 0u; i < maxLen; ++i) {
        if (x + cellPix.x > clipRight) break;
        uint ch = ramByte(RTX_WIDGET_POOL + strOff + i);
        if (ch == 0u) break;
        if (ch < 32u || ch >= 0x80u) continue;
        ink = max(ink, dosHudGlyph(ch, pix, x, p0pix.y, cellPix));
        x += cellPix.x;
    }
    return ink;
}

vec2 rtxWidgetTextCell(vec2 contentSz) {
    float uiSc = max(contentSz.y / 540.0, 0.75) * 1.35;
    return aosHudTextCell(uiSc);
}

float rtxWidgetInk(vec2 pix, vec2 p0pix, vec2 cellPix, uint strOff, uint maxLen, float clipRight) {
    float ink = rtxWidgetRamStr(pix, p0pix, cellPix, strOff, maxLen, clipRight);
    float border = 0.0;
    border = max(border, rtxWidgetRamStr(pix + vec2(-1.0, 0.0), p0pix, cellPix, strOff, maxLen, clipRight));
    border = max(border, rtxWidgetRamStr(pix + vec2(1.0, 0.0), p0pix, cellPix, strOff, maxLen, clipRight));
    border = max(border, rtxWidgetRamStr(pix + vec2(0.0, -1.0), p0pix, cellPix, strOff, maxLen, clipRight));
    border = max(border, rtxWidgetRamStr(pix + vec2(0.0, 1.0), p0pix, cellPix, strOff, maxLen, clipRight));
    return max(ink, border * 0.92);
}

vec3 rtxPaintWidget(uint idx, vec2 winLocal, vec2 uv, vec2 contentOrg, vec2 contentSz,
        ThemePal th, int hoverId, vec3 underCol) {
    uint base = RTX_WIDGET_RAM + 0x10u + idx * 16u;
    uint wtype = ramByte(base);
    uint wflags = ramByte(base + 1u);
    uint wstyle = ramByte(base + 11u);
    vec4 r = rtxWidgetRect(idx);
    if (uv.x < r.x || uv.x >= r.z || uv.y < r.y || uv.y >= r.w) return vec3(-1.0);
    vec2 cen = (r.xy + r.zw) * 0.5;
    vec2 halfSz = (r.zw - r.xy) * 0.5;
    vec2 lp = uv - cen;
    bool hov = int(idx) == hoverId;
    uint strOff = uint(ramByte(base + 12u) | (uint(ramByte(base + 13u)) << 8u));
    uint state = uint(ramByte(base + 10u));
    int ex0 = int(ramByte(base + 14u));
    int ex1 = int(ramByte(base + 15u));
    if (ex0 > 127) ex0 -= 256;
    if (ex1 > 127) ex1 -= 256;
    vec2 rPix0 = contentOrg + r.xy * contentSz;
    vec2 rPix1 = contentOrg + r.zw * contentSz;
    vec2 cenPix = (rPix0 + rPix1) * 0.5;
    vec2 cell = rtxWidgetTextCell(contentSz);
    float uiSc = max(contentSz.y / 540.0, 0.75) * 1.35;
    vec2 txtTop = vec2(rPix0.x + 8.0 * uiSc, rPix0.y + 6.0 * uiSc);

    if (wtype == 0u) {
        float d = rtxWidgetRoundBox(lp, halfSz, 0.012);
        vec3 face = (wstyle == 1u) ? mix(rtxPanelBg(th), th.zebra, 0.42) : rtxPanelBg(th);
        vec3 edge = rtxPanelEdge(th);
        vec3 col = mix(face, edge, smoothstep(0.0, -0.004, d));
        if (wstyle == 1u && lp.y < -halfSz.y + 0.020)
            col = mix(col, th.accent * 0.50 + vec3(0.10, 0.12, 0.16), 0.82);
        col = mix(col, th.accent * 0.22 + th.zebra, hov ? 0.35 : 0.0);
        return max(col, vec3(0.0));
    }
    if (wtype == 1u) {
        float clipR = rPix1.x - 4.0 * uiSc;
        vec2 lblCell = (wstyle == 1u) ? cell * 1.05 : cell;
        float ink = rtxWidgetInk(winLocal, txtTop, lblCell, strOff, 64u, clipR);
        if (ink < 0.02) return vec3(-1.0);
        vec3 inkCol = (wstyle == 1u)
            ? mix(th.accent * 0.55 + th.title * 0.45, AOS_INK_BLACK, 0.62)
            : mix(th.label, AOS_INK_BLACK, 0.78);
        return mixHudInk(underCol, ink, 0.0, inkCol);
    }
    if (wtype == 2u) {
        float box = 14.0 * uiSc;
        vec2 b0pix = vec2(rPix0.x + 8.0, txtTop.y);
        vec2 buv = (winLocal - b0pix) / box;
        vec3 col = underCol;
        if (all(greaterThanEqual(buv, vec2(0.0))) && all(lessThan(buv, vec2(1.0)))) {
            float d = rtxWidgetRoundBox(buv - 0.5, vec2(0.42), 0.08);
            vec3 boxCol = mix(rtxPanelEdge(th), th.accent * 0.65 + vec3(0.28, 0.28, 0.30),
                smoothstep(0.04, -0.02, d) * (((wflags & 8u) != 0u || state != 0u) ? 1.0 : 0.0));
            boxCol = mix(rtxPanelBg(th), boxCol, smoothstep(0.06, -0.02, d));
            col = mix(col, boxCol, smoothstep(0.06, -0.02, d));
        }
        float ink = rtxWidgetInk(winLocal, vec2(b0pix.x + box + 6.0 * uiSc, txtTop.y),
            cell, strOff, 48u, rPix1.x - 4.0);
        if (ink > 0.02)
            col = mixHudInk(col, ink, 0.0, AOS_INK_BLACK);
        return col;
    }
    if (wtype == 3u) {
        bool focused = (wflags & 4u) != 0u;
        float d = rtxWidgetRoundBox(lp, halfSz * 0.95, 0.01);
        vec3 col = mix(rtxPanelEdge(th), th.accent * 0.45 + vec3(0.22, 0.22, 0.24),
            (hov || focused) ? 0.78 : 0.35);
        col = mix(underCol, col, smoothstep(0.02, -0.01, d));
        float ink = rtxWidgetInk(winLocal, txtTop, cell, strOff, 36u, rPix1.x - 8.0 * uiSc);
        return mixHudInk(col, ink, 0.0, AOS_INK_BLACK);
    }
    if (wtype == 4u) {
        float d = rtxWidgetRoundBox(lp, halfSz * 0.95, 0.008);
        vec3 col = mix(underCol, rtxPanelEdge(th), smoothstep(0.015, -0.01, d));
        if (hov) col = mix(col, th.accent * 0.35 + vec3(0.20, 0.20, 0.22), 0.55);
        float ink = rtxWidgetInk(winLocal, txtTop, cell, strOff, 40u, rPix1.x - 20.0 * uiSc);
        col = mixHudInk(col, ink, 0.0, AOS_INK_BLACK);
        float ax = r.z - 0.02;
        if (uv.x > ax - 0.015 && uv.x < ax && uv.y > cen.y - 0.012 && uv.y < cen.y + 0.012)
            col = mix(col, RTX_WIDGET_DIM, 0.8);
        return col;
    }
    if (wtype == 5u) {
        vec3 track = mix(vec3(0.12, 0.12, 0.14), th.panel * 0.35, 0.5);
        uint top = uint(ramByte(RTX_WIDGET_RAM + 2u) | (uint(ramByte(RTX_WIDGET_RAM + 3u)) << 8u));
        uint total = uint(ramByte(RTX_WIDGET_RAM + 4u) | (uint(ramByte(RTX_WIDGET_RAM + 5u)) << 8u));
        float frac = float(top) / max(float(total), 1.0);
        float thumbH = max(halfSz.y * 0.35, 0.04);
        float thumbY = -halfSz.y + thumbH + frac * (halfSz.y * 2.0 - thumbH * 2.0);
        float d = abs(lp.y - thumbY) - thumbH;
        vec3 col = track;
        if (d < 0.0) col = mix(th.accent, th.gemTint, 0.48);
        return col;
    }
    if (wtype == 6u) {
        vec3 col = underCol;
        float fill = float(state) / 255.0;
        vec2 fp = lp + vec2(halfSz.x * (fill * 2.0 - 1.0), 0.0);
        float d = rtxWidgetRoundBox(fp, vec2(halfSz.x * fill, halfSz.y * 0.55), 0.006);
        col = mix(col, th.accent * 0.55 + vec3(0.24, 0.24, 0.26), smoothstep(0.01, -0.01, d));
        float ink = rtxWidgetInk(winLocal, vec2(rPix0.x + 8.0 * uiSc, txtTop.y), cell, strOff, 32u, rPix1.x - 4.0);
        return mixHudInk(col, ink, 0.0, AOS_INK_BLACK);
    }
    if (wtype == 7u) {
        float rad = min(halfSz.x, halfSz.y) * 0.82;
        float d = length(lp) - rad;
        vec3 col = mix(underCol, rtxPanelEdge(th), smoothstep(0.01, -0.01, d));
        vec2 dot = vec2(float(ex0) / 127.0, float(ex1) / 127.0) * rad * 0.72;
        if (length(lp - dot) < rad * 0.14) col = mix(col, th.accent * 0.7 + vec3(0.30, 0.30, 0.32), 0.9);
        float ink = rtxWidgetInk(winLocal, vec2(rPix0.x, txtTop.y), cell, strOff, 24u, rPix1.x - 4.0);
        return mixHudInk(col, ink * 0.7, 0.0, AOS_INK_DIM);
    }
    if (wtype == 8u) {
        vec3 col = rtxPanelBg(th);
        float sz = min(halfSz.x, halfSz.y) * 0.22;
        uint mask = state | (uint(ex0) << 8);
        vec2 dirs[4] = vec2[4](vec2(0.0, -sz), vec2(0.0, sz), vec2(-sz, 0.0), vec2(sz, 0.0));
        for (int i = 0; i < 4; ++i) {
            vec2 p = dirs[i];
            float d = rtxWidgetRoundBox(lp - p, vec2(sz * 0.85), 0.02);
            bool on = (mask & (1u << uint(i))) != 0u;
            vec3 c = on ? th.accent * 0.6 + vec3(0.26, 0.26, 0.28) : rtxPanelEdge(th);
            col = mix(col, c, smoothstep(0.01, -0.01, d));
        }
        return col;
    }
    return vec3(-1.0);
}

vec3 rtxWidgetPanelColor(vec2 winLocal, vec2 winSize, float titleH, float hudH, ThemePal th) {
    if (!rtxWidgetsActive()) return vec3(-1.0);
    vec2 contentOrg = rtxWidgetContentOrg(winSize, titleH, hudH);
    vec2 contentSz = rtxWidgetContentSize(winSize, titleH, hudH);
    vec2 uv = (winLocal - contentOrg) / contentSz;
    uv = clamp(uv, vec2(0.0), vec2(1.0));
    int hoverId = int(ramByte(RTX_WIDGET_RAM + 6u));
    if (hoverId >= int(rtxWidgetCount())) hoverId = -1;
    vec3 col = rtxPanelBg(th);
    uint cnt = rtxWidgetCount();
    for (uint i = 0u; i < cnt; ++i) {
        uint wtype = ramByte(RTX_WIDGET_RAM + 0x10u + i * 16u);
        if (wtype != 0u) continue;
        vec3 w = rtxPaintWidget(i, winLocal, uv, contentOrg, contentSz, th, hoverId, col);
        if (w.x >= 0.0) col = w;
    }
    for (uint i = 0u; i < cnt; ++i) {
        uint wtype = ramByte(RTX_WIDGET_RAM + 0x10u + i * 16u);
        if (wtype == 0u) continue;
        vec3 w = rtxPaintWidget(i, winLocal, uv, contentOrg, contentSz, th, hoverId, col);
        if (w.x >= 0.0) col = w;
    }
    return max(col, vec3(0.0));
}

vec3 rtxWmTitleBarForTab(vec2 winLocal, vec2 winSize, float titleH, uint tabIdx, ThemePal th) {
    float uiSc = max(winSize.x / 1920.0, 0.75) * 1.35;
    vec2 muv = vec2(winLocal.x / max(winSize.x, 1.0), winLocal.y / max(titleH, 1.0));
    vec3 base = aosSdTitleBarWin31(winLocal, winSize, titleH, true);
    if (base.x < 0.0) base = mix(vec3(0.20, 0.20, 0.22), vec3(0.28, 0.28, 0.30), muv.y);
    vec2 cell = vec2(8.0, 14.0) * uiSc;
    float rowY = titleH * 0.5 - 7.0 * uiSc;
    float ink = 0.0;
    float inkB = 0.0;
    float titleX = 48.0 * uiSc;
    uint titleBase = AOS_TASKBAR_RAM + 0x10u + tabIdx * AOS_TAB_STRIDE;
    ink = max(ink, dosHudRamStr(winLocal, titleX, rowY, cell, titleBase, 22u));
    inkB = max(inkB, dosHudRamStrBorder(winLocal, titleX, rowY, cell, titleBase, 22u));
    float iconSz = titleH * 0.58;
    float iconX = 10.0 * uiSc;
    float iconY = (titleH - iconSz) * 0.5;
    if (winLocal.x >= iconX && winLocal.x < iconX + iconSz
            && winLocal.y >= iconY && winLocal.y < iconY + iconSz) {
        vec2 il = (winLocal - vec2(iconX, iconY)) / iconSz;
        uint iconSlot = uint(ramByte(titleBase + AOS_TAB_STRIDE - 1u));
        base = mix(base, aosBlendIcon(base, sampleAosIconRgba(iconSlot, il)), 0.88);
    }
    vec3 c = rtxWmTitleAsciiControls(winLocal, winSize, titleH, 0u, th);
    if (c.x > -1.5) return c;
    return mixHudInk(base, ink, inkB, AOS_INK_BLACK);
}

vec3 aosBackgroundWindowBody(vec2 winLocal, vec2 winSize, float titleH, float hudH,
        uint tabIdx, ThemePal th) {
    if (winLocal.y < titleH || winLocal.y >= winSize.y - hudH) return vec3(-1.0);
    if (rtxWidgetsActive()) {
        vec3 widgets = rtxWidgetPanelColor(winLocal, winSize, titleH, hudH, th);
        if (widgets.x >= 0.0) return widgets;
    }
    vec3 col = rtxPanelBg(th);
    float uiSc = max(winSize.x / 1920.0, 0.75) * 1.35;
    vec2 cell = aosHudTextCell(uiSc);
    uint idBase = AOS_IDENTITY_RAM + tabIdx * AOS_IDENTITY_STRIDE;
    float tagY = titleH + 28.0 * uiSc;
    float ink = 0.0;
    float inkB = 0.0;
    ink = max(ink, dosHudRamStr(winLocal, 24.0 * uiSc, tagY, cell, idBase, AOS_IDENTITY_TAG_LEN));
    inkB = max(inkB, dosHudRamStrBorder(winLocal, 24.0 * uiSc, tagY, cell, idBase, AOS_IDENTITY_TAG_LEN));
    float aboutY = tagY + 36.0 * uiSc;
    uint aboutBase = idBase + AOS_IDENTITY_TAG_LEN;
    ink = max(ink, dosHudRamStr(winLocal, 24.0 * uiSc, aboutY, cell, aboutBase, 48u));
    inkB = max(inkB, dosHudRamStrBorder(winLocal, 24.0 * uiSc, aboutY, cell, aboutBase, 48u));
    float recentY = aboutY + 44.0 * uiSc;
    uint recentBase = idBase + AOS_IDENTITY_RECENT1_OFF;
    ink = max(ink, dosHudRamStr(winLocal, 24.0 * uiSc, recentY, cell, recentBase, 32u));
    inkB = max(inkB, dosHudRamStrBorder(winLocal, 24.0 * uiSc, recentY, cell, recentBase, 32u));
    float recent2Y = recentY + 28.0 * uiSc;
    uint recent2Base = idBase + AOS_IDENTITY_RECENT2_OFF;
    ink = max(ink, dosHudRamStr(winLocal, 24.0 * uiSc, recent2Y, cell, recent2Base, 32u));
    inkB = max(inkB, dosHudRamStrBorder(winLocal, 24.0 * uiSc, recent2Y, cell, recent2Base, 32u));
    float snapY = recent2Y + 28.0 * uiSc;
    uint snapBase = idBase + AOS_IDENTITY_SNAP_OFF;
    ink = max(ink, dosHudRamStr(winLocal, 24.0 * uiSc, snapY, cell, snapBase, 24u));
    inkB = max(inkB, dosHudRamStrBorder(winLocal, 24.0 * uiSc, snapY, cell, snapBase, 24u));
    float hintY = winSize.y - hudH - 40.0 * uiSc;
    ink = max(ink, dosHudRun(winLocal, 24.0 * uiSc, hintY, cell,
        67u, 108u, 105u, 99u, 107u, 32u, 116u, 97u));
    inkB = max(inkB, dosHudRunBorder(winLocal, 24.0 * uiSc, hintY, cell,
        67u, 108u, 105u, 99u, 107u, 32u, 116u, 97u));
    ink = max(ink, dosHudRun(winLocal, 24.0 * uiSc + cell.x * 5.5, hintY, cell,
        98u, 97u, 114u, 32u, 116u, 111u, 32u, 102u));
    inkB = max(inkB, dosHudRunBorder(winLocal, 24.0 * uiSc + cell.x * 5.5, hintY, cell,
        98u, 97u, 114u, 32u, 116u, 111u, 32u, 102u));
    float iconSz = min(winSize.x, winSize.y - titleH - hudH) * 0.22;
    float iconX = winSize.x * 0.5 - iconSz * 0.5;
    float iconY = titleH + (winSize.y - titleH - hudH) * 0.42;
    if (winLocal.x >= iconX && winLocal.x < iconX + iconSz
            && winLocal.y >= iconY && winLocal.y < iconY + iconSz) {
        vec2 il = (winLocal - vec2(iconX, iconY)) / iconSz;
        uint titleBase = AOS_TASKBAR_RAM + 0x10u + tabIdx * AOS_TAB_STRIDE;
        uint iconSlot = uint(ramByte(titleBase + AOS_TAB_STRIDE - 1u));
        col = mix(col, aosBlendIcon(col, sampleAosIconRgba(iconSlot, il)), 0.92);
    }
    col = mixHudInkDark(col, vec4(ink, 0.0, 0.0, 0.0), inkB);
    return max(col, vec3(0.0));
}

vec3 aosBackgroundWindowAt(vec2 pix, vec2 img, float taskH, uint tabIdx,
        vec2 p0, vec2 sz, ThemePal th) {
    if (pix.x < p0.x || pix.y < p0.y || pix.x >= p0.x + sz.x || pix.y >= p0.y + sz.y)
        return vec3(-1.0);
    vec2 winLocal = pix - p0;
    float titleH = rtxWmTitleH(img);
    float hudH = max(float(field.data_bus[58]), 1.0);
    float uiSc = max(sz.x / 1920.0, 0.75) * 1.35;
    vec3 shell = aosSdfComposeWindow(winLocal, sz, titleH, hudH, th, uiSc, false);
    if (winLocal.y < titleH)
        return rtxWmTitleBarForTab(winLocal, sz, titleH, tabIdx, th);
    if (winLocal.y >= sz.y - hudH) {
        vec3 status = aosSdStatusBarWin31(winLocal, sz, hudH);
        if (status.x >= 0.0) return status;
    }
    vec3 body = aosBackgroundWindowBody(winLocal, sz, titleH, hudH, tabIdx, th);
    if (body.x >= 0.0) return body;
    if (shell.x >= 0.0) return shell;
    return rtxPanelBg(th);
}

vec3 aosFocusedWindowLayer(vec2 pix, vec2 img, float taskH, ThemePal th) {
    if (!aosDesktopActive()) return vec3(-1.0);
    uint packW = field.data_bus[60];
    uint packH = field.data_bus[61];
    uint surfCount = packW >> 24u;
    if (surfCount == 0u) return vec3(-1.0);
    uint surfFlags = (packH >> 16u) & 0xFFu;
    if ((surfFlags & 1u) != 0u) return vec3(-1.0);
    int focusTab = int((packW >> 16u) & 0xFFu);
    if (focusTab < 0) focusTab = 0;
    float ox = float(field.data_bus[46]);
    float oy = float(field.data_bus[47]);
    float pw = float(packW & 0xFFFFu);
    float ph = float(packH & 0xFFFFu);
    if (pw < 8.0 || ph < 8.0) return vec3(-1.0);
    return aosBackgroundWindowAt(pix, img, taskH, uint(focusTab), vec2(ox, oy), vec2(pw, ph), th);
}

vec3 aosBackgroundWindows(vec2 pix, vec2 img, float taskH, ThemePal th) {
    if (!aosDesktopActive() || aosPanelHidden()) return vec3(-1.0);
    uint surfCount = uint(ramByte(AOS_SURFACE_RAM));
    if (surfCount <= 1u) return vec3(-1.0);
    int focusTab = int(ramByte(AOS_SURFACE_RAM + 1u));
    vec3 best = vec3(-1.0);
    for (uint i = 0u; i < surfCount && i < 8u; ++i) {
        if (int(i) == focusTab) continue;
        uint base = AOS_SURFACE_RAM + 0x10u + i * 16u;
        uint flags = ramByte(base + 11u);
        if ((flags & 1u) != 0u) continue;
        float ox = float(uint(ramByte(base)) | (uint(ramByte(base + 1u)) << 8u));
        float oy = float(uint(ramByte(base + 2u)) | (uint(ramByte(base + 3u)) << 8u));
        float pw = float(uint(ramByte(base + 4u)) | (uint(ramByte(base + 5u)) << 8u));
        float ph = float(uint(ramByte(base + 6u)) | (uint(ramByte(base + 7u)) << 8u));
        if (pw < 8.0 || ph < 8.0) continue;
        vec3 c = aosBackgroundWindowAt(pix, img, taskH, i, vec2(ox, oy), vec2(pw, ph), th);
        if (c.x >= 0.0) best = c;
    }
    return best;
}

vec3 amouranthOsBackdrop(vec2 pix, vec2 img, ThemePal th) {
    if (!aosChromeActive())
        return vec3(-1.0);

    float taskH = aosTbTaskH(img);

    vec3 focusedWin = aosFocusedWindowLayer(pix, img, taskH, th);
    if (focusedWin.x >= 0.0) return focusedWin;

    vec3 bgWin = aosBackgroundWindows(pix, img, taskH, th);
    if (bgWin.x >= 0.0) return bgWin;

    if (aosInsidePanel(pix, img))
        return vec3(-1.0);

    return aosDarkDesktop(pix, img, taskH, th);
}

#endif