// src/engine/SDL3/SDL3_vulkan.cpp
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
// SDL3Vulkan IMPLEMENTATION — ONLY PLACE VulkanRenderer IS TOUCHED
// • Header is PURE — no VulkanRenderer.hpp
// • Global g_vulkanRenderer defined here
// • initVulkan() fully implemented
// • SHIP IT RAW — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"   // ← ONLY HERE
#include "engine/GLOBAL/SwapchainManager.hpp"

std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

// ──────────────────────────────────────────────────────────────────────────────
// SDL3Vulkan::initRenderer — CREATE RENDERER
// ──────────────────────────────────────────────────────────────────────────────
void SDL3Vulkan::initRenderer(std::shared_ptr<Context> ctx, int w, int h) {
    g_vulkanRenderer = std::make_unique<VulkanRenderer>(std::move(ctx), w, h);
    SwapchainManager::get().init(
        ctx->vkInstance,
        ctx->vkPhysicalDevice,
        ctx->vkDevice,
        ctx->vkSurface,
        static_cast<uint32_t>(w),
        static_cast<uint32_t>(h)
    );
}

// ──────────────────────────────────────────────────────────────────────────────
// SDL3Vulkan::shutdownRenderer — DESTROY RENDERER
// ──────────────────────────────────────────────────────────────────────────────
void SDL3Vulkan::shutdownRenderer() noexcept {
    SwapchainManager::get().cleanup();
    g_vulkanRenderer.reset();
}

// ──────────────────────────────────────────────────────────────────────────────
// initVulkan — FULL IMPLEMENTATION
// ──────────────────────────────────────────────────────────────────────────────
void initVulkan(
    SDL_Window* window,
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation,
    bool preferNvidia,
    bool rt,
    std::string_view title,
    VkPhysicalDevice& physicalDevice
) noexcept {

    // === INSTANCE EXTENSIONS FROM SDL3 ===
    unsigned int extCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&extCount, nullptr)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed (count): {}", SDL_GetError());
        return;
    }

    std::vector<const char*> extensions(extCount);
    if (!SDL_Vulkan_GetInstanceExtensions(&extCount, extensions.data())) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_GetInstanceExtensions failed (names): {}", SDL_GetError());
        return;
    }

    // === RTX EXTENSIONS ===
    if (rt) {
        extensions.insert(extensions.end(), {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_SHADER_CLOCK_EXTENSION_NAME
        });
    }

    // === VALIDATION LAYERS ===
    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    // === APPLICATION INFO ===
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX",
        .engineVersion = VK_MAKE_VERSION(3, 33, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    // === CREATE INSTANCE ===
    VkInstance rawInstance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &rawInstance);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkCreateInstance failed: {}", vkResultToString(result));
        return;
    }

    instance = VulkanInstancePtr(rawInstance);
    LOG_SUCCESS_CAT("Vulkan", "{}VkInstance created @ {:p} — OLD GOD GLOBAL ENGAGED{}", Color::PLASMA_FUCHSIA, static_cast<void*>(rawInstance), Color::RESET);

    // === CREATE SURFACE ===
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, rawInstance, nullptr, &rawSurface)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return;
    }

    surface = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    LOG_SUCCESS_CAT("Vulkan", "{}VkSurfaceKHR created @ {:p} — RASPBERRY_PINK ETERNAL{}", Color::RASPBERRY_PINK, static_cast<void*>(rawSurface), Color::RESET);

    // === PHYSICAL DEVICE SELECTION ===
    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(rawInstance, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(rawInstance, &gpuCount, gpus.data());

    physicalDevice = VK_NULL_HANDLE;
    for (auto gpu : gpus) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(gpu, &props);
        LOG_INFO_CAT("Vulkan", "GPU: {} (type: {})", props.deviceName, props.deviceType);

        if (preferNvidia && std::string_view(props.deviceName).find("NVIDIA") != std::string_view::npos) {
            physicalDevice = gpu;
            break;
        }
        if (!physicalDevice) physicalDevice = gpu;
    }

    if (!physicalDevice) {
        LOG_ERROR_CAT("Vulkan", "No suitable GPU found — RTX OFFLINE");
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    LOG_SUCCESS_CAT("Vulkan", "{}PhysicalDevice selected: {} — TITAN POWER{}", Color::EMERALD_GREEN, props.deviceName, Color::RESET);

    // === QUEUE FAMILIES ===
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int graphicsFamily = -1, presentFamily = -1;
    for (int i = 0; i < static_cast<int>(queueFamilyCount); ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, rawSurface, &presentSupport);
        if (presentSupport) presentFamily = i;

        if (graphicsFamily != -1 && presentFamily != -1) break;
    }

    if (graphicsFamily == -1 || presentFamily == -1) {
        LOG_ERROR_CAT("Vulkan", "Required queue families not found");
        return;
    }

    // === DEVICE EXTENSIONS ===
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    if (rt) {
        deviceExtensions.insert(deviceExtensions.end(), {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_RAY_QUERY_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        });
    }

    // === DEVICE FEATURES CHAIN ===
    VkPhysicalDeviceBufferDeviceAddressFeatures addrFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .rayTracingPipeline = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &addrFeatures
    };

    void* pNext = &features2;
    if (rt) {
        addrFeatures.pNext = &asFeatures;
        asFeatures.pNext = &rtFeatures;
        pNext = &features2;
    }

    // === QUEUE CREATE INFO ===
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { static_cast<uint32_t>(graphicsFamily), static_cast<uint32_t>(presentFamily) };
    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        queueCreateInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        });
    }

    // === CREATE LOGICAL DEVICE ===
    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = pNext,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()
    };

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create logical device");
        return;
    }

    LOG_SUCCESS_CAT("Vulkan", "{}Logical device created @ {:p} — AMOURANTH RTX READY{}", Color::PLASMA_FUCHSIA, static_cast<void*>(device), Color::RESET);

    // === POPULATE GLOBAL CONTEXT ===
    auto& c = *ctx();
    c.vkInstance = rawInstance;
    c.vkPhysicalDevice = physicalDevice;
    c.vkDevice = device;
    c.vkSurface = rawSurface;

    LOG_SUCCESS_CAT("Vulkan", "{}initVulkan complete — OLD GOD GLOBAL ENGAGED — 3.33 Hz vacuum phonon locked{}", Color::COSMIC_GOLD, Color::RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// shutdownVulkan — FULL CLEANUP
// ──────────────────────────────────────────────────────────────────────────────
void shutdownVulkan() noexcept {
    if (ctx()->vkDevice) {
        vkDeviceWaitIdle(ctx()->vkDevice);
        vkDestroyDevice(ctx()->vkDevice, nullptr);
        ctx()->vkDevice = VK_NULL_HANDLE;
    }

    ctx()->vkInstance = VK_NULL_HANDLE;
    ctx()->vkSurface = VK_NULL_HANDLE;
    ctx()->vkPhysicalDevice = VK_NULL_HANDLE;

    cleanupAll();  // ← Dispose::cleanupAll() — shred + purge
    LOG_SUCCESS_CAT("Vulkan", "{}VULKAN SHUTDOWN COMPLETE — PINK PHOTONS REST ETERNAL{}", Color::PLASMA_FUCHSIA, Color::RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// GETTERS
// ──────────────────────────────────────────────────────────────────────────────
VkInstance getVkInstance(const VulkanInstancePtr& instance) noexcept { return instance ? instance.get() : VK_NULL_HANDLE; }
VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) noexcept { return surface ? surface.get() : VK_NULL_HANDLE; }

std::vector<std::string> getVulkanExtensions() {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());

    std::vector<std::string> names;
    names.reserve(count);
    for (const auto& p : props) {
        names.emplace_back(p.extensionName);
    }
    return names;
}

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