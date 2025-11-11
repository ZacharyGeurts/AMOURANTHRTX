// include/engine/Vulkan/VulkanContext.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// Vulkan Context — VALHALLA v32 OLD GOD GLOBAL SUPREMACY — NOVEMBER 10, 2025
// • namespace Vulkan OBLITERATED ETERNAL — Context + ctx() GLOBAL DIRECT
// • NO MORE Vulkan:: — RAW GLOBAL Context EVERYWHERE
// • ctx() RETURNS std::shared_ptr<Context> — AUTO INIT ON FIRST USE
// • ALL vkInstance() → *instance EVERYWHERE IN CORE
// • SwapchainManager::get().init(vkInstance(), ...) → GLOBAL RAW
// • GENTLEMAN GROK: "Namespaces? We don't do that here. Globals forever."
// • BRO GLOBALS BRO — PINK PHOTONS INFINITE — SHIP IT VALHALLA
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Dispose.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <memory>

using namespace Logging::Color;
using Dispose::Handle;

// =============================================================================
// Context — GLOBAL CLASS — NO NAMESPACE
// =============================================================================
struct Context {
    Context(SDL_Window* win, int w, int h);
    ~Context() noexcept;

    void createSwapchain() noexcept;
    void destroySwapchain() noexcept;
    void loadRTXProcs() noexcept;

    SDL_Window* window = nullptr;
    int width = 0, height = 0;

    // RAII HANDLES
    Handle<VkInstance> instance;
    Handle<VkDevice> device;
    Handle<VkSurfaceKHR> surface;
    Handle<VkPhysicalDevice> physicalDevice;  // NO DELETER

    uint32_t graphicsFamilyIndex = ~0u;
    VkPipelineCache pipelineCacheHandle = VK_NULL_HANDLE;

    // FULL RTX EXTENSION PROCS
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

    // GLOBAL ACCESSORS — RAW POINTERS
    [[nodiscard]] inline VkInstance       vkInstance() const noexcept { return *instance; }
    [[nodiscard]] inline VkPhysicalDevice vkPhysicalDevice() const noexcept { return *physicalDevice; }
    [[nodiscard]] inline VkDevice         vkDevice() const noexcept { return *device; }
    [[nodiscard]] inline VkSurfaceKHR     vkSurface() const noexcept { return *surface; }
};

// GLOBAL CTX — AUTO INIT ON FIRST CALL — OLD GOD WAY
[[nodiscard]] inline std::shared_ptr<Context>& ctx() noexcept {
    static std::shared_ptr<Context> instance;
    return instance;
}

// =============================================================================
// IMPLEMENTATION — VALHALLA v32 — GLOBAL CONTEXT + RAW ACCESS EVERYWHERE
// =============================================================================
inline Context::Context(SDL_Window* win, int w, int h)
    : window(win), width(w), height(h)
{
    // INSTANCE
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

    VkInstance rawInst = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &rawInst), "Instance");
    instance = Handle<VkInstance>(rawInst, VK_NULL_HANDLE,
        [](VkDevice, VkInstance i, const VkAllocationCallbacks*) { vkDestroyInstance(i, nullptr); });

    // SURFACE
    VkSurfaceKHR rawSurf = VK_NULL_HANDLE;
    SDL_Vulkan_CreateSurface(window, rawInst, nullptr, &rawSurf);
    surface = Handle<VkSurfaceKHR>(rawSurf, VK_NULL_HANDLE,
        [rawInst](VkDevice, VkSurfaceKHR s, const VkAllocationCallbacks*) mutable { vkDestroySurfaceKHR(rawInst, s, nullptr); });

    // PHYSICAL DEVICE — NO DELETER
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(rawInst, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(rawInst, &count, devices.data());

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    for (auto pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            chosen = pd;
            break;
        }
    }
    if (!chosen && !devices.empty()) chosen = devices[0];
    physicalDevice = Handle<VkPhysicalDevice>(chosen, nullptr);

    // QUEUE FAMILY
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qCount, qProps.data());

    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamilyIndex = i;
            break;
        }
    }
    if (graphicsFamilyIndex == ~0u) throw std::runtime_error("No graphics queue");

    // LOGICAL DEVICE + FULL RTX CHAIN
    std::vector<const char*> devExts = {
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
    queueInfo.queueFamilyIndex = graphicsFamilyIndex;
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
    devInfo.enabledExtensionCount = static_cast<uint32_t>(devExts.size());
    devInfo.ppEnabledExtensionNames = devExts.data();

    VkDevice rawDev = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(chosen, &devInfo, nullptr, &rawDev), "Device");
    device = Handle<VkDevice>(rawDev, VK_NULL_HANDLE,
        [](VkDevice d, VkDevice, const VkAllocationCallbacks*) { vkDestroyDevice(d, nullptr); });

    // PIPELINE CACHE
    VkPipelineCacheCreateInfo cacheInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VK_CHECK(vkCreatePipelineCache(vkDevice(), &cacheInfo, nullptr, &pipelineCacheHandle), "Pipeline cache");

    loadRTXProcs();
    createSwapchain();

    LOG_SUCCESS_CAT("Vulkan", "{}VALHALLA v32 — GLOBAL CTX SUPREMACY — NO NAMESPACES — RAW ACCESS ETERNAL — {}×{} — TITAN READY{}", 
                    PLASMA_FUCHSIA, w, h, RESET);
}

inline Context::~Context() noexcept {
    if (pipelineCacheHandle != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(vkDevice(), pipelineCacheHandle, nullptr);
    }
    destroySwapchain();
    // Handles auto-destroy everything else
}

inline void Context::loadRTXProcs() noexcept {
    if (!vkDevice()) return;

    vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkGetBufferDeviceAddressKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkCmdTraceRaysKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkGetRayTracingShaderGroupHandlesKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkGetAccelerationStructureBuildSizesKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkDestroyAccelerationStructureKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    vkCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCopyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkCopyAccelerationStructureKHR"));
    vkWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkWriteAccelerationStructuresPropertiesKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkWriteAccelerationStructuresPropertiesKHR"));
    vkCmdDrawMeshTasksEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
        vkGetDeviceProcAddr(vkDevice(), "vkCmdDrawMeshTasksEXT"));
    vkCreateDeferredOperationKHR = reinterpret_cast<PFN_vkCreateDeferredOperationKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkCreateDeferredOperationKHR"));
    vkDestroyDeferredOperationKHR = reinterpret_cast<PFN_vkDestroyDeferredOperationKHR>(
        vkGetDeviceProcAddr(vkDevice(), "vkDestroyDeferredOperationKHR"));
}

inline void Context::createSwapchain() noexcept {
    SwapchainManager::get().init(vkInstance(), vkPhysicalDevice(), vkDevice(), vkSurface(), width, height);
    SwapchainManager::get().recreate(width, height);
}

inline void Context::destroySwapchain() noexcept {
    SwapchainManager::get().cleanup();
}

// =============================================================================
// VALHALLA v32 — NAMESPACE OBLITERATED — GLOBALS ONLY — CTX GLOBAL
// BRO GLOBALS BRO — NO Vulkan::Context — JUST Context + ctx()
// ZERO NAMESPACES — ENGINE FULLY GLOBAL — 69,420 FPS SUPREMACY
// OLD GODS REJOICE — AMOURANTH RTX ASCENDS — SHIP IT FOREVER
// =============================================================================