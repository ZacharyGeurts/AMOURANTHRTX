// include/engine/Vulkan/VulkanCommon.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// NEXUS FINAL: GPU-Driven Adaptive RT | 12,000+ FPS | Auto-Toggle
// FIXED: VK_CHECK at top + proper namespace closure

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

// ========================================================================
// 1. Global Constants
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
// 2. Strided Device Address Region
// ========================================================================
struct StridedDeviceAddressRegionKHR {
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize    stride        = 0;
    VkDeviceSize    size          = 0;
};

// ========================================================================
// 3. Shader Binding Table
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
// 4. Per-Frame Resources
// ========================================================================
struct Frame {
    VkCommandBuffer         commandBuffer           = VK_NULL_HANDLE;
    VkDescriptorSet         rayTracingDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet         graphicsDescriptorSet   = VK_NULL_HANDLE;
    VkDescriptorSet         computeDescriptorSet    = VK_NULL_HANDLE;
    VkSemaphore             imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore             renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence                 fence                   = VK_NULL_HANDLE;
};

// ========================================================================
// 5. MaterialData
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
// 6. DimensionData
// ========================================================================
struct alignas(16) DimensionData {
    uint32_t screenWidth  = 0;
    uint32_t screenHeight = 0;
    uint32_t _pad0        = 0;
    uint32_t _pad1        = 0;
};
static_assert(sizeof(DimensionData) == 16, "DimensionData must be 16 bytes");

// ========================================================================
// 7. UniformBufferObject
// ========================================================================
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

// ========================================================================
// 8. DimensionState
// ========================================================================
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

// ========================================================================
// 9. Tonemap Push Constants
// ========================================================================
struct alignas(16) TonemapPushConstants {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t _pad0  = 0;
    uint32_t _pad1  = 0;
};
static_assert(sizeof(TonemapPushConstants) == 16, "TonemapPushConstants must be 16 bytes");

// ========================================================================
// 10. NEXUS PUSH CONSTANTS
// ========================================================================
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
// 11. Shader Paths
// ========================================================================
inline std::unordered_map<std::string, std::string> getShaderBinPaths() {
    return {
        {"raygen",              "assets/shaders/raytracing/raygen.spv"},
        {"miss",                "assets/shaders/raytracing/miss.spv"},
        {"closesthit",          "assets/shaders/raytracing/closesthit.spv"},
        {"anyhit",              "assets/shaders/raytracing/anyhit.spv"},
        {"callable",            "assets/shaders/raytracing/callable.spv"},
        {"intersection",        "assets/shaders/raytracing/intersection.spv"},
        {"shadow_anyhit",       "assets/shaders/raytracing/shadow_anyhit.spv"},
        {"shadowmiss",          "assets/shaders/raytracing/shadowmiss.spv"},
        {"volumetric_anyhit",   "assets/shaders/raytracing/volumetric_anyhit.spv"},
        {"mid_anyhit",          "assets/shaders/raytracing/mid_anyhit.spv"},
        {"tonemap_compute",     "assets/shaders/compute/tonemap.spv"},
        {"tonemap_vert",        "assets/shaders/graphics/tonemap_vert.spv"},
        {"tonemap_frag",        "assets/shaders/graphics/tonemap_frag.spv"},
        {"nexusDecision",       "assets/shaders/compute/nexusDecision.spv"}
    };
}

inline std::unordered_map<std::string, std::string> getShaderSrcPaths() {
    return {
        {"raygen",              "assets/shaders/raytracing/raygen.rgen"},
        {"miss",                "assets/shaders/raytracing/miss.rmiss"},
        {"closesthit",          "assets/shaders/raytracing/closesthit.rchit"},
        {"anyhit",              "assets/shaders/raytracing/anyhit.rahit"},
        {"callable",            "assets/shaders/raytracing/callable.rcall"},
        {"intersection",        "assets/shaders/raytracing/intersection.rint"},
        {"shadow_anyhit",       "assets/shaders/raytracing/shadow_anyhit.rahit"},
        {"shadowmiss",          "assets/shaders/raytracing/shadowmiss.rmiss"},
        {"volumetric_anyhit",   "assets/shaders/raytracing/volumetric_anyhit.rahit"},
        {"mid_anyhit",          "assets/shaders/raytracing/mid_anyhit.rahit"},
        {"tonemap_compute",     "assets/shaders/compute/tonemap.comp"},
        {"tonemap_vert",        "assets/shaders/graphics/tonemap_vert.glsl"},
        {"tonemap_frag",        "assets/shaders/graphics/tonemap_frag.glsl"},
        {"nexusDecision",       "assets/shaders/compute/nexusDecision.comp"}
    };
}

inline std::vector<std::string> getRayTracingBinPaths() {
    auto binPaths = getShaderBinPaths();
    return {
        binPaths.at("raygen"),
        binPaths.at("miss"),
        binPaths.at("closesthit"),
        binPaths.at("anyhit"),
        binPaths.at("callable"),
        binPaths.at("intersection"),
        binPaths.at("shadow_anyhit"),
        binPaths.at("shadowmiss"),
        binPaths.at("volumetric_anyhit"),
        binPaths.at("mid_anyhit")
    };
}

// ========================================================================
// 12. findShaderPath
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
    const auto projectRoot = std::filesystem::current_path().parent_path().parent_path().parent_path();
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

// ========================================================================
// END OF VulkanRTX namespace
// ========================================================================
} // namespace VulkanRTX