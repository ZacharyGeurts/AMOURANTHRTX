#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Materials Library (C) 2025-2026 Zachary Robert Geurts
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include <glm/glm.hpp>
#include <array>
#include <cstdint>

// ────────────────────────────────────────────────
// Material Flags (bitfield – fast path decisions in shaders)
// ────────────────────────────────────────────────
namespace MaterialFlags {
    constexpr uint32_t TRANSMISSION     = 1u << 0;
    constexpr uint32_t SUBSURFACE       = 1u << 1;
    constexpr uint32_t CLEARCOAT        = 1u << 2;
    constexpr uint32_t SHEEN            = 1u << 3;
    constexpr uint32_t THIN_FILM        = 1u << 4;
    constexpr uint32_t ANISOTROPY       = 1u << 5;
    constexpr uint32_t EMISSIVE         = 1u << 6;
    constexpr uint32_t PROCEDURAL       = 1u << 7;
    constexpr uint32_t VOLUMETRIC_HINT  = 1u << 8;
    constexpr uint32_t DOUBLE_SIDED     = 1u << 9;
}

// ────────────────────────────────────────────────
// Single Material Layer (16-byte aligned for GPU)
// ────────────────────────────────────────────────
struct alignas(16) MaterialLayer {
    glm::vec4 baseColor             {1.0f, 1.0f, 1.0f, 1.0f};   // .a = opacity

    glm::vec4 emissive              {0.0f, 0.0f, 0.0f, 0.0f};   // .a = emission multiplier

    float     metallic              = 0.0f;
    float     roughness             = 0.5f;
    float     specular              = 0.5f;
    float     ior                   = 1.50f;

    float     transmission          = 0.0f;
    float     transmissionRoughness = 0.0f;
    float     thinFilm              = 0.0f;
    float     thinFilmThickness_nm  = 350.0f;

    float     subsurface            = 0.0f;
    glm::vec3 subsurfaceColor       {0.8f, 0.6f, 0.5f};
    float     subsurfaceRadiusScale = 1.0f;

    float     clearcoat             = 0.0f;
    float     clearcoatRoughness    = 0.03f;

    float     sheen                 = 0.0f;
    glm::vec3 sheenTint             {1.0f, 1.0f, 1.0f};

    float     anisotropy            = 0.0f;
    float     anisoRotation         = 0.0f;

    uint32_t  procType              = 0;                        // 0 = none, 1–N procedural types
    float     procScale             = 8.0f;
    float     procStrength          = 0.35f;
    float     procOffsetSeed        = 0.0f;

    uint32_t  flags                 = 0;
    uint32_t  padding[3]            = {0,0,0};
};

// ────────────────────────────────────────────────
// Full Material — supports up to 5 layered combinations
// (for near-infinite variety via blending)
// ────────────────────────────────────────────────
struct alignas(16) Material {
    std::array<MaterialLayer, 5> layers;
    uint32_t                     layerCount = 1;                // 1–5
    float                        layerBlendFactors[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t                     padding[2] = {0, 0};
};

// ────────────────────────────────────────────────
// Pre-defined base materials (use as starting points / layers)
// Indexed from 0 — expand freely
// ────────────────────────────────────────────────
namespace Materials {

// 0 – Perfect Mirror (chrome-like)
inline constexpr MaterialLayer Mirror {
    .baseColor = {1.0f, 1.0f, 1.0f, 1.0f},
    .metallic  = 1.0f,
    .roughness = 0.0f,
    .specular  = 0.8f
};

// 1 – Polished Gold
inline constexpr MaterialLayer PolishedGold {
    .baseColor = {1.00f, 0.78f, 0.34f, 1.0f},
    .metallic  = 1.0f,
    .roughness = 0.08f,
    .specular  = 0.6f
};

// 2 – Rough Gold (brushed)
inline constexpr MaterialLayer BrushedGold {
    .baseColor = {0.98f, 0.75f, 0.30f, 1.0f},
    .metallic  = 1.0f,
    .roughness = 0.45f,
    .specular  = 0.55f,
    .anisotropy = 0.6f
};

// 3 – Matte Black Plastic
inline constexpr MaterialLayer MatteBlackPlastic {
    .baseColor = {0.04f, 0.04f, 0.04f, 1.0f},
    .roughness = 0.92f,
    .specular  = 0.35f
};

// 4 – Clear Glass (perfect)
inline constexpr MaterialLayer ClearGlass {
    .baseColor              = {0.95f, 0.97f, 1.00f, 0.98f},
    .roughness              = 0.00f,
    .ior                    = 1.50f,
    .transmission           = 1.0f,
    .transmissionRoughness  = 0.0f
};

// 5 – Frosted Glass
inline constexpr MaterialLayer FrostedGlass {
    .baseColor              = {0.92f, 0.95f, 1.00f, 0.92f},
    .roughness              = 0.12f,
    .ior                    = 1.50f,
    .transmission           = 0.95f,
    .transmissionRoughness  = 0.18f
};

// 6 – Water (puddle / thin layer)
inline constexpr MaterialLayer Water {
    .baseColor              = {0.90f, 0.96f, 1.00f, 0.85f},
    .roughness              = 0.00f,
    .ior                    = 1.33f,
    .transmission           = 0.98f,
    .transmissionRoughness  = 0.02f
};

// 7 – Jade (translucent gem)
inline constexpr MaterialLayer Jade {
    .baseColor              = {0.20f, 0.90f, 0.50f, 1.0f},
    .roughness              = 0.12f,
    .ior                    = 1.52f,
    .subsurface             = 0.65f,
    .subsurfaceColor        = {0.10f, 0.80f, 0.40f},
    .subsurfaceRadiusScale  = 1.2f
};

// 8 – Skin (human-like subsurface)
inline constexpr MaterialLayer Skin {
    .baseColor              = {0.80f, 0.55f, 0.45f, 1.0f},
    .roughness              = 0.45f,
    .specular               = 0.28f,
    .subsurface             = 0.75f,
    .subsurfaceColor        = {0.90f, 0.50f, 0.40f},
    .subsurfaceRadiusScale  = 0.9f,
    .sheen                  = 0.15f
};

// 9 – Velvet (fabric)
inline constexpr MaterialLayer Velvet {
    .baseColor = {0.15f, 0.05f, 0.08f, 1.0f},
    .roughness = 0.85f,
    .sheen     = 0.65f,
    .sheenTint = {0.9f, 0.6f, 0.7f}
};

// 10 – Emissive Neon Pink
inline constexpr MaterialLayer NeonPink {
    .baseColor = {1.0f, 0.10f, 0.60f, 1.0f},
    .emissive  = {12.0f, 1.2f, 6.0f, 20.0f},
    .roughness = 0.40f
};

// 11 – Emissive White Light Bulb
inline constexpr MaterialLayer LightBulb {
    .baseColor = {1.0f, 0.98f, 0.90f, 1.0f},
    .emissive  = {8.0f, 7.5f, 6.0f, 15.0f},
    .roughness = 0.70f
};

// 12 – Iridescent Soap Bubble (thin-film)
inline constexpr MaterialLayer SoapBubble {
    .baseColor              = {1.0f, 1.0f, 1.0f, 0.40f},
    .roughness              = 0.00f,
    .ior                    = 1.33f,
    .transmission           = 0.95f,
    .thinFilm               = 1.0f,
    .thinFilmThickness_nm   = 480.0f
};

// 13 – Car Paint (clearcoat + metallic)
inline constexpr MaterialLayer CarPaintRed {
    .baseColor              = {0.90f, 0.10f, 0.12f, 1.0f},
    .metallic               = 0.9f,
    .roughness              = 0.04f,
    .clearcoat              = 1.0f,
    .clearcoatRoughness     = 0.02f
};

// 14 – Rough Concrete
inline constexpr MaterialLayer RoughConcrete {
    .baseColor = {0.45f, 0.45f, 0.42f, 1.0f},
    .roughness = 0.88f,
    .specular  = 0.22f
};

// 15 – Worn Leather
inline constexpr MaterialLayer WornLeather {
    .baseColor = {0.35f, 0.18f, 0.10f, 1.0f},
    .roughness = 0.65f,
    .specular  = 0.30f,
    .sheen     = 0.12f
};

} // namespace Materials

// ────────────────────────────────────────────────
// Example combined / layered materials (copy & modify)
// ────────────────────────────────────────────────

// Example: Gold with thin-film iridescence overlay
inline constexpr Material IridescentGold = []() {
    Material m{};
    m.layers[0] = Materials::PolishedGold;
    m.layers[1] = Materials::SoapBubble;
    m.layerBlendFactors[0] = 0.85f;
    m.layerBlendFactors[1] = 0.15f;
    m.layerCount = 2;
    return m;
}();

// Example: Frosted Jade Glass
inline constexpr Material FrostedJade = []() {
    Material m{};
    m.layers[0] = Materials::Jade;
    m.layers[1] = Materials::FrostedGlass;
    m.layerBlendFactors[0] = 0.70f;
    m.layerBlendFactors[1] = 0.30f;
    m.layerCount = 2;
    return m;
}();

// Example: Neon-lit skin (cyberpunk)
inline constexpr Material CyberSkin = []() {
    Material m{};
    m.layers[0] = Materials::Skin;
    m.layers[1] = Materials::NeonPink;
    m.layerBlendFactors[0] = 0.80f;
    m.layerBlendFactors[1] = 0.20f;
    m.layerCount = 2;
    return m;
}();

// Add more combinations here as needed...

// ────────────────────────────────────────────────
// Usage in RayCanvas / elsewhere:
//   std::vector<Material> sceneMaterials = { Materials::PolishedGold, IridescentGold, FrostedJade, ... };
//   Upload to GPU buffer → assign materialIndex per primitive
// ────────────────────────────────────────────────