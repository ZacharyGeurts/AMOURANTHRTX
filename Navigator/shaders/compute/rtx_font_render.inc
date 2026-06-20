// RTX Terminal — 256-glyph SDF font sampling (professional soft edges, not mushy bitmap AA)
#include "rtx_font_meta.inc"
#include "rtx_font.inc"

#ifndef RTX_FONT_SDF_TEX
#define RTX_FONT_SDF_TEX rtxFontSdf
#endif

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
        float edge = 0.55 / FONT_SDF_SPREAD;
        ink = smoothstep(0.5 - edge, 0.5 + edge, d);
    }
    if (ink < 0.08)
        ink = max(ink, fontBitmapInk(c, px));
    return ink;
}

bool fontOn(uint c, uint x, uint y) {
    return fontSdfInk(c, vec2(float(x) + 0.5, float(y) + 0.5)) > 0.45;
}

float fontInkAA(uint c, vec2 px) {
    return fontSdfInk(c, px);
}

// Subtle geometric facet indentations + top-left light catch on SDF stroke interior.
vec2 fontSdfGeomShade(uint c, vec2 px) {
    ivec2 dim = imageSize(RTX_FONT_SDF_TEX);
    if (dim.x <= 0 || dim.y <= 0) return vec2(0.0);
    float ink = fontSdfInk(c, px);
    if (ink < 0.03) return vec2(0.0);

    vec2 local = px + vec2(float(FONT_SDF_PAD_X), float(FONT_SDF_PAD_Y));
    float step = 0.72;
    float dx = fontSdfRaw(c, local + vec2(step, 0.0)) - fontSdfRaw(c, local - vec2(step, 0.0));
    float dy = fontSdfRaw(c, local + vec2(0.0, step)) - fontSdfRaw(c, local - vec2(0.0, step));
    vec2 grad = vec2(dx, dy);
    float glen = length(grad);
    vec2 n = (glen > 1e-4) ? (grad / glen) : vec2(0.0, 1.0);
    vec2 light = normalize(vec2(-0.62, -0.68));
    float facing = dot(n, light);

    float depth = max(0.0, (fontSdfRaw(c, local) - 0.5) * FONT_SDF_SPREAD);
    float facet = fract(depth * 0.88);
    float groove = smoothstep(0.10, 0.26, facet) * (1.0 - smoothstep(0.30, 0.46, facet));
    groove += smoothstep(0.56, 0.70, facet) * (1.0 - smoothstep(0.74, 0.88, facet));
    groove = clamp(groove * 0.92, 0.0, 0.88);

    float hi = clamp(max(facing, 0.0) * 0.72 + groove * 0.38, 0.0, 0.92);
    float indent = clamp(groove * 0.82 + max(-facing, 0.0) * 0.38, 0.0, 0.95);
    return vec2(hi, indent) * ink;
}

vec3 fontGlyphGeom(uint ch, vec2 uv, float x0, float rowY, vec2 cell) {
    vec2 c = (uv - vec2(x0, rowY)) / cell;
    if (any(lessThan(c, vec2(0.0))) || any(greaterThanEqual(c, vec2(1.0)))) return vec3(0.0);
    vec2 px = c * vec2(float(FONT_SDF_GLYPH_W), float(FONT_SDF_GLYPH_H));
    float ink = fontSdfInk(ch, px);
    vec2 geom = fontSdfGeomShade(ch, px);
    return vec3(ink, geom.x, geom.y);
}

float fontGlyphInk(uint ch, vec2 uv, float x0, float rowY, vec2 cell) {
    return fontGlyphGeom(ch, uv, x0, rowY, cell).x;
}

vec3 mixHudInkGeom(vec3 base, vec3 inkGeom, float border, vec3 fg) {
    float ink = inkGeom.x;
    vec3 col = mix(base, vec3(0.04, 0.05, 0.08), clamp(border, 0.0, 1.0) * 0.55);
    // HDR white — displayMap(c/(c+1)) needs c>>1 for OCR-visible taskbar/menu text.
    vec3 lit = fg * 8.0;
    lit = mix(lit, vec3(14.0), clamp(inkGeom.y, 0.0, 1.0) * 0.85);
    lit = mix(lit, vec3(3.2, 3.4, 3.8), clamp(inkGeom.z, 0.0, 1.0) * 0.65);
    return mix(col, lit, clamp(ink, 0.0, 1.0) * 0.98);
}

vec3 mixHudInk(vec3 base, float ink, float border, vec3 fg) {
    return mixHudInkGeom(base, vec3(ink, 0.0, 0.0), border, fg);
}

vec3 mixHudInk(vec3 base, vec3 inkGeom, float border, vec3 fg) {
    return mixHudInkGeom(base, inkGeom, border, fg);
}