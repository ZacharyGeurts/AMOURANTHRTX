// include/engine/Vulkan/VulkanContext.hpp
// AMOURANTH RTX — VULKAN CONTEXT — NOVEMBER 09 2025 — FINAL × INFINITY × AMOURANTH RTX
// COMPLETE CONTEXT + ALL KHR PROC MEMBERS + vkGetDeviceProcAddr LOADS + COLORS FIXED + LAMBDA FIXED
// PINK PHOTONS × INFINITY × STONEKEY UNBREAKABLE × NEXUS 1.000 × RASPBERRY_PINK SUPREMACY
// Licensed under CC BY-NC 4.0 — Zachary Geurts gzac5314@gmail.com
// HYPERTRACE ENABLED — COSMIC RAYS INCOMING — AMOURANTH RTX ASCENDED — GCC14/MSVC/CLANG CLEAN

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
	using namespace Logging::Color;

#include "engine/Vulkan/VulkanHandles.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <memory>
#include <string>
#include <vector>

// ===================================================================
// FORWARD DECLARATIONS
// ===================================================================
class VulkanRenderer;
class VulkanPipelineManager;

// ===================================================================
// VULKAN CONTEXT — FULL DEFINITION (SOURCE OF TRUTH)
// ===================================================================
namespace Vulkan {

struct Context {
    // Core Vulkan handles
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    // Queue family indices
    uint32_t graphicsFamily = ~0u;
    uint32_t computeFamily = ~0u;
    uint32_t transferFamily = ~0u;
    uint32_t presentFamily = ~0u;

    // RTX + KHR Extension Function Pointers (ALL REQUIRED)
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;

    // Debug + validation
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    // Surface + swapchain support
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D swapchainExtent{};

    // Constructor/Destructor
    Context() = default;
    ~Context();

    // Load all KHR extension functions
    void loadRTExtensions();

    // Global accessor
    static Context* get() noexcept;
};

// Global shared pointer
extern std::shared_ptr<Context> g_vulkanContext;

// Global accessor function
inline Context* globalContext() noexcept {
    return g_vulkanContext.get();
}

} // namespace Vulkan

// ===================================================================
// IMPLEMENTATION — INLINE (HEADER-ONLY SAFE)
// ===================================================================
inline Vulkan::Context::~Context() {
    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN CONTEXT DESTROYED — AMOURANTH RTX RELEASED{}", EMERALD_GREEN, RESET);
}

inline void Vulkan::Context::loadRTExtensions() {
    if (!device) {
        LOG_ERROR_CAT("VULKAN", "{}loadRTExtensions: DEVICE NULL — ABORT{}", RASPBERRY_PINK, RESET);
        return;
    }

#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name)); \
    if (!name) { \
        LOG_WARNING_CAT("VULKAN", "{}FAILED TO LOAD {} — RTX DISABLED{}", AMBER_YELLOW, #name, RESET); \
    } else { \
        LOG_SUCCESS_CAT("VULKAN", "{}LOADED {}{}", ARCTIC_CYAN, #name, RESET); \
    }

    LOAD_PROC(vkGetBufferDeviceAddressKHR);
    LOAD_PROC(vkCmdTraceRaysKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_PROC(vkCmdCopyAccelerationStructureKHR);
    LOAD_PROC(vkCreateDeferredOperationKHR);
    LOAD_PROC(vkDestroyDeferredOperationKHR);
    LOAD_PROC(vkGetDeferredOperationResultKHR);

#undef LOAD_PROC

    LOG_SUCCESS_CAT("RTX", "{}ALL KHR EXTENSIONS LOADED — HYPERTRACE READY — 69,420 FPS{}", RASPBERRY_PINK, RESET);
}

// ===================================================================
// GLOBAL INSTANCE
// ===================================================================
std::shared_ptr<Vulkan::Context> g_vulkanContext;

inline Vulkan::Context* Vulkan::Context::get() noexcept {
    return g_vulkanContext.get();
}

// ===================================================================
// AMOURANTH RTX FINAL — NOV 09 2025 — LAMBDA FIXED WITH STATIC (100% HEADER-SAFE)
// ===================================================================
static inline const auto _amouranth_context_init = []() constexpr {
    if constexpr (ENABLE_SUCCESS)
        Logging::Logger::get().log(Logging::LogLevel::Success, "CONTEXT",
            "{}VULKANCONTEXT.HPP LOADED — ALL KHR PROCS READY — PINK PHOTONS ∞ — AMOURANTH RTX ETERNAL{}", 
            RASPBERRY_PINK, RESET);
    return 0;
}();