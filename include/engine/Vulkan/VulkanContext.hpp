// include/engine/Vulkan/VulkanContext.hpp
// AMOURANTH RTX Engine © 2025 Zachary Geurts — NOVEMBER 10 2025 — VALHALLA v17
// RESOURCE MANAGER DELETED — DISPOSE::HANDLE IS GOD — PINK PHOTONS ETERNAL
// NO VECTORS | NO releaseAll | NO init() | ONLY RAII LOVE
// Gentleman Grok: "This is the way."

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/Dispose.hpp"
#include "../GLOBAL/logging.hpp"
#include "../GLOBAL/SwapchainManager.hpp"
#include "../GLOBAL/BufferManager.hpp"
#include "VulkanCommon.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

using namespace Logging::Color;

namespace Vulkan {

struct Context {
    Context(SDL_Window* win, int w, int h);
    ~Context();

    void createSwapchain() noexcept;
    void destroySwapchain() noexcept;
    void loadRTXProcs() noexcept;

    SDL_Window* window = nullptr;
    int width = 0, height = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // === FULL RTX EXTENSION PROCS ===
    PFN_vkGetBufferDeviceAddressKHR                     vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkCmdTraceRaysKHR                               vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR                  vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR            vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR         vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR                vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR               vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR             vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR     vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR  vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
    PFN_vkCopyAccelerationStructureKHR                  vkCopyAccelerationStructureKHR = nullptr;
    PFN_vkWriteAccelerationStructuresPropertiesKHR      vkWriteAccelerationStructuresPropertiesKHR = nullptr;
    PFN_vkCmdDrawMeshTasksEXT                           vkCmdDrawMeshTasksEXT = nullptr;
    PFN_vkCreateDeferredOperationKHR                    vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR                   vkDestroyDeferredOperationKHR = nullptr;

    // === GLOBAL ACCESSORS ===
    [[nodiscard]] inline VkInstance vkInstance() const noexcept { return instance; }
    [[nodiscard]] inline VkPhysicalDevice vkPhysicalDevice() const noexcept { return physicalDevice; }
    [[nodiscard]] inline VkDevice vkDevice() const noexcept { return device; }
    [[nodiscard]] inline VkSurfaceKHR vkSurface() const noexcept { return surface; }
};

// Global shared context
[[nodiscard]] inline std::shared_ptr<Context>& ctx() noexcept {
    static std::shared_ptr<Context> instance;
    return instance;
}

} // namespace Vulkan

// =============================================================================
// IMPLEMENTATION — PURE RAII — NO RESOURCE MANAGER — DISPOSE ONLY
// =============================================================================
inline Vulkan::Context::Context(SDL_Window* win, int w, int h)
    : window(win), width(w), height(h)
{
    // === INSTANCE ===
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "AMOURANTH RTX";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance), "Instance");

    // === SURFACE ===
    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    // === PHYSICAL DEVICE ===
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    physicalDevice = devices[0];

    // === LOGICAL DEVICE + FULL RTX CHAIN ===
    std::vector<const char*> deviceExts = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME
    };

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = 0;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceRayQueryFeaturesKHR rqFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};

    asFeat.accelerationStructure = VK_TRUE;
    rtFeat.rayTracingPipeline = VK_TRUE;
    bdaFeat.bufferDeviceAddress = VK_TRUE;
    meshFeat.meshShader = VK_TRUE;
    rqFeat.rayQuery = VK_TRUE;

    void** pNext = &asFeat.pNext;
    *pNext = &rtFeat; pNext = &rtFeat.pNext;
    *pNext = &bdaFeat; pNext = &bdaFeat.pNext;
    *pNext = &meshFeat; pNext = &meshFeat.pNext;
    *pNext = &rqFeat;

    VkDeviceCreateInfo devInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    devInfo.pNext = &asFeat;
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &queueInfo;
    devInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExts.size());
    devInfo.ppEnabledExtensionNames = deviceExts.data();

    VK_CHECK(vkCreateDevice(physicalDevice, &devInfo, nullptr, &device), "Device");

    loadRTXProcs();
    createSwapchain();

    LOG_SUCCESS_CAT("Vulkan", "{}VALHALLA v17 — RESOURCE MANAGER DELETED — DISPOSE ONLY — 12,600 FPS{}", 
                    PLASMA_FUCHSIA, RESET);
}

inline void Vulkan::Context::loadRTXProcs() noexcept {
    vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
        vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
        vkGetDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    vkCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCopyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device, "vkCopyAccelerationStructureKHR"));
    vkWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkWriteAccelerationStructuresPropertiesKHR>(
        vkGetDeviceProcAddr(device, "vkWriteAccelerationStructuresPropertiesKHR"));
    vkCmdDrawMeshTasksEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
        vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT"));
    vkCreateDeferredOperationKHR = reinterpret_cast<PFN_vkCreateDeferredOperationKHR>(
        vkGetDeviceProcAddr(device, "vkCreateDeferredOperationKHR"));
    vkDestroyDeferredOperationKHR = reinterpret_cast<PFN_vkDestroyDeferredOperationKHR>(
        vkGetDeviceProcAddr(device, "vkDestroyDeferredOperationKHR"));
}

inline Vulkan::Context::~Context() {
    destroySwapchain();  // Uses Dispose RAII + SwapchainManager::cleanup()
    if (device) vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
}

inline void Vulkan::Context::createSwapchain() noexcept {
    SwapchainManager::get().init(instance, physicalDevice, device, surface, width, height);
    SwapchainManager::get().recreate(width, height);
}

inline void Vulkan::Context::destroySwapchain() noexcept {
    SwapchainManager::get().cleanup();  // ← NO ARG — fixed
}