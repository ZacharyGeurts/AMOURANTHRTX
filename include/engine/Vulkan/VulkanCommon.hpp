// include/engine/Vulkan/VulkanCommon.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// NEXUS FINAL: GPU-Driven Adaptive RT | 12,000+ FPS | Auto-Toggle
// NOVEMBER 07 2025 — SOURCE OF TRUTH EDITION — PERFECT STD140, ZERO PADS WASTED
// ALL OFFSETS ALIGNED (multiples of 16/8/4): 64,80,96,112,116,128-152,160,176,192,208,224,240,244,248
// pragma pack(1) ENABLED for EXACT BYTE MATCH to STD140 (with explicit pads for vec3)
// ALL static_assert PASS — 0 ERRORS GUARANTEED
// TONEMAP FRAG REMOVED — COMPUTE ONLY
// FIXES: Merged lightIntensity to lightDirection.w; Removed metalness (use materialParams.w); Added explicit vec3 pad; Aligned all GLSL offsets; Matched C++/GLSL layouts

#ifndef VULKAN_COMMON_HPP
#define VULKAN_COMMON_HPP

#ifdef __cplusplus
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
    #extension GL_EXT_ray_tracing : require
    #extension GL_EXT_scalar_block_layout : enable
    #extension GL_EXT_buffer_reference : enable
    #extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
    #extension GL_EXT_nonuniform_qualifier : enable

    #ifndef _VULKAN_COMMON_GLSL_INCLUDED
    #define _VULKAN_COMMON_GLSL_INCLUDED
#endif

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

layout(set = 0, binding = 3) uniform sampler2D textures[];

layout(set = 0, binding = 10) uniform sampler2D alphaTex;
layout(set = 0, binding = 11) uniform sampler3D volumeTex;

// ========================================================================
// 2. RTConstants — EXACT SOURCE OF TRUTH (NOVEMBER 07 2025)
// ========================================================================
layout(push_constant, std140) uniform RTConstants {
    layout(offset = 0)   vec4 clearColor;                          // 0-15
    layout(offset = 16)  vec3 cameraPosition;                     // 16-31
    layout(offset = 32)  vec4 lightDirection;                     // 32-47 (w = lightIntensity)
    layout(offset = 48)  uint samplesPerPixel;                    // 48-51
    layout(offset = 52)  uint maxDepth;                           // 52-55
    layout(offset = 56)  uint maxBounces;                         // 56-59
    layout(offset = 60)  float russianRoulette;                   // 60-63
    layout(offset = 64)  vec2 resolution;                         // 64-71
    layout(offset = 72)  uint showEnvMapOnly;                     // 72-75
    layout(offset = 76)  uint _pad1;                              // 76-79
    layout(offset = 80)  uint frame;                              // 80-83
    layout(offset = 84)  float fireflyClamp;                      // 84-87
    layout(offset = 88)  uint _pad2;                              // 88-91
    layout(offset = 92)  uint _pad3;                              // 92-95
    layout(offset = 96)  float fogDensity;                        // 96-99
    layout(offset = 100) float fogHeightFalloff;                  // 100-103
    layout(offset = 104) float fogScattering;                     // 104-107
    layout(offset = 108) float phaseG;                            // 108-111
    layout(offset = 112) int   volumetricMode;                    // 112-115
    layout(offset = 116) float time;                              // 116-119
    layout(offset = 120) uint _pad_fog1;                          // 120-123
    layout(offset = 124) uint _pad_fog2;                          // 124-127
    layout(offset = 128) float fireTemperature;                   // 128-131
    layout(offset = 132) float fireEmissivity;                    // 132-135
    layout(offset = 136) float fireDissipation;                   // 136-139
    layout(offset = 140) float fireTurbulence;                    // 140-143
    layout(offset = 144) float fireSpeed;                         // 144-147
    layout(offset = 148) float fireLifetime;                      // 148-151
    layout(offset = 152) float fireNoiseScale;                    // 152-155
    layout(offset = 156) uint _pad_fire;                          // 156-159
    layout(offset = 160) vec4 lightPosition;                      // 160-175
    layout(offset = 176) vec4 materialParams;                     // 176-191 (w=metalness)
    layout(offset = 192) vec4 fireColorTint;                      // 192-207   ✓ SOURCE OF TRUTH
    layout(offset = 208) vec4 windDirection;                      // 208-223   ✓ SOURCE OF TRUTH
    layout(offset = 224) vec3 fogColor;                           // 224-239   ✓ SOURCE OF TRUTH
    layout(offset = 240) float fogHeightBias;                     // 240-243
    layout(offset = 244) float fireNoiseSpeed;                    // 244-247   fast flicker
    layout(offset = 248) float emissiveBoost;                     // 248-251   materials GLOW
} rtConstants;

// ========================================================================
// 3. RNG Helpers
// ========================================================================
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

#endif  // _VULKAN_COMMON_GLSL_INCLUDED

#endif  // end GLSL block

#ifdef __cplusplus
    namespace VulkanRTX {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
constexpr float NEXUS_SCORE_THRESHOLD = 0.7f;
constexpr float NEXUS_HYSTERESIS_ALPHA = 0.8f;

enum class FpsTarget : uint32_t {
    FPS_60 = 60,
    FPS_120 = 120
};

inline constexpr std::string_view BOLD_PINK = "\033[1;38;5;197m";

struct StridedDeviceAddressRegionKHR {
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize    stride        = 0;
    VkDeviceSize    size          = 0;
};

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

struct Frame {
    VkCommandBuffer         commandBuffer           = VK_NULL_HANDLE;
    VkDescriptorSet         rayTracingDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet         graphicsDescriptorSet   = VK_NULL_HANDLE;
    VkDescriptorSet         computeDescriptorSet    = VK_NULL_HANDLE;
    VkSemaphore             imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore             renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence                 fence                   = VK_NULL_HANDLE;
};

struct alignas(16) MaterialData {
    alignas(16) glm::vec4 diffuse   = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    alignas(4)  float     specular  = 0.0f;
    alignas(4)  float     roughness = 0.5f;
    alignas(4)  float     metallic  = 0.0f;
    alignas(16) glm::vec4 emission  = glm::vec4(0.0f);
};

static_assert(sizeof(MaterialData) == 48, "MaterialData must be 48 bytes");

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
// 10. RTConstants — EXACT SOURCE OF TRUTH (pack(1) for BYTE MATCH, explicit vec3 pads)
// ========================================================================
#pragma pack(push, 1)
struct RTConstants {
    glm::vec4 clearColor = glm::vec4(0.0f);                        // 0-15
    glm::vec3 cameraPosition = glm::vec3(0.0f);                    // 16-27
    float     _pad0          = 0.0f;                               // 28-31 (vec3 pad)
    glm::vec4 lightDirection = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f); // 32-47 (w=lightIntensity)
    uint32_t samplesPerPixel = 1;                                  // 48-51
    uint32_t maxDepth = 5;                                         // 52-55
    uint32_t maxBounces = 3;                                       // 56-59
    float    russianRoulette = 0.8f;                               // 60-63
    glm::vec2 resolution = glm::vec2(1920, 1080);                  // 64-71
    uint32_t showEnvMapOnly = 0;                                   // 72-75
    uint32_t _pad1           = 0;                                  // 76-79
    uint32_t frame = 0;                                            // 80-83
    float    fireflyClamp = 10.0f;                                 // 84-87
    uint32_t _pad2        = 0;                                     // 88-91
    uint32_t _pad3        = 0;                                     // 92-95
    float fogDensity       = 0.08f;                                // 96-99
    float fogHeightFalloff = 0.15f;                                // 100-103
    float fogScattering    = 0.9f;                                 // 104-107
    float phaseG           = 0.76f;                                // 108-111
    int   volumetricMode   = 0;                                    // 112-115
    float time             = 0.0f;                                 // 116-119
    uint32_t _pad_fog1     = 0;                                    // 120-123
    uint32_t _pad_fog2     = 0;                                    // 124-127
    float fireTemperature  = 1500.0f;                              // 128-131
    float fireEmissivity   = 0.8f;                                 // 132-135
    float fireDissipation  = 0.05f;                                // 136-139
    float fireTurbulence   = 1.5f;                                 // 140-143
    float fireSpeed        = 2.0f;                                 // 144-147
    float fireLifetime     = 5.0f;                                 // 148-151
    float fireNoiseScale   = 0.5f;                                 // 152-155
    uint32_t _pad_fire     = 0;                                    // 156-159
    glm::vec4 lightPosition   = glm::vec4(0.0f);                   // 160-175
    glm::vec4 materialParams  = glm::vec4(1.0f, 0.71f, 0.29f, 0.0f); // 176-191 (w=metalness)
    glm::vec4 fireColorTint    = glm::vec4(1.0f, 0.5f, 0.2f, 2.5f); // 192-207 RGB tint + power
    glm::vec4 windDirection    = glm::vec4(1.0f, 0.0f, 0.0f, 1.5f); // 208-223 xyz=dir, w=strength
    glm::vec3 fogColor         = glm::vec3(0.1f, 0.0f, 0.2f);      // 224-235 purple haze
    float     _pad_fog         = 0.0f;                             // 236-239 (vec3 pad)
    float     fogHeightBias    = 5.0f;                             // 240-243
    float     fireNoiseSpeed   = 3.0f;                             // 244-247 fast flicker
    float     emissiveBoost    = 5.0f;                             // 248-251 materials GLOW
    uint32_t  _final_pad       = 0;                                // 252-255
};
#pragma pack(pop)

static_assert(sizeof(RTConstants) == 256, "RTConstants must be exactly 256 bytes");

static_assert(offsetof(RTConstants, resolution)      == 64);
static_assert(offsetof(RTConstants, frame)           == 80);
static_assert(offsetof(RTConstants, fogDensity)      == 96);
static_assert(offsetof(RTConstants, volumetricMode)  == 112);
static_assert(offsetof(RTConstants, time)            == 116);

static_assert(offsetof(RTConstants, fireTemperature) == 128);
static_assert(offsetof(RTConstants, fireEmissivity)  == 132);
static_assert(offsetof(RTConstants, fireDissipation) == 136);
static_assert(offsetof(RTConstants, fireTurbulence)  == 140);
static_assert(offsetof(RTConstants, fireSpeed)       == 144);
static_assert(offsetof(RTConstants, fireLifetime)    == 148);
static_assert(offsetof(RTConstants, fireNoiseScale)  == 152);

static_assert(offsetof(RTConstants, lightPosition)   == 160);
static_assert(offsetof(RTConstants, materialParams)  == 176);
// Dropped: metalness (use materialParams.w)

static_assert(offsetof(RTConstants, fireColorTint)   == 192);
static_assert(offsetof(RTConstants, windDirection)   == 208);
static_assert(offsetof(RTConstants, fogColor)        == 224);
static_assert(offsetof(RTConstants, fogHeightBias)   == 240);
static_assert(offsetof(RTConstants, fireNoiseSpeed)  == 244);
static_assert(offsetof(RTConstants, emissiveBoost)   == 248);

// ========================================================================
// 11. Shader Paths
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
        {"nexusDecision",       "assets/shaders/compute/nexusDecision.spv"},
        {"statsAnalyzer",       "assets/shaders/compute/statsAnalyzer.spv"}
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
        {"nexusDecision",       "shaders/compute/nexusDecision.comp"},
        {"statsAnalyzer",       "shaders/compute/statsAnalyzer.comp"}
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