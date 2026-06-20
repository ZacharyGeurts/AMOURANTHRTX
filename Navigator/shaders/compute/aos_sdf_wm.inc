// SDF window chrome — dark Win31 shell, sunken client, title band, status strip.

#ifndef AOS_SDF_WM_INC
#define AOS_SDF_WM_INC

vec3 aosSdfWindowShellLayer(vec2 local, vec2 winSize, float titleH, float hudH,
        ThemePal th, float uiSc) {
    return aosSdWin31WindowShell(local, winSize, titleH, hudH, uiSc);
}

vec3 aosSdfWindowTitleLayer(vec2 local, vec2 winSize, float titleH, ThemePal th, bool focused) {
    if (local.y >= titleH) return vec3(-1.0);
    vec3 band = aosSdTitleBarWin31(local, winSize, titleH, focused);
    if (band.x >= 0.0) return band;
    return vec3(-1.0);
}

vec3 aosSdfWindowStatusLayer(vec2 local, vec2 winSize, float hudH, ThemePal th) {
    if (local.y < winSize.y - hudH) return vec3(-1.0);
    vec3 status = aosSdStatusBarWin31(local, winSize, hudH);
    if (status.x >= 0.0) return status;
    return vec3(-1.0);
}

vec3 aosSdfWindowCloseBtn(vec2 local, vec2 winSize, float titleH, float btnSz, bool hovered) {
    float bx = winSize.x - btnSz - 8.0;
    float by = (titleH - btnSz) * 0.5;
    return aosSdWmCloseButton(local, vec2(bx, by), btnSz, hovered);
}

vec3 aosSdfComposeWindow(vec2 local, vec2 winSize, float titleH, float hudH,
        ThemePal th, float uiSc, bool focused) {
    vec3 shell = aosSdfWindowShellLayer(local, winSize, titleH, hudH, th, uiSc);
    if (local.y < titleH) {
        vec3 title = aosSdfWindowTitleLayer(local, winSize, titleH, th, focused);
        if (title.x >= 0.0) return title;
    }
    if (local.y >= winSize.y - hudH) {
        vec3 status = aosSdfWindowStatusLayer(local, winSize, hudH, th);
        if (status.x >= 0.0) return status;
    }
    if (shell.x >= 0.0) return shell;
    return vec3(-1.0);
}

#endif