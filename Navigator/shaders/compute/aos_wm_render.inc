// SDF window chrome — Aqua panels, title band, status strip, raised shadow.

#ifndef AOS_WM_RENDER_INC
#define AOS_WM_RENDER_INC

vec3 aosWmWindowShell(vec2 local, vec2 winSize, float titleH, float hudH,
        vec3 accent, vec3 border, float scale) {
    return aosSdWindowShell(local, winSize, titleH, hudH, accent, border, scale);
}

vec3 aosWmTitleBand(vec2 local, vec2 winSize, float titleH, vec3 accent, bool focused) {
    return aosSdTitleBarAqua(local, winSize, titleH, accent, focused);
}

vec3 aosWmStatusBand(vec2 local, vec2 winSize, float hudH, vec3 border) {
    return aosSdStatusBar(local, winSize, hudH, border);
}

vec3 aosWmCloseBtn(vec2 local, vec2 origin, float sz, bool hovered) {
    return aosSdWmCloseButton(local, origin, sz, hovered);
}

vec3 aosWmPaintFrame(vec2 winLocal, vec2 winSize, float titleH, float hudH,
        ThemePal th, float uiSc) {
    vec3 shell = aosWmWindowShell(winLocal, winSize, titleH, hudH, th.accent, th.border, uiSc);
    if (winLocal.y < titleH) {
        vec3 band = aosWmTitleBand(winLocal, winSize, titleH, th.accent, true);
        if (band.x >= 0.0) return band;
    }
    if (winLocal.y >= winSize.y - hudH) {
        vec3 status = aosWmStatusBand(winLocal, winSize, hudH, th.border);
        if (status.x >= 0.0) return status;
    }
    return shell;
}

#endif