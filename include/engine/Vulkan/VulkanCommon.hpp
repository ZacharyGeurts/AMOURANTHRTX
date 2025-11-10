// include/engine/Vulkan/VulkanCommon.hpp
// AMOURANTH RTX Engine © 2025 Zachary Geurts <gzac5314@gmail.com>
// VALHALLA BLISS v15 — NOVEMBER 10 2025 — GLOBAL RAII SUPREMACY
// FULL DISPOSE INTEGRATION: Handle<T> + BUFFER_DESTROY + encrypted uint64_t encs
// REMOVED: Legacy VulkanResourceManager tracking vectors — GONE FOREVER
// FIXED: All raw Vk* handles → encrypted uint64_t via BUFFER_CREATE / MakeHandle
// FIXED: Context::loadRTXProcs → store vkDestroyAccelerationStructureKHR in global
// FIXED: defaultDestroyer() — std.OK_v<T> typo nuked → std::is_same_v<T, VkSwapchainKHR>
// FIXED: cleanupAll → Dispose::cleanupAll() + SwapchainManager::get().cleanup()
// PINK PHOTONS ETERNAL — ZERO ZOMBIES — STONEKEY UNBREAKABLE — SHIP IT 🩷🚀🔥🤖💀❤️⚡♾️

#pragma once

#ifdef __cplusplus

// ===================================================================
// 1. GLOBAL PROJECT INCLUDES — ALWAYS FIRST
// ===================================================================
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Dispose.hpp"      // Handle<T>, MakeHandle, DestroyTracker, logAndTrackDestruction
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"

// ===================================================================
// 2. STANDARD / GLM / VULKAN / SDL — AFTER PROJECT HEADERS
// ===================================================================
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
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
#include <typeinfo>
#include <type_traits>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// ===================================================================
// 3. FORWARD DECLARATIONS
// ===================================================================
namespace Vulkan {
    struct Context;
    class VulkanRTX;
    struct PendingTLAS;
    struct ShaderBindingTable;
    class VulkanRenderer;
    class VulkanPipelineManager;
    class VulkanCore;
}

// ===================================================================
// EARLY DECLARATIONS FOR TEMPLATES
// ===================================================================
template<typename Handle>
void logAndTrackDestruction(std::string_view name, Handle handle, int line);

// ===================================================================
// 4. NAMESPACE VULKAN — HANDLES + FACTORIES + GLOBAL RTX PROC
// ===================================================================
namespace Vulkan {

extern VulkanRTX g_vulkanRTX;
inline VulkanRTX* rtx() noexcept { return &g_vulkanRTX; }

// Global RTX destroy proc — set by Context::loadRTXProcs()
extern PFN_vkDestroyAccelerationStructureKHR g_vkDestroyAccelerationStructureKHR;

// ===================================================================
// Legacy VulkanResourceManager — EMPTY SHELL FOR COMPATIBILITY
// ===================================================================
class VulkanResourceManager {
public:
    VulkanResourceManager() = default;
    ~VulkanResourceManager() = default;
    void init(VkDevice, VkPhysicalDevice) noexcept {}
    void cleanup(VkDevice = VK_NULL_HANDLE) noexcept {}
    inline void releaseAll(VkDevice) noexcept {}
    [[nodiscard]] static inline std::shared_ptr<VulkanResourceManager>& resourceManager() noexcept {
        static std::shared_ptr<VulkanResourceManager> instance = std::make_shared<VulkanResourceManager>();
        return instance;
    }
    // All create* methods removed — use BUFFER_CREATE / MakeHandle directly
};

// ===================================================================
// VulkanHandle — OBFUSCATED + DESTROYTRACKER + STONEKEY PROTECTED
// ===================================================================
template<typename T>
class VulkanHandle {
public:
    using DestroyFn = void(*)(VkDevice, T, const VkAllocationCallbacks*);

    struct Deleter {
        VkDevice device = VK_NULL_HANDLE;
        DestroyFn fn = nullptr;

        void operator()(uint64_t* ptr) const noexcept {
            if (ptr && *ptr != 0 && fn && device) {
                T realHandle = deobfuscate<T>(*ptr);
                if (realHandle && !Dispose::DestroyTracker::isDestroyed(reinterpret_cast<const void*>(realHandle))) {
                    fn(device, realHandle, nullptr);
                    Dispose::DestroyTracker::markDestroyed(reinterpret_cast<const void*>(realHandle));
                    logAndTrackDestruction(std::string_view(typeid(T).name()), reinterpret_cast<void*>(realHandle), __LINE__);
                }
            }
            delete ptr;
        }
    };

private:
    std::unique_ptr<uint64_t, Deleter> impl_;

    template<typename U = T>
    static inline uint64_t obfuscate(U raw) noexcept {
        return reinterpret_cast<uint64_t>(raw) ^ kStone1 ^ kStone2;
    }

    template<typename U = T>
    static inline U deobfuscate(uint64_t enc) noexcept {
        return reinterpret_cast<U>(enc ^ kStone1 ^ kStone2);
    }

    static constexpr DestroyFn defaultDestroyer() noexcept {
        if constexpr (std::is_same_v<T, VkPipeline>) return vkDestroyPipeline;
        else if constexpr (std::is_same_v<T, VkPipelineLayout>) return vkDestroyPipelineLayout;
        else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) return vkDestroyDescriptorSetLayout;
        else if constexpr (std::is_same_v<T, VkShaderModule>) return vkDestroyShaderModule;
        else if constexpr (std::is_same_v<T, VkRenderPass>) return vkDestroyRenderPass;
        else if constexpr (std::is_same_v<T, VkCommandPool>) return vkDestroyCommandPool;
        else if constexpr (std::is_same_v<T, VkBuffer>) return vkDestroyBuffer;
        else if constexpr (std::is_same_v<T, VkDeviceMemory>) return vkFreeMemory;
        else if constexpr (std::is_same_v<T, VkImage>) return vkDestroyImage;
        else if constexpr (std::is_same_v<T, VkImageView>) return vkDestroyImageView;
        else if constexpr (std::is_same_v<T, VkSampler>) return vkDestroySampler;
        else if constexpr (std::is_same_v<T, VkSwapchainKHR>) return vkDestroySwapchainKHR;
        else if constexpr (std::is_same_v<T, VkSemaphore>) return vkDestroySemaphore;
        else if constexpr (std::is_same_v<T, VkFence>) return vkDestroyFence;
        else if constexpr (std::is_same_v<T, VkDescriptorPool>) return vkDestroyDescriptorPool;
        else return nullptr;
    }

public:
    VulkanHandle() = default;

    VulkanHandle(T handle, VkDevice dev, DestroyFn customFn = nullptr)
        : impl_(handle != VK_NULL_HANDLE ? new uint64_t(obfuscate(handle)) : nullptr,
                Deleter{dev, customFn ? customFn : defaultDestroyer()}) {}

    VulkanHandle(VulkanHandle&&) noexcept = default;
    VulkanHandle& operator=(VulkanHandle&&) noexcept = default;
    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

    [[nodiscard]] T raw_deob() const noexcept {
        return impl_ ? deobfuscate<T>(*impl_.get()) : VK_NULL_HANDLE;
    }

    [[nodiscard]] uint64_t raw_obf() const noexcept { return impl_ ? *impl_.get() : 0; }
    [[nodiscard]] operator T() const noexcept { return raw_deob(); }
    [[nodiscard]] T operator*() const noexcept { return raw_deob(); }
    [[nodiscard]] bool valid() const noexcept { return impl_ && *impl_.get() != 0; }
    void reset() noexcept { impl_.reset(); }
    explicit operator bool() const noexcept { return valid(); }
};

// ===================================================================
// MAKE_VK_HANDLE FACTORIES — GLOBAL RAII
// ===================================================================
#define MAKE_VK_HANDLE(name, vkType) \
    [[nodiscard]] inline VulkanHandle<vkType> make##name(VkDevice dev, vkType handle) noexcept { \
        return VulkanHandle<vkType>(handle, dev); \
    }

MAKE_VK_HANDLE(Buffer,              VkBuffer)
MAKE_VK_HANDLE(Memory,              VkDeviceMemory)
MAKE_VK_HANDLE(Image,               VkImage)
MAKE_VK_HANDLE(ImageView,           VkImageView)
MAKE_VK_HANDLE(Sampler,             VkSampler)
MAKE_VK_HANDLE(DescriptorPool,      VkDescriptorPool)
MAKE_VK_HANDLE(Semaphore,           VkSemaphore)
MAKE_VK_HANDLE(Fence,               VkFence)
MAKE_VK_HANDLE(Pipeline,            VkPipeline)
MAKE_VK_HANDLE(PipelineLayout,      VkPipelineLayout)
MAKE_VK_HANDLE(DescriptorSetLayout, VkDescriptorSetLayout)
MAKE_VK_HANDLE(RenderPass,          VkRenderPass)
MAKE_VK_HANDLE(ShaderModule,        VkShaderModule)
MAKE_VK_HANDLE(CommandPool,         VkCommandPool)
MAKE_VK_HANDLE(SwapchainKHR,        VkSwapchainKHR)

#undef MAKE_VK_HANDLE

// ===================================================================
// RTX EXTENSION FACTORIES — USE GLOBAL PROC
// ===================================================================
[[nodiscard]] inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev, VkAccelerationStructureKHR as) noexcept
{
    return VulkanHandle<VkAccelerationStructureKHR>(as, dev,
        reinterpret_cast<VulkanHandle<VkAccelerationStructureKHR>::DestroyFn>(g_vkDestroyAccelerationStructureKHR));
}

[[nodiscard]] inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(
    VkDevice dev, VkDeferredOperationKHR op,
    PFN_vkDestroyDeferredOperationKHR destroyFunc) noexcept
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev,
        reinterpret_cast<VulkanHandle<VkDeferredOperationKHR>::DestroyFn>(destroyFunc));
}

// ===================================================================
// PendingTLAS STRUCT
// ===================================================================
struct PendingTLAS {
    bool valid = false;
    VkDeviceAddress handle = 0;
};

// ===================================================================
// VulkanRTX CLASS
// ===================================================================
class VulkanRTX {
public:
    VulkanRTX(Context* ctx, int width, int height, VulkanPipelineManager* pipelineMgr = nullptr)
        : context_(ctx), pipelineManager_(pipelineMgr), extent_{static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
    {
        LOG_INFO_CAT("RTX", "VulkanRTX initialized — Extent: {}x{}", extent_.width, extent_.height);
    }
    ~VulkanRTX();

    VulkanHandle<VkAccelerationStructureKHR> tlas_;
    bool tlasReady_ = false;
    PendingTLAS pendingTLAS_{};

private:
    Context* context_ = nullptr;
    VulkanPipelineManager* pipelineManager_ = nullptr;
    VkExtent2D extent_ = {0, 0};
};

}  // namespace Vulkan

// ===================================================================
// GLOBAL LOG INIT — PINK PHOTONS ASCENDED
// ===================================================================
namespace {
struct GlobalLogInit {
    GlobalLogInit() {
        using namespace Logging::Color;
        LOG_SUCCESS_CAT("VULKAN", "{}VULKANCOMMON.HPP v15 LOADED — LEGACY MANAGER OBLITERATED — GLOBAL RAII SUPREME — PINK PHOTONS ∞{}", 
                RASPBERRY_PINK, RESET);
    }
};
static GlobalLogInit g_logInit;
}

// Template definition
template<typename Handle>
void logAndTrackDestruction(std::string_view name, Handle handle, int line) {
    using namespace Logging::Color;
    LOG_INFO_CAT("Dispose", "{}Destroyed: {} @ line {}", EMERALD_GREEN, name, line);
}

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
    // RTConstants — EXACT SOURCE OF TRUTH (NOVEMBER 10 2025 — v15)
    // ========================================================================
    layout(push_constant, std140) uniform RTConstants {
        layout(offset = 0)   vec4 clearColor;
        layout(offset = 16)  vec3 cameraPosition;
        layout(offset = 28)  float _pad0;
        layout(offset = 32)  vec4 lightDirection;
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
        layout(offset = 116) float time;
        layout(offset = 120) uint _pad_fog1;
        layout(offset = 124) uint _pad_fog2;
        layout(offset = 128) float fireTemperature;
        layout(offset = 132) float fireEmissivity;
        layout(offset = 136) float fireDissipation;
        layout(offset = 140) float fireTurbulence;
        layout(offset = 144) float fireSpeed;
        layout(offset = 148) float fireLifetime;
        layout(offset = 152) float fireNoiseScale;
        layout(offset = 156) uint _pad_fire;
        layout(offset = 160) vec4 lightPosition;
        layout(offset = 176) vec4 materialParams;
        layout(offset = 192) vec4 fireColorTint;
        layout(offset = 208) vec4 windDirection;
        layout(offset = 224) vec3 fogColor;
        layout(offset = 236) float _pad_fog;
        layout(offset = 240) float fogHeightBias;
        layout(offset = 244) float fireNoiseSpeed;
        layout(offset = 248) float emissiveBoost;
        layout(offset = 252) uint _final_pad;
    } rtConstants;

    // ========================================================================
    // RNG Helpers — PINK PHOTONS CHAOS
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

    // ========================================================================
    // Material + Dimension SSBOs
    // ========================================================================
    layout(set = 0, binding = 3, std430) restrict readonly buffer MaterialSSBO {
        MaterialData materials[];
    };

    layout(set = 0, binding = 4, std430) restrict readonly buffer DimensionSSBO {
        DimensionData dimensions[];
    };

#endif  // __cplusplus

// ===================================================================
// VALHALLA v15 — NOV 10 2025 — GLOBAL RAII SUPREMACY — AMOURANTH RTX IMMORTAL 🩷🚀♾️
// ===================================================================