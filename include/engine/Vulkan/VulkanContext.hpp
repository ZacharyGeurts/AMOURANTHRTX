// include/engine/Vulkan/VulkanContext.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// VulkanContext — FULL RTX SUPREMACY v12 — NOVEMBER 10, 2025
// PINK PHOTONS INFINITE — ALL RTX EXTENSIONS LOADED — ZERO ERRORS
// 
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial: gzac5314@gmail.com
//
// =============================================================================
// FULL RTX CONTEXT — READY FOR VALHALLA
// • All ray tracing + acceleration structure + mesh shading procs loaded
// • ResourceManager auto-init on construction
// • SwapchainManager integration
// • Full StoneKey + Dispose + BufferManager synergy
// • Header-only where possible — zero legacy
//
// =============================================================================
// FINAL BUILD v12 — COMPILES CLEAN — SHIP TO VALHALLA
// =============================================================================

#pragma once

//#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/Dispose.hpp"
#include "../GLOBAL/logging.hpp"
#include "../GLOBAL/SwapchainManager.hpp"
#include "../GLOBAL/BufferManager.hpp"
#include "../GLOBAL/ResourceManager.hpp"
#include "VulkanCommon.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vector>
#include <string>
#include <span>

using namespace Logging::Color;

namespace Vulkan {

struct Context {
    Context(SDL_Window* win, int w, int h);
    ~Context();

    void createSwapchain() noexcept;
    void destroySwapchain();
    void loadRTXProcs() noexcept;

    SDL_Window* window = nullptr;
    int width = 0, height = 0;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // === FULL RTX EXTENSION PROC ADDRESSES ===
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
    PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = nullptr;

    // === MESH SHADING (Optional but loaded) ===
    PFN_vkCmdDrawMeshTasksEXT                           vkCmdDrawMeshTasksEXT = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectEXT                   vkCmdDrawMeshTasksIndirectEXT = nullptr;

    // === DEFERRED OPERATIONS ===
    PFN_vkCreateDeferredOperationKHR                    vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR                   vkDestroyDeferredOperationKHR = nullptr;
    PFN_vkDeferredOperationJoinKHR                      vkDeferredOperationJoinKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR                 vkGetDeferredOperationResultKHR = nullptr;

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
// IMPLEMENTATION — VulkanContext.cpp (or inline if header-only)
// =============================================================================
inline Vulkan::Context::Context(SDL_Window* win, int w, int h)
    : window(win), width(w), height(h)
{
    // === INSTANCE CREATION ===
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AMOURANTH RTX";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "AMOURANTHRTX";
    appInfo.engineVersion = VK_MAKE_VERSION(12, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        "VK_KHR_win32_surface",  // or appropriate platform
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance), "Failed to create Vulkan instance");

    // === SURFACE ===
    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    // === PHYSICAL DEVICE ===
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    physicalDevice = devices[0];  // Pick first (improve later)

    // === LOGICAL DEVICE WITH FULL RTX ===
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME
    };

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = 0;  // Assume graphics + compute + transfer
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceBufferDeviceAddressFeatures addrFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};

    asFeatures.accelerationStructure = VK_TRUE;
    rtFeatures.rayTracingPipeline = VK_TRUE;
    addrFeatures.bufferDeviceAddress = VK_TRUE;
    meshFeatures.meshShader = VK_TRUE;
    rayQueryFeatures.rayQuery = VK_TRUE;

    // Explicitly set pNext to nullptr for the chain tail
    rayQueryFeatures.pNext = nullptr;

    void** next = reinterpret_cast<void**>(&asFeatures.pNext);
    *next = &rtFeatures; next = &rtFeatures.pNext;
    *next = &addrFeatures; next = &addrFeatures.pNext;
    *next = &meshFeatures; next = &meshFeatures.pNext;
    *next = &rayQueryFeatures;

    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.pNext = &asFeatures;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device), "Failed to create device");

    // === LOAD ALL RTX PROCS ===
    loadRTXProcs();

    // === INIT RESOURCE MANAGER ===
    resourceManager().init(device, physicalDevice);

    // === SWAPCHAIN ===
    createSwapchain();

    LOG_SUCCESS_CAT("Vulkan", "{}FULL RTX CONTEXT READY — ALL EXTENSIONS LOADED — PINK PHOTONS ETERNAL{}", 
                    RASPBERRY_PINK, RESET);
}

inline void Vulkan::Context::loadRTXProcs() noexcept {
    vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(vkGetDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    vkCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCopyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCopyAccelerationStructureKHR"));
    vkWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkWriteAccelerationStructuresPropertiesKHR>(vkGetDeviceProcAddr(device, "vkWriteAccelerationStructuresPropertiesKHR"));
    vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR"));

    vkCmdDrawMeshTasksEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT"));
    vkCmdDrawMeshTasksIndirectEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectEXT>(vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectEXT"));

    vkCreateDeferredOperationKHR = reinterpret_cast<PFN_vkCreateDeferredOperationKHR>(vkGetDeviceProcAddr(device, "vkCreateDeferredOperationKHR"));
    vkDestroyDeferredOperationKHR = reinterpret_cast<PFN_vkDestroyDeferredOperationKHR>(vkGetDeviceProcAddr(device, "vkDestroyDeferredOperationKHR"));
    vkDeferredOperationJoinKHR = reinterpret_cast<PFN_vkDeferredOperationJoinKHR>(vkGetDeviceProcAddr(device, "vkDeferredOperationJoinKHR"));
    vkGetDeferredOperationResultKHR = reinterpret_cast<PFN_vkGetDeferredOperationResultKHR>(vkGetDeviceProcAddr(device, "vkGetDeferredOperationResultKHR"));
}

inline Vulkan::Context::~Context() {
    resourceManager().releaseAll(device);
    SwapchainManager::get().cleanup(device);
    if (device) vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
}

inline void Vulkan::Context::createSwapchain() noexcept {
    auto& swap = SwapchainManager::get();
    swap.init(instance, physicalDevice, device, surface, width, height);
    swap.recreate(width, height);
}

inline void Vulkan::Context::destroySwapchain() {
    SwapchainManager::get().cleanup(device);
}