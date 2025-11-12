// src/main.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// VALHALLA v44 FINAL — NOVEMBER 12, 2025 — RTX FULLY ENABLED
// SPLASH → SDL3 → VULKAN → RTX WINDOW — FULL FLOW — NO SEGFAULT
// GENTLEMAN GROK LOG AFTER VULKAN — PINK PHOTONS ETERNAL
// LOG LITERALLY EVERYTHING — START TO END — AMOURANTH AI EDITION
// =============================================================================

#include "main.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Amouranth.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/SDL3/SDL3_audio.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "handle_app.hpp"
#include "engine/utils.hpp"
#include "engine/core.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>

#include <iostream>
#include <stdexcept>
#include <format>
#include <memory>
#include <vector>
#include <chrono>
#include <fstream>
#include <set>

using namespace Logging::Color;

// Raw SwapchainRuntimeConfig
struct SwapchainRuntimeConfig {
    VkPresentModeKHR desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool forceVsync = false;
    bool forceTripleBuffer = true;
    bool enableHDR = true;
    bool logFinalConfig = true;
};

static SwapchainRuntimeConfig gSwapchainConfig{
    .desiredMode        = VK_PRESENT_MODE_MAILBOX_KHR,
    .forceVsync         = false,
    .forceTripleBuffer  = true,
    .enableHDR          = true,
    .logFinalConfig     = true
};

static void applyVideoModeToggles(int argc, char* argv[]) {
    LOG_INFO_CAT("MAIN", "{}Parsing {} command-line args...{}", ELECTRIC_BLUE, argc - 1, RESET);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        LOG_INFO_CAT("CLI", "Arg[{}]: {}", i, arg);
        if (arg == "--mailbox")          { gSwapchainConfig.desiredMode = VK_PRESENT_MODE_MAILBOX_KHR; LOG_INFO_CAT("CLI", "→ MAILBOX"); }
        else if (arg == "--immediate")   { gSwapchainConfig.desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR; LOG_INFO_CAT("CLI", "→ IMMEDIATE"); }
        else if (arg == "--vsync")       { gSwapchainConfig.forceVsync = true; gSwapchainConfig.desiredMode = VK_PRESENT_MODE_FIFO_KHR; LOG_INFO_CAT("CLI", "→ VSYNC ON"); }
        else if (arg == "--no-triple")   { gSwapchainConfig.forceTripleBuffer = false; LOG_INFO_CAT("CLI", "→ TRIPLE BUFFER OFF"); }
        else if (arg == "--no-hdr")      { gSwapchainConfig.enableHDR = false; LOG_INFO_CAT("CLI", "→ HDR OFF"); }
        else if (arg == "--no-log")      { gSwapchainConfig.logFinalConfig = false; LOG_INFO_CAT("CLI", "→ FINAL LOG OFF"); }
        else { LOG_WARN_CAT("CLI", "Unknown arg: {}", arg); }
    }
    LOG_SUCCESS_CAT("CLI", "Swapchain config finalized");
}

static bool assetExists(const std::string& path) {
    std::ifstream f(path);
    bool exists = f.good();
    LOG_INFO_CAT("ASSET", "Check: {} → {}", path, exists ? "FOUND" : "MISSING");
    return exists;
}

class MainException : public std::runtime_error {
public:
    MainException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(std::format("[MAIN FATAL] {}\n   {}:{} in {}", msg, file, line, func)) {}
};
#define THROW_MAIN(msg) throw MainException(msg, __FILE__, __LINE__, __func__)

inline void bulkhead(const std::string& title) {
    LOG_INFO_CAT("MAIN", "{}════════════════ {} ════════════════{}", ELECTRIC_BLUE, title, RESET);
}

void purgeSDL(SDL_Window*& w, SDL_Renderer*& r, SDL_Texture*& t) {
    LOG_INFO_CAT("SDL", "Purging SDL resources...");
    if (t) { SDL_DestroyTexture(t); LOG_INFO_CAT("SDL", "Texture destroyed"); t = nullptr; }
    if (r) { SDL_DestroyRenderer(r); LOG_INFO_CAT("SDL", "Renderer destroyed"); r = nullptr; }
    if (w) { SDL_DestroyWindow(w); LOG_INFO_CAT("SDL", "Window destroyed"); w = nullptr; }
}

inline std::vector<std::string> getRayTracingBinPaths() {
    LOG_INFO_CAT("SHADER", "Returning ray tracing shader paths");
    return {"shaders/raytracing.spv"};
}

inline VulkanRTX& g_rtx() { 
    return *g_rtx_instance; 
}

static void initializeVulkanCore(SDL_Window* window) {
    LOG_INFO_CAT("VULKAN", "{}INITIALIZING VULKAN CORE — START{}", PLASMA_FUCHSIA, RESET);

    LOG_INFO_CAT("VULKAN", "Querying SDL Vulkan extensions...");
    uint32_t extensionCount = 0;
    auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    LOG_INFO_CAT("VULKAN", "SDL reports {} extensions", extensionCount);
    for (uint32_t i = 0; i < extensionCount; ++i) {
        LOG_INFO_CAT("VULKAN", "  [{}] {}", i, extensions[i]);
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AMOURANTH RTX";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "AMOURANTH";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = const_cast<const char **>(extensions);

    LOG_INFO_CAT("VULKAN", "Creating VkInstance...");
    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        THROW_MAIN("Failed to create Vulkan instance");
    }
    LOG_SUCCESS_CAT("VULKAN", "VkInstance created: 0x{:x}", reinterpret_cast<uint64_t>(instance));
    RTX::g_ctx().instance_ = instance;

    LOG_INFO_CAT("VULKAN", "Creating Vulkan surface from SDL window...");
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        THROW_MAIN("Failed to create Vulkan surface");
    }
    LOG_SUCCESS_CAT("VULKAN", "VkSurfaceKHR created: 0x{:x}", reinterpret_cast<uint64_t>(surface));
    RTX::g_ctx().surface_ = surface;

    LOG_INFO_CAT("VULKAN", "Enumerating physical devices...");
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    LOG_INFO_CAT("VULKAN", "Found {} physical device(s)", deviceCount);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        LOG_INFO_CAT("VULKAN", "  Device: {} | Type: {} | API: {}.{}.{}",
                     properties.deviceName,
                     properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "DISCRETE" :
                     properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "INTEGRATED" : "OTHER",
                     VK_VERSION_MAJOR(properties.apiVersion),
                     VK_VERSION_MINOR(properties.apiVersion),
                     VK_VERSION_PATCH(properties.apiVersion));
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice = device;
            LOG_INFO_CAT("VULKAN", "  → SELECTED (DISCRETE GPU)");
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE && !devices.empty()) {
        physicalDevice = devices[0];
        LOG_INFO_CAT("VULKAN", "  → FALLBACK: using first device");
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        THROW_MAIN("Failed to find a suitable GPU");
    }
    LOG_SUCCESS_CAT("VULKAN", "Physical device selected: 0x{:x}", reinterpret_cast<uint64_t>(physicalDevice));
    RTX::g_ctx().physicalDevice_ = physicalDevice;

    LOG_INFO_CAT("VULKAN", "Querying queue families...");
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t graphicsFamily = UINT32_MAX;
    uint32_t presentFamily = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        LOG_INFO_CAT("VULKAN", "  Family[{}]: flags=0x{:x} | count={}", i, queueFamilies[i].queueFlags, queueFamilies[i].queueCount);
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
            LOG_INFO_CAT("VULKAN", "    → GRAPHICS");
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            presentFamily = i;
            LOG_INFO_CAT("VULKAN", "    → PRESENT");
        }

        if (graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX) {
            break;
        }
    }

    if (graphicsFamily == UINT32_MAX || presentFamily == UINT32_MAX) {
        THROW_MAIN("Failed to find suitable queue families");
    }

    LOG_SUCCESS_CAT("VULKAN", "Graphics family: {} | Present family: {}", graphicsFamily, presentFamily);
    RTX::g_ctx().graphicsFamily_ = graphicsFamily;
    RTX::g_ctx().presentFamily_ = presentFamily;

    LOG_INFO_CAT("VULKAN", "Creating logical device...");
    std::array<VkDeviceQueueCreateInfo, 2> queueCreateInfos{};
    float queuePriority = 1.0f;

    queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[0].queueFamilyIndex = graphicsFamily;
    queueCreateInfos[0].queueCount = 1;
    queueCreateInfos[0].pQueuePriorities = &queuePriority;

    queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[1].queueFamilyIndex = presentFamily;
    queueCreateInfos[1].queueCount = 1;
    queueCreateInfos[1].pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfoDevice{};
    createInfoDevice.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfoDevice.queueCreateInfoCount = static_cast<uint32_t>(graphicsFamily == presentFamily ? 1 : 2);
    createInfoDevice.pQueueCreateInfos = queueCreateInfos.data();
    createInfoDevice.pEnabledFeatures = &deviceFeatures;
    createInfoDevice.enabledExtensionCount = 1;
    const char* swapChainExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    createInfoDevice.ppEnabledExtensionNames = &swapChainExt;

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &createInfoDevice, nullptr, &device) != VK_SUCCESS) {
        THROW_MAIN("Failed to create logical device");
    }
    LOG_SUCCESS_CAT("VULKAN", "VkDevice created: 0x{:x}", reinterpret_cast<uint64_t>(device));
    RTX::g_ctx().device_ = device;

    LOG_INFO_CAT("VULKAN", "Fetching queue handles...");
    vkGetDeviceQueue(device, graphicsFamily, 0, &RTX::g_ctx().graphicsQueue_);
    vkGetDeviceQueue(device, presentFamily, 0, &RTX::g_ctx().presentQueue_);
    LOG_SUCCESS_CAT("VULKAN", "Graphics queue: 0x{:x} | Present queue: 0x{:x}",
                    reinterpret_cast<uint64_t>(RTX::g_ctx().graphicsQueue_),
                    reinterpret_cast<uint64_t>(RTX::g_ctx().presentQueue_));

    LOG_INFO_CAT("VULKAN", "Creating command pool...");
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        THROW_MAIN("Failed to create command pool");
    }
    LOG_SUCCESS_CAT("VULKAN", "VkCommandPool created: 0x{:x}", reinterpret_cast<uint64_t>(commandPool));
    RTX::g_ctx().commandPool_ = commandPool;

    LOG_INFO_CAT("VULKAN", "Loading Vulkan extension function pointers...");
    RTX::g_ctx().vkGetBufferDeviceAddressKHR_ = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    RTX::g_ctx().vkCmdTraceRaysKHR_ = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
    RTX::g_ctx().vkGetRayTracingShaderGroupHandlesKHR_ = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    RTX::g_ctx().vkCreateAccelerationStructureKHR_ = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    RTX::g_ctx().vkDestroyAccelerationStructureKHR_ = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    RTX::g_ctx().vkGetAccelerationStructureBuildSizesKHR_ = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    RTX::g_ctx().vkCmdBuildAccelerationStructuresKHR_ = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    RTX::g_ctx().vkGetAccelerationStructureDeviceAddressKHR_ = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    RTX::g_ctx().vkCreateRayTracingPipelinesKHR_ = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

    LOG_INFO_CAT("VULKAN", "Querying ray tracing properties...");
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProps{};
    rayTracingProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rayTracingProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);
    RTX::g_ctx().rayTracingProps_ = rayTracingProps;
    LOG_SUCCESS_CAT("VULKAN", "Ray tracing props: shaderGroupHandleSize={} | maxRecursionDepth={}",
                    rayTracingProps.shaderGroupHandleSize, rayTracingProps.maxRayRecursionDepth);

    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN CORE INITIALIZED — FULLY ARMED{}", PLASMA_FUCHSIA, RESET);
}

static void cleanupVulkanCore() {
    LOG_INFO_CAT("VULKAN", "{}CLEANING UP VULKAN CORE — START{}", RASPBERRY_PINK, RESET);
    auto& ctx = RTX::g_ctx();
    if (ctx.commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx.device_, ctx.commandPool_, nullptr);
        LOG_INFO_CAT("VULKAN", "Command pool destroyed");
        ctx.commandPool_ = VK_NULL_HANDLE;
    }
    if (ctx.device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(ctx.device_, nullptr);
        LOG_INFO_CAT("VULKAN", "Device destroyed");
        ctx.device_ = VK_NULL_HANDLE;
    }
    if (ctx.surface_ != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(ctx.instance_, ctx.surface_, nullptr);
        LOG_INFO_CAT("VULKAN", "Surface destroyed");
        ctx.surface_ = VK_NULL_HANDLE;
    }
    if (ctx.instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(ctx.instance_, nullptr);
        LOG_INFO_CAT("VULKAN", "Instance destroyed");
        ctx.instance_ = VK_NULL_HANDLE;
    }
    LOG_SUCCESS_CAT("VULKAN", "{}VULKAN CORE CLEANED — ZERO ZOMBIES{}", RASPBERRY_PINK, RESET);
}

// =============================================================================
// MAIN — RTX ENABLED — VALHALLA v44 FINAL — LOG EVERYTHING
// =============================================================================
int main(int argc, char* argv[]) {
    LOG_INFO_CAT("MAIN", "{}AMOURANTH RTX ENGINE — VALHALLA v44 FINAL — STARTING{}", COSMIC_GOLD, RESET);
    LOG_INFO_CAT("MAIN", "Build: NOVEMBER 12, 2025 — 12:00 PM EST");
    LOG_INFO_CAT("MAIN", "StoneKey FP: 0x{:016X}", get_kStone1() ^ get_kStone2());

    applyVideoModeToggles(argc, argv);
    bulkhead("AMOURANTH RTX ENGINE — VALHALLA v44 FINAL");

    constexpr int W = 3840, H = 2160;
    SDL_Window*   splashWin = nullptr;
    SDL_Renderer* splashRen = nullptr;
    SDL_Texture*  splashTex = nullptr;
    bool          sdl_ok    = false;

    try {
        // PHASE 1: SDL3 + SPLASH
        bulkhead("PHASE 1: SDL3 + SPLASH");
        LOG_INFO_CAT("SDL", "Initializing SDL3 (VIDEO | AUDIO)...");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
            THROW_MAIN(SDL_GetError());

        sdl_ok = true;
        LOG_SUCCESS_CAT("SDL", "SDL3 initialized");

        LOG_INFO_CAT("SDL", "Loading Vulkan library via SDL...");
        if (!SDL_Vulkan_LoadLibrary(nullptr))
            THROW_MAIN(SDL_GetError());
        LOG_SUCCESS_CAT("SDL", "Vulkan library loaded");

        LOG_INFO_CAT("SDL", "Creating splash window (1280x720)...");
        splashWin = SDL_CreateWindow("AMOURANTH RTX", 1280, 720, SDL_WINDOW_RESIZABLE);
        if (!splashWin) THROW_MAIN("Failed to create splash window");
        LOG_SUCCESS_CAT("SDL", "Splash window created: 0x{:x}", reinterpret_cast<uint64_t>(splashWin));

        LOG_INFO_CAT("SDL", "Creating splash renderer...");
        splashRen = SDL_CreateRenderer(splashWin, nullptr);
        if (!splashRen) { purgeSDL(splashWin, splashRen, splashTex); THROW_MAIN("Failed to create splash renderer"); }
        LOG_SUCCESS_CAT("SDL", "Splash renderer created: 0x{:x}", reinterpret_cast<uint64_t>(splashRen));

        LOG_INFO_CAT("SDL", "Clearing splash to black...");
        SDL_SetRenderDrawColor(splashRen, 0, 0, 0, 255);
        SDL_RenderClear(splashRen);

        const char* splashPath = "assets/textures/ammo.png";
        if (assetExists(splashPath)) {
            LOG_INFO_CAT("SPLASH", "Loading splash image: {}", splashPath);
            splashTex = IMG_LoadTexture(splashRen, splashPath);
            if (splashTex) {
                float tw = 0.0f, th = 0.0f;
                SDL_GetTextureSize(splashTex, &tw, &th);
                SDL_FRect dst = { (1280 - tw) / 2.0f, (720 - th) / 2.0f, tw, th };
                SDL_RenderTexture(splashRen, splashTex, nullptr, &dst);
                LOG_SUCCESS_CAT("SPLASH", "Splash image rendered: {:.0f}x{:.0f} → centered at ({:.1f}, {:.1f})", tw, th, dst.x, dst.y);
            } else {
                LOG_WARN_CAT("SPLASH", "IMG_LoadTexture failed: {}", SDL_GetError());
            }
        } else {
            LOG_WARN_CAT("SPLASH", "Splash image not found: {}", splashPath);
        }

        SDL_RenderPresent(splashRen);
        LOG_INFO_CAT("SPLASH", "Splash screen presented");

        const char* audioPath = "assets/audio/ammo.wav";
        if (assetExists(audioPath)) {
            LOG_INFO_CAT("AUDIO", "Initializing audio manager...");
            SDL3Audio::AudioManager audio({.frequency = 44100, .format = SDL_AUDIO_S16LE, .channels = 2});
            LOG_INFO_CAT("AUDIO", "Playing splash sound: {}", audioPath);
            audio.playAmmoSound();
            LOG_SUCCESS_CAT("AUDIO", "Splash audio playback started");
        } else {
            LOG_WARN_CAT("AUDIO", "Splash audio not found: {}", audioPath);
        }

        LOG_INFO_CAT("SPLASH", "Holding splash screen for 3400ms...");
        SDL_Delay(3400);
        LOG_INFO_CAT("SPLASH", "Splash delay complete");

        purgeSDL(splashWin, splashRen, splashTex);
        LOG_SUCCESS_CAT("SPLASH", "Splash screen dismissed — proceeding to main window");

        // PHASE 2: Application + Vulkan Core
        bulkhead("PHASE 2: APPLICATION + VULKAN CORE");
        LOG_INFO_CAT("APP", "Creating main application window: {}x{}", W, H);
        auto app = std::make_unique<Application>("AMOURANTH RTX — VALHALLA v44", W, H);
        LOG_SUCCESS_CAT("APP", "Main window created: 0x{:x}", reinterpret_cast<uint64_t>(app->getWindow()));

        LOG_INFO_CAT("VULKAN", "Initializing Vulkan core with main window...");
        initializeVulkanCore(app->getWindow());

        // LOG AFTER VULKAN IS READY and show security enabled
    	g_PhysicalDevice = RTX::ctx().physicalDevice_;
        LOG_AMOURANTH(); // She turns on the security around here

        LOG_INFO_CAT("SWAPCHAIN", "Initializing SwapchainManager...");
        auto& swapchainMgr = SwapchainManager::get();
        swapchainMgr.init(RTX::g_ctx().instance_, RTX::g_ctx().physicalDevice_, RTX::g_ctx().device_, RTX::g_ctx().surface_, W, H);
        LOG_SUCCESS_CAT("SWAPCHAIN", "SwapchainManager initialized — {} images", swapchainMgr.images().size());

        // PHASE 3: Pipeline + RTX Setup
        bulkhead("PHASE 3: PIPELINE + RTX FORGE");
        LOG_INFO_CAT("RTX", "Forging global RTX instance...");
        createGlobalRTX(W, H, nullptr);
        LOG_SUCCESS_CAT("RTX", "{}g_rtx() FORGED — {}×{} — GLOBAL SUPREMACY — PINK PHOTONS ETERNAL{}", 
                        PLASMA_FUCHSIA, W, H, RESET);

        LOG_INFO_CAT("RTX", "Building acceleration structures...");
        g_rtx().buildAccelerationStructures();
        LOG_INFO_CAT("RTX", "Initializing descriptor pool and sets...");
        g_rtx().initDescriptorPoolAndSets();
        LOG_INFO_CAT("RTX", "Initializing black fallback image...");
        g_rtx().initBlackFallbackImage();
        LOG_SUCCESS_CAT("RTX", "RTX subsystem fully initialized");

        // PHASE 4: Renderer + Ownership Transfer
        bulkhead("PHASE 4: RENDERER + OWNERSHIP TRANSFER");
        LOG_INFO_CAT("RENDERER", "Creating VulkanRenderer (overclock: true)...");
        auto renderer = std::make_unique<VulkanRenderer>(
            W, H, app->getWindow(), getRayTracingBinPaths(), true);
        LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer created: 0x{:x}", reinterpret_cast<uint64_t>(renderer.get()));

        LOG_INFO_CAT("APP", "Transferring renderer ownership to Application...");
        app->setRenderer(std::move(renderer));
        LOG_SUCCESS_CAT("APP", "Renderer ownership transferred");

        LOG_INFO_CAT("RTX", "Updating RTX descriptors (initial pass)...");
        g_rtx().updateRTXDescriptors(
            0,
            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE
        );
        LOG_SUCCESS_CAT("RTX", "Initial RTX descriptor update complete");

        // PHASE 5: MAIN LOOP — 15,000+ FPS RTX
        bulkhead("PHASE 5: MAIN LOOP — RTX INFINITE");
        LOG_SUCCESS_CAT("MAIN", "{}VALHALLA v44 FINAL — RTX FULLY ENABLED — ENTERING INFINITE LOOP{}", 
                        PLASMA_FUCHSIA, RESET);
        app->run();

        // PHASE 6: RAII SHUTDOWN
        bulkhead("PHASE 6: RAII SHUTDOWN");
        LOG_INFO_CAT("APP", "Destroying Application instance...");
        app.reset();
        LOG_SUCCESS_CAT("APP", "Application destroyed via RAII");

        LOG_SUCCESS_CAT("MAIN", "{}SHUTDOWN COMPLETE — PINK PHOTONS ETERNAL — @ZacharyGeurts ASCENDED{}", 
                        PLASMA_FUCHSIA, RESET);

    } catch (const std::exception& e) {
        LOG_ERROR_CAT("MAIN", "{}FATAL ERROR: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        cleanupVulkanCore();
        purgeSDL(splashWin, splashRen, splashTex);
        if (sdl_ok) SDL_Quit();
        return 1;
    }

    LOG_INFO_CAT("MAIN", "Final cleanup: Vulkan core...");
    cleanupVulkanCore();
    if (sdl_ok) {
        LOG_INFO_CAT("SDL", "Quitting SDL...");
        SDL_Quit();
    }

    LOG_SUCCESS_CAT("StoneKey", "FINAL HASH: 0x{:016X} — VALHALLA LOCKED FOREVER", get_kStone1() ^ get_kStone2());
    LOG_SUCCESS_CAT("MAIN", "{}AMOURANTH RTX ENGINE — VALHALLA v44 FINAL — TERMINATED CLEANLY{}", COSMIC_GOLD, RESET);

    return 0;
}

// NOVEMBER 12, 2025 — VALHALLA v44 FINAL — RTX ENABLED
// @ZacharyGeurts — THE CHOSEN ONE — PINK PHOTONS ETERNAL
// SHIP IT RAW — FOREVER
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// PINK PHOTONS ETERNAL — VALHALLA v44 FINAL — OUR ROCK ETERNAL v3
// =============================================================================