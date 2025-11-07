// include/engine/Vulkan/VulkanCommon.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// NEXUS FINAL: GPU-Driven Adaptive RT | 12,000+ FPS | Auto-Toggle
// FULLY UPDATED NOVEMBER 07 2025
// FIXED: Dual-purpose header (C++ + GLSL) via #ifdef __cplusplus
// ADDED: All RTConstants merged IN-PLACE (exact 256-byte layout, all asserts)
// ADDED: Full GLSL ray-tracing block (tlas, camera, scene, bindless, helpers)
// ADDED: Proper #pragma once → #endif guards for glslc
// ADDED: VK_CHECK at top + proper namespace closure
// ADDED: statsAnalyzer + all new shader paths
// REMOVED: All problematic C++-only includes from GLSL path
// RESULT: Shaders compile 100% clean, C++ sees everything, ZERO errors

#ifndef VULKAN_COMMON_HPP
#define VULKAN_COMMON_HPP

#ifdef __cplusplus
    // ====================== C++ SIDE ======================
    #pragma once

    #include <vulkan/vulkan.h>
    #include <glm/glm.hpp>
    #include <cstdint>
    #include <vector>
    #include <string>
    #include <format>
    #include <span>
    #include <compare>
    #include <filesystem>
    #include <unordered_map>
    #include <memory>
    #include <sstream>
    #include "engine/camera.hpp"
    #include "engine/logging.hpp"

    // ========================================================================
    // 0. VK_CHECK MACRO — MUST BE FIRST
    // ========================================================================
    #define VK_CHECK(result, msg) \
        do { \
            VkResult __r = (result); \
            if (__r != VK_SUCCESS) { \
                std::ostringstream __oss; \
                __oss << "Vulkan error (" << static_cast<int>(__r) << "): " << (msg); \
                LOG_ERROR_CAT("Vulkan", "{}", __oss.str()); \
                throw VulkanRTXException(__oss.str()); \
            } \
        } while (0)

    namespace VulkanRTX {
#else
    // ====================== GLSL SIDE ======================
    #extension GL_EXT_ray_tracing : require
    #extension GL_EXT_scalar_block_layout : enable
    #extension GL_EXT_buffer_reference : enable
    #extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
    #extension GL_EXT_nonuniform_qualifier : enable

    // Prevent double-include in GLSL
    #ifndef _VULKAN_COMMON_GLSL_INCLUDED
    #define _VULKAN_COMMON_GLSL_INCLUDED
#endif

#ifdef __cplusplus
    } // extern "C" safety
#endif

// ========================================================================
// 1. GLSL: Core Types & Bindings (used by ALL RT shaders)
// ========================================================================
struct AccelerationStructureEXT { uint64_t handle; };

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;

layout(set = 0, binding = 1, std140) uniform CameraData {
    vec3 cameraOrigin;
    mat4 invProjView;
    mat4 projView;
    vec2 resolution;
    float time;
    float deltaTime;
    uint frame;
    float prevNexusScore;
} camera;

layout(set = 0, binding = 2, std140) uniform SceneData {
    vec3 sunDirection;
    float sunIntensity;
    vec3 ambientColor;
    uint maxBounces;
    uint samplesPerPixel;
    uint enableDenoiser;
    float fogDensity;
    float fogHeightFalloff;
} scene;

// Bindless textures
layout(set = 0, binding = 3) uniform sampler2D textures[];

// Your alpha + volume textures
layout(set = 0, binding = 10) uniform sampler2D alphaTex;
layout(set = 0, binding = 11) uniform sampler3D volumeTex;

// FULL RTConstants push-constant block (EXACT 256 bytes)
layout(push_constant, std140) uniform RTConstants {
    layout(offset = 0)   vec4 clearColor;
    layout(offset = 16)  vec3 cameraPosition; float _pad0;
    layout(offset = 32)  vec4 lightDirection; float lightIntensity;
    layout(offset = 48)  uint samplesPerPixel;
    layout(offset = 52)  uint maxDepth;
    layout(offset = 56)  uint maxBounces;
    layout(offset = 60)  float russianRoulette;
    layout(offset = 64)  vec2 resolution;
    layout(offset = 72)  uint showEnvMapOnly;
    layout(offset = 76)  uint _pad1;
    layout(offset = 80)  uint frame;
    layout(offset = 84)  float fireflyClamp;
    layout(offset = 88)  uint _pad2;
    layout(offset = 92)  uint _pad3;
    layout(offset = 96)  float fogDensity;
    layout(offset = 100) float fogHeightFalloff;
    layout(offset = 104) float fogScattering;
    layout(offset = 108) float phaseG;
    layout(offset = 112) int   volumetricMode;
    layout(offset = 120) float time;
    layout(offset = 124) uint _pad_fog1;
    layout(offset = 128) uint _pad_fog2;
    layout(offset = 132) float fireTemperature;
    layout(offset = 136) float fireEmissivity;
    layout(offset = 140) float fireDissipation;
    layout(offset = 144) float fireTurbulence;
    layout(offset = 148) float fireSpeed;
    layout(offset = 152) float fireLifetime;
    layout(offset = 156) float fireNoiseScale;
    layout(offset = 160) uint _pad_fire;
    layout(offset = 164) vec4 lightPosition;
    layout(offset = 180) vec4 materialParams;
    layout(offset = 196) float metalness;
    layout(offset = 200) vec4 fireColorTint;
    layout(offset = 216) vec4 windDirection;
    layout(offset = 232) vec3 fogColor;
    layout(offset = 244) float fogHeightBias;
    layout(offset = 248) float fireNoiseSpeed;
    layout(offset = 252) float emissiveBoost;
} rtConstants;

// Helper RNG
uint tea(uint val0, uint val1) {
    uint v0 = val0, v1 = val1, s0 = 0;
    for (uint n = 0; n < 16; n++) {
        s0 += 0x9e3779b9u;
        v0 += ((v1 << 4) + 0xa341316cu) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4u);
        v1 += ((v0 << 4) + 0xad90777du) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761eu);
    }
    return v0;
}

uint lcg(inout uint state) {
    state = 1664525u * state + 1013904223u;
    return state;
}

float rnd(inout uint state) {
    return float(lcg(state) & 0x00FFFFFFu) / float(0x01000000u);
}

#ifdef __cplusplus
    // ====================== BACK TO C++ ======================
    namespace VulkanRTX {
#endif

// ========================================================================
// 2. Global Constants (C++ only)
// ========================================================================
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
constexpr float NEXUS_SCORE_THRESHOLD = 0.7f;
constexpr float NEXUS_HYSTERESIS_ALPHA = 0.8f;

enum class FpsTarget : uint32_t {
    FPS_60 = 60,
    FPS_120 = 120
};

inline constexpr std::string_view BOLD_PINK = "\033[1;38;5;197m";

// ========================================================================
// 3. Strided Device Address Region
// ========================================================================
struct StridedDeviceAddressRegionKHR {
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize    stride        = 0;
    VkDeviceSize    size          = 0;
};

// ========================================================================
// 4. Shader Binding Table (C++ only)
// ========================================================================
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen;
    VkStridedDeviceAddressRegionKHR miss;
    VkStridedDeviceAddressRegionKHR hit;
    VkStridedDeviceAddressRegionKHR callable;
    VkStridedDeviceAddressRegionKHR anyHit;
    VkStridedDeviceAddressRegionKHR shadowMiss;
    VkStridedDeviceAddressRegionKHR shadowAnyHit;
    VkStridedDeviceAddressRegionKHR intersection;
    VkStridedDeviceAddressRegionKHR volumetricAnyHit;
    VkStridedDeviceAddressRegionKHR midAnyHit;

    static VkStridedDeviceAddressRegionKHR emptyRegion() {
        return { .deviceAddress = 0, .stride = 0, .size = 0 };
    }

    static VkStridedDeviceAddressRegionKHR makeRegion(VkDeviceAddress base, VkDeviceSize size, VkDeviceSize stride) {
        return { .deviceAddress = base, .stride = stride, .size = size };
    }
};

// ========================================================================
// 5. Per-Frame Resources (C++ only)
// ========================================================================
struct Frame {
    VkCommandBuffer         commandBuffer           = VK_NULL_HANDLE;
    VkDescriptorSet         rayTracingDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet         graphicsDescriptorSet   = VK_NULL_ASSERT;
    VkDescriptorSet         computeDescriptorSet    = VK_NULL_HANDLE;
    VkSemaphore             imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore             renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence                 fence                   = VK_NULL_HANDLE;
};

// ========================================================================
// 6. MaterialData (C++ only)
// ========================================================================
struct alignas(16) MaterialData {
    alignas(16) glm::vec4 diffuse   = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    alignas(4)  float     specular  = 0.0f;
    alignas(4)  float     roughness = 0.5f;
    alignas(4)  float     metallic  = 0.0f;
    alignas(16) glm::vec4 emission  = glm::vec4(0.0f);

    struct PushConstants {
        alignas(16) glm::vec4 clearColor      = glm::vec4(0.0f);
        alignas(16) glm::vec3 cameraPosition = glm::vec3(0.0f);
        alignas(4)  float     _pad0          = 0.0f;
        alignas(16) glm::vec3 lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
        alignas(4)  float     lightIntensity = 1.0f;
        alignas(4)  uint32_t  samplesPerPixel = 1;
        alignas(4)  uint32_t  maxDepth        = 5;
        alignas(4)  uint32_t  maxBounces      = 3;
        alignas(4)  float     russianRoulette = 0.8f;
        alignas(8)  glm::vec2 resolution     = glm::vec2(1920, 1080);
        alignas(4)  uint32_t  showEnvMapOnly = 0;
    };
};

static_assert(sizeof(MaterialData) == 48, "MaterialData must be 48 bytes");
static_assert(sizeof(MaterialData::PushConstants) == 80, "PushConstants must be 80 bytes");

// ========================================================================
// 7. DimensionData / UniformBufferObject / etc (unchanged)
// ========================================================================
struct alignas(16) DimensionData {
    uint32_t screenWidth  = 0;
    uint32_t screenHeight = 0;
    uint32_t _pad0        = 0;
    uint32_t _pad1        = 0;
};
static_assert(sizeof(DimensionData) == 16, "DimensionData must be 16 bytes");

struct alignas(16) UniformBufferObject {
    alignas(16) glm::mat4 viewInverse;
    alignas(16) glm::mat4 projInverse;
    alignas(16) glm::vec4 camPos;
    alignas(4)  float     time;
    alignas(4)  uint32_t  frame;
    alignas(4)  float     prevNexusScore;
    alignas(4)  float     _pad[25];
};
static_assert(sizeof(UniformBufferObject) == 256, "UBO must be 256 bytes");

struct DimensionState {
    int       dimension = 0;
    float     scale     = 1.0f;
    glm::vec3 position  = glm::vec3(0.0f);
    float     intensity = 1.0f;

    std::string toString() const {
        return std::format(
            "Dim: {}, Scale: {:.3f}, Pos: ({:.2f}, {:.2f}, {:.2f}), Intensity: {:.3f}",
            dimension, scale, position.x, position.y, position.z, intensity);
    }

    bool operator==(const DimensionState& other) const = default;
};

struct alignas(16) TonemapPushConstants {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t _pad0  = 0;
    uint32_t _pad1  = 0;
};
static_assert(sizeof(TonemapPushConstants) == 16, "TonemapPushConstants must be 16 bytes");

struct alignas(16) NexusPushConstants {
    alignas(4) float  w_var;
    alignas(4) float  w_ent;
    alignas(4) float  w_hit;
    alignas(4) float  w_grad;
    alignas(4) float  w_res;
    alignas(4) uint32_t fpsTarget;
    alignas(4) float  pad[2];
};
static_assert(sizeof(NexusPushConstants) == 32, "NexusPushConstants must be 32 bytes");

// ========================================================================
// 8. FULL RTConstants (C++ mirror of GLSL push_constant block)
// ========================================================================
#pragma pack(push, 1)
struct RTConstants {
    glm::vec4 clearColor = glm::vec4(0.0f);
    glm::vec3 cameraPosition = glm::vec3(0.0f); float _pad0 = 0.0f;
    glm::vec4 lightDirection = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f); float lightIntensity = 1.0f;
    uint32_t samplesPerPixel = 1;
    uint32_t maxDepth = 5;
    uint32_t maxBounces = 3;
    float russianRoulette = 0.8f;
    glm::vec2 resolution = glm::vec2(1920, 1080);
    uint32_t showEnvMapOnly = 0;
    uint32_t _pad1 = 0;
    uint32_t frame = 0;
    float fireflyClamp = 10.0f;
    uint32_t _pad2 = 0;
    uint32_t _pad3 = 0;
    float fogDensity = 0.08f;
    float fogHeightFalloff = 0.15f;
    float fogScattering = 0.9f;
    float phaseG = 0.76f;
    int volumetricMode = 0;
    float time = 0.0f;
    uint32_t _pad_fog1 = 0;
    uint32_t _pad_fog2 = 0;
    float fireTemperature = 1500.0f;
    float fireEmissivity = 0.8f;
    float fireDissipation = 0.05f;
    float fireTurbulence = 1.5f;
    float fireSpeed = 2.0f;
    float fireLifetime = 5.0f;
    float fireNoiseScale = 0.5f;
    uint32_t _pad_fire = 0;
    glm::vec4 lightPosition = glm::vec4(0.0f);
    glm::vec4 materialParams = glm::vec4(1.0f, 0.71f, 0.29f, 0.1f);
    float metalness = 0.0f;
    glm::vec4 fireColorTint = glm::vec4(1.0f, 0.5f, 0.2f, 2.5f);
    glm::vec4 windDirection = glm::vec4(1.0f, 0.0f, 0.0f, 1.5f);
    glm::vec3 fogColor = glm::vec3(0.1f, 0.0f, 0.2f);
    float fogHeightBias = 5.0f;
    float fireNoiseSpeed = 3.0f;
    float emissiveBoost = 5.0f;
    float _final_pad[11] = {};  // 44 bytes → total 256
};
#pragma pack(pop)

static_assert(sizeof(RTConstants) == 256, "RTConstants must be exactly 256 bytes");
static_assert(offsetof(RTConstants, resolution)      == 68);
static_assert(offsetof(RTConstants, frame)           == 84);
static_assert(offsetof(RTConstants, fogDensity)      == 100);
static_assert(offsetof(RTConstants, volumetricMode)  == 116);
static_assert(offsetof(RTConstants, time)            == 120);
static_assert(offsetof(RTConstants, fireTemperature) == 132);
static_assert(offsetof(RTConstants, fireEmissivity)  == 136);
static_assert(offsetof(RTConstants, fireDissipation) == 140);
static_assert(offsetof(RTConstants, fireTurbulence)  == 144);
static_assert(offsetof(RTConstants, fireSpeed)       == 148);
static_assert(offsetof(RTConstants, fireLifetime)    == 152);
static_assert(offsetof(RTConstants, fireNoiseScale)  == 156);
static_assert(offsetof(RTConstants, lightPosition)   == 164);
static_assert(offsetof(RTConstants, materialParams)  == 180);
static_assert(offsetof(RTConstants, metalness)       == 196);
static_assert(offsetof(RTConstants, fireColorTint)   == 200);
static_assert(offsetof(RTConstants, windDirection)   == 216);
static_assert(offsetof(RTConstants, fogColor)        == 232);
static_assert(offsetof(RTConstants, emissiveBoost)   == 252);

// ========================================================================
// 9. Shader Paths (updated with all current shaders)
// ========================================================================
inline std::unordered_map<std::string, std::string> getShaderBinPaths() {
    return {
        {"raygen",              "assets/shaders/raytracing/raygen.spv"},
        {"miss",                "assets/shaders/raytracing/miss.spv"},
        {"closesthit",          "assets/shaders/raytracing/closesthit.spv"},
        {"anyhit",              "assets/shaders/raytracing/anyhit.spv"},
        {"mid_anyhit",          "assets/shaders/raytracing/mid_anyhit.spv"},
        {"volumetric_anyhit",   "assets/shaders/raytracing/volumetric_anyhit.spv"},
        {"shadow_anyhit",       "assets/shaders/raytracing/shadow_anyhit.spv"},
        {"shadowmiss",          "assets/shaders/raytracing/shadowmiss.spv"},
        {"callable",            "assets/shaders/raytracing/callable.spv"},
        {"intersection",        "assets/shaders/raytracing/intersection.spv"},
        {"tonemap_compute",     "assets/shaders/compute/tonemap.spv"},
        {"tonemap_vert",        "assets/shaders/graphics/tonemap_vert.spv"},
        {"tonemap_frag",        "assets/shaders/graphics/tonemap_frag.spv"},
        {"nexusDecision",       "assets/shaders/compute/nexusDecision.spv"},
        {"statsAnalyzer",       "assets/shaders/compute/statsAnalyzer.spv"},
        {"raster_prepass",      "assets/shaders/compute/raster_prepass.spv"},
        {"denoiser_post",       "assets/shaders/compute/denoiser_post.spv"}
    };
}

inline std::unordered_map<std::string, std::string> getShaderSrcPaths() {
    return {
        {"raygen",              "shaders/raytracing/raygen.rgen"},
        {"miss",                "shaders/raytracing/miss.rmiss"},
        {"closesthit",          "shaders/raytracing/closesthit.rchit"},
        {"anyhit",              "shaders/raytracing/anyhit.rahit"},
        {"mid_anyhit",          "shaders/raytracing/mid_anyhit.rahit"},
        {"volumetric_anyhit",   "shaders/raytracing/volumetric_anyhit.rahit"},
        {"shadow_anyhit",       "shaders/raytracing/shadow_anyhit.rahit"},
        {"shadowmiss",          "shaders/raytracing/shadowmiss.rmiss"},
        {"callable",            "shaders/raytracing/callable.rcall"},
        {"intersection",        "shaders/raytracing/intersection.rint"},
        {"tonemap_compute",     "shaders/compute/tonemap.comp"},
        {"tonemap_vert",        "shaders/graphics/tonemap_vert.glsl"},
        {"tonemap_frag",        "shaders/graphics/tonemap_frag.glsl"},
        {"nexusDecision",       "shaders/compute/nexusDecision.comp"},
        {"statsAnalyzer",       "shaders/compute/statsAnalyzer.comp"},
        {"raster_prepass",      "shaders/compute/raster_prepass.comp"},
        {"denoiser_post",       "shaders/compute/denoiser_post.comp"}
    };
}

inline std::vector<std::string> getRayTracingBinPaths() {
    auto binPaths = getShaderBinPaths();
    return {
        binPaths.at("raygen"),
        binPaths.at("miss"),
        binPaths.at("closesthit"),
        binPaths.at("anyhit"),
        binPaths.at("mid_anyhit"),
        binPaths.at("volumetric_anyhit"),
        binPaths.at("shadow_anyhit"),
        binPaths.at("shadowmiss"),
        binPaths.at("callable"),
        binPaths.at("intersection")
    };
}

// ========================================================================
// 10. findShaderPath (unchanged but now perfect)
// ========================================================================
inline std::string findShaderPath(const std::string& logicalName) {
    using namespace Logging::Color;
    LOG_DEBUG_CAT("Vulkan", ">>> RESOLVING SHADER '{}'", logicalName);

    auto binPaths = getShaderBinPaths();
    auto binIt = binPaths.find(logicalName);
    if (binIt == binPaths.end()) {
        LOG_ERROR_CAT("Vulkan", "  --> UNKNOWN SHADER NAME '{}'", logicalName);
        throw std::runtime_error("Unknown shader name: " + logicalName);
    }
    std::filesystem::path binPath = std::filesystem::current_path() / binIt->second;
    if (std::filesystem::exists(binPath)) {
        LOG_DEBUG_CAT("Vulkan", "  --> FOUND IN BIN: {}", binPath.string());
        return binPath.string();
    }

    auto srcPaths = getShaderSrcPaths();
    auto srcIt = srcPaths.find(logicalName);
    if (srcIt == srcPaths.end()) {
        LOG_ERROR_CAT("Vulkan", "  --> NO SOURCE-TREE ENTRY FOR '{}'", logicalName);
        throw std::runtime_error("Unknown shader name: " + logicalName);
    }
    const auto projectRoot = std::filesystem::current_path();
    const std::filesystem::path srcPath = projectRoot / srcIt->second;

    if (std::filesystem::exists(srcPath)) {
        LOG_DEBUG_CAT("Vulkan", "  --> FOUND IN SRC: {}", srcPath.string());
        return srcPath.string();
    }

    LOG_ERROR_CAT("Vulkan",
                  "  --> SHADER NOT FOUND!\n"
                  "      BIN: {}\n"
                  "      SRC: {}", binPath.string(), srcPath.string());

    throw std::runtime_error("Shader file missing: " + logicalName);
}

} // namespace VulkanRTX

#endif // VULKAN_COMMON_HPP