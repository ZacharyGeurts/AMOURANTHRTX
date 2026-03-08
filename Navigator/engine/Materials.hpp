#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cstdint>

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

struct alignas(16) MaterialLayer {
    glm::vec4 baseColor             {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissive              {0.0f, 0.0f, 0.0f, 0.0f};

    float     metallic              = 0.0f;
    float     roughness             = 0.5f;
    float     specular              = 0.5f;
    float     specularTint          = 0.0f;

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

    uint32_t  procType              = 0;
    float     procScale             = 8.0f;
    float     procStrength          = 0.35f;
    float     procOffsetSeed        = 0.0f;

    uint32_t  flags                 = 0;
    uint32_t  padding[2]            = {0,0};
};

struct alignas(16) Material {
    std::array<MaterialLayer, 5> layers;
    uint32_t                     layerCount = 1;
    float                        layerBlendFactors[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t                     padding[2] = {0, 0};
};

namespace Materials {

inline constexpr MaterialLayer Chrome {
    .baseColor             = {1.0f, 1.0f, 1.0f, 1.0f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 1.0f,
    .roughness             = 0.0f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer PolishedGold {
    .baseColor             = {1.00f, 0.78f, 0.34f, 1.0f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 1.0f,
    .roughness             = 0.05f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer BrushedMetal {
    .baseColor             = {0.95f, 0.95f, 0.95f, 1.0f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 1.0f,
    .roughness             = 0.4f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.8f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer MattePlastic {
    .baseColor             = {0.05f, 0.05f, 0.05f, 1.0f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 0.0f,
    .roughness             = 0.9f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer ClearCoatedPlastic {
    .baseColor             = {0.8f, 0.1f, 0.1f, 1.0f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 0.0f,
    .roughness             = 0.3f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 1.0f,
    .clearcoatRoughness    = 0.01f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer Glass {
    .baseColor             = {0.96f, 0.97f, 1.00f, 0.98f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 0.0f,
    .roughness             = 0.0f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 1.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer FrostedGlass {
    .baseColor             = {0.94f, 0.96f, 1.00f, 0.92f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 0.0f,
    .roughness             = 0.15f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.95f,
    .transmissionRoughness = 0.2f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer Water {
    .baseColor             = {0.92f, 0.97f, 1.00f, 0.85f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 0.0f,
    .roughness             = 0.0f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.33f,
    .transmission          = 1.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer Skin {
    .baseColor             = {0.82f, 0.55f, 0.48f, 1.0f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 0.0f,
    .roughness             = 0.4f,
    .specular              = 0.3f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.8f,
    .subsurfaceColor       = {0.92f, 0.48f, 0.38f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.2f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer VelvetRed {
    .baseColor             = {0.25f, 0.05f, 0.08f, 1.0f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 0.0f,
    .roughness             = 0.9f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.7f,
    .sheenTint             = {0.92f, 0.55f, 0.6f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer NeonCyan {
    .baseColor             = {0.05f, 1.0f, 0.95f, 1.0f},
    .emissive              = {5.0f, 25.0f, 22.0f, 18.0f},
    .metallic              = 0.0f,
    .roughness             = 0.35f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.50f,
    .transmission          = 0.0f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 0.0f,
    .thinFilmThickness_nm  = 350.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

inline constexpr MaterialLayer IridescentFilm {
    .baseColor             = {1.0f, 1.0f, 1.0f, 0.35f},
    .emissive              = {0.0f, 0.0f, 0.0f, 0.0f},
    .metallic              = 0.0f,
    .roughness             = 0.0f,
    .specular              = 0.5f,
    .specularTint          = 0.0f,
    .ior                   = 1.33f,
    .transmission          = 0.98f,
    .transmissionRoughness = 0.0f,
    .thinFilm              = 1.0f,
    .thinFilmThickness_nm  = 420.0f,
    .subsurface            = 0.0f,
    .subsurfaceColor       = {0.8f, 0.6f, 0.5f},
    .subsurfaceRadiusScale = 1.0f,
    .clearcoat             = 0.0f,
    .clearcoatRoughness    = 0.03f,
    .sheen                 = 0.0f,
    .sheenTint             = {1.0f, 1.0f, 1.0f},
    .anisotropy            = 0.0f,
    .anisoRotation         = 0.0f,
    .procType              = 0,
    .procScale             = 8.0f,
    .procStrength          = 0.35f,
    .procOffsetSeed        = 0.0f,
    .flags                 = 0
};

} // namespace Materials

// Layered examples (top-level, not inside Materials namespace)
inline constexpr Material CarPaintBlue = []() {
    Material m{};
    m.layers[0].baseColor = {0.05f, 0.25f, 0.95f, 1.0f};
    m.layers[0].metallic  = 0.85f;
    m.layers[0].roughness = 0.08f;
    m.layers[1] = Materials::IridescentFilm;
    m.layers[1].thinFilmThickness_nm = 520.0f;
    m.layerBlendFactors[0] = 0.92f;
    m.layerBlendFactors[1] = 0.08f;
    m.layerCount = 2;
    return m;
}();

inline constexpr Material WornLeatherBrown = []() {
    Material m{};
    m.layers[0].baseColor = {0.38f, 0.22f, 0.12f, 1.0f};
    m.layers[0].roughness = 0.75f;
    m.layers[0].sheen     = 0.25f;
    m.layerCount = 1;
    return m;
}();

inline constexpr Material CyberPunkSkin = []() {
    Material m{};
    m.layers[0] = Materials::Skin;
    m.layers[1] = Materials::NeonCyan;
    m.layerBlendFactors[0] = 0.75f;
    m.layerBlendFactors[1] = 0.25f;
    m.layerCount = 2;
    return m;
}();