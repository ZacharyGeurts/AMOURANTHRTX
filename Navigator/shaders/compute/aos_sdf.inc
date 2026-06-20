// Zero-cost analytic SDF desktop primitives — per-pixel, no textures, no noise march.

float aosSdRoundBox(vec2 p, vec2 halfSz, float r) {
    vec2 q = abs(p) - halfSz + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float aosSdBox(vec2 p, vec2 halfSz) {
    vec2 d = abs(p) - halfSz;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// rad: x=tl, y=tr, z=br, w=bl (y increases downward)
float aosSdRoundRect(vec2 pix, vec2 b0, vec2 b1, vec4 rad) {
    vec2 cen = (b0 + b1) * 0.5;
    vec2 p = pix - cen;
    vec2 b = (b1 - b0) * 0.5;
    p.y = -p.y;
    vec4 r = vec4(rad.x, rad.y, rad.w, rad.z);
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r.x;
}

vec3 aosSdGradientY(vec2 pix, vec2 b0, vec2 b1, vec3 top, vec3 bot) {
    float t = (pix.y - b0.y) / max(b1.y - b0.y, 1.0);
    return mix(top, bot, clamp(t, 0.0, 1.0));
}

vec3 aosSdRoundPanel(vec2 pix, vec2 b0, vec2 b1, float r,
        vec3 top, vec3 bot, vec3 border, float borderW) {
    if (!all(greaterThanEqual(pix, b0)) || !all(lessThan(pix, b1)))
        return vec3(-1.0);
    vec2 cen = (b0 + b1) * 0.5;
    vec2 halfSz = (b1 - b0) * 0.5;
    float d = aosSdRoundBox(pix - cen, halfSz, r);
    vec3 fill = aosSdGradientY(pix, b0, b1, top, bot);
    if (d > 0.5) return vec3(-1.0);
    float rim = smoothstep(borderW, 0.0, abs(d));
    vec3 col = mix(fill, border, rim * 0.82);
    return clamp(col, 0.0, 1.0);
}

vec3 aosSdRoundPanelHover(vec2 pix, vec2 b0, vec2 b1, float r,
        vec3 top, vec3 bot, vec3 border, vec3 accent, bool hover, float borderW) {
    vec3 col = aosSdRoundPanel(pix, b0, b1, r, top, bot, border, borderW);
    if (col.x < 0.0) return col;
    if (hover)
        col = mix(col, accent * 0.40 + top * 0.60, 0.58);
    return col;
}

vec3 aosSdDropShadow(vec2 pix, vec2 b0, vec2 b1, float r, vec2 offset, vec3 shadow, float alpha) {
    vec2 sb0 = b0 + offset;
    vec2 sb1 = b1 + offset;
    if (!all(greaterThanEqual(pix, sb0)) || !all(lessThan(pix, sb1)))
        return vec3(-1.0);
    vec2 cen = (sb0 + sb1) * 0.5;
    vec2 halfSz = (sb1 - sb0) * 0.5;
    float d = aosSdRoundBox(pix - cen, halfSz, r);
    float mask = 1.0 - smoothstep(0.0, 2.0, d);
    return shadow * mask * alpha;
}

vec3 aosSdDropShadowRect(vec2 pix, vec2 b0, vec2 b1, vec4 rad, vec2 offset, vec3 shadow, float alpha) {
    vec2 sb0 = b0 + offset;
    vec2 sb1 = b1 + offset;
    float d = aosSdRoundRect(pix, sb0, sb1, rad);
    float mask = 1.0 - smoothstep(-1.0, 3.0, d);
    if (mask < 0.01) return vec3(-1.0);
    float inside = aosSdRoundRect(pix, b0, b1, rad);
    if (inside <= 0.0) mask *= smoothstep(-0.5, 1.5, inside);
    return shadow * mask * alpha;
}

vec3 aosSdTitleBar(vec2 local, vec2 winSize, float titleH, vec3 accent, bool focused) {
    if (local.y < 0.0 || local.y >= titleH) return vec3(-1.0);
    float t = local.y / max(titleH, 1.0);
    vec3 col = mix(vec3(0.20, 0.20, 0.22), vec3(0.28, 0.28, 0.30), t);
    if (focused) {
        float line = smoothstep(titleH - 1.5, titleH - 0.5, local.y);
        col = mix(col, accent * 0.55 + vec3(0.18, 0.18, 0.20), line * 0.65);
    }
    float edgeD = min(min(local.x, winSize.x - local.x), min(local.y, titleH - local.y));
    col = mix(col, vec3(0.38, 0.38, 0.42), smoothstep(0.0, 1.0, edgeD) * 0.35);
    return col;
}

vec3 aosSdTitleBarAqua(vec2 local, vec2 winSize, float titleH, vec3 accent, bool focused) {
    if (local.y < 0.0 || local.y >= titleH) return vec3(-1.0);
    float t = local.y / max(titleH, 1.0);
    vec3 aquaTop = accent * 0.88 + vec3(0.06, 0.10, 0.14);
    vec3 aquaMid = accent * 0.72 + vec3(0.10, 0.14, 0.18);
    vec3 col = mix(aquaTop, aquaMid, t);
    float band = smoothstep(titleH - 4.0, titleH - 1.0, local.y);
    col = mix(col, vec3(0.48, 0.52, 0.58), band * 0.55);
    if (!focused)
        col = mix(col, vec3(0.42, 0.44, 0.48), 0.62);
    float edgeD = min(min(local.x, winSize.x - local.x), min(local.y, titleH - local.y));
    col = mix(col, vec3(0.98, 0.99, 1.0), smoothstep(0.0, 1.2, edgeD) * 0.22);
    return col;
}

vec3 aosSdStatusBar(vec2 local, vec2 winSize, float hudH, vec3 border) {
    float y0 = winSize.y - hudH;
    if (local.y < y0 || local.y >= winSize.y) return vec3(-1.0);
    float t = (local.y - y0) / max(hudH, 1.0);
    vec3 top = vec3(0.82, 0.84, 0.88);
    vec3 bot = vec3(0.70, 0.72, 0.76);
    vec3 col = mix(top, bot, t);
    float edge = smoothstep(0.0, 1.5, local.y - y0);
    col = mix(col, border * 0.55 + vec3(0.90), edge * 0.35);
    return col;
}

vec3 aosSdWindowShell(vec2 local, vec2 winSize, float titleH, float hudH,
        vec3 accent, vec3 border, float scale) {
    vec2 b0 = vec2(0.0);
    vec2 b1 = winSize;
    vec4 rad = vec4(16.0, 16.0, 5.0, 5.0) * scale;

    vec3 shadow = aosSdDropShadowRect(local, b0, b1, rad,
        vec2(5.0, 8.0) * scale, vec3(0.01, 0.03, 0.08), 0.92);
    if (shadow.x >= 0.0) return shadow;

    float d = aosSdRoundRect(local, b0, b1, rad);
    if (d > 0.5) return vec3(-1.0);

    vec3 bodyTop = mix(vec3(0.95, 0.96, 0.98), accent * 0.06 + vec3(0.92), 0.40);
    vec3 bodyMid = vec3(0.90, 0.91, 0.94);
    vec3 bodyBot = vec3(0.84, 0.86, 0.90);
    vec3 fill = mix(bodyTop, bodyMid, clamp(local.y / max(winSize.y * 0.35, 1.0), 0.0, 1.0));
    fill = mix(fill, bodyBot, clamp((local.y - winSize.y * 0.5) / max(winSize.y * 0.5, 1.0), 0.0, 1.0));

    float rim = smoothstep(2.5 * scale, 0.0, abs(d));
    fill = mix(fill, border * 0.70 + vec3(0.98), rim * 0.55);
    float lift = smoothstep(4.0 * scale, 0.0, d) * 0.05;
    fill += vec3(lift);

    if (local.y >= titleH && local.y < winSize.y - hudH)
        fill = mix(vec3(0.97, 0.98, 1.0), vec3(0.92, 0.93, 0.96),
            smoothstep(titleH, titleH + 10.0 * scale, local.y) * 0.65);

    return clamp(fill, 0.0, 1.0);
}

vec3 aosSdTaskbarRail(vec2 pix, vec2 vp, float taskH, vec3 accent, float scale) {
    if (pix.y < vp.y - taskH) return vec3(-1.0);
    float relY = (pix.y - (vp.y - taskH)) / max(taskH, 1.0);
    vec3 bar = mix(vec3(0.08, 0.09, 0.11), vec3(0.15, 0.16, 0.19), relY);
    float topBand = smoothstep(0.0, 0.18, relY);
    bar = mix(accent * 0.78 + vec3(0.08, 0.22, 0.42), bar, topBand);
    float glow = smoothstep(0.12, 0.35, relY);
    bar = mix(bar, accent * 0.22 + vec3(0.12, 0.13, 0.16), glow * 0.45);
    return clamp(bar, 0.0, 1.0);
}

vec3 aosSdWindowFrame(vec2 local, vec2 winSize, float hudH, vec3 border) {
    vec2 cen = vec2(winSize.x * 0.5, (winSize.y - hudH) * 0.5);
    vec2 halfSz = winSize * 0.5 - vec2(1.0);
    float d = aosSdRoundBox(local - cen, halfSz, 3.0);
    float rim = smoothstep(2.0, 0.0, abs(d));
    if (rim < 0.01) return vec3(-1.0);
    return border * rim;
}

vec3 aosSdWmButton(vec2 local, vec2 origin, float sz, vec3 baseCol, vec3 hotCol, bool hovered) {
    vec2 p = local - origin;
    if (any(lessThan(p, vec2(0.0))) || any(greaterThanEqual(p, vec2(sz))))
        return vec3(-2.0);
    vec2 cen = vec2(sz * 0.5);
    float d = aosSdRoundBox(p - cen, vec2(sz * 0.5 - 2.0, sz * 0.5 - 2.0), 4.0);
    float body = 1.0 - smoothstep(-0.5, 0.5, d);
    vec3 fill = mix(baseCol, hotCol, hovered ? 0.72 : 0.0);
    float rim = smoothstep(1.5, 0.0, abs(d));
    return mix(fill, vec3(0.92), rim * 0.28) * body;
}

vec3 aosSdWmCloseButton(vec2 local, vec2 origin, float sz, bool hovered) {
    vec2 p = local - origin;
    if (any(lessThan(p, vec2(0.0))) || any(greaterThanEqual(p, vec2(sz))))
        return vec3(-2.0);
    vec2 cen = vec2(sz * 0.5);
    float d = aosSdRoundBox(p - cen, vec2(sz * 0.5 - 2.0, sz * 0.5 - 2.0), 4.0);
    float body = 1.0 - smoothstep(-0.5, 0.5, d);
    vec3 base = mix(vec3(0.55, 0.58, 0.62), vec3(0.82, 0.18, 0.22), hovered ? 0.88 : 0.0);
    vec3 fill = mix(vec3(0.48, 0.50, 0.54), base, body);
    float rim = smoothstep(1.5, 0.0, abs(d));
    fill = mix(fill, vec3(0.98), rim * 0.30) * body;
    if (body > 0.02) {
        vec2 lp = (p - cen) / (sz * 0.5);
        float x1 = 1.0 - smoothstep(0.32, 0.38, abs(lp.x - lp.y));
        float x2 = 1.0 - smoothstep(0.32, 0.38, abs(lp.x + lp.y));
        float xMark = max(x1, x2);
        fill = mix(fill, vec3(0.98), xMark * body * (hovered ? 1.0 : 0.85));
    }
    return fill;
}

vec3 aosSdMenuSheet(vec2 rel, vec2 panelSize, float scale, vec3 top, vec3 bot, vec3 border) {
    vec3 fill = mix(top, bot, rel.y / max(panelSize.y, 1.0));
    vec2 halfSz = panelSize * 0.5;
    float d = aosSdRoundBox(rel - halfSz, halfSz, 6.0 * scale);
    float rim = smoothstep(3.0 * scale, 0.0, abs(d));
    return mix(fill, border, rim * 0.75);
}

// Win31 dark 3D bevel — raised (light top/left, shadow bottom/right).
vec3 aosSdWin31Hi() { return vec3(0.78, 0.80, 0.84); }
vec3 aosSdWin31Lo() { return vec3(0.14, 0.14, 0.16); }

vec3 aosSdWin31Raised(vec2 pix, vec2 b0, vec2 b1, vec3 face, float bevel) {
    if (!all(greaterThanEqual(pix, b0)) || !all(lessThan(pix, b1)))
        return vec3(-1.0);
    float dT = pix.y - b0.y;
    float dL = pix.x - b0.x;
    float dB = b1.y - pix.y;
    float dR = b1.x - pix.x;
    vec3 col = face;
    if (dT < bevel) col = mix(aosSdWin31Hi(), face, dT / bevel);
    else if (dL < bevel) col = mix(aosSdWin31Hi(), face, dL / bevel);
    else if (dB < bevel) col = mix(aosSdWin31Lo(), face, dB / bevel);
    else if (dR < bevel) col = mix(aosSdWin31Lo(), face, dR / bevel);
    return clamp(col, 0.0, 1.0);
}

vec3 aosSdWin31Sunken(vec2 pix, vec2 b0, vec2 b1, vec3 face, float bevel) {
    if (!all(greaterThanEqual(pix, b0)) || !all(lessThan(pix, b1)))
        return vec3(-1.0);
    float dT = pix.y - b0.y;
    float dL = pix.x - b0.x;
    float dB = b1.y - pix.y;
    float dR = b1.x - pix.x;
    vec3 col = face;
    if (dT < bevel) col = mix(aosSdWin31Lo(), face, dT / bevel);
    else if (dL < bevel) col = mix(aosSdWin31Lo(), face, dL / bevel);
    else if (dB < bevel) col = mix(aosSdWin31Hi(), face, dB / bevel);
    else if (dR < bevel) col = mix(aosSdWin31Hi(), face, dR / bevel);
    return clamp(col, 0.0, 1.0);
}

vec3 aosSdWin31Panel(vec2 pix, vec2 b0, vec2 b1, vec3 face, float bevel) {
    vec3 shadow = aosSdDropShadow(pix, b0, b1, 2.0, vec2(3.0, 4.0), vec3(0.0, 0.0, 0.0), 0.55);
    if (shadow.x >= 0.0) return shadow;
    return aosSdWin31Raised(pix, b0, b1, face, bevel);
}

vec3 aosSdTitleBarWin31(vec2 local, vec2 winSize, float titleH, bool focused) {
    if (local.y < 0.0 || local.y >= titleH) return vec3(-1.0);
    vec3 face = focused ? vec3(0.42, 0.44, 0.48) : vec3(0.46, 0.47, 0.51);
    return aosSdWin31Raised(local, vec2(0.0, 0.0), vec2(winSize.x, titleH), face, 2.0);
}

vec3 aosSdStatusBarWin31(vec2 local, vec2 winSize, float hudH) {
    float y0 = winSize.y - hudH;
    if (local.y < y0 || local.y >= winSize.y) return vec3(-1.0);
    return aosSdWin31Sunken(local, vec2(0.0, y0), vec2(winSize.x, winSize.y),
        vec3(0.08, 0.09, 0.11), 2.0);
}

vec3 aosSdWin31WindowShell(vec2 local, vec2 winSize, float titleH, float hudH, float scale) {
    vec2 b0 = vec2(0.0);
    vec2 b1 = winSize;
    vec3 shadow = aosSdDropShadow(local, b0, b1, 3.0, vec2(4.0, 6.0) * scale,
        vec3(0.0, 0.0, 0.0), 0.72);
    if (shadow.x >= 0.0) return shadow;
    vec3 frame = aosSdWin31Raised(local, b0, b1, vec3(0.38, 0.40, 0.44), 3.0 * scale);
    if (frame.x < 0.0) return vec3(-1.0);
    if (local.y >= titleH && local.y < winSize.y - hudH)
        return aosSdWin31Sunken(local, vec2(4.0, titleH + 2.0),
            vec2(winSize.x - 4.0, winSize.y - hudH - 2.0), vec3(0.28, 0.30, 0.34), 2.0);
    return frame;
}