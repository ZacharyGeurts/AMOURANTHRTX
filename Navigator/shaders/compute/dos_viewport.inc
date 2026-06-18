// DOS / VGA panel viewport — terminal content above bottom status HUD.
struct DoomViewport {
    bool inside;
    bool letterbox;
    vec2 gameUv;
    vec2 p0;
    vec2 p1;
};

DoomViewport doomViewportMap(vec2 pix, vec2 img) {
    DoomViewport v;
    v.inside = false;
    v.letterbox = false;
    v.gameUv = vec2(0.0);
    v.p0 = vec2(0.0);
    v.p1 = vec2(0.0);
    float hudH = max(float(field.data_bus[58]), 1.0);
    bool panelStretch = (field.control & CTRL_DOS_PANEL_STRETCH) != 0u;
    vec2 winP0;
    vec2 winP1;
    if (panelStretch) {
        winP0 = vec2(0.0);
        winP1 = img;
    } else {
        winP0 = vec2(float(field.data_bus[46]), float(field.data_bus[47]));
        float pw = float(field.data_bus[60]);
        float ph = float(field.data_bus[61]);
        if (pw > 1.0 && ph > 1.0)
            winP1 = winP0 + vec2(pw, ph);
        else
            winP1 = winP0 + vec2(DOOM_VIEW_W, DOOM_VIEW_H + hudH);
    }
    vec2 p0 = winP0;
    vec2 p1 = winP1 - vec2(0.0, hudH);
    v.p0 = p0;
    v.p1 = p1;
    if (pix.x < p0.x || pix.y < p0.y || pix.x >= p1.x || pix.y >= p1.y)
        return v;
    v.inside = true;
    vec2 vpSize = p1 - p0;
    vec2 local = (pix - p0) / vpSize;
    uint vmode = ramByte(0x449u);
    if (vmode == 0x13u) {
        float activeH = float(DOOM_H) * DOOM_PIXEL_SCALE;
        float frameH = vpSize.y;
        float bar = (frameH - activeH * (vpSize.y / max(vpSize.y, DOOM_VIEW_H))) / max(frameH, 1.0);
        float y0 = bar * 0.5;
        float y1 = 1.0 - y0;
        if (local.y < y0 || local.y >= y1) {
            v.letterbox = true;
            return v;
        }
        v.gameUv = vec2(local.x, (local.y - y0) / max(y1 - y0, 0.001));
    } else {
        v.gameUv = local;
    }
    return v;
}

bool doomPixelInViewport(vec2 pix, vec2 img) {
    return doomViewportMap(pix, img).inside;
}

bool dosPanelBorderHit(vec2 pix, DoomViewport v) {
    vec2 vpSize = v.p1 - v.p0;
    vec2 lp = pix - v.p0;
    return lp.x < 1.5 || lp.y < 1.5 || lp.x >= vpSize.x - 1.5 || lp.y >= vpSize.y - 1.5;
}

bool dosWindowBorderHit(vec2 local, vec2 size) {
    return local.x < 1.0 || local.y < 1.0 || local.x >= size.x - 1.0 || local.y >= size.y - 1.0;
}