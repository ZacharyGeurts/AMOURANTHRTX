/**
 * AMOURANTH RTX Engine — Materials Library (FULL FILE — READY FOR YOUR MATERIALDEFS)
 * (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
 * Dual licensed: GPL v3 or commercial
 * AMOURANTH FOREVER 💖
 */

#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cstdint>

#define MAT_COUNT 256

namespace Materials {

namespace MaterialFlags {
    constexpr uint32_t TRANSMISSION      = 1u << 0;
    constexpr uint32_t SUBSURFACE        = 1u << 1;
    constexpr uint32_t CLEARCOAT         = 1u << 2;
    constexpr uint32_t FUZZ              = 1u << 3;
    constexpr uint32_t THIN_FILM         = 1u << 4;
    constexpr uint32_t ANISOTROPY        = 1u << 5;
    constexpr uint32_t EMISSIVE          = 1u << 6;
    constexpr uint32_t PROCEDURAL        = 1u << 7;
    constexpr uint32_t SPECULAR_HAZE     = 1u << 8;
    constexpr uint32_t RETROREFLECTION   = 1u << 9;
}

struct alignas(16) MaterialLayer {
    glm::vec4 baseColor          {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissiveColor      {0.0f, 0.0f, 0.0f, 0.0f};

    float metallic               = 0.0f;
    float roughness              = 0.5f;
    float specular               = 0.5f;
    float specularTint           = 0.0f;

    float ior                    = 1.5f;
    float transmission           = 0.0f;
    float transmissionRoughness  = 0.0f;

    float thinFilm               = 0.0f;
    float thinFilmThickness_nm   = 350.0f;

    float subsurface             = 0.0f;
    glm::vec3 subsurfaceColor    {0.8f, 0.6f, 0.5f};
    float subsurfaceRadiusScale  = 1.2f;

    float coat                   = 0.0f;
    float coatRoughness          = 0.03f;
    float coatIOR                = 1.5f;

    float fuzz                   = 0.0f;
    glm::vec3 fuzzTint           {1.0f, 1.0f, 1.0f};
    float fuzzRoughness          = 0.5f;

    float anisotropy             = 0.0f;
    float anisoRotation          = 0.0f;

    float specularHaze           = 0.0f;
    float hazeSpread             = 0.5f;
    float retroReflection        = 0.0f;
    float emissionWeight         = 1.0f;

    uint32_t procType            = 0;
    float procScale              = 8.0f;
    float procStrength           = 0.35f;
    float procOffsetSeed         = 0.0f;

    uint32_t flags               = 0;

    uint32_t albedoTextureID     = 0xFFFFFFFFu;
    uint32_t normalTextureID     = 0xFFFFFFFFu;
    uint32_t roughnessTextureID  = 0xFFFFFFFFu;
    uint32_t metallicTextureID   = 0xFFFFFFFFu;
    uint32_t emissiveTextureID   = 0xFFFFFFFFu;
    uint32_t thinFilmMaskTextureID = 0xFFFFFFFFu;

    uint32_t padding[2]          = {0, 0};
};

struct alignas(16) Material {
    std::array<MaterialLayer, 5> layers;
    uint32_t                     layerCount          = 1;
    float                        layerBlendFactors[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t                     padding[2]          = {0, 0};
};

struct MaterialDef {
    uint32_t baseLayerIndex;
    uint32_t layerCount;

    glm::vec4 baseColorTint      {1.0f,1.0f,1.0f,1.0f};
    glm::vec4 emissiveBoost      {0.0f,0.0f,0.0f,0.0f};

    float metallicOverride       = -1.0f;
    float roughnessOverride      = -1.0f;
    float transmissionOverride   = -1.0f;
    float iorOverride            = -1.0f;

    float thinFilmThickness_nm   = 0.0f;
    float subsurfaceAmount       = 0.0f;

    uint32_t extraFlags          = 0;

    uint32_t albedoTextureID     = 0xFFFFFFFFu;
    uint32_t normalTextureID     = 0xFFFFFFFFu;
    uint32_t roughnessTextureID  = 0xFFFFFFFFu;
    uint32_t metallicTextureID   = 0xFFFFFFFFu;
    uint32_t emissiveTextureID   = 0xFFFFFFFFu;
    uint32_t thinFilmMaskTextureID = 0xFFFFFFFFu;
};

// =============================================================================
// Base layers — 8 entries (indices 0-7)
// =============================================================================
constexpr MaterialLayer BaseLayers[8] = {
    // 0: Clear Dielectric / Glass
    { {0.96f,0.97f,1.00f,1.0f}, {0,0,0,0}, 0.0f,0.12f,0.5f,0.0f,1.50f,0.0f,0.0f,0.0f,350.0f,0.0f,{0.8f,0.6f,0.5f},1.2f,0.0f,0.03f,1.50f,0.0f,{1.0f,1.0f,1.0f},0.5f,0.0f,0.0f,0.0f,0.5f,0.0f,1.0f,0,8.0f,0.35f,0.0f, MaterialFlags::TRANSMISSION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu, {0,0} },
    // 1: Metal
    { {1.00f,0.78f,0.34f,1.0f}, {0,0,0,0}, 1.0f,0.08f,0.5f,0.0f,1.50f,0.0f,0.0f,0.0f,350.0f,0.0f,{0.8f,0.6f,0.5f},1.2f,0.0f,0.03f,1.50f,0.0f,{1.0f,1.0f,1.0f},0.5f,0.0f,0.0f,0.0f,0.5f,0.0f,1.0f,0,8.0f,0.35f,0.0f, 0, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu, {0,0} },
    // 2: Clear Glass
    { {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, 0.0f,0.0f,0.5f,0.0f,1.52f,1.0f,0.0f,0.3f,350.0f,0.0f,{0.8f,0.6f,0.5f},1.2f,0.0f,0.03f,1.50f,0.0f,{1.0f,1.0f,1.0f},0.5f,0.0f,0.0f,0.08f,0.5f,0.0f,1.0f,0,8.0f,0.35f,0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu, {0,0} },
    // 3: Frosted Glass
    { {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, 0.0f,0.0f,0.52f,0.0f,1.48f,0.94f,0.45f,0.0f,350.0f,0.18f,{0.98f,0.96f,0.94f},0.9f,0.0f,0.03f,1.50f,0.0f,{1.0f,1.0f,1.0f},0.5f,0.0f,0.0f,0.22f,0.5f,0.0f,1.0f,0,8.0f,0.35f,0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu, {0,0} },
    // 4: Iridescent Glass
    { {0.98f,0.98f,0.99f,1.0f}, {0,0,0,0}, 0.0f,0.0f,0.5f,0.0f,1.50f,0.98f,0.0f,1.0f,420.0f,0.0f,{0.8f,0.6f,0.5f},1.2f,0.0f,0.03f,1.50f,0.0f,{1.0f,1.0f,1.0f},0.5f,0.0f,0.0f,0.12f,0.5f,0.0f,1.0f,0,8.0f,0.35f,0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu, {0,0} },
    // 5: Red Diamond
    { {0.98f,0.12f,0.14f,1.0f}, {0.18f,0.02f,0.03f,1.0f}, 0.0f,0.015f,0.58f,0.0f,1.95f,0.92f,0.04f,0.65f,480.0f,0.0f,{0.8f,0.6f,0.5f},1.2f,0.0f,0.03f,1.50f,0.0f,{1.0f,1.0f,1.0f},0.5f,0.0f,0.0f,0.15f,0.5f,0.12f,1.0f,0,8.0f,0.35f,0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu, {0,0} },
    // 6: Holo Orb Base
    { {0.92f,0.96f,1.00f,1.0f}, {0.8f,0.4f,1.4f,1.0f}, 0.0f,0.04f,0.6f,0.0f,1.58f,0.7f,0.0f,0.85f,360.0f,0.0f,{0.8f,0.6f,0.5f},1.2f,0.0f,0.03f,1.50f,0.0f,{1.0f,1.0f,1.0f},0.5f,0.0f,0.0f,0.22f,0.5f,0.0f,2.2f,1,12.0f,0.45f,0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu, {0,0} },
    // 7: Ocean Water
    { {0.02f,0.08f,0.18f,1.0f}, {0,0,0,0}, 0.0f,0.0f,0.52f,0.0f,1.33f,0.88f,0.12f,0.0f,350.0f,0.06f,{0.04f,0.12f,0.28f},1.2f,0.95f,0.02f,1.33f,0.0f,{1.0f,1.0f,1.0f},0.5f,0.0f,0.0f,0.0f,0.5f,0.0f,1.0f,0,8.0f,0.35f,0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::CLEARCOAT | MaterialFlags::SUBSURFACE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu, {0,0} }
};

// =============================================================================
// Compact definition table for all 256 materials
// =============================================================================
constexpr MaterialDef MaterialDefs[256] = {
    // 0: Water Ocean
    {7, 1, {1.0f,1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f,0.0f}, -1.0f,-1.0f,-1.0f,-1.0f, 0.0f, 0.06f, MaterialFlags::TRANSMISSION | MaterialFlags::CLEARCOAT | MaterialFlags::SUBSURFACE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},

    // 1: Red Diamond IT
    {5, 2, {1.0f,1.0f,1.0f,1.0f}, {0.18f,0.02f,0.03f,1.0f}, -1.0f,-1.0f,0.92f,1.95f, 480.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},

    // 2: Red Diamond IT Glow
    {5, 2, {1.0f,1.0f,1.0f,1.0f}, {0.28f,0.05f,0.06f,1.0f}, -1.0f,-1.0f,0.92f,1.95f, 480.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::RETROREFLECTION | MaterialFlags::EMISSIVE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},

    // 3: Rainbow Holo Orb Base
    {6, 1, {1.0f,1.0f,1.0f,1.0f}, {0.8f,0.4f,1.4f,1.0f}, -1.0f,-1.0f,-1.0f,-1.0f, 360.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},

    // Orbs 4-35 (all 32 unique patterns)
    {6, 1, {0.95f,0.92f,1.00f,1.0f}, {1.2f,0.3f,0.8f,1.0f}, -1.0f,0.04f,0.75f,1.62f, 380.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.90f,0.98f,0.95f,1.0f}, {0.4f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.68f,1.55f, 340.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.98f,0.93f,0.97f,1.0f}, {0.7f,0.5f,1.3f,1.0f}, -1.0f,0.04f,0.72f,1.59f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.99f,0.94f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.69f,1.61f, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,1.00f,1.0f}, {0.5f,1.2f,0.8f,1.0f}, -1.0f,0.04f,0.74f,1.56f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.94f,0.92f,0.98f,1.0f}, {0.9f,0.4f,1.1f,1.0f}, -1.0f,0.04f,0.71f,1.63f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.97f,0.96f,1.0f}, {0.6f,1.0f,0.7f,1.0f}, -1.0f,0.04f,0.73f,1.57f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.95f,0.94f,0.99f,1.0f}, {1.1f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.70f,1.59f, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.6f,1.1f,0.7f,1.0f}, -1.0f,0.04f,0.72f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.91f,0.97f,1.00f,1.0f}, {1.0f,0.5f,1.2f,1.0f}, -1.0f,0.04f,0.74f,1.60f, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.97f,0.99f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1.0f,0.04f,0.71f,1.57f, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},

    // Glass 36-65 (30 glass variants)
    {2, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.18f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.95f,0.96f,0.98f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.97f,0.95f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.12f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.95f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.99f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.17f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.97f,0.98f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.88f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.95f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.95f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.13f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.95f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.99f,0.95f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.17f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.97f,0.98f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.88f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.95f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.95f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.13f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.95f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.99f,0.95f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.17f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.97f,0.98f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.88f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.95f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.95f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.13f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.95f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.99f,0.95f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.17f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.97f,0.98f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.88f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.95f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.95f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.13f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.95f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.99f,0.95f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.17f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.97f,0.98f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.88f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.95f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},

    // Textured Glass 66-72
    {2, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 1u,2u,3u,4u,5u,6u},
    {3, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.18f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 7u,8u,9u,10u,11u,12u},
    {4, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 13u,14u,15u,16u,17u,18u},
    {2, 1, {0.95f,0.96f,0.98f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 19u,20u,21u,22u,23u,24u},
    {3, 1, {0.99f,0.97f,0.95f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.12f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 25u,26u,27u,28u,29u,30u},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::RETROREFLECTION, 31u,32u,33u,34u,35u,36u},
    {2, 1, {0.96f,0.95f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 37u,38u,39u,40u,41u,42u},

    // Textured Orbs 73-79
    {6, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 43u,44u,45u,46u,47u,48u},
    {6, 1, {0.95f,0.92f,1.00f,1.0f}, {1.2f,0.3f,0.8f,1.0f}, -1,-1,-1,-1, 380.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 49u,50u,51u,52u,53u,54u},
    {6, 1, {0.90f,0.98f,0.95f,1.0f}, {0.4f,1.1f,0.7f,1.0f}, -1,-1,-1,-1, 340.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 55u,56u,57u,58u,59u,60u},
    {6, 1, {0.98f,0.93f,0.97f,1.0f}, {0.7f,0.5f,1.3f,1.0f}, -1,-1,-1,-1, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 61u,62u,63u,64u,65u,66u},
    {6, 1, {0.91f,0.99f,0.94f,1.0f}, {1.0f,0.6f,0.9f,1.0f}, -1,-1,-1,-1, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 67u,68u,69u,70u,71u,72u},
    {6, 1, {0.93f,0.95f,1.00f,1.0f}, {0.5f,1.2f,0.8f,1.0f}, -1,-1,-1,-1, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 73u,74u,75u,76u,77u,78u},
    {6, 1, {0.94f,0.92f,0.98f,1.0f}, {0.9f,0.4f,1.1f,1.0f}, -1,-1,-1,-1, 375.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 79u,80u,81u,82u,83u,84u},

    // Textured Diamonds 80-86
    {5, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE, 85u,86u,87u,88u,89u,90u},
    {5, 1, {0.98f,0.13f,0.15f,1.0f}, {0.20f,0.03f,0.04f,1.0f}, -1,-1,0.93f,1.96f, 490.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE, 91u,92u,93u,94u,95u,96u},
    {5, 1, {0.98f,0.11f,0.13f,1.0f}, {0.22f,0.02f,0.03f,1.0f}, -1,-1,0.91f,1.94f, 460.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE, 97u,98u,99u,100u,101u,102u},
    {5, 1, {0.97f,0.14f,0.16f,1.0f}, {0.19f,0.04f,0.05f,1.0f}, -1,-1,0.94f,1.97f, 500.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE, 103u,104u,105u,106u,107u,108u},
    {4, 1, {0.97f,0.97f,0.98f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM, 109u,110u,111u,112u,113u,114u},
    {5, 1, {0.98f,0.13f,0.15f,1.0f}, {0.20f,0.03f,0.04f,1.0f}, -1,-1,0.93f,1.96f, 490.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE, 115u,116u,117u,118u,119u,120u},
    {5, 1, {0.97f,0.14f,0.16f,1.0f}, {0.19f,0.04f,0.05f,1.0f}, -1,-1,0.94f,1.97f, 500.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE, 121u,122u,123u,124u,125u,126u},

    // Special diamonds & crystals 87-90 (indices adjusted to fit)
    {2, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {5, 1, {0.98f,0.12f,0.14f,1.0f}, {0.18f,0.02f,0.03f,1.0f}, -1,-1,0.92f,1.95f, 480.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.18f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},

    // Stylized 91-97
    {8, 1, {0.90f,0.64f,0.56f,1.0f}, {0,0,0,0}, -1,0.42f,-1,-1, 0.0f, 0.86f, MaterialFlags::SUBSURFACE | MaterialFlags::FUZZ | MaterialFlags::RETROREFLECTION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {9, 1, {0.18f,0.04f,0.09f,1.0f}, {0,0,0,0}, -1,0.92f,-1,-1, 0.0f, 0.12f, MaterialFlags::FUZZ | MaterialFlags::SUBSURFACE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {10, 1, {0.92f,0.26f,0.16f,1.0f}, {0,0,0,0}, -1,0.20f,-1,-1, 0.0f, 0.0f, MaterialFlags::CLEARCOAT | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {11, 1, {0.10f,0.42f,0.90f,1.0f}, {0,0,0,0}, 0.90f,0.09f,-1,-1, 0.0f, 0.0f, MaterialFlags::CLEARCOAT | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {1, 2, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::CLEARCOAT | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {9, 2, {0.90f,0.64f,0.56f,1.0f}, {0,0,0,0}, -1,0.42f,-1,-1, 0.0f, 0.86f, MaterialFlags::SUBSURFACE | MaterialFlags::FUZZ | MaterialFlags::RETROREFLECTION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {7, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.06f, MaterialFlags::TRANSMISSION | MaterialFlags::CLEARCOAT | MaterialFlags::SUBSURFACE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},

    // 98-255: Remaining slots (filled with sensible unique variations of clear glass / holo orb style)
    {0, 1, {1.0f,1.0f,1.0f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,1.00f,1.0f}, {0.8f,0.4f,1.4f,1.0f}, -1,-1,-1,-1, 360.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.15f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.95f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.5f,1.1f,1.0f}, -1,-1,-1,-1, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.14f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.97f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.94f,0.93f,0.99f,1.0f}, {0.9f,0.6f,1.0f,1.0f}, -1,-1,-1,-1, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.96f,0.95f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.16f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.92f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.98f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 350.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.15f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.99f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 360.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.14f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.96f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.94f,0.93f,0.98f,1.0f}, {0.9f,0.5f,1.0f,1.0f}, -1,-1,-1,-1, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.16f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.15f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.94f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 360.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.14f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.16f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.95f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.15f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.14f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.96f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.16f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.15f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.94f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 360.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.14f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.16f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.95f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.15f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.14f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.94f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 360.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.16f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.15f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.95f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 365.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.14f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 370.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.97f,0.98f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.98f,0.97f,0.96f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.16f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.95f,0.96f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.94f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.93f,0.95f,0.98f,1.0f}, {0.7f,0.6f,1.1f,1.0f}, -1,-1,-1,-1, 360.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {2, 1, {0.96f,0.97f,0.99f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {3, 1, {0.99f,0.96f,0.97f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 0.0f, 0.15f, MaterialFlags::TRANSMISSION | MaterialFlags::SUBSURFACE | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {4, 1, {0.94f,0.98f,1.00f,1.0f}, {0,0,0,0}, -1,-1,-1,-1, 1.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::SPECULAR_HAZE, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu},
    {6, 1, {0.92f,0.96f,0.99f,1.0f}, {0.8f,0.5f,1.2f,1.0f}, -1,-1,-1,-1, 355.0f, 0.0f, MaterialFlags::TRANSMISSION | MaterialFlags::THIN_FILM | MaterialFlags::EMISSIVE | MaterialFlags::PROCEDURAL, 0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu,0xFFFFFFFFu}
};

// =============================================================================
// Generator
// =============================================================================
constexpr Material generateMaterial(const MaterialDef& def) noexcept {
    Material mat{};
    mat.layerCount = (def.layerCount > 5) ? 5u : def.layerCount;

    for (uint32_t i = 0; i < mat.layerCount; ++i) {
        if (def.baseLayerIndex >= 8) {
            // Safety: clamp to last valid base layer if your table has bad index
            MaterialLayer layer = BaseLayers[7];
            layer.baseColor = {1.0f, 0.0f, 1.0f, 1.0f}; // obvious error color
            mat.layers[i] = layer;
            continue;
        }

        MaterialLayer layer = BaseLayers[def.baseLayerIndex];

        layer.baseColor      = layer.baseColor * def.baseColorTint;
        layer.emissiveColor  = layer.emissiveColor + def.emissiveBoost;

        if (def.metallicOverride >= 0.0f)     layer.metallic = def.metallicOverride;
        if (def.roughnessOverride >= 0.0f)    layer.roughness = def.roughnessOverride;
        if (def.transmissionOverride >= 0.0f) layer.transmission = def.transmissionOverride;
        if (def.iorOverride >= 0.0f)          layer.ior = def.iorOverride;

        if (def.thinFilmThickness_nm > 0.0f) {
            layer.thinFilm = 1.0f;
            layer.thinFilmThickness_nm = def.thinFilmThickness_nm;
        }
        if (def.subsurfaceAmount > 0.0f) {
            layer.subsurface = def.subsurfaceAmount;
        }

        layer.flags |= def.extraFlags;

        if (def.albedoTextureID != 0xFFFFFFFFu) layer.albedoTextureID = def.albedoTextureID;
        if (def.normalTextureID != 0xFFFFFFFFu) layer.normalTextureID = def.normalTextureID;
        if (def.roughnessTextureID != 0xFFFFFFFFu) layer.roughnessTextureID = def.roughnessTextureID;
        if (def.metallicTextureID != 0xFFFFFFFFu) layer.metallicTextureID = def.metallicTextureID;
        if (def.emissiveTextureID != 0xFFFFFFFFu) layer.emissiveTextureID = def.emissiveTextureID;
        if (def.thinFilmMaskTextureID != 0xFFFFFFFFu) layer.thinFilmMaskTextureID = def.thinFilmMaskTextureID;

        mat.layers[i] = layer;
        mat.layerBlendFactors[i] = (i == 0) ? 1.0f : 0.0f;
    }

    return mat;
}

// =============================================================================
// Final array — generated from your MaterialDefs
// =============================================================================
inline constexpr std::array<Material, 256> AllMaterials = []() constexpr {
    std::array<Material, 256> arr{};

    for (uint32_t i = 0; i < 256; ++i) {
        arr[i] = generateMaterial(MaterialDefs[i]);
    }

    return arr;
}();

} // namespace Materials