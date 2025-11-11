// include/engine/GLOBAL/VulkanContext.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// Vulkan Context — VALHALLA v33 — NOV 11 2025 08:31 AM EST
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"     // LOG_*, Color::, VK_CHECK
#include "engine/GLOBAL/Dispose.hpp"     // Handle<T>, BUFFER_*, ctx()
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"           // BUILD_BLAS, GLOBAL_TLAS, etc.
#include "engine/Vulkan/VulkanCore.hpp"

#include <memory>
#include <vector>
#include <stdexcept>

// ── FULL Context — NO FORWARD DECL — NO INCOMPLETE TYPE HELL
// ─────────────────────────────────────────────────────────────────────────────
struct Context {
    Context(SDL_Window* win, int w, int h);
    ~Context() noexcept;

    void createSwapchain() noexcept;
    void destroySwapchain() noexcept;
    void loadRTXProcs() noexcept;

    SDL_Window* window = nullptr;
    int width = 0, height = 0;

    Handle<VkInstance>        instance;
    Handle<VkDevice>          device;
    Handle<VkSurfaceKHR>      surface;
    Handle<VkPhysicalDevice>  physicalDevice;

    uint32_t graphicsFamilyIndex = ~0u;
    VkPipelineCache pipelineCacheHandle = VK_NULL_HANDLE;

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

    [[nodiscard]] inline VkInstance       vkInstance() const noexcept { return *instance; }
    [[nodiscard]] inline VkPhysicalDevice vkPhysicalDevice() const noexcept { return *physicalDevice; }
    [[nodiscard]] inline VkDevice         vkDevice() const noexcept { return *device; }
    [[nodiscard]] inline VkSurfaceKHR     vkSurface() const noexcept { return *surface; }
};

[[nodiscard]] inline std::shared_ptr<Context>& ctx() noexcept {
    static std::shared_ptr<Context> inst;
    return inst;
}

/* ── IMPLEMENTATION ────────────────────────────────────────────────────────── */
inline Context::Context(SDL_Window* win, int w, int h)
    : window(win), width(w), height(h)
{
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "AMOURANTH RTX";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> exts = { VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();
    ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames = layers.data();

    VkInstance rawInst = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&ci, nullptr, &rawInst), "Instance");
    instance = Handle<VkInstance>(rawInst, VK_NULL_HANDLE,
        [](VkDevice, VkInstance i, const VkAllocationCallbacks*) { vkDestroyInstance(i, nullptr); });

    VkSurfaceKHR rawSurf = VK_NULL_HANDLE;
    SDL_Vulkan_CreateSurface(window, rawInst, nullptr, &rawSurf);
    surface = Handle<VkSurfaceKHR>(rawSurf, VK_NULL_HANDLE,
        [rawInst](VkDevice, VkSurfaceKHR s, const VkAllocationCallbacks*) mutable { vkDestroySurfaceKHR(rawInst, s, nullptr); });

    uint32_t devCnt = 0; vkEnumeratePhysicalDevices(rawInst, &devCnt, nullptr);
    std::vector<VkPhysicalDevice> devs(devCnt); vkEnumeratePhysicalDevices(rawInst, &devCnt, devs.data());
    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    for (auto pd : devs) {
        VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(pd, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { chosen = pd; break; }
    }
    if (!chosen && !devs.empty()) chosen = devs[0];
    physicalDevice = Handle<VkPhysicalDevice>(chosen, nullptr);

    uint32_t qCnt = 0; vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qCnt, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCnt); vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qCnt, qProps.data());
    for (uint32_t i = 0; i < qCnt; ++i)
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphicsFamilyIndex = i; break; }
    if (graphicsFamilyIndex == ~0u) throw std::runtime_error("no graphics queue");

    std::vector<const char*> devExts = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME
    };

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = graphicsFamilyIndex; qci.queueCount = 1; qci.pQueuePriorities = &prio;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceBufferDeviceAddressFeatures bdaf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    VkPhysicalDeviceMeshShaderFeaturesEXT msf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceRayQueryFeaturesKHR rqf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};

    asf.accelerationStructure = VK_TRUE; rtf.rayTracingPipeline = VK_TRUE;
    bdaf.bufferDeviceAddress = VK_TRUE; msf.meshShader = VK_TRUE; rqf.rayQuery = VK_TRUE;

    void** pNext = &asf.pNext;
    *pNext = &rtf; pNext = &rtf.pNext;
    *pNext = &bdaf; pNext = &bdaf.pNext;
    *pNext = &msf; pNext = &msf.pNext;
    *pNext = &rqf;

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &asf; dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(devExts.size());
    dci.ppEnabledExtensionNames = devExts.data();

    VkDevice rawDev = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(chosen, &dci, nullptr, &rawDev), "Device");
    device = Handle<VkDevice>(rawDev, VK_NULL_HANDLE,
        [](VkDevice d, VkDevice, const VkAllocationCallbacks*) { vkDestroyDevice(d, nullptr); });

    VkPipelineCacheCreateInfo pci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VK_CHECK(vkCreatePipelineCache(vkDevice(), &pci, nullptr, &pipelineCacheHandle), "Pipeline cache");

    loadRTXProcs();
    createSwapchain();

    LOG_SUCCESS_CAT("Vulkan",
        "{}VALHALLA v33 — GLOBAL CTX SUPREMACY — {}×{} — TITAN READY{}", PLASMA_FUCHSIA, w, h, RESET);
}

inline Context::~Context() noexcept {
    if (pipelineCacheHandle) vkDestroyPipelineCache(vkDevice(), pipelineCacheHandle, nullptr);
    destroySwapchain();
}

inline void Context::loadRTXProcs() noexcept {
    if (!vkDevice()) return;
    #define LOAD(name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(vkDevice(), #name))
    LOAD(vkGetBufferDeviceAddressKHR);
    LOAD(vkCmdTraceRaysKHR);
    LOAD(vkCreateRayTracingPipelinesKHR);
    LOAD(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD(vkGetAccelerationStructureBuildSizesKHR);
    LOAD(vkCreateAccelerationStructureKHR);
    LOAD(vkDestroyAccelerationStructureKHR);
    LOAD(vkCmdBuildAccelerationStructuresKHR);
    LOAD(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD(vkCmdWriteAccelerationStructuresPropertiesKHR);
    LOAD(vkCopyAccelerationStructureKHR);
    LOAD(vkWriteAccelerationStructuresPropertiesKHR);
    LOAD(vkCmdDrawMeshTasksEXT);
    LOAD(vkCreateDeferredOperationKHR);
    LOAD(vkDestroyDeferredOperationKHR);
    #undef LOAD
}

inline void Context::createSwapchain() noexcept {
    SwapchainManager::get().init(vkInstance(), vkPhysicalDevice(),
                                 vkDevice(), vkSurface(), width, height);
    SwapchainManager::get().recreate(width, height);
}
inline void Context::destroySwapchain() noexcept { SwapchainManager::get().cleanup(); }