#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cstdint>

namespace MaterialFlags {
    constexpr uint32_t TRANSMISSION          = 1u << 0;   // dielectric refraction / thin-walled
    constexpr uint32_t SUBSURFACE            = 1u << 1;   // Burley / EON diffusion
    constexpr uint32_t CLEARCOAT             = 1u << 2;   // top gloss (decoupled IOR)
    constexpr uint32_t FUZZ                  = 1u << 3;   // OpenPBR microflake / evolved sheen
    constexpr uint32_t THIN_FILM             = 1u << 4;   // iridescence
    constexpr uint32_t ANISOTROPY            = 1u << 5;
    constexpr uint32_t EMISSIVE              = 1u << 6;
    constexpr uint32_t PROCEDURAL            = 1u << 7;   // noise / neural-baked / LTC hint
    constexpr uint32_t VOLUMETRIC_HINT       = 1u << 8;   // SSS volume / path-traced
    constexpr uint32_t DOUBLE_SIDED          = 1u << 9;
    constexpr uint32_t SPECULAR_HAZE         = 1u << 10;  // OpenPBR 1.2 smudged reflection
    constexpr uint32_t RETROREFLECTION       = 1u << 11;  // OpenPBR 1.2 tail / moon-like bounce
    constexpr uint32_t THIN_WALLED           = 1u << 12;  // special-case single-sided transmissive
}

struct alignas(16) MaterialLayer {
    glm::vec4 baseColor                {1.0f, 1.0f, 1.0f, 1.0f};  // albedo / tint
    glm::vec4 emissiveColor            {0.0f, 0.0f, 0.0f, 0.0f};

    float metallic                     = 0.0f;           // 0=dielectric base, 1=metal
    float roughness                    = 0.5f;           // microfacet roughness (base)
    float specular                     = 0.5f;           // F0 scale (~0.04 default)
    float specularTint                 = 0.0f;

    float ior                          = 1.50f;          // base dielectric IOR
    float transmission                 = 0.0f;           // refractive strength
    float transmissionRoughness        = 0.0f;

    float thinFilm                     = 0.0f;           // iridescence strength
    float thinFilmThickness_nm         = 350.0f;

    float subsurface                   = 0.0f;           // Burley/EON strength
    glm::vec3 subsurfaceColor          {0.8f, 0.6f, 0.5f};
    float subsurfaceRadiusScale        = 1.2f;           // modern: higher for softer cartoon SSS

    float coat                         = 0.0f;           // OpenPBR coat layer strength
    float coatRoughness                = 0.03f;
    float coatIOR                      = 1.50f;          // decoupled coat IOR

    float fuzz                         = 0.0f;           // OpenPBR fuzz (microflake/sheen evo)
    glm::vec3 fuzzTint                 {1.0f, 1.0f, 1.0f};
    float fuzzRoughness                = 0.5f;           // fuzz lobe spread

    float anisotropy                   = 0.0f;           // -1..1
    float anisoRotation                = 0.0f;

    // OpenPBR 1.2 WIP / SIGGRAPH 2025 extensions
    float specularHaze                 = 0.0f;           // smudged/hazy specular strength
    float hazeSpread                   = 0.5f;           // haze lobe width
    float retroReflection              = 0.0f;           // retro tail boost (moon/road sign)
    float emissionWeight               = 1.0f;           // emission scaling (nits-aware)

    uint32_t procType                  = 0;              // 0=none, 1=Perlin, 2=neural-bake, 3=LTC/CLTC hint
    float procScale                    = 8.0f;
    float procStrength                 = 0.35f;
    float procOffsetSeed               = 0.0f;

    uint32_t flags                     = 0;
    uint32_t padding[2]                = {0, 0};
};

struct alignas(16) Material {
    std::array<MaterialLayer, 5> layers;
    uint32_t                     layerCount              = 1;
    float                        layerBlendFactors[5]    = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t                     padding[2]              = {0, 0};
};

namespace Materials {

// ── Core OpenPBR 1.1 / 1.2 production presets ─────────────────────────────

inline constexpr MaterialLayer OpenPBR_DielectricBase {
    .baseColor   = {0.96f, 0.97f, 1.00f, 1.0f},
    .metallic    = 0.0f,
    .roughness   = 0.12f,
    .specular    = 0.5f,
    .ior         = 1.50f,
    .flags       = 0
};

inline constexpr MaterialLayer OpenPBR_Metal {
    .baseColor   = {1.00f, 0.78f, 0.34f, 1.0f},  // gold example
    .metallic    = 1.0f,
    .roughness   = 0.08f,
    .specular    = 0.5f,
    .flags       = 0
};

// ── 2026 Disney/Pixar stylized tuned presets ──────────────────────────────

inline constexpr MaterialLayer DisneyCartoonSkin {
    .baseColor             = {0.90f, 0.64f, 0.56f, 1.0f},
    .metallic              = 0.0f,
    .roughness             = 0.42f,
    .specular              = 0.48f,
    .specularTint          = 0.12f,
    .ior                   = 1.40f,
    .subsurface            = 0.86f,
    .subsurfaceColor       = {0.94f, 0.54f, 0.44f},
    .subsurfaceRadiusScale = 1.25f,
    .fuzz                  = 0.22f,
    .fuzzTint              = {1.0f, 0.96f, 0.93f},
    .fuzzRoughness         = 0.65f,
    .retroReflection       = 0.10f,
    .flags                 = MaterialFlags::SUBSURFACE | MaterialFlags::FUZZ | MaterialFlags::RETROREFLECTION
};

inline constexpr MaterialLayer DisneyVelvetFabric {
    .baseColor             = {0.18f, 0.04f, 0.09f, 1.0f},
    .metallic              = 0.0f,
    .roughness             = 0.92f,
    .specular              = 0.38f,
    .ior                   = 1.42f,
    .subsurface            = 0.12f,
    .fuzz                  = 0.84f,
    .fuzzTint              = {0.96f, 0.68f, 0.78f},
    .fuzzRoughness         = 0.70f,
    .flags                 = MaterialFlags::FUZZ | MaterialFlags::SUBSURFACE
};

inline constexpr MaterialLayer PixarToyPlastic {
    .baseColor             = {0.92f, 0.26f, 0.16f, 1.0f},
    .metallic              = 0.0f,
    .roughness             = 0.20f,
    .specular              = 0.52f,
    .ior                   = 1.48f,
    .coat                  = 0.90f,
    .coatRoughness         = 0.035f,
    .coatIOR               = 1.48f,
    .specularHaze          = 0.12f,
    .flags                 = MaterialFlags::CLEARCOAT | MaterialFlags::SPECULAR_HAZE
};

inline constexpr MaterialLayer OpenPBR_GlossyPaint {
    .baseColor             = {0.10f, 0.42f, 0.90f, 1.0f},
    .metallic              = 0.90f,
    .roughness             = 0.09f,
    .specular              = 0.55f,
    .ior                   = 1.52f,
    .coat                  = 1.0f,
    .coatRoughness         = 0.02f,
    .coatIOR               = 1.52f,
    .specularHaze          = 0.08f,
    .flags                 = MaterialFlags::CLEARCOAT | MaterialFlags::SPECULAR_HAZE
};

// ── 2026+ Frosted / Translucent / Etched Glass preset ─────────────────────

inline constexpr MaterialLayer OpenPBR_FrostedGlass {
    .baseColor             = {0.97f, 0.98f, 0.99f, 1.0f},  // near neutral white
    .metallic              = 0.0f,
    .roughness             = 0.0f,                         // base reflection remains relatively sharp
    .specular              = 0.52f,
    .ior                   = 1.48f,
    .transmission          = 0.94f,
    .transmissionRoughness = 0.45f,                        // primary frosted diffusion control (0.35–0.65 typical)
    .subsurface            = 0.18f,                        // subtle milky volume look
    .subsurfaceColor       = {0.98f, 0.96f, 0.94f},
    .subsurfaceRadiusScale = 0.9f,
    .specularHaze          = 0.22f,                        // extra softening of specular highlights
    .flags                 = MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE
};

// ── Example layered / combined materials ──────────────────────────────────

inline constexpr Material DisneyPrincessGown = []() {
    Material m{};
    m.layers[0] = DisneyVelvetFabric;                      // main velvet body
    m.layers[1] = PixarToyPlastic;                         // glossy satin trim / accents
    m.layers[1].baseColor    = {0.98f, 0.94f, 0.48f, 1.0f};
    m.layers[1].specularHaze = 0.18f;
    m.layerBlendFactors[0]   = 0.78f;
    m.layerBlendFactors[1]   = 0.22f;
    m.layerCount = 2;
    return m;
}();

inline constexpr Material CartoonCharacterSkin = []() {
    Material m{};
    m.layers[0]                   = DisneyCartoonSkin;
    m.layers[0].retroReflection   = 0.12f;                 // stronger rim lighting pop
    m.layers[0].fuzz              = 0.24f;
    m.layerCount = 1;
    return m;
}();

inline constexpr Material ShinyRetroRobot = []() {
    Material m{};
    m.layers[0]                   = OpenPBR_GlossyPaint;
    m.layers[0].baseColor         = {0.95f, 0.20f, 0.10f, 1.0f}; // red candy paint
    m.layers[1]                   = OpenPBR_Metal;
    m.layers[1].baseColor         = {0.92f, 0.96f, 1.00f, 1.0f}; // bright chrome
    m.layers[1].specularHaze      = 0.15f;
    m.layers[1].retroReflection   = 0.20f;
    m.layerBlendFactors[0]        = 0.72f;
    m.layerBlendFactors[1]        = 0.28f;
    m.layerCount = 2;
    return m;
}();

inline constexpr Material FrostedGlassObject = []() {
    Material m{};
    m.layers[0]    = OpenPBR_FrostedGlass;
    m.layerCount   = 1;
    return m;
}();

// ── Additional 2026 production presets (now implemented) ───────────────────

inline constexpr MaterialLayer OpenPBR_Chrome {
    .baseColor             = {0.95f, 0.95f, 0.95f, 1.0f},
    .metallic              = 1.0f,
    .roughness             = 0.02f,
    .specular              = 0.6f,
    .ior                   = 1.0f,   // metals usually ignore IOR
    .flags                 = 0
};

inline constexpr MaterialLayer OpenPBR_PolishedGold {
    .baseColor             = {1.00f, 0.78f, 0.34f, 1.0f},
    .metallic              = 1.0f,
    .roughness             = 0.04f,
    .specular              = 0.55f,
    .flags                 = 0
};

inline constexpr MaterialLayer OpenPBR_BrushedMetal {
    .baseColor             = {0.85f, 0.85f, 0.88f, 1.0f},
    .metallic              = 1.0f,
    .roughness             = 0.35f,
    .anisotropy            = 0.75f,
    .anisoRotation         = 0.0f,
    .flags                 = MaterialFlags::ANISOTROPY
};

inline constexpr MaterialLayer OpenPBR_ClearGlass {
    .baseColor             = {0.96f, 0.97f, 0.99f, 1.0f},
    .metallic              = 0.0f,
    .roughness             = 0.0f,
    .specular              = 0.5f,
    .ior                   = 1.50f,
    .transmission          = 1.0f,
    .transmissionRoughness = 0.0f,
    .flags                 = MaterialFlags::TRANSMISSION
};

inline constexpr MaterialLayer OpenPBR_SkinBase {
    .baseColor             = {0.88f, 0.62f, 0.54f, 1.0f},
    .metallic              = 0.0f,
    .roughness             = 0.38f,
    .specular              = 0.48f,
    .specularTint          = 0.15f,
    .ior                   = 1.38f,
    .subsurface            = 0.78f,
    .subsurfaceColor       = {0.92f, 0.58f, 0.48f},
    .subsurfaceRadiusScale = 1.4f,
    .fuzz                  = 0.18f,
    .fuzzTint              = {1.0f, 0.92f, 0.88f},
    .fuzzRoughness         = 0.60f,
    .flags                 = MaterialFlags::SUBSURFACE | MaterialFlags::FUZZ
};

inline constexpr MaterialLayer OpenPBR_NeonCyan {
    .baseColor             = {0.0f, 0.0f, 0.0f, 1.0f},
    .emissiveColor         = {0.1f, 1.0f, 1.0f, 8.0f},
    .metallic              = 0.0f,
    .roughness             = 0.9f,
    .flags                 = MaterialFlags::EMISSIVE
};

} // namespace Materials