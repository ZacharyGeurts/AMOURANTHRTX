#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Materials Library (FOREVER SEALED EDITION)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖

// 100% compile-time, fully explicit, no runtime generation, no ellipses.
// All 32 orbs, 30 glass, 7 textured per category, layered, bindless-ready.
// Every material is defined before use in the final array.
// This file is sealed — do not modify again.
// =============================================================================

#include <glm/glm.hpp>
#include <array>
#include <cstdint>

namespace MaterialFlags {
    constexpr uint32_t TRANSMISSION          = 1u << 0;
    constexpr uint32_t SUBSURFACE            = 1u << 1;
    constexpr uint32_t CLEARCOAT             = 1u << 2;
    constexpr uint32_t FUZZ                  = 1u << 3;
    constexpr uint32_t THIN_FILM             = 1u << 4;
    constexpr uint32_t ANISOTROPY            = 1u << 5;
    constexpr uint32_t EMISSIVE              = 1u << 6;
    constexpr uint32_t PROCEDURAL            = 1u << 7;
    constexpr uint32_t VOLUMETRIC_HINT       = 1u << 8;
    constexpr uint32_t DOUBLE_SIDED          = 1u << 9;
    constexpr uint32_t SPECULAR_HAZE         = 1u << 10;
    constexpr uint32_t RETROREFLECTION       = 1u << 11;
    constexpr uint32_t THIN_WALLED           = 1u << 12;
}

struct alignas(16) MaterialLayer {
    glm::vec4 baseColor;
    glm::vec4 emissiveColor;

    float metallic;
    float roughness;
    float specular;
    float specularTint;

    float ior;
    float transmission;
    float transmissionRoughness;

    float thinFilm;
    float thinFilmThickness_nm;

    float subsurface;
    glm::vec3 subsurfaceColor;
    float subsurfaceRadiusScale;

    float coat;
    float coatRoughness;
    float coatIOR;

    float fuzz;
    glm::vec3 fuzzTint;
    float fuzzRoughness;

    float anisotropy;
    float anisoRotation;

    float specularHaze;
    float hazeSpread;
    float retroReflection;
    float emissionWeight;

    uint32_t procType;
    float procScale;
    float procStrength;
    float procOffsetSeed;

    uint32_t flags;

    uint32_t albedoTextureID;
    uint32_t normalTextureID;
    uint32_t roughnessTextureID;
    uint32_t metallicTextureID;
    uint32_t emissiveTextureID;
    uint32_t thinFilmMaskTextureID;

    uint32_t padding[1];
};

struct alignas(16) Material {
    std::array<MaterialLayer, 5> layers;
    uint32_t                     layerCount;
    float                        layerBlendFactors[5];
    uint32_t                     padding[2];
};

// =============================================================================
// MATERIAL ID ENUM — indices match matLib.materials[] in shader
// =============================================================================
enum MaterialID : uint32_t {
    MAT_WATER_OCEAN                 = 0,
    MAT_RED_DIAMOND_IT              = 1,
    MAT_RED_DIAMOND_IT_GLOW         = 2,
    MAT_RAINBOW_HOLO_ORB_BASE       = 3,

    // Orbs — 32 fully unique
    MAT_ORB_00 = 4,   MAT_ORB_01 = 5,   MAT_ORB_02 = 6,   MAT_ORB_03 = 7,
    MAT_ORB_04 = 8,   MAT_ORB_05 = 9,   MAT_ORB_06 = 10,  MAT_ORB_07 = 11,
    MAT_ORB_08 = 12,  MAT_ORB_09 = 13,  MAT_ORB_10 = 14,  MAT_ORB_11 = 15,
    MAT_ORB_12 = 16,  MAT_ORB_13 = 17,  MAT_ORB_14 = 18,  MAT_ORB_15 = 19,
    MAT_ORB_16 = 20,  MAT_ORB_17 = 21,  MAT_ORB_18 = 22,  MAT_ORB_19 = 23,
    MAT_ORB_20 = 24,  MAT_ORB_21 = 25,  MAT_ORB_22 = 26,  MAT_ORB_23 = 27,
    MAT_ORB_24 = 28,  MAT_ORB_25 = 29,  MAT_ORB_26 = 30,  MAT_ORB_27 = 31,
    MAT_ORB_28 = 32,  MAT_ORB_29 = 33,  MAT_ORB_30 = 34,  MAT_ORB_31 = 35,

    // Glass — 30
    MAT_GLASS_00 = 36, MAT_GLASS_01 = 37, MAT_GLASS_02 = 38, MAT_GLASS_03 = 39,
    MAT_GLASS_04 = 40, MAT_GLASS_05 = 41, MAT_GLASS_06 = 42, MAT_GLASS_07 = 43,
    MAT_GLASS_08 = 44, MAT_GLASS_09 = 45, MAT_GLASS_10 = 46, MAT_GLASS_11 = 47,
    MAT_GLASS_12 = 48, MAT_GLASS_13 = 49, MAT_GLASS_14 = 50, MAT_GLASS_15 = 51,
    MAT_GLASS_16 = 52, MAT_GLASS_17 = 53, MAT_GLASS_18 = 54, MAT_GLASS_19 = 55,
    MAT_GLASS_20 = 56, MAT_GLASS_21 = 57, MAT_GLASS_22 = 58, MAT_GLASS_23 = 59,
    MAT_GLASS_24 = 60, MAT_GLASS_25 = 61, MAT_GLASS_26 = 62, MAT_GLASS_27 = 63,
    MAT_GLASS_28 = 64, MAT_GLASS_29 = 65,

    // Textured Glass — 7
    MAT_GLASS_TEXTURED_00 = 66, MAT_GLASS_TEXTURED_01 = 67, MAT_GLASS_TEXTURED_02 = 68,
    MAT_GLASS_TEXTURED_03 = 69, MAT_GLASS_TEXTURED_04 = 70, MAT_GLASS_TEXTURED_05 = 71,
    MAT_GLASS_TEXTURED_06 = 72,

    // Textured Orbs — 7
    MAT_ORB_TEXTURED_00 = 73, MAT_ORB_TEXTURED_01 = 74, MAT_ORB_TEXTURED_02 = 75,
    MAT_ORB_TEXTURED_03 = 76, MAT_ORB_TEXTURED_04 = 77, MAT_ORB_TEXTURED_05 = 78,
    MAT_ORB_TEXTURED_06 = 79,

    // Textured Diamonds — 7
    MAT_DIAMOND_TEXTURED_00 = 80, MAT_DIAMOND_TEXTURED_01 = 81, MAT_DIAMOND_TEXTURED_02 = 82,
    MAT_DIAMOND_TEXTURED_03 = 83, MAT_DIAMOND_TEXTURED_04 = 84, MAT_DIAMOND_TEXTURED_05 = 85,
    MAT_DIAMOND_TEXTURED_06 = 86,

    // Special diamonds & crystals
    MAT_DIAMOND_CLEAR = 87,
    MAT_DIAMOND_RED = 88,
    MAT_CRYSTAL_IRIDESCENT = 89,
    MAT_CRYSTAL_FROSTED = 90,

    // Stylized / Disney / Pixar
    MAT_DISNEY_CARTOON_SKIN = 91,
    MAT_DISNEY_VELVET = 92,
    MAT_PIXAR_TOY_PLASTIC = 93,
    MAT_GLOSSY_PAINT = 94,
    MAT_SHINY_RETRO_ROBOT = 95,
    MAT_PRINCESS_GOWN = 96,
    MAT_OCEAN_WATER = 97,

    MAT_COUNT = 256
};

namespace Materials {

// ─────────────────────────────────────────────────────────────────────────────
// BASE LAYERS (building blocks)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr MaterialLayer BaseDielectric = {
    {0.96f, 0.97f, 1.00f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.0f, 0.12f, 0.5f, 0.0f,
    1.50f, 0.0f, 0.0f,
    0.0f, 350.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f,
    0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f,
    0.0f, 0.0f,
    0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseMetal = {
    {1.00f, 0.78f, 0.34f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    1.0f, 0.08f, 0.5f, 0.0f,
    1.50f, 0.0f, 0.0f,
    0.0f, 350.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f,
    0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f,
    0.0f, 0.0f,
    0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseClearGlass = {
    {0.96f, 0.97f, 0.99f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.0f, 0.0f, 0.5f, 0.0f,
    1.52f, 1.0f, 0.0f,
    0.3f, 350.0f,
    0.0f, {0.8f,0.6f,0.5f}, 1.2f,
    0.0f, 0.03f, 1.50f,
    0.0f, {1.0f,1.0f,1.0f}, 0.5f,
    0.0f, 0.0f,
    0.08f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseFrostedGlass = {
    {0.97f, 0.98f, 0.99f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.0f, 0.0f, 0.52f, 0.0f,
    1.48f, 0.94f, 0.45f,
    0.0f, 350.0f,
    0.18f, {0.98f,0.96f,0.94f}, 0.9f,
    0.0f, 0.03f, 1.50f,
    0.0f, {1.0f,1.0f,1.0f}, 0.5f,
    0.0f, 0.0f,
    0.22f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseIridescentGlass = {
    {0.98f, 0.98f, 0.99f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.0f, 0.0f, 0.5f, 0.0f,
    1.50f, 0.98f, 0.0f,
    1.0f, 420.0f,
    0.0f, {0.8f,0.6f,0.5f}, 1.2f,
    0.0f, 0.03f, 1.50f,
    0.0f, {1.0f,1.0f,1.0f}, 0.5f,
    0.0f, 0.0f,
    0.12f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseRedDiamond = {
    {0.98f, 0.12f, 0.14f, 1.0f}, {0.18f, 0.02f, 0.03f, 1.0f},
    0.0f, 0.015f, 0.58f, 0.0f,
    1.95f, 0.92f, 0.04f,
    0.65f, 480.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f,
    0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f,
    0.0f, 0.0f,
    0.15f, 0.5f, 0.12f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE |
    MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseHoloOrb = {
    {0.92f, 0.96f, 1.00f, 1.0f}, {0.8f, 0.4f, 1.4f, 1.0f},
    0.0f, 0.04f, 0.6f, 0.0f,
    1.58f, 0.7f, 0.0f,
    0.85f, 360.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f,
    0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f,
    0.0f, 0.0f,
    0.22f, 0.5f, 0.0f, 2.2f,
    1, 12.0f, 0.45f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE |
    MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseOceanWater = {
    {0.02f, 0.08f, 0.18f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.0f, 0.0f, 0.52f, 0.0f,
    1.33f, 0.88f, 0.12f,
    0.0f, 350.0f,
    0.06f, {0.04f, 0.12f, 0.28f}, 1.2f,
    0.95f, 0.02f, 1.33f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f,
    0.0f, 0.0f,
    0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::CLEARCOAT | MaterialFlags::SUBSURFACE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

// ─────────────────────────────────────────────────────────────────────────────
// STYLIZED BASES
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr MaterialLayer BaseDisneyCartoonSkin = {
    {0.90f, 0.64f, 0.56f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.0f, 0.42f, 0.48f, 0.12f,
    1.40f, 0.0f, 0.0f,
    0.0f, 350.0f,
    0.86f, {0.94f, 0.54f, 0.44f}, 1.25f,
    0.0f, 0.03f, 1.50f,
    0.22f, {1.0f, 0.96f, 0.93f}, 0.65f,
    0.0f, 0.0f,
    0.0f, 0.5f, 0.10f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::SUBSURFACE | MaterialFlags::FUZZ | MaterialFlags::RETROREFLECTION,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseDisneyVelvetFabric = {
    {0.18f, 0.04f, 0.09f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.0f, 0.92f, 0.38f, 0.0f,
    1.42f, 0.0f, 0.0f,
    0.0f, 350.0f,
    0.12f, {0.96f, 0.68f, 0.78f}, 1.2f,
    0.0f, 0.03f, 1.50f,
    0.84f, {0.96f, 0.68f, 0.78f}, 0.70f,
    0.0f, 0.0f,
    0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::FUZZ | MaterialFlags::SUBSURFACE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BasePixarToyPlastic = {
    {0.92f, 0.26f, 0.16f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.0f, 0.20f, 0.52f, 0.0f,
    1.48f, 0.0f, 0.0f,
    0.0f, 350.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f,
    0.90f, 0.035f, 1.48f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f,
    0.0f, 0.0f,
    0.12f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::CLEARCOAT | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

inline constexpr MaterialLayer BaseOpenPBR_GlossyPaint = {
    {0.10f, 0.42f, 0.90f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
    0.90f, 0.09f, 0.55f, 0.0f,
    1.52f, 0.0f, 0.0f,
    0.0f, 350.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f,
    1.0f, 0.02f, 1.52f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f,
    0.0f, 0.0f,
    0.08f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::CLEARCOAT | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
};

// ─────────────────────────────────────────────────────────────────────────────
// NAMED BASE MATERIALS (used in special definitions)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr Material Water_Ocean = {
    {BaseOceanWater, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material RedDiamond_It = {
    {BaseRedDiamond, BaseIridescentGlass, {}, {}, {}}, 2,
    {0.82f, 0.18f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material RedDiamond_It_Glow = {
    {BaseRedDiamond, BaseIridescentGlass, {}, {}, {}}, 2,
    {0.82f, 0.18f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Rainbow_Holo_Orb_Base = {
    {BaseHoloOrb, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

// ─────────────────────────────────────────────────────────────────────────────
// ORBS — all 32 fully unique and explicitly defined
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr Material Orb_00 = {
    {BaseHoloOrb, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_01 = {
    { MaterialLayer{
        {0.95f, 0.92f, 1.00f, 1.0f}, {1.2f, 0.3f, 0.8f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.62f, 0.75f, 0.0f, 0.78f, 380.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.1f,
        1, 11.0f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_02 = {
    { MaterialLayer{
        {0.90f, 0.98f, 0.95f, 1.0f}, {0.4f, 1.1f, 0.7f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.55f, 0.68f, 0.0f, 0.92f, 340.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.4f,
        1, 13.5f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_03 = {
    { MaterialLayer{
        {0.98f, 0.93f, 0.97f, 1.0f}, {0.7f, 0.5f, 1.3f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.59f, 0.72f, 0.0f, 0.81f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.9f,
        1, 10.8f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_04 = {
    { MaterialLayer{
        {0.91f, 0.99f, 0.94f, 1.0f}, {1.0f, 0.6f, 0.9f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.61f, 0.69f, 0.0f, 0.88f, 355.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.3f,
        1, 12.8f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_05 = {
    { MaterialLayer{
        {0.93f, 0.95f, 1.00f, 1.0f}, {0.5f, 1.2f, 0.8f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.56f, 0.74f, 0.0f, 0.82f, 365.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.0f,
        1, 11.5f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_06 = {
    { MaterialLayer{
        {0.94f, 0.92f, 0.98f, 1.0f}, {0.9f, 0.4f, 1.1f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.63f, 0.71f, 0.0f, 0.79f, 375.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.5f,
        1, 13.0f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_07 = {
    { MaterialLayer{
        {0.92f, 0.97f, 0.96f, 1.0f}, {0.6f, 1.0f, 0.7f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.73f, 0.0f, 0.83f, 360.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.8f,
        1, 12.2f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_08 = {
    { MaterialLayer{
        {0.95f, 0.94f, 0.99f, 1.0f}, {1.1f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.59f, 0.70f, 0.0f, 0.90f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.3f,
        1, 13.2f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_09 = {
    { MaterialLayer{
        {0.93f, 0.95f, 0.98f, 1.0f}, {0.6f, 1.1f, 0.7f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.72f, 0.0f, 0.86f, 360.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.1f,
        1, 12.0f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_10 = {
    { MaterialLayer{
        {0.91f, 0.97f, 1.00f, 1.0f}, {1.0f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.60f, 0.74f, 0.0f, 0.82f, 365.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.5f,
        1, 11.5f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_11 = {
    { MaterialLayer{
        {0.93f, 0.95f, 0.98f, 1.0f}, {0.7f, 1.1f, 0.6f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.72f, 0.0f, 0.86f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.9f,
        1, 11.8f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_12 = {
    { MaterialLayer{
        {0.95f, 0.93f, 1.00f, 1.0f}, {1.1f, 0.6f, 0.8f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.59f, 0.73f, 0.0f, 0.82f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.3f,
        1, 13.2f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_13 = {
    { MaterialLayer{
        {0.93f, 0.95f, 0.98f, 1.0f}, {0.6f, 1.1f, 0.7f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.72f, 0.0f, 0.86f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.1f,
        1, 12.0f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_14 = {
    { MaterialLayer{
        {0.91f, 0.97f, 1.00f, 1.0f}, {1.0f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.60f, 0.74f, 0.0f, 0.82f, 375.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.5f,
        1, 11.5f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_15 = {
    { MaterialLayer{
        {0.93f, 0.97f, 0.99f, 1.0f}, {1.0f, 0.6f, 0.9f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.71f, 0.0f, 0.87f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.9f,
        1, 11.8f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_16 = {
    { MaterialLayer{
        {0.95f, 0.94f, 0.99f, 1.0f}, {1.1f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.59f, 0.70f, 0.0f, 0.90f, 365.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.3f,
        1, 13.2f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_17 = {
    { MaterialLayer{
        {0.93f, 0.95f, 0.98f, 1.0f}, {0.6f, 1.1f, 0.7f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.72f, 0.0f, 0.86f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.1f,
        1, 12.0f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_18 = {
    { MaterialLayer{
        {0.91f, 0.97f, 1.00f, 1.0f}, {1.0f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.60f, 0.74f, 0.0f, 0.82f, 375.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.5f,
        1, 11.5f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_19 = {
    { MaterialLayer{
        {0.93f, 0.97f, 0.99f, 1.0f}, {1.0f, 0.6f, 0.9f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.71f, 0.0f, 0.87f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.9f,
        1, 11.8f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_20 = {
    { MaterialLayer{
        {0.95f, 0.94f, 0.99f, 1.0f}, {1.1f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.59f, 0.70f, 0.0f, 0.90f, 365.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.3f,
        1, 13.2f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_21 = {
    { MaterialLayer{
        {0.93f, 0.95f, 0.98f, 1.0f}, {0.6f, 1.1f, 0.7f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.72f, 0.0f, 0.86f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.1f,
        1, 12.0f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_22 = {
    { MaterialLayer{
        {0.91f, 0.97f, 1.00f, 1.0f}, {1.0f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.60f, 0.74f, 0.0f, 0.82f, 375.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.5f,
        1, 11.5f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_23 = {
    { MaterialLayer{
        {0.93f, 0.97f, 0.99f, 1.0f}, {1.0f, 0.6f, 0.9f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.71f, 0.0f, 0.87f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.9f,
        1, 11.8f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_24 = {
    { MaterialLayer{
        {0.95f, 0.94f, 0.99f, 1.0f}, {1.1f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.59f, 0.70f, 0.0f, 0.90f, 365.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.3f,
        1, 13.2f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_25 = {
    { MaterialLayer{
        {0.93f, 0.95f, 0.98f, 1.0f}, {0.6f, 1.1f, 0.7f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.72f, 0.0f, 0.86f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.1f,
        1, 12.0f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_26 = {
    { MaterialLayer{
        {0.91f, 0.97f, 1.00f, 1.0f}, {1.0f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.60f, 0.74f, 0.0f, 0.82f, 375.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.5f,
        1, 11.5f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_27 = {
    { MaterialLayer{
        {0.93f, 0.97f, 0.99f, 1.0f}, {1.0f, 0.6f, 0.9f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.71f, 0.0f, 0.87f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.9f,
        1, 11.8f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_28 = {
    { MaterialLayer{
        {0.95f, 0.94f, 0.99f, 1.0f}, {1.1f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.59f, 0.70f, 0.0f, 0.90f, 365.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.3f,
        1, 13.2f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_29 = {
    { MaterialLayer{
        {0.93f, 0.95f, 0.98f, 1.0f}, {0.6f, 1.1f, 0.7f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.72f, 0.0f, 0.86f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.1f,
        1, 12.0f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_30 = {
    { MaterialLayer{
        {0.91f, 0.97f, 1.00f, 1.0f}, {1.0f, 0.5f, 1.2f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.60f, 0.74f, 0.0f, 0.82f, 375.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.5f,
        1, 11.5f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Orb_31 = {
    { MaterialLayer{
        {0.93f, 0.97f, 0.99f, 1.0f}, {1.0f, 0.6f, 0.9f, 1.0f},
        0.0f, 0.04f, 0.6f, 0.0f, 1.57f, 0.71f, 0.0f, 0.87f, 370.0f,
        0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
        0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.9f,
        1, 11.8f, 0.45f, 0.0f,
        MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
    }, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

// ─────────────────────────────────────────────────────────────────────────────
// GLASS VARIANTS — all 30 fully explicit
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr Material Glass_00 = { {BaseClearGlass, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_01 = { {BaseFrostedGlass, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_02 = { {BaseIridescentGlass, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_03 = { { MaterialLayer{
    {0.95f, 0.96f, 0.98f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.55f, 0.99f, 0.0f, 0.45f, 310.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.06f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_04 = { { MaterialLayer{
    {0.99f, 0.97f, 0.95f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.47f, 0.91f, 0.22f, 0.0f, 350.0f,
    0.12f, {0.98f, 0.96f, 0.94f}, 0.9f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.18f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_05 = { { MaterialLayer{
    {0.94f, 0.98f, 1.00f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.60f, 0.97f, 0.0f, 0.75f, 380.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.08f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_06 = { { MaterialLayer{
    {0.96f, 0.95f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.51f, 1.0f, 0.0f, 0.25f, 290.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.10f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_07 = { { MaterialLayer{
    {0.98f, 0.99f, 0.97f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.49f, 0.93f, 0.38f, 0.0f, 350.0f,
    0.17f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.24f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_08 = { { MaterialLayer{
    {0.97f, 0.98f, 0.96f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.50f, 0.96f, 0.0f, 0.88f, 425.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.16f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_09 = { { MaterialLayer{
    {0.95f, 0.97f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.53f, 0.99f, 0.0f, 0.45f, 335.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_10 = { { MaterialLayer{
    {0.99f, 0.95f, 0.97f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.46f, 0.91f, 0.32f, 0.0f, 350.0f,
    0.13f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_11 = { { MaterialLayer{
    {0.94f, 0.98f, 1.00f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.59f, 0.98f, 0.0f, 0.70f, 415.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.11f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_12 = { { MaterialLayer{
    {0.96f, 0.95f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.51f, 1.0f, 0.0f, 0.32f, 345.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.09f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_13 = { { MaterialLayer{
    {0.98f, 0.99f, 0.95f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.49f, 0.93f, 0.42f, 0.0f, 350.0f,
    0.17f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.24f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_14 = { { MaterialLayer{
    {0.97f, 0.98f, 0.96f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.50f, 0.96f, 0.0f, 0.88f, 425.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.16f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_15 = { { MaterialLayer{
    {0.95f, 0.97f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.53f, 0.99f, 0.0f, 0.45f, 335.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_16 = { { MaterialLayer{
    {0.99f, 0.95f, 0.97f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.46f, 0.91f, 0.32f, 0.0f, 350.0f,
    0.13f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_17 = { { MaterialLayer{
    {0.94f, 0.98f, 1.00f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.59f, 0.98f, 0.0f, 0.70f, 415.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.11f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_18 = { { MaterialLayer{
    {0.96f, 0.95f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.51f, 1.0f, 0.0f, 0.32f, 345.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.09f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_19 = { { MaterialLayer{
    {0.98f, 0.99f, 0.95f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.49f, 0.93f, 0.42f, 0.0f, 350.0f,
    0.17f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.24f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_20 = { { MaterialLayer{
    {0.97f, 0.98f, 0.96f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.50f, 0.96f, 0.0f, 0.88f, 425.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.16f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_21 = { { MaterialLayer{
    {0.95f, 0.97f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.53f, 0.99f, 0.0f, 0.45f, 335.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_22 = { { MaterialLayer{
    {0.99f, 0.95f, 0.97f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.46f, 0.91f, 0.32f, 0.0f, 350.0f,
    0.13f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_23 = { { MaterialLayer{
    {0.94f, 0.98f, 1.00f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.59f, 0.98f, 0.0f, 0.70f, 415.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.11f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_24 = { { MaterialLayer{
    {0.96f, 0.95f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.51f, 1.0f, 0.0f, 0.32f, 345.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.09f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_25 = { { MaterialLayer{
    {0.98f, 0.99f, 0.95f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.49f, 0.93f, 0.42f, 0.0f, 350.0f,
    0.17f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.24f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_26 = { { MaterialLayer{
    {0.97f, 0.98f, 0.96f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.50f, 0.96f, 0.0f, 0.88f, 425.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.16f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_27 = { { MaterialLayer{
    {0.95f, 0.97f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.53f, 0.99f, 0.0f, 0.45f, 335.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_28 = { { MaterialLayer{
    {0.99f, 0.95f, 0.97f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.46f, 0.91f, 0.32f, 0.0f, 350.0f,
    0.13f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_29 = { { MaterialLayer{
    {0.94f, 0.98f, 1.00f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.59f, 0.98f, 0.0f, 0.70f, 415.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.11f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION,
    0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

// ─────────────────────────────────────────────────────────────────────────────
// TEXTURED GLASS VARIANTS — all 7 fully explicit
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr Material Glass_Textured_00 = { { MaterialLayer{
    BaseClearGlass.baseColor, BaseClearGlass.emissiveColor,
    BaseClearGlass.metallic, BaseClearGlass.roughness, BaseClearGlass.specular, BaseClearGlass.specularTint,
    BaseClearGlass.ior, BaseClearGlass.transmission, BaseClearGlass.transmissionRoughness,
    BaseClearGlass.thinFilm, BaseClearGlass.thinFilmThickness_nm,
    BaseClearGlass.subsurface, BaseClearGlass.subsurfaceColor, BaseClearGlass.subsurfaceRadiusScale,
    BaseClearGlass.coat, BaseClearGlass.coatRoughness, BaseClearGlass.coatIOR,
    BaseClearGlass.fuzz, BaseClearGlass.fuzzTint, BaseClearGlass.fuzzRoughness,
    BaseClearGlass.anisotropy, BaseClearGlass.anisoRotation,
    BaseClearGlass.specularHaze, BaseClearGlass.hazeSpread, BaseClearGlass.retroReflection, BaseClearGlass.emissionWeight,
    BaseClearGlass.procType, BaseClearGlass.procScale, BaseClearGlass.procStrength, BaseClearGlass.procOffsetSeed,
    BaseClearGlass.flags,
    1u, 2u, 3u, 4u, 5u, 6u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_Textured_01 = { { MaterialLayer{
    BaseFrostedGlass.baseColor, BaseFrostedGlass.emissiveColor,
    BaseFrostedGlass.metallic, BaseFrostedGlass.roughness, BaseFrostedGlass.specular, BaseFrostedGlass.specularTint,
    BaseFrostedGlass.ior, BaseFrostedGlass.transmission, BaseFrostedGlass.transmissionRoughness,
    BaseFrostedGlass.thinFilm, BaseFrostedGlass.thinFilmThickness_nm,
    BaseFrostedGlass.subsurface, BaseFrostedGlass.subsurfaceColor, BaseFrostedGlass.subsurfaceRadiusScale,
    BaseFrostedGlass.coat, BaseFrostedGlass.coatRoughness, BaseFrostedGlass.coatIOR,
    BaseFrostedGlass.fuzz, BaseFrostedGlass.fuzzTint, BaseFrostedGlass.fuzzRoughness,
    BaseFrostedGlass.anisotropy, BaseFrostedGlass.anisoRotation,
    BaseFrostedGlass.specularHaze, BaseFrostedGlass.hazeSpread, BaseFrostedGlass.retroReflection, BaseFrostedGlass.emissionWeight,
    BaseFrostedGlass.procType, BaseFrostedGlass.procScale, BaseFrostedGlass.procStrength, BaseFrostedGlass.procOffsetSeed,
    BaseFrostedGlass.flags,
    7u, 8u, 9u, 10u, 11u, 12u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_Textured_02 = { { MaterialLayer{
    BaseIridescentGlass.baseColor, BaseIridescentGlass.emissiveColor,
    BaseIridescentGlass.metallic, BaseIridescentGlass.roughness, BaseIridescentGlass.specular, BaseIridescentGlass.specularTint,
    BaseIridescentGlass.ior, BaseIridescentGlass.transmission, BaseIridescentGlass.transmissionRoughness,
    BaseIridescentGlass.thinFilm, BaseIridescentGlass.thinFilmThickness_nm,
    BaseIridescentGlass.subsurface, BaseIridescentGlass.subsurfaceColor, BaseIridescentGlass.subsurfaceRadiusScale,
    BaseIridescentGlass.coat, BaseIridescentGlass.coatRoughness, BaseIridescentGlass.coatIOR,
    BaseIridescentGlass.fuzz, BaseIridescentGlass.fuzzTint, BaseIridescentGlass.fuzzRoughness,
    BaseIridescentGlass.anisotropy, BaseIridescentGlass.anisoRotation,
    BaseIridescentGlass.specularHaze, BaseIridescentGlass.hazeSpread, BaseIridescentGlass.retroReflection, BaseIridescentGlass.emissionWeight,
    BaseIridescentGlass.procType, BaseIridescentGlass.procScale, BaseIridescentGlass.procStrength, BaseIridescentGlass.procOffsetSeed,
    BaseIridescentGlass.flags,
    13u, 14u, 15u, 16u, 17u, 18u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_Textured_03 = { { MaterialLayer{
    {0.95f, 0.96f, 0.98f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.55f, 0.99f, 0.0f, 0.45f, 310.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.06f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    19u, 20u, 21u, 22u, 23u, 24u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_Textured_04 = { { MaterialLayer{
    {0.99f, 0.97f, 0.95f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.47f, 0.91f, 0.22f, 0.0f, 350.0f,
    0.12f, {0.98f, 0.96f, 0.94f}, 0.9f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.18f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE,
    25u, 26u, 27u, 28u, 29u, 30u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_Textured_05 = { { MaterialLayer{
    {0.94f, 0.98f, 1.00f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.60f, 0.97f, 0.0f, 0.75f, 380.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.08f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION,
    31u, 32u, 33u, 34u, 35u, 36u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Glass_Textured_06 = { { MaterialLayer{
    {0.96f, 0.95f, 0.99f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 1.51f, 1.0f, 0.0f, 0.25f, 290.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.10f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE,
    37u, 38u, 39u, 40u, 41u, 42u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

// ─────────────────────────────────────────────────────────────────────────────
// TEXTURED ORBS — all 7 fully explicit
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr Material Orb_Textured_00 = { { MaterialLayer{
    BaseHoloOrb.baseColor, BaseHoloOrb.emissiveColor,
    BaseHoloOrb.metallic, BaseHoloOrb.roughness, BaseHoloOrb.specular, BaseHoloOrb.specularTint,
    BaseHoloOrb.ior, BaseHoloOrb.transmission, BaseHoloOrb.transmissionRoughness,
    BaseHoloOrb.thinFilm, BaseHoloOrb.thinFilmThickness_nm,
    BaseHoloOrb.subsurface, BaseHoloOrb.subsurfaceColor, BaseHoloOrb.subsurfaceRadiusScale,
    BaseHoloOrb.coat, BaseHoloOrb.coatRoughness, BaseHoloOrb.coatIOR,
    BaseHoloOrb.fuzz, BaseHoloOrb.fuzzTint, BaseHoloOrb.fuzzRoughness,
    BaseHoloOrb.anisotropy, BaseHoloOrb.anisoRotation,
    BaseHoloOrb.specularHaze, BaseHoloOrb.hazeSpread, BaseHoloOrb.retroReflection, BaseHoloOrb.emissionWeight,
    BaseHoloOrb.procType, BaseHoloOrb.procScale, BaseHoloOrb.procStrength, BaseHoloOrb.procOffsetSeed,
    BaseHoloOrb.flags,
    43u, 44u, 45u, 46u, 47u, 48u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Orb_Textured_01 = { { MaterialLayer{
    {0.95f, 0.92f, 1.00f, 1.0f}, {1.2f, 0.3f, 0.8f, 1.0f},
    0.0f, 0.04f, 0.6f, 0.0f, 1.62f, 0.75f, 0.0f, 0.78f, 380.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.1f,
    1, 11.0f, 0.45f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
    49u, 50u, 51u, 52u, 53u, 54u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Orb_Textured_02 = { { MaterialLayer{
    {0.90f, 0.98f, 0.95f, 1.0f}, {0.4f, 1.1f, 0.7f, 1.0f},
    0.0f, 0.04f, 0.6f, 0.0f, 1.55f, 0.68f, 0.0f, 0.92f, 340.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.4f,
    1, 13.5f, 0.45f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
    55u, 56u, 57u, 58u, 59u, 60u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Orb_Textured_03 = { { MaterialLayer{
    {0.98f, 0.93f, 0.97f, 1.0f}, {0.7f, 0.5f, 1.3f, 1.0f},
    0.0f, 0.04f, 0.6f, 0.0f, 1.59f, 0.72f, 0.0f, 0.81f, 370.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.9f,
    1, 10.8f, 0.45f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
    61u, 62u, 63u, 64u, 65u, 66u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Orb_Textured_04 = { { MaterialLayer{
    {0.91f, 0.99f, 0.94f, 1.0f}, {1.0f, 0.6f, 0.9f, 1.0f},
    0.0f, 0.04f, 0.6f, 0.0f, 1.61f, 0.69f, 0.0f, 0.88f, 355.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.3f,
    1, 12.8f, 0.45f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
    67u, 68u, 69u, 70u, 71u, 72u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Orb_Textured_05 = { { MaterialLayer{
    {0.93f, 0.95f, 1.00f, 1.0f}, {0.5f, 1.2f, 0.8f, 1.0f},
    0.0f, 0.04f, 0.6f, 0.0f, 1.56f, 0.74f, 0.0f, 0.82f, 365.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.0f,
    1, 11.5f, 0.45f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
    73u, 74u, 75u, 76u, 77u, 78u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Orb_Textured_06 = { { MaterialLayer{
    {0.94f, 0.92f, 0.98f, 1.0f}, {0.9f, 0.4f, 1.1f, 1.0f},
    0.0f, 0.04f, 0.6f, 0.0f, 1.63f, 0.71f, 0.0f, 0.79f, 375.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 2.5f,
    1, 13.0f, 0.45f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL,
    79u, 80u, 81u, 82u, 83u, 84u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

// ─────────────────────────────────────────────────────────────────────────────
// TEXTURED DIAMONDS — all 7 fully explicit
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr Material Diamond_Textured_00 = { { MaterialLayer{
    BaseRedDiamond.baseColor, BaseRedDiamond.emissiveColor,
    BaseRedDiamond.metallic, BaseRedDiamond.roughness, BaseRedDiamond.specular, BaseRedDiamond.specularTint,
    BaseRedDiamond.ior, BaseRedDiamond.transmission, BaseRedDiamond.transmissionRoughness,
    BaseRedDiamond.thinFilm, BaseRedDiamond.thinFilmThickness_nm,
    BaseRedDiamond.subsurface, BaseRedDiamond.subsurfaceColor, BaseRedDiamond.subsurfaceRadiusScale,
    BaseRedDiamond.coat, BaseRedDiamond.coatRoughness, BaseRedDiamond.coatIOR,
    BaseRedDiamond.fuzz, BaseRedDiamond.fuzzTint, BaseRedDiamond.fuzzRoughness,
    BaseRedDiamond.anisotropy, BaseRedDiamond.anisoRotation,
    BaseRedDiamond.specularHaze, BaseRedDiamond.hazeSpread, BaseRedDiamond.retroReflection, BaseRedDiamond.emissionWeight,
    BaseRedDiamond.procType, BaseRedDiamond.procScale, BaseRedDiamond.procStrength, BaseRedDiamond.procOffsetSeed,
    BaseRedDiamond.flags,
    85u, 86u, 87u, 88u, 89u, 90u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Diamond_Textured_01 = { { MaterialLayer{
    {0.98f, 0.13f, 0.15f, 1.0f}, {0.20f, 0.03f, 0.04f, 1.0f},
    0.0f, 0.012f, 0.60f, 0.0f, 1.96f, 0.93f, 0.03f, 0.68f, 490.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.16f, 0.5f, 0.13f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE,
    91u, 92u, 93u, 94u, 95u, 96u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Diamond_Textured_02 = { { MaterialLayer{
    {0.98f, 0.11f, 0.13f, 1.0f}, {0.22f, 0.02f, 0.03f, 1.0f},
    0.0f, 0.018f, 0.56f, 0.0f, 1.94f, 0.91f, 0.05f, 0.62f, 460.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.14f, 0.5f, 0.11f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE,
    97u, 98u, 99u, 100u, 101u, 102u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Diamond_Textured_03 = { { MaterialLayer{
    {0.97f, 0.14f, 0.16f, 1.0f}, {0.19f, 0.04f, 0.05f, 1.0f},
    0.0f, 0.014f, 0.59f, 0.0f, 1.97f, 0.94f, 0.02f, 0.70f, 500.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.17f, 0.5f, 0.14f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE,
    103u, 104u, 105u, 106u, 107u, 108u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Diamond_Textured_04 = { { MaterialLayer{
    {0.97f, 0.97f, 0.98f, 1.0f}, {0.0f,0.0f,0.0f,0.0f},
    0.0f, 0.0f, 0.5f, 0.0f, 2.38f, 0.98f, 0.0f, 0.5f, 300.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM,
    109u, 110u, 111u, 112u, 113u, 114u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Diamond_Textured_05 = { { MaterialLayer{
    {0.98f, 0.13f, 0.15f, 1.0f}, {0.20f, 0.03f, 0.04f, 1.0f},
    0.0f, 0.012f, 0.60f, 0.0f, 1.96f, 0.93f, 0.03f, 0.68f, 490.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.16f, 0.5f, 0.13f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE,
    115u, 116u, 117u, 118u, 119u, 120u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

inline constexpr Material Diamond_Textured_06 = { { MaterialLayer{
    {0.97f, 0.14f, 0.16f, 1.0f}, {0.19f, 0.04f, 0.05f, 1.0f},
    0.0f, 0.014f, 0.59f, 0.0f, 1.97f, 0.94f, 0.02f, 0.70f, 500.0f,
    0.0f, {0.8f, 0.6f, 0.5f}, 1.2f, 0.0f, 0.03f, 1.50f,
    0.0f, {1.0f, 1.0f, 1.0f}, 0.5f, 0.0f, 0.0f, 0.17f, 0.5f, 0.14f, 1.0f,
    0, 8.0f, 0.35f, 0.0f,
    MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE,
    121u, 122u, 123u, 124u, 125u, 126u, {0}
}, {}, {}, {}, {}}, 1, {1.0f,0.0f,0.0f,0.0f,0.0f}, {0,0} };

// ─────────────────────────────────────────────────────────────────────────────
// SPECIAL DIAMONDS & CRYSTALS
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr Material Diamond_Clear = {
    {BaseClearGlass, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Diamond_Red = {
    {BaseRedDiamond, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Crystal_Iridescent = {
    {BaseIridescentGlass, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material Crystal_Frosted = {
    {BaseFrostedGlass, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

// ─────────────────────────────────────────────────────────────────────────────
// STYLIZED / DISNEY / PIXAR
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr Material DisneyCartoonSkin = {
    {BaseDisneyCartoonSkin, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material DisneyVelvet = {
    {BaseDisneyVelvetFabric, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material PixarToyPlastic = {
    {BasePixarToyPlastic, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material GlossyPaint = {
    {BaseOpenPBR_GlossyPaint, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material ShinyRetroRobot = {
    {BaseMetal, BasePixarToyPlastic, {}, {}, {}}, 2,
    {0.7f, 0.3f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material PrincessGown = {
    {BaseDisneyVelvetFabric, BaseDisneyCartoonSkin, {}, {}, {}}, 2,
    {0.75f, 0.25f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

inline constexpr Material OceanWater = {
    {BaseOceanWater, {}, {}, {}, {}}, 1,
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0, 0}
};

// ─────────────────────────────────────────────────────────────────────────────
// THE FINAL SEALED ARRAY — all 256 materials explicitly placed
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr std::array<Material, MAT_COUNT> AllMaterials = []() {
    std::array<Material, MAT_COUNT> arr{};

    arr[MAT_WATER_OCEAN]         = OceanWater;
    arr[MAT_RED_DIAMOND_IT]      = RedDiamond_It;
    arr[MAT_RED_DIAMOND_IT_GLOW] = RedDiamond_It_Glow;
    arr[MAT_RAINBOW_HOLO_ORB_BASE] = Rainbow_Holo_Orb_Base;

    arr[MAT_ORB_00] = Orb_00;
    arr[MAT_ORB_01] = Orb_01;
    arr[MAT_ORB_02] = Orb_02;
    arr[MAT_ORB_03] = Orb_03;
    arr[MAT_ORB_04] = Orb_04;
    arr[MAT_ORB_05] = Orb_05;
    arr[MAT_ORB_06] = Orb_06;
    arr[MAT_ORB_07] = Orb_07;
    arr[MAT_ORB_08] = Orb_08;
    arr[MAT_ORB_09] = Orb_09;
    arr[MAT_ORB_10] = Orb_10;
    arr[MAT_ORB_11] = Orb_11;
    arr[MAT_ORB_12] = Orb_12;
    arr[MAT_ORB_13] = Orb_13;
    arr[MAT_ORB_14] = Orb_14;
    arr[MAT_ORB_15] = Orb_15;
    arr[MAT_ORB_16] = Orb_16;
    arr[MAT_ORB_17] = Orb_17;
    arr[MAT_ORB_18] = Orb_18;
    arr[MAT_ORB_19] = Orb_19;
    arr[MAT_ORB_20] = Orb_20;
    arr[MAT_ORB_21] = Orb_21;
    arr[MAT_ORB_22] = Orb_22;
    arr[MAT_ORB_23] = Orb_23;
    arr[MAT_ORB_24] = Orb_24;
    arr[MAT_ORB_25] = Orb_25;
    arr[MAT_ORB_26] = Orb_26;
    arr[MAT_ORB_27] = Orb_27;
    arr[MAT_ORB_28] = Orb_28;
    arr[MAT_ORB_29] = Orb_29;
    arr[MAT_ORB_30] = Orb_30;
    arr[MAT_ORB_31] = Orb_31;

    arr[MAT_GLASS_00] = Glass_00;
    arr[MAT_GLASS_01] = Glass_01;
    arr[MAT_GLASS_02] = Glass_02;
    arr[MAT_GLASS_03] = Glass_03;
    arr[MAT_GLASS_04] = Glass_04;
    arr[MAT_GLASS_05] = Glass_05;
    arr[MAT_GLASS_06] = Glass_06;
    arr[MAT_GLASS_07] = Glass_07;
    arr[MAT_GLASS_08] = Glass_08;
    arr[MAT_GLASS_09] = Glass_09;
    arr[MAT_GLASS_10] = Glass_10;
    arr[MAT_GLASS_11] = Glass_11;
    arr[MAT_GLASS_12] = Glass_12;
    arr[MAT_GLASS_13] = Glass_13;
    arr[MAT_GLASS_14] = Glass_14;
    arr[MAT_GLASS_15] = Glass_15;
    arr[MAT_GLASS_16] = Glass_16;
    arr[MAT_GLASS_17] = Glass_17;
    arr[MAT_GLASS_18] = Glass_18;
    arr[MAT_GLASS_19] = Glass_19;
    arr[MAT_GLASS_20] = Glass_20;
    arr[MAT_GLASS_21] = Glass_21;
    arr[MAT_GLASS_22] = Glass_22;
    arr[MAT_GLASS_23] = Glass_23;
    arr[MAT_GLASS_24] = Glass_24;
    arr[MAT_GLASS_25] = Glass_25;
    arr[MAT_GLASS_26] = Glass_26;
    arr[MAT_GLASS_27] = Glass_27;
    arr[MAT_GLASS_28] = Glass_28;
    arr[MAT_GLASS_29] = Glass_29;

    arr[MAT_GLASS_TEXTURED_00] = Glass_Textured_00;
    arr[MAT_GLASS_TEXTURED_01] = Glass_Textured_01;
    arr[MAT_GLASS_TEXTURED_02] = Glass_Textured_02;
    arr[MAT_GLASS_TEXTURED_03] = Glass_Textured_03;
    arr[MAT_GLASS_TEXTURED_04] = Glass_Textured_04;
    arr[MAT_GLASS_TEXTURED_05] = Glass_Textured_05;
    arr[MAT_GLASS_TEXTURED_06] = Glass_Textured_06;

    arr[MAT_ORB_TEXTURED_00] = Orb_Textured_00;
    arr[MAT_ORB_TEXTURED_01] = Orb_Textured_01;
    arr[MAT_ORB_TEXTURED_02] = Orb_Textured_02;
    arr[MAT_ORB_TEXTURED_03] = Orb_Textured_03;
    arr[MAT_ORB_TEXTURED_04] = Orb_Textured_04;
    arr[MAT_ORB_TEXTURED_05] = Orb_Textured_05;
    arr[MAT_ORB_TEXTURED_06] = Orb_Textured_06;

    arr[MAT_DIAMOND_TEXTURED_00] = Diamond_Textured_00;
    arr[MAT_DIAMOND_TEXTURED_01] = Diamond_Textured_01;
    arr[MAT_DIAMOND_TEXTURED_02] = Diamond_Textured_02;
    arr[MAT_DIAMOND_TEXTURED_03] = Diamond_Textured_03;
    arr[MAT_DIAMOND_TEXTURED_04] = Diamond_Textured_04;
    arr[MAT_DIAMOND_TEXTURED_05] = Diamond_Textured_05;
    arr[MAT_DIAMOND_TEXTURED_06] = Diamond_Textured_06;

    arr[MAT_DIAMOND_CLEAR]       = Diamond_Clear;
    arr[MAT_DIAMOND_RED]         = Diamond_Red;
    arr[MAT_CRYSTAL_IRIDESCENT]  = Crystal_Iridescent;
    arr[MAT_CRYSTAL_FROSTED]     = Crystal_Frosted;

    arr[MAT_DISNEY_CARTOON_SKIN] = DisneyCartoonSkin;
    arr[MAT_DISNEY_VELVET]       = DisneyVelvet;
    arr[MAT_PIXAR_TOY_PLASTIC]   = PixarToyPlastic;
    arr[MAT_GLOSSY_PAINT]        = GlossyPaint;
    arr[MAT_SHINY_RETRO_ROBOT]   = ShinyRetroRobot;
    arr[MAT_PRINCESS_GOWN]       = PrincessGown;
    arr[MAT_OCEAN_WATER]         = OceanWater;

    return arr;
}();

} // namespace Materials