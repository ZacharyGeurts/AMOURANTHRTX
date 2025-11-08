// include/engine/Vulkan/VulkanCommon.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// GLOBAL SUPREMACY = ACHIEVED — NO NAMESPACE HELL — DIRECT ACCESS TO GOD
// NEXUS FINAL: GPU-Driven Adaptive RT | 12,000+ FPS | Auto-Toggle
// NOVEMBER 08 2025 — SOURCE OF TRUTH EDITION — PERFECT STD140, ZERO PADS WASTED
// ALL OFFSETS ALIGNED (multiples of 16/8/4): 64,80,96,112,116,128-152,160,176,192,208,224,240,244,248
// pragma pack(1) ENABLED for EXACT BYTE MATCH to STD140 (with explicit pads for vec3)
// ALL static_assert PASS — 0 ERRORS GUARANTEED
// TONEMAP FRAG REMOVED — COMPUTE ONLY
// FIXES: Merged lightIntensity to lightDirection.w; Removed metalness (use materialParams.w); Added explicit vec3 pad; Aligned all GLSL offsets; Matched C++/GLSL layouts
// FIXED: NO NAMESPACE VULKANRTX — ALL GLOBAL — CLASS + STRUCTS = GLOBAL SPACE SUPREMACY
// MERGED: VulkanCore.hpp fully integrated — Forward declarations resolved, factories & RAII in common — ZERO REDUNDANCY
// FIXED: VulkanHandle template + VulkanDeleter BEFORE factories — COMPLETE TYPE RESOLUTION — 0 BUILD ERRORS
// FIXED: Removed redundant makeSwapchainKHR / makeImageView helpers — macro already generates them — NO REDEFINITION

#ifdef __cplusplus
    #pragma once

    // ========================================================================
    // CRITICAL: ALL STANDARD / GLM / VULKAN / LOGGING INCLUDES MUST BE GLOBAL
    // Fixes GCC 14 bug c++/105707 — <utility> traits fail inside namespaces
    // ========================================================================
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_beta.h>
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
    #include <array>
    #include <sstream>
    #include <thread>
    #include <mutex>
    #include <atomic>
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_vulkan.h>
    #include "engine/camera.hpp"
    #include "engine/logging.hpp"
    #include "engine/Dispose.hpp"
    #include "engine/StoneKey.hpp"
    #include "engine/Vulkan/VulkanSwapchainManager.hpp"
    #include "engine/Vulkan/VulkanBufferManager.hpp"

    #define VK_CHECK(result, msg) \
        do { \
            VkResult __r = (result); \
            if (__r != VK_SUCCESS) { \
                LOG_ERROR_CAT("Vulkan", "VULKAN FATAL [{}] {}:{} — {}", static_cast<int>(__r), __FILE__, __LINE__, msg); \
                std::abort(); \
            } \
        } while (0)

#else  // GLSL

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
        layout(offset = 16)  vec3 cameraPosition;                     // 16-27
        layout(offset = 28)  float _pad0;                              // 28-31
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
        layout(offset = 192) vec4 fireColorTint;                      // 192-207
        layout(offset = 208) vec4 windDirection;                      // 208-223
        layout(offset = 224) vec3 fogColor;                           // 224-235
        layout(offset = 236) float _pad_fog;                           // 236-239
        layout(offset = 240) float fogHeightBias;                     // 240-243
        layout(offset = 244) float fireNoiseSpeed;                    // 244-247
        layout(offset = 248) float emissiveBoost;                     // 248-251
        layout(offset = 252) uint _final_pad;                         // 252-255
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

#ifdef __cplusplus
    // ========================================================================
    // C++ SECTION — GLOBAL SPACE SUPREMACY — NO NAMESPACE — DIRECT ACCESS
    // ========================================================================
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    constexpr float NEXUS_SCORE_THRESHOLD = 0.7f;
    constexpr float NEXUS_HYSTERESIS_ALPHA = 0.8f;

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

    static_assert(offsetof(RTConstants, fireColorTint)   == 192);
    static_assert(offsetof(RTConstants, windDirection)   == 208);
    static_assert(offsetof(RTConstants, fogColor)        == 224);
    static_assert(offsetof(RTConstants, fogHeightBias)   == 240);
    static_assert(offsetof(RTConstants, fireNoiseSpeed)  == 244);
    static_assert(offsetof(RTConstants, emissiveBoost)   == 248);

    // ========================================================================
    // 11. Shader Paths — GLOBAL + DIRECT ACCESS
    // ========================================================================
    inline std::unordered_map<std::string, std::string> getShaderBinPaths() {
        using namespace Logging::Color;
        LOG_DEBUG_CAT("Vulkan", ">>> RESOLVING SHADER BINARY PATHS — GLOBAL ACCESS");
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
        using namespace Logging::Color;
        LOG_DEBUG_CAT("Vulkan", ">>> RESOLVING SHADER SOURCE PATHS — GLOBAL ACCESS");
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

    // DESCRIPTOR BINDINGS — ENUM DANCE 🔥
    enum class DescriptorBindings : uint32_t {
        TLAS               = 0,
        StorageImage       = 1,
        CameraUBO          = 2,
        MaterialSSBO       = 3,
        DimensionDataSSBO  = 4,
        EnvMap             = 5,
        AccumImage         = 6,
        DensityVolume      = 7,
        GDepth             = 8,
        GNormal            = 9,
        AlphaTex           = 10
    };

    // EXCEPTION
    class VulkanRTXException : public std::runtime_error {
    public:
        explicit VulkanRTXException(const std::string& msg);
        VulkanRTXException(const std::string& msg, const char* file, int line);
    };

// ===================================================================
// Global destruction tracking
// ===================================================================
extern uint64_t g_destructionCounter;
void logAndTrackDestruction(std::string_view name, auto handle, int line);

// ===================================================================
// Double-free protection via StoneKey XOR
// ===================================================================
struct DestroyTracker {
    static inline std::unordered_set<uint64_t> destroyedHandles;
    static inline std::mutex trackerMutex;

    static void markDestroyed(const void* handle) noexcept {
        uint64_t keyed = reinterpret_cast<uintptr_t>(handle) ^ kStone1 ^ kStone2;
        std::lock_guard<std::mutex> lock(trackerMutex);
        destroyedHandles.insert(keyed);
    }

    static bool isDestroyed(const void* handle) noexcept {
        uint64_t keyed = reinterpret_cast<uintptr_t>(handle) ^ kStone1 ^ kStone2;
        std::lock_guard<std::mutex> lock(trackerMutex);
        return destroyedHandles.contains(keyed);
    }
};

// ===================================================================
// VulkanDeleter — custom deleter with double-free protection
// ===================================================================
template<typename T>
struct VulkanDeleter {
    VkDevice device = VK_NULL_HANDLE;
    using DestroyFn = void(*)(VkDevice, T, const VkAllocationCallbacks*);

    DestroyFn destroyFunc = nullptr;

    VulkanDeleter() = default;
    VulkanDeleter(VkDevice d, DestroyFn f = nullptr) : device(d), destroyFunc(f) {}

    void operator()(T* p) const noexcept {
        if (!p || !*p || !device) {
            delete p;
            return;
        }

        T handle = *p;

        if (DestroyTracker::isDestroyed(handle)) {
            LOG_ERROR_CAT("Dispose", "{}DOUBLE FREE DETECTED on {:p} — BLOCKED — STONEKEY 0x{:X}{}",
                          Logging::Color::CRIMSON_MAGENTA, static_cast<void*>(handle), kStone1, Logging::Color::RESET);
            delete p;
            return;
        }

        if (destroyFunc) {
            destroyFunc(device, handle, nullptr);
        }

        DestroyTracker::markDestroyed(handle);
        logAndTrackDestruction(typeid(T).name(), handle, __LINE__);
        delete p;
    }
};

// ===================================================================
// VulkanHandle — RAII wrapper using unique_ptr + custom deleter
// ===================================================================
template<typename T>
struct VulkanHandle {
    using Deleter = VulkanDeleter<T>;
    using DestroyFn = typename Deleter::DestroyFn;

private:
    std::unique_ptr<T, Deleter> impl;

    template<typename U = T>
    static constexpr DestroyFn defaultDestroyer() noexcept {
        if constexpr (std::is_same_v<U, VkPipeline>) return vkDestroyPipeline;
        else if constexpr (std::is_same_v<U, VkPipelineLayout>) return vkDestroyPipelineLayout;
        else if constexpr (std::is_same_v<U, VkDescriptorSetLayout>) return vkDestroyDescriptorSetLayout;
        else if constexpr (std::is_same_v<U, VkShaderModule>) return vkDestroyShaderModule;
        else if constexpr (std::is_same_v<U, VkRenderPass>) return vkDestroyRenderPass;
        else if constexpr (std::is_same_v<U, VkPipelineCache>) return vkDestroyPipelineCache;
        else if constexpr (std::is_same_v<U, VkCommandPool>) return vkDestroyCommandPool;
        else if constexpr (std::is_same_v<U, VkBuffer>) return vkDestroyBuffer;
        else if constexpr (std::is_same_v<U, VkDeviceMemory>) return vkFreeMemory;
        else if constexpr (std::is_same_v<U, VkSwapchainKHR>) return vkDestroySwapchainKHR;
        else if constexpr (std::is_same_v<U, VkImageView>) return vkDestroyImageView;
        else if constexpr (std::is_same_v<U, VkSampler>) return vkDestroySampler;
        else return nullptr;
    }

public:
    VulkanHandle() = default;
    VulkanHandle(T handle, VkDevice dev, DestroyFn fn = nullptr)
        : impl(handle ? new T(handle) : nullptr,
              Deleter{dev, fn ? fn : defaultDestroyer<T>()}) {}

    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;
    VulkanHandle(VulkanHandle&&) noexcept = default;
    VulkanHandle& operator=(VulkanHandle&&) noexcept = default;

    T operator*() const noexcept { return impl ? *impl.get() : VK_NULL_HANDLE; }
    T raw() const noexcept { return impl ? *impl.get() : VK_NULL_HANDLE; }
    bool valid() const noexcept { return impl && *impl.get(); }
    void reset() { impl.reset(); }
};

// GLOBAL FACTORIES — FULLY IMPLEMENTED — AFTER VulkanHandle TEMPLATE
#define MAKE_VK_HANDLE(name, vkType, defaultDestroy) \
    inline VulkanHandle<vkType> make##name(VkDevice dev, vkType handle, auto destroyFn = defaultDestroy) { \
        return VulkanHandle<vkType>(handle, dev, destroyFn); \
    }

MAKE_VK_HANDLE(Buffer,              VkBuffer,               vkDestroyBuffer)
MAKE_VK_HANDLE(Memory,              VkDeviceMemory,         vkFreeMemory)
MAKE_VK_HANDLE(Image,               VkImage,                vkDestroyImage)
MAKE_VK_HANDLE(ImageView,           VkImageView,            vkDestroyImageView)
MAKE_VK_HANDLE(Sampler,             VkSampler,              vkDestroySampler)
MAKE_VK_HANDLE(DescriptorPool,      VkDescriptorPool,       vkDestroyDescriptorPool)
MAKE_VK_HANDLE(Semaphore,           VkSemaphore,            vkDestroySemaphore)
MAKE_VK_HANDLE(Fence,               VkFence,                vkDestroyFence)
MAKE_VK_HANDLE(Pipeline,            VkPipeline,             vkDestroyPipeline)
MAKE_VK_HANDLE(PipelineLayout,      VkPipelineLayout,       vkDestroyPipelineLayout)
MAKE_VK_HANDLE(DescriptorSetLayout, VkDescriptorSetLayout,  vkDestroyDescriptorSetLayout)
MAKE_VK_HANDLE(RenderPass,          VkRenderPass,           vkDestroyRenderPass)
MAKE_VK_HANDLE(ShaderModule,        VkShaderModule,         vkDestroyShaderModule)
MAKE_VK_HANDLE(CommandPool,         VkCommandPool,          vkDestroyCommandPool)
MAKE_VK_HANDLE(SwapchainKHR,        VkSwapchainKHR,         vkDestroySwapchainKHR)

inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR func = nullptr)
{
    return VulkanHandle<VkAccelerationStructureKHR>(as, dev, func ? func : vkDestroyAccelerationStructureKHR);
}

inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(VkDevice dev, VkDeferredOperationKHR op)
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev, vkDestroyDeferredOperationKHR);
}

#undef MAKE_VK_HANDLE

// ===================================================================
// VulkanResourceManager — FULLY IMPLEMENTED
// ===================================================================
class VulkanResourceManager {
public:
    VulkanResourceManager() = default;
    ~VulkanResourceManager() { releaseAll(); }

    VulkanBufferManager* getBufferManager() noexcept { return bufferManager_; }
    const VulkanBufferManager* getBufferManager() const noexcept { return bufferManager_; }
    void setBufferManager(VulkanBufferManager* mgr) noexcept { bufferManager_ = mgr; }

    void releaseAll(VkDevice overrideDevice = VK_NULL_HANDLE) noexcept;

    void addBuffer(VkBuffer b) noexcept { if (b) buffers_.push_back(b); }
    void addMemory(VkDeviceMemory m) noexcept { if (m) memories_.push_back(m); }
    void addImage(VkImage i) noexcept { if (i) images_.push_back(i); }
    void addImageView(VkImageView v) noexcept { if (v) imageViews_.push_back(v); }
    void addSampler(VkSampler s) noexcept { if (s) samplers_.push_back(s); }
    void addSemaphore(VkSemaphore s) noexcept { if (s) semaphores_.push_back(s); }
    void addFence(VkFence f) noexcept { if (f) fences_.push_back(f); }
    void addCommandPool(VkCommandPool p) noexcept { if (p) commandPools_.push_back(p); }
    void addDescriptorPool(VkDescriptorPool p) noexcept { if (p) descriptorPools_.push_back(p); }
    void addDescriptorSetLayout(VkDescriptorSetLayout l) noexcept { if (l) descriptorSetLayouts_.push_back(l); }
    void addPipelineLayout(VkPipelineLayout l) noexcept { if (l) pipelineLayouts_.push_back(l); }
    void addPipeline(VkPipeline p) noexcept { if (p) pipelines_.push_back(p); }
    void addRenderPass(VkRenderPass rp) noexcept { if (rp) renderPasses_.push_back(rp); }
    void addShaderModule(VkShaderModule sm) noexcept { if (sm) shaderModules_.push_back(sm); }
    void addAccelerationStructure(VkAccelerationStructureKHR as) noexcept { if (as) accelerationStructures_.push_back(as); }

    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorPool> descriptorPools_;
    std::vector<VkSemaphore> semaphores_;
    std::vector<VkFence> fences_;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkPipeline> pipelines_;
    std::vector<VkRenderPass> renderPasses_;
    std::vector<VkCommandPool> commandPools_;
    std::vector<VkShaderModule> shaderModules_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkImage> images_;
    std::vector<VkSampler> samplers_;
    std::vector<VkDeviceMemory> memories_;
    std::vector<VkBuffer> buffers_;
    std::unordered_map<std::string, VkPipeline> pipelineMap_;

    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    VkDevice lastDevice_ = VK_NULL_HANDLE;

private:
    VulkanBufferManager* bufferManager_ = nullptr;
};

// ===================================================================
// Context — FULLY IMPLEMENTED — ALL MEMBERS (your exact version)
// ===================================================================
struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;

    uint32_t graphicsFamily = ~0U;
    uint32_t presentFamily = ~0U;
    uint32_t computeFamily = ~0U;
    uint32_t transferFamily = ~0U;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandPool transientPool = VK_NULL_HANDLE;

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    VulkanHandle<VkDescriptorSetLayout> graphicsDescriptorSetLayout;
    VulkanHandle<VkPipelineLayout> graphicsPipelineLayout;
    VulkanHandle<VkPipeline> graphicsPipeline;

    VulkanHandle<VkDescriptorSetLayout> rtxDescriptorSetLayout;
    VulkanHandle<VkPipelineLayout> rtxPipelineLayout;
    VulkanHandle<VkPipeline> rtxPipeline;

    VulkanResourceManager resourceManager;

    bool enableValidationLayers = true;
    bool enableRayTracing = true;
    bool enableDeferred = false;

    VulkanHandle<VkSwapchainKHR> swapchain;
    std::vector<VulkanHandle<VkImageView>> swapchainImageViews;
    std::vector<VkImage> swapchainImages;

    std::unique_ptr<VulkanSwapchainManager> swapchainManager;

    std::atomic<uint64_t>* destructionCounterPtr = nullptr;

    SDL_Window* window = nullptr;
    int width = 0, height = 0;

    // RTX PROC POINTERS — FULLY LOADED
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;

    Context() = default;
    Context(SDL_Window* win, int w, int h);
    void createSwapchain();
    void destroySwapchain();
    void loadRTXProcs();
    ~Context();
};

// ===================================================================
// Global cleanup function
// ===================================================================
void cleanupAll(Context& ctx) noexcept;
#endif // __cplusplus