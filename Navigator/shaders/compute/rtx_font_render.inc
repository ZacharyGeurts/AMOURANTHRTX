// RTX Terminal — 256-glyph SDF font (crisp raised strokes, dual-polarity fringe)
#include "rtx_font_meta.inc"
#include "rtx_font.inc"

#ifndef RTX_FONT_SDF_TEX
#define RTX_FONT_SDF_TEX rtxFontSdf
#endif

const vec3 FONT_INK_LIGHT  = vec3(0.97, 0.98, 1.00);
const vec3 FONT_INK_DARK   = vec3(0.07, 0.08, 0.10);
const vec3 FONT_FRINGE_DARK  = vec3(0.02, 0.03, 0.05);
const vec3 FONT_FRINGE_LIGHT = vec3(0.94, 0.96, 0.99);

bool fontBitmapOn(uint c, uint x, uint y) {
    if (c < 32u || c > 126u || x >= 8u || y >= 16u) return false;
    uint gi = (c - 32u) * 4u;
    uint word = FONT_ATLAS[gi + (y >> 2u)];
    uint row = (word >> ((y & 3u) * 8u)) & 0xFFu;
    return ((row >> (7u - x)) & 1u) != 0u;
}

float fontBitmapInk(uint c, vec2 px) {
    uint ix = uint(floor(px.x));
    uint iy = uint(floor(px.y));
    float fx = fract(px.x);
    float fy = fract(px.y);
    float ink = 0.0;
    for (int dy = 0; dy <= 1; ++dy) {
        for (int dx = 0; dx <= 1; ++dx) {
            if (fontBitmapOn(c, ix + uint(dx), iy + uint(dy))) {
                float wx = 1.0 - abs(fx - float(dx));
                float wy = 1.0 - abs(fy - float(dy));
                ink = max(ink, wx * wy);
            }
        }
    }
    return clamp(ink, 0.0, 1.0);
}

vec2 fontSdfCellOrigin(uint c) {
    c &= 0xFFu;
    uint col = c & 15u;
    uint row = c >> 4u;
    return vec2(float(col) * float(FONT_SDF_CELL_W), float(row) * float(FONT_SDF_CELL_H));
}

float fontSdfRaw(uint c, vec2 localPx) {
    ivec2 dim = imageSize(RTX_FONT_SDF_TEX);
    if (dim.x <= 0 || dim.y <= 0) return 0.0;
    vec2 atlasPx = fontSdfCellOrigin(c) + localPx;
    if (atlasPx.x < 0.0 || atlasPx.y < 0.0
            || atlasPx.x >= float(dim.x) || atlasPx.y >= float(dim.y))
        return 0.0;
    return imageLoad(RTX_FONT_SDF_TEX, ivec2(atlasPx)).r / 255.0;
}

// px: glyph-local coordinates 0..FONT_W x 0..FONT_H (sub-pixel OK)
float fontSdfInk(uint c, vec2 px) {
    ivec2 dim = imageSize(RTX_FONT_SDF_TEX);
    float ink = 0.0;
    if (dim.x > 0 && dim.y > 0) {
        vec2 local = px + vec2(float(FONT_SDF_PAD_X), float(FONT_SDF_PAD_Y));
        float d = fontSdfRaw(c, local);
        float edge = 0.30 / FONT_SDF_SPREAD;
        ink = smoothstep(0.5 - edge, 0.5 + edge * 0.55, d);
    }
    if (ink < 0.06)
        ink = max(ink, fontBitmapInk(c, px));
    return ink;
}

// Crisp halo ring outside glyph edge — SDF fringe (not mushy bitmap AA).
float fontSdfFringe(uint c, vec2 px) {
    ivec2 dim = imageSize(RTX_FONT_SDF_TEX);
    if (dim.x <= 0 || dim.y <= 0) return 0.0;
    vec2 local = px + vec2(float(FONT_SDF_PAD_X), float(FONT_SDF_PAD_Y));
    float d = fontSdfRaw(c, local);
    float haloW = 2.4 / FONT_SDF_SPREAD;
    float outer = smoothstep(0.5 - haloW * 2.0, 0.5 - haloW * 0.25, d);
    float core = smoothstep(0.5 - 0.06 / FONT_SDF_SPREAD, 0.5 + 0.05 / FONT_SDF_SPREAD, d);
    return clamp(outer * (1.0 - core), 0.0, 1.0);
}

bool fontOn(uint c, uint x, uint y) {
    return fontSdfInk(c, vec2(float(x) + 0.5, float(y) + 0.5)) > 0.48;
}

float fontInkAA(uint c, vec2 px) {
    return fontSdfInk(c, px);
}

// Raised Win31-style facet indentations + top-left light catch on SDF stroke interior.
vec2 fontSdfGeomShade(uint c, vec2 px) {
    ivec2 dim = imageSize(RTX_FONT_SDF_TEX);
    if (dim.x <= 0 || dim.y <= 0) return vec2(0.0);
    float ink = fontSdfInk(c, px);
    if (ink < 0.03) return vec2(0.0);

    vec2 local = px + vec2(float(FONT_SDF_PAD_X), float(FONT_SDF_PAD_Y));
    float step = 0.65;
    float dx = fontSdfRaw(c, local + vec2(step, 0.0)) - fontSdfRaw(c, local - vec2(step, 0.0));
    float dy = fontSdfRaw(c, local + vec2(0.0, step)) - fontSdfRaw(c, local - vec2(0.0, step));
    vec2 grad = vec2(dx, dy);
    float glen = length(grad);
    vec2 n = (glen > 1e-4) ? (grad / glen) : vec2(0.0, 1.0);
    vec2 light = normalize(vec2(-0.58, -0.72));

    float depth = max(0.0, (fontSdfRaw(c, local) - 0.5) * FONT_SDF_SPREAD);
    float bevelHi = smoothstep(0.0, 1.8, depth) * (1.0 - smoothstep(1.8, 4.2, depth));
    float facet = fract(depth * 0.92);
    float groove = smoothstep(0.08, 0.22, facet) * (1.0 - smoothstep(0.26, 0.42, facet));
    groove += smoothstep(0.58, 0.72, facet) * (1.0 - smoothstep(0.76, 0.90, facet));
    groove = clamp(groove * 0.88, 0.0, 0.90);

    float facing = dot(n, light);
    float hi = clamp(max(facing, 0.0) * 0.78 + bevelHi * 0.62 + groove * 0.28, 0.0, 0.95);
    float indent = clamp(groove * 0.78 + max(-facing, 0.0) * 0.42, 0.0, 0.92);
    return vec2(hi, indent) * ink;
}

// vec4(ink, highlight, indent, fringe)
vec4 fontGlyphPack(uint ch, vec2 uv, float x0, float rowY, vec2 cell) {
    vec2 c = (uv - vec2(x0, rowY)) / cell;
    if (any(lessThan(c, vec2(0.0))) || any(greaterThanEqual(c, vec2(1.0))))
        return vec4(0.0);
    vec2 px = c * vec2(float(FONT_SDF_GLYPH_W), float(FONT_SDF_GLYPH_H));
    float ink = fontSdfInk(ch, px);
    vec2 geom = fontSdfGeomShade(ch, px);
    float fringe = fontSdfFringe(ch, px);
    return vec4(ink, geom.x, geom.y, fringe);
}

vec3 fontGlyphGeom(uint ch, vec2 uv, float x0, float rowY, vec2 cell) {
    vec4 pack = fontGlyphPack(ch, uv, x0, rowY, cell);
    return pack.xyz;
}

float fontGlyphInk(uint ch, vec2 uv, float x0, float rowY, vec2 cell) {
    return fontGlyphPack(ch, uv, x0, rowY, cell).x;
}

float fontGlyphFringe(uint ch, vec2 uv, float x0, float rowY, vec2 cell) {
    return fontGlyphPack(ch, uv, x0, rowY, cell).w;
}

// lightOnDark: true = white letters + black fringe; false = black letters + white fringe
vec3 mixHudInkPolar(vec3 base, vec4 inkPack, float border, vec3 fg, bool lightOnDark) {
    float ink = inkPack.x;
    float fringe = max(border, inkPack.w);
    vec3 fringeCol = lightOnDark ? FONT_FRINGE_DARK : FONT_FRINGE_LIGHT;
    vec3 col = mix(base, fringeCol, clamp(fringe, 0.0, 1.0) * 0.82);
    vec3 lit = lightOnDark ? fg * 8.5 : fg * 5.5;
    lit = mix(lit, vec3(15.0), clamp(inkPack.y, 0.0, 1.0) * 0.88);
    lit = mix(lit, vec3(3.0, 3.2, 3.6), clamp(inkPack.z, 0.0, 1.0) * 0.62);
    return mix(col, lit, clamp(ink, 0.0, 1.0) * 0.98);
}

vec3 mixHudInkLight(vec3 base, vec4 inkPack, float border) {
    return mixHudInkPolar(base, inkPack, border, FONT_INK_LIGHT, true);
}

vec3 mixHudInkDark(vec3 base, vec4 inkPack, float border) {
    return mixHudInkPolar(base, inkPack, border, FONT_INK_DARK, false);
}

vec3 mixHudInkGeom(vec3 base, vec3 inkGeom, float border, vec3 fg) {
    vec4 pack = vec4(inkGeom, 0.0);
    return mixHudInkPolar(base, pack, border, fg, true);
}

vec3 mixHudInk(vec3 base, float ink, float border, vec3 fg) {
    return mixHudInkGeom(base, vec3(ink, 0.0, 0.0), border, fg);
}

vec3 mixHudInk(vec3 base, vec3 inkGeom, float border, vec3 fg) {
    return mixHudInkGeom(base, inkGeom, border, fg);
}