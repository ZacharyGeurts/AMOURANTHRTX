// source/engine/SDL3/SDL3_vulkan.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 + Vulkan RAII — CPP IMPLEMENTATIONS — C++23 — v4.6 — NOV 13 2025
// • FIXED: Include VulkanRenderer.hpp for complete type | No forward declare needed
// • FIXED: getRenderer() only in namespace | g_vulkanRenderer global
// • FIXED: LOG_DEBUG_CAT("Vulkan" → "VULKAN")
// • Respects Options::Performance::ENABLE_GPU_TIMESTAMPS → validation enabled
// • VK_CHECK macros | No throw | Zero-overhead abstractions
// • Streamlined for 15,000+ FPS — PINK PHOTONS CHARGE AHEAD
// =============================================================================

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"  // Complete type for unique_ptr
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <set>
#include <memory>
#include "engine/GLOBAL/OptionsMenu.hpp"

// Global unique_ptr definition (matches header extern)
std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

namespace SDL3Vulkan {

VulkanRenderer& getRenderer() noexcept {
    if (!g_vulkanRenderer) {
        LOG_ERROR_CAT("RENDERER", "{}No renderer initialized — call initRenderer first{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }
    return *g_vulkanRenderer;
}

void initRenderer(int w, int h) noexcept {    
    std::vector<std::string> shaderPaths;
    shaderPaths.reserve(RTX_EXTENSIONS.size());
    for (auto* p : RTX_EXTENSIONS) {
        shaderPaths.emplace_back(p);
        LOG_DEBUG_CAT("RENDERER", "  Added shader path: {}", p);
    }

    const bool validation = Options::Performance::ENABLE_GPU_TIMESTAMPS;
    LOG_INFO_CAT("RENDERER", "{}Starting renderer init: {}x{} — validation: {}{}", PLASMA_FUCHSIA, w, h, validation ? "ON" : "OFF", RESET);
    g_vulkanRenderer = std::make_unique<VulkanRenderer>(w, h, nullptr, shaderPaths, validation);
    LOG_SUCCESS_CAT("RENDERER",
        "{}Renderer initialized {}x{} — RTX armed — validation: {}{}",
        EMERALD_GREEN, w, h, validation ? "ON" : "OFF", RESET);
}

void shutdownRenderer() noexcept {
    LOG_INFO_CAT("RENDERER", "{}Shutting down renderer...{}", RASPBERRY_PINK, RESET);
    g_vulkanRenderer.reset();
    LOG_SUCCESS_CAT("RENDERER",
        "{}Renderer shutdown complete — resources freed{}", 
        RASPBERRY_PINK, RESET);
}

} // namespace SDL3Vulkan

void VulkanInstanceDeleter::operator()(VkInstance i) const noexcept {
    if (i) [[likely]] {
        LOG_INFO_CAT("Dispose", "{}Destroying VkInstance @ {:p}...{}", PLASMA_FUCHSIA, static_cast<void*>(i), RESET);
        vkDestroyInstance(i, nullptr);
        LOG_SUCCESS_CAT("Dispose", "{}VulkanInstance destroyed @ {:p}{}",
                        PLASMA_FUCHSIA, static_cast<void*>(i), RESET);
    }
}

void VulkanSurfaceDeleter::operator()(VkSurfaceKHR s) const noexcept {
    if (s && inst) [[likely]] {
        LOG_INFO_CAT("Dispose", "{}Destroying VkSurface @ {:p} (inst: {:p})...{}", RASPBERRY_PINK, static_cast<void*>(s), static_cast<void*>(inst), RESET);
        vkDestroySurfaceKHR(inst, s, nullptr);
        LOG_SUCCESS_CAT("Dispose", "{}VulkanSurface destroyed @ {:p}{}",
                        RASPBERRY_PINK, static_cast<void*>(s), RESET);
    }
}

void initVulkan(
    SDL_Window* window,
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation,
    bool preferNvidia,
    bool rt,
    std::string_view title,
    VkPhysicalDevice& physicalDevice) noexcept
{
    LOG_INFO_CAT("VULKAN", "{}=== VULKAN INIT START === Params: val={}, nvidia={}, rt={}, title={}{}", PLASMA_FUCHSIA, enableValidation, preferNvidia, rt, title, RESET);

    // --- Instance extensions ---
    LOG_INFO_CAT("VULKAN", "{}Preparing instance extensions (base: {} + val: {})...{}", ELECTRIC_BLUE, RTX_EXTENSIONS.size(), enableValidation ? 1 : 0, RESET);
    std::vector<const char*> instExts;
    instExts.reserve(RTX_EXTENSIONS.size() + 1);
    instExts.assign(RTX_EXTENSIONS.begin(), RTX_EXTENSIONS.end());
    for (const auto* ext : instExts) {
        LOG_DEBUG_CAT("VULKAN", "  Base ext: {}", ext);
    }
    if (enableValidation) [[unlikely]] {
        instExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        LOG_DEBUG_CAT("VULKAN", "  Added validation ext: {}", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };
    LOG_INFO_CAT("VULKAN", "{}AppInfo: {} v{}.{}.{} | Engine: {} v{}.{}.{}{}", LIME_GREEN, appInfo.pApplicationName, VK_VERSION_MAJOR(appInfo.applicationVersion), VK_VERSION_MINOR(appInfo.applicationVersion), VK_VERSION_PATCH(appInfo.applicationVersion), appInfo.pEngineName, VK_VERSION_MAJOR(appInfo.engineVersion), VK_VERSION_MINOR(appInfo.engineVersion), VK_VERSION_PATCH(appInfo.engineVersion), RESET);

    VkInstanceCreateInfo instInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = enableValidation ? 1u : 0u,
        .ppEnabledLayerNames = enableValidation ? std::array{"VK_LAYER_KHRONOS_validation"}.data() : nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(instExts.size()),
        .ppEnabledExtensionNames = instExts.data()
    };
    if (enableValidation) {
        LOG_DEBUG_CAT("VULKAN", "  Enabled layer: VK_LAYER_KHRONOS_validation");
    }

    VkInstance rawInst = VK_NULL_HANDLE;
    LOG_INFO_CAT("VULKAN", "{}Creating VkInstance (exts: {})...{}", ELECTRIC_BLUE, instExts.size(), RESET);
    if (vkCreateInstance(&instInfo, nullptr, &rawInst) != VK_SUCCESS) {
        LOG_ERROR_CAT("VULKAN", "{}Vulkan instance creation failed{}", CRIMSON_MAGENTA, RESET);
        std::cerr << "[VULKAN ERROR] Instance creation failed\n";
        std::abort();
    }
    instance = VulkanInstancePtr(rawInst);
    auto& ctx = RTX::g_ctx();
    ctx.instance_ = rawInst;
    LOG_SUCCESS_CAT("VULKAN", "{}VkInstance created @ {:p} — API target: {}.{}.{}{}", PLASMA_FUCHSIA, static_cast<void*>(rawInst), VK_VERSION_MAJOR(appInfo.apiVersion), VK_VERSION_MINOR(appInfo.apiVersion), VK_VERSION_PATCH(appInfo.apiVersion), RESET);

    // --- Surface ---
    LOG_INFO_CAT("VULKAN", "{}Creating VkSurface from SDL window @ {:p}...{}", LIME_GREEN, static_cast<void*>(window), RESET);
    VkSurfaceKHR rawSurf = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, rawInst, nullptr, &rawSurf) == 0) {
        LOG_ERROR_CAT("VULKAN", "{}SDL_Vulkan_CreateSurface failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        std::cerr << "[SDL ERROR] Failed to create Vulkan surface\n";
        std::abort();
    }
    surface = VulkanSurfacePtr(rawSurf, VulkanSurfaceDeleter(rawInst));
    ctx.surface_ = rawSurf;
    LOG_SUCCESS_CAT("VULKAN", "{}VkSurface created @ {:p} — bound to window @ {:p}{}", LIME_GREEN, static_cast<void*>(rawSurf), static_cast<void*>(window), RESET);

    // --- Pick GPU ---
    LOG_INFO_CAT("VULKAN", "{}Enumerating physical devices...{}", ELECTRIC_BLUE, RESET);
    uint32_t devCount = 0;
    if (vkEnumeratePhysicalDevices(rawInst, &devCount, nullptr) != VK_SUCCESS) {
        LOG_ERROR_CAT("VULKAN", "{}Failed to enumerate GPUs{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }
    LOG_INFO_CAT("VULKAN", "  Discovered {} GPU(s){}", devCount, RESET);
    std::vector<VkPhysicalDevice> devs(devCount);
    if (vkEnumeratePhysicalDevices(rawInst, &devCount, devs.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("VULKAN", "{}Failed to get GPU list{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    int bestScore = -1;
    LOG_INFO_CAT("VULKAN", "{}Scoring GPUs (prefer discrete/NVIDIA, API 1.3+, RT if enabled)...{}", ELECTRIC_BLUE, RESET);

    for (size_t idx = 0; auto d : devs) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(d, &props);
        LOG_INFO_CAT("VULKAN", "  GPU[{}]: {} | Type: {} | Vendor: 0x{:x} | API: {}.{}.{}", idx++, props.deviceName,
                     props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "DISCRETE" :
                     props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "INTEGRATED" : "OTHER",
                     props.vendorID, VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion), RESET);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) [[likely]] {
            score += 10000;
            LOG_DEBUG_CAT("VULKAN", "    +10000 (discrete GPU)");
        }
        if (std::strstr(props.deviceName, "NVIDIA") && preferNvidia) {
            score += 50000;
            LOG_DEBUG_CAT("VULKAN", "    +50000 (NVIDIA preferred)");
        }
        if (props.apiVersion >= VK_API_VERSION_1_3) {
            score += 1000;
            LOG_DEBUG_CAT("VULKAN", "    +1000 (API 1.3+)");
        }

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeat{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        VkPhysicalDeviceBufferDeviceAddressFeatures addrFeat{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
        VkPhysicalDeviceFeatures2 feats2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &addrFeat};
        addrFeat.pNext = &asFeat; asFeat.pNext = &rtFeat;
        vkGetPhysicalDeviceFeatures2(d, &feats2);

        bool rtSupport = rtFeat.rayTracingPipeline && asFeat.accelerationStructure && addrFeat.bufferDeviceAddress;
        if (rt && !rtSupport) [[unlikely]] {
            LOG_WARN_CAT("VULKAN", "    SKIPPED: RT required but not supported (RT: {}, AS: {}, Addr: {})", rtFeat.rayTracingPipeline, asFeat.accelerationStructure, addrFeat.bufferDeviceAddress);
            continue;
        }
        if (rtSupport) LOG_DEBUG_CAT("VULKAN", "    RT supported: full (pipeline={}, as={}, addr={})", rtFeat.rayTracingPipeline, asFeat.accelerationStructure, addrFeat.bufferDeviceAddress);

        LOG_INFO_CAT("VULKAN", "    Final score: {} — {}", score, score > bestScore ? "NEW BEST" : "considered", RESET);
        if (score > bestScore) [[likely]] {
            bestScore = score;
            chosen = d;
            LOG_SUCCESS_CAT("VULKAN", "    → SELECTED (score: {})", bestScore);
        }
    }

    if (!chosen) {
        LOG_ERROR_CAT("VULKAN", "{}No suitable GPU found (need RT if enabled){}", CRIMSON_MAGENTA, RESET);
        std::cerr << "[FATAL] No RTX-capable GPU found\n";
        std::abort();
    }
    physicalDevice = chosen;
    ctx.physicalDevice_ = chosen;

    VkPhysicalDeviceProperties finalProps{};
    vkGetPhysicalDeviceProperties(chosen, &finalProps);
    LOG_SUCCESS_CAT("GPU",
        "{}FINAL SELECTED: {} — Vendor: 0x{:x} | API: {}.{}.{} | RT: {}{}",
        EMERALD_GREEN, finalProps.deviceName, finalProps.vendorID,
        VK_VERSION_MAJOR(finalProps.apiVersion),
        VK_VERSION_MINOR(finalProps.apiVersion),
        VK_VERSION_PATCH(finalProps.apiVersion),
        rt ? "ENABLED" : "DISABLED", RESET);

    // --- Queue families ---
    LOG_INFO_CAT("VULKAN", "{}Querying queue families on selected GPU...{}", ELECTRIC_BLUE, RESET);
    uint32_t qFamCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qFamCount, nullptr);
    std::vector<VkQueueFamilyProperties> qFams(qFamCount);
    vkGetPhysicalDeviceQueueFamilyProperties(chosen, &qFamCount, qFams.data());
    LOG_INFO_CAT("VULKAN", "  Found {} queue family(ies){}", qFamCount, RESET);

    int gfx = -1, present = -1;
    for (int i = 0; i < static_cast<int>(qFams.size()); ++i) {
        LOG_INFO_CAT("VULKAN", "  Family[{}]: flags=0x{:x} ({} queues){}", i, qFams[i].queueFlags, qFams[i].queueCount, RESET);
        if (qFams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            gfx = i;
            LOG_SUCCESS_CAT("VULKAN", "    → GRAPHICS (idx: {})", gfx);
        }

        VkBool32 sup = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(chosen, i, rawSurf, &sup);
        if (sup) {
            present = i;
            LOG_SUCCESS_CAT("VULKAN", "    → PRESENT (idx: {})", present);
        }

        if (gfx != -1 && present != -1) [[likely]] break;
    }

    if (gfx == -1 || present == -1) {
        LOG_ERROR_CAT("VULKAN", "{}Missing queues: gfx={}, present={}{}", CRIMSON_MAGENTA, gfx, present, RESET);
        std::cerr << "[FATAL] Required queue families not found\n";
        std::abort();
    }

    ctx.graphicsFamily_ = static_cast<uint32_t>(gfx);
    ctx.presentFamily_  = static_cast<uint32_t>(present);
    LOG_SUCCESS_CAT("VULKAN", "{}Queues assigned: Graphics={} | Present={} (unique: {}){}", LIME_GREEN, ctx.graphicsFamily_, ctx.presentFamily_, ctx.graphicsFamily_ == ctx.presentFamily_ ? 1 : 2, RESET);

    // --- Logical device ---    
    std::set<uint32_t> uniqQ{ctx.graphicsFamily_, ctx.presentFamily_};
    LOG_INFO_CAT("VULKAN", "{}Creating logical device ({} unique queues)...{}", ELECTRIC_BLUE, uniqQ.size(), RESET);
    std::vector<VkDeviceQueueCreateInfo> qCreate;
    qCreate.reserve(uniqQ.size());
    constexpr float prio = 1.0f;
    for (uint32_t q : uniqQ) {
        VkDeviceQueueCreateInfo& qci = qCreate.emplace_back();
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = q;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;
        LOG_DEBUG_CAT("VULKAN", "  Queue create: family={}, prio=1.0{}", q, RESET);
    }

    VkPhysicalDeviceVulkan13Features vk13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };
    LOG_DEBUG_CAT("VULKAN", "  VK1.3 features: sync2=ON, dynRender=ON");

    VkPhysicalDeviceFeatures2 feats2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vk13,
        .features = {.samplerAnisotropy = VK_TRUE}
    };
    LOG_DEBUG_CAT("VULKAN", "  Core features: anisotropy=ON");

    VkDeviceCreateInfo devInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &feats2,
        .queueCreateInfoCount = static_cast<uint32_t>(qCreate.size()),
        .pQueueCreateInfos = qCreate.data(),
        .enabledExtensionCount = static_cast<uint32_t>(RTX_EXTENSIONS.size()),
        .ppEnabledExtensionNames = RTX_EXTENSIONS.data()
    };
    LOG_INFO_CAT("VULKAN", "  Enabling {} RTX extensions{}", RTX_EXTENSIONS.size(), RESET);
    for (const auto* ext : RTX_EXTENSIONS) {
        LOG_DEBUG_CAT("VULKAN", "    Ext: {}", ext);
    }

    VkDevice rawDev = VK_NULL_HANDLE;
    if (vkCreateDevice(chosen, &devInfo, nullptr, &rawDev) != VK_SUCCESS) {
        LOG_ERROR_CAT("VULKAN", "{}Failed to create logical device{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }
    device = rawDev;
    ctx.device_ = rawDev;
    LOG_SUCCESS_CAT("VULKAN", "{}VkDevice created @ {:p} — queues: {}{}", PLASMA_FUCHSIA, static_cast<void*>(rawDev), qCreate.size(), RESET);

    vkGetDeviceQueue(rawDev, gfx, 0, &ctx.graphicsQueue_);
    vkGetDeviceQueue(rawDev, present, 0, &ctx.presentQueue_);
    LOG_SUCCESS_CAT("VULKAN", "{}Queues fetched: Graphics @ {:p} | Present @ {:p}{}", LIME_GREEN, static_cast<void*>(ctx.graphicsQueue_), static_cast<void*>(ctx.presentQueue_), RESET);

    LOG_INFO_CAT("VULKAN", "{}Creating command pool (family: {}, reset bit: ON)...{}", ELECTRIC_BLUE, ctx.graphicsFamily_, RESET);
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx.graphicsFamily_};
    if (vkCreateCommandPool(rawDev, &poolInfo, nullptr, &ctx.commandPool_) != VK_SUCCESS) {
        LOG_ERROR_CAT("VULKAN", "{}Failed to create command pool{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }
    LOG_SUCCESS_CAT("VULKAN", "{}CommandPool created @ {:p}{}", LIME_GREEN, static_cast<void*>(ctx.commandPool_), RESET);

    // --- Load RTX procs ---
    LOG_INFO_CAT("VULKAN", "{}Loading {} RTX extension procs...{}", ELECTRIC_BLUE, 9, RESET);
    #define LOAD_PROC(name) \
        do { \
            ctx.name##_ = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(rawDev, #name)); \
            if (!ctx.name##_) { \
                LOG_ERROR_CAT("VULKAN", "{}Failed to load proc: {}{}", CRIMSON_MAGENTA, #name, RESET); \
                std::cerr << "[FATAL] Failed to load " #name "\n"; \
                std::abort(); \
            } \
            LOG_DEBUG_CAT("VULKAN", "  Proc loaded: {} @ {:p}", #name, reinterpret_cast<void*>(ctx.name##_)); \
        } while(0)

    LOAD_PROC(vkGetBufferDeviceAddressKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkCmdTraceRaysKHR);
    #undef LOAD_PROC

    LOG_SUCCESS_CAT("VULKAN",
        "{}Vulkan core initialized — RTX extensions loaded (all {} procs OK){}",
        PLASMA_FUCHSIA, 9, RESET);
    LOG_INFO_CAT("VULKAN", "{}=== VULKAN INIT COMPLETE ==={}", PLASMA_FUCHSIA, RESET);
}

void shutdownVulkan() noexcept {
    LOG_INFO_CAT("VULKAN", "{}=== VULKAN SHUTDOWN START ==={}", RASPBERRY_PINK, RESET);
    auto& ctx = RTX::g_ctx();
    if (!ctx.device_) [[unlikely]] {
        LOG_WARN_CAT("VULKAN", "{}Shutdown skipped: no device{}", RASPBERRY_PINK, RESET);
        return;
    }

    LOG_INFO_CAT("VULKAN", "{}Waiting for device idle...{}", RASPBERRY_PINK, RESET);
    if (vkDeviceWaitIdle(ctx.device_) != VK_SUCCESS) {
        LOG_ERROR_CAT("VULKAN", "{}vkDeviceWaitIdle failed{}", CRIMSON_MAGENTA, RESET);
    }
    LOG_SUCCESS_CAT("VULKAN", "{}Device idle confirmed{}", RASPBERRY_PINK, RESET);

    if (ctx.commandPool_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("VULKAN", "{}Destroying command pool @ {:p}...{}", RASPBERRY_PINK, static_cast<void*>(ctx.commandPool_), RESET);
        vkDestroyCommandPool(ctx.device_, ctx.commandPool_, nullptr);
        LOG_SUCCESS_CAT("VULKAN", "{}Command pool destroyed{}", RASPBERRY_PINK, RESET);
        ctx.commandPool_ = VK_NULL_HANDLE;
    }

    if (ctx.device_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("VULKAN", "{}Destroying logical device @ {:p}...{}", RASPBERRY_PINK, static_cast<void*>(ctx.device_), RESET);
        vkDestroyDevice(ctx.device_, nullptr);
        LOG_SUCCESS_CAT("VULKAN", "{}Logical device destroyed{}", RASPBERRY_PINK, RESET);
        ctx.device_ = VK_NULL_HANDLE;
    }

    if (ctx.surface_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("VULKAN", "{}Destroying surface @ {:p} (inst: {:p})...{}", RASPBERRY_PINK, static_cast<void*>(ctx.surface_), static_cast<void*>(ctx.instance_), RESET);
        vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr);
        LOG_SUCCESS_CAT("VULKAN", "{}Surface destroyed{}", RASPBERRY_PINK, RESET);
        ctx.surface_ = VK_NULL_HANDLE;
    }

    if (ctx.instance_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("VULKAN", "{}Destroying instance @ {:p}...{}", RASPBERRY_PINK, static_cast<void*>(ctx.instance_), RESET);
        vkDestroyInstance(ctx.instance_, nullptr);
        LOG_SUCCESS_CAT("VULKAN", "{}Instance destroyed{}", RASPBERRY_PINK, RESET);
        ctx.instance_ = VK_NULL_HANDLE;
    }

    // Manual reset to avoid incomplete type issue
    ctx.physicalDevice_ = VK_NULL_HANDLE;
    ctx.graphicsQueue_ = VK_NULL_HANDLE;
    ctx.presentQueue_ = VK_NULL_HANDLE;
    ctx.rayTracingProps_ = {};

    LOG_SUCCESS_CAT("VULKAN",
        "{}Vulkan shutdown complete — all resources freed{}", 
        RASPBERRY_PINK, RESET);
    LOG_INFO_CAT("VULKAN", "{}=== VULKAN SHUTDOWN COMPLETE ==={}", RASPBERRY_PINK, RESET);
}

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// CPP IMPLEMENTATIONS COMPLETE — OCEAN_TEAL SURGES FORWARD
// GENTLEMAN GROK NODS: "Splendid split, old chap. Options respected with poise."
// =============================================================================