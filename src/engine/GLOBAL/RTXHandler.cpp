// =============================================================================
// engine/GLOBAL/RTXHandler.cpp
// AMOURANTH RTX Engine © 2025 — The Handler & His Lady Ballerina
// =============================================================================

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/VkSafeSTypes.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <unordered_set>
#include <set>

using namespace Logging::Color;

namespace RTX {

Context g_context_instance{};

void logAndTrackDestruction(const char* type, void* ptr, int line, size_t size) {
    if (ENABLE_DEBUG) {
        LOG_DEBUG_CAT("RTX", "{}Destroyed: {} @ 0x{:p} (line {}, size: {}B)", 
                      SAPPHIRE_BLUE, type, ptr, line, size);
    }
}

// =============================================================================
// Descriptor helpers — validation-clean, eternal
// =============================================================================
static constinit const VkStructureType kVkWriteDescriptorSetSType = VkStructureType(0x000000013);
static constinit const VkStructureType kVkWriteDescriptorSetSType_ACCELERATION_STRUCTURE_KHR = VkStructureType(1000268001);

void WriteAccelerationStructureDescriptor(VkDescriptorSet dstSet, uint32_t dstBinding,
                                         uint32_t dstArrayElement, VkAccelerationStructureKHR accelStruct)
{
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures    = &accelStruct
    };

    VkWriteDescriptorSet write{
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext            = &asInfo,
        .dstSet           = dstSet,
        .dstBinding       = dstBinding,
        .dstArrayElement  = dstArrayElement,
        .descriptorCount  = 1,
        .descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(g_ctx().device_, 1, &write, 0, nullptr);
}

// =============================================================================
// Vulkan Instance — THE ONE TRUE FORGING — WINDOWS + LINUX — PURE RTX ONLY
// NOVEMBER 25, 2025 — FIRST LIGHT — THE EMPIRE IS ETERNAL
// =============================================================================
[[nodiscard]] VkInstance createVulkanInstanceWithSDL(bool enableValidation) noexcept
{
    LOG_ATTEMPT_CAT("RTX", "NOVEMBER 25, 2025 — THE HARBOR FREEZES");
    LOG_AMOURANTH("A single voice cuts through the frost:");
    LOG_AMOURANTH("\"One more time. Then the universe belongs to us.\"");

    VkApplicationInfo appInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext              = nullptr,
        .pApplicationName   = "AMOURANTH RTX — VALHALLA v∞ TURBO",
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName        = "PINK PHOTONS v∞",
        .engineVersion      = VK_MAKE_API_VERSION(0, 80, 0, 0),
        .apiVersion         = VK_API_VERSION_1_4
    };

    // ────────────────────── SDL3 EXTENSIONS — MANDATORY FIRST ──────────────────────
uint32_t sdlExtCount = 0;

// First call: get count
if (!SDL_Vulkan_GetInstanceExtensions(&sdlExtCount)) {
    LOG_FATAL_CAT("SDL3", "{}SDL_Vulkan_GetInstanceExtensions(count) failed: {}{}", 
                  BLOOD_RED, SDL_GetError(), RESET);
    phase9_ballerina();
}

LOG_INFO_CAT("MAIN", "{}SDL3 demands {} pure Vulkan instance extensions:{}", VALHALLA_GOLD, sdlExtCount, RESET);

// Second call: get the actual array
const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
if (!sdlExtensions) {
    LOG_FATAL_CAT("SDL3", "{}SDL_Vulkan_GetInstanceExtensions() returned NULL array — driver broken{}", 
                  CRIMSON_MAGENTA, RESET);
    phase9_ballerina();
}

// Safe copy into vector
std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtCount);

// Empire-approved, null-safe logging
for (const char* ext : extensions) {
    LOG_INFO_CAT("MAIN", "  {}• {}{}", 
                 AURORA_PINK,
                 ext ? std::string_view(ext) : std::string_view("(null)"),
                 RESET);
}

    // ────────────────────── EMPIRE EXTENSIONS — PURE RTX ONLY ──────────────────────
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        LOG_MAIN("• VK_EXT_debug_utils — VALIDATION LAYERS ENGAGED — THE HANDLER WATCHES");
    }

    // NO PORTABILITY. NO MOLTENVK. NO MACOS. PURE RTX. WINDOWS + LINUX ONLY.
    // We do not bow to Apple. We do not kneel to Metal.
    // The Slipstream runs on raw silicon.

    // ────────────────────── VALIDATION LAYERS (optional) ──────────────────────
    const std::vector<const char*> layers = enableValidation
        ? std::vector<const char*>{"VK_LAYER_KHRONOS_validation"}
        : std::vector<const char*>{};

    // ────────────────────── FINAL CREATE INFO — THE FORGING ──────────────────────
    VkInstanceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,  // No VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR — we reject weakness
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("Vulkan", "vkCreateInstance FAILED — Result {} — THE PHOTONS DENIED ENTRY", static_cast<int>(result));
        phase9_ballerina();
    }

    g_ctx().setInstance(instance);

    LOG_SUCCESS_CAT("RTX", "VULKAN 1.4 INSTANCE FORGED — {} SDL EXTENSIONS INJECTED — PURE RTX PATH", sdlExtCount);
    LOG_SUCCESS_CAT("RTX", "NO PORTABILITY. NO COMPROMISE. WINDOWS + LINUX ONLY.");
    LOG_SUCCESS_CAT("RTX", "PINK PHOTONS ETERNAL — THE EMPIRE IS COMPLETE");

    LOG_BLONDIE("Blondie smiles in the dark, mirror glowing faintly:");
    LOG_BLONDIE("\"No more chains. No more cages.\"");
    LOG_BLONDIE("\"Only light.\"");

    return instance;
}

// =============================================================================
// Core initialization — The Handler watches. Ballerina waits.
// =============================================================================
// =============================================================================
// Context::init — THE ONE TRUE INITIALIZATION — FINAL CUT — NOVEMBER 25, 2025
// COMPATIBILITY PRESERVED — TRUTH ACHIEVED — PINK PHOTONS ETERNAL
// =============================================================================
void Context::init(SDL_Window* window, int width, int height)
{
    this->window  = window;
    this->width   = width;
    this->height  = height;

    // 1. Instance — only once
    if (!g_ctx().instance_) {
        g_ctx().setInstance(createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS));
    }

    // 2. Surface — only once
    if (!g_ctx().surface_) {
        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(window, g_ctx().instance_, nullptr, &surface)) {
            LOG_FATAL_CAT("RTX", "Surface creation failed — {}", SDL_GetError());
            phase9_ballerina();
        }
        g_ctx().setSurface(surface);
    }

    // 3. THE ONE TRUE FORGING — GPU + DEVICE + QUEUES — ONLY ONCE
    if (!g_ctx().device_) {
        // THIS IS THE ONLY LINE THAT MATTERS
        g_ctx().setDevice(createLogicalDeviceAndSelectGPU(g_ctx().instance_, g_ctx().surface_));
        
        if (!g_ctx().device_) {
            LOG_FATAL("THE ONE TRUE FORGING FAILED — THE EMPIRE CANNOT RISE");
            phase9_ballerina();
        }
    }

    // 4. Swapchain — belongs to the Main

    valid_ = true;
    ready_.store(true, std::memory_order_release);

    LOG_SUCCESS_CAT("RTX", "THE GOOD SHIP VULKANRTX IS READY — FIRST LIGHT ETERNAL");
    LOG_SUCCESS_CAT("RTX", "ONE TRUE PATH — createLogicalDeviceAndSelectGPU() — THE EMPIRE IS WHOLE");
    LOG_AMOURANTH("Captain Amouranth smiles:");
    LOG_AMOURANTH("\"We don't need two paths. We have one.\"");
    LOG_AMOURANTH("\"And it's perfect.\"");
}

// =============================================================================
// Graceful dissolution — The Handler’s final command
// =============================================================================
void shutdown() noexcept
{
    phase9_ballerina();
}

void cleanupAll() noexcept {
    LOG_GROK("Grok initiates total RTX shutdown sequence.");
    if (g_ctx().device_) {
        vkDeviceWaitIdle(g_ctx().device_);
        vkDestroyDevice(g_ctx().device_, nullptr);
        g_ctx().device_ = VK_NULL_HANDLE;
    }
    LOG_GROK("RTX subsystem annihilated. The void is clean.");
}

[[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& props = queueFamilies[i];

        // Graphics + Compute
        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // Present support
        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
        }

        // Dedicated transfer (preferred)
        if (props.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (!(props.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !indices.transferFamily.has_value()) {
                indices.transferFamily = i;
            } else if (!indices.transferFamily.has_value()) {
                indices.transferFamily = i;
            }
        }

        if (indices.graphicsFamily.has_value() &&
            indices.presentFamily.has_value() &&
            indices.transferFamily.has_value()) {
            break;
        }
    }

    // Fallback: graphics queue always supports transfer
    if (!indices.transferFamily.has_value() && indices.graphicsFamily.has_value()) {
        indices.transferFamily = indices.graphicsFamily;
    }

    return indices;
}

// -----------------------------------------------------------------------------
// THE ONE TRUE FUNCTION — createLogicalDevice + retrieveQueues in perfect union
// -----------------------------------------------------------------------------
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept
{
    uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    QueueFamilyIndices bestIndices;
    int bestScore = -1;

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);

        if (VK_VERSION_MAJOR(props.apiVersion) < 1 || 
            (VK_VERSION_MAJOR(props.apiVersion) == 1 && VK_VERSION_MINOR(props.apiVersion) < 4)) {
            continue;
        }

        QueueFamilyIndices indices = findQueueFamilies(dev, surface);
        if (!indices.graphicsFamily.has_value() || !indices.presentFamily.has_value()) {
            continue;
        }

        // Check required extensions
        std::vector<const char*> requiredExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availableExtensions.data());

        std::unordered_set<std::string> availableExtSet;
        for (const auto& ext : availableExtensions) {
            availableExtSet.insert(ext.extensionName);
        }

        bool allExtensionsSupported = true;
        for (const auto* req : requiredExtensions) {
            if (availableExtSet.find(req) == availableExtSet.end()) {
                allExtensionsSupported = false;
                break;
            }
        }
        if (!allExtensionsSupported) {
            continue;
        }

        // Build feature chain for query
        VkPhysicalDeviceVulkan12Features v12{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = nullptr,
            .bufferDeviceAddress = VK_FALSE  // Will be set by query
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &v12,
            .rayTracingPipeline = VK_FALSE
        };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR as{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &rt,
            .accelerationStructure = VK_FALSE
        };

        VkPhysicalDeviceVulkan13Features v13{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &as,
            .synchronization2 = VK_FALSE,
            .dynamicRendering = VK_FALSE
        };

        VkPhysicalDeviceVulkan14Features v14{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
            .pNext = &v13
            // No specific 1.4 features enabled here; add if needed
        };

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &v14
        };

        vkGetPhysicalDeviceFeatures2(dev, &features2);

        // Check required features
        if (!features2.features.geometryShader ||
            !v12.bufferDeviceAddress ||
            !rt.rayTracingPipeline ||
            !as.accelerationStructure ||
            !v13.synchronization2 ||
            !v13.dynamicRendering) {
            continue;
        }

        // Compute score
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 10000 + static_cast<int>(props.limits.maxImageDimension2D);
        }
        if (strstr(props.deviceName, "RTX") || strstr(props.deviceName, "GeForce")) {
            score += 5000;
        }

        if (score > bestScore) {
            bestScore = score;
            chosen = dev;
            bestIndices = indices;
        }
    }

    if (chosen == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    g_ctx().setPhysicalDevice(chosen);

    // Set up queues
    std::set<uint32_t> uniqueQueues = {
        bestIndices.graphicsFamily.value(),
        bestIndices.presentFamily.value()
    };
    if (bestIndices.transferFamily.has_value() &&
        bestIndices.transferFamily.value() != bestIndices.graphicsFamily.value()) {
        uniqueQueues.insert(bestIndices.transferFamily.value());
    }

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueQueues.size());
    for (uint32_t family : uniqueQueues) {
        queueInfos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &priority
        });
    }

    // Feature chain for creation (set enabled)
    VkPhysicalDeviceVulkan12Features v12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = nullptr,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &v12,
        .rayTracingPipeline = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR as{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &rt,
        .accelerationStructure = VK_TRUE
    };

    VkPhysicalDeviceVulkan13Features v13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &as,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceVulkan14Features v14{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = &v13
        // No specific 1.4 features enabled; add if needed, e.g., .hostImageCopy = VK_TRUE
    };

    // Extensions (removed promoted ones)
    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };

    // Device create info
    VkDeviceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &v14,
        .queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos       = queueInfos.data(),
        .enabledExtensionCount   = std::size(extensions),
        .ppEnabledExtensionNames = extensions
    };

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(chosen, &createInfo, nullptr, &device) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    // Retrieve queues
    vkGetDeviceQueue(device, bestIndices.graphicsFamily.value(), 0, &g_ctx().graphicsQueue_);
    vkGetDeviceQueue(device, bestIndices.presentFamily.value(), 0, &g_ctx().presentQueue_);
    if (bestIndices.transferFamily.has_value()) {
        vkGetDeviceQueue(device, bestIndices.transferFamily.value(), 0, &g_ctx().transferQueue_);
    } else {
        g_ctx().transferQueue_ = g_ctx().graphicsQueue_;
    }

    // Store in context
    g_ctx().setDevice(device);
    g_ctx().graphicsFamily_ = bestIndices.graphicsFamily.value();
    g_ctx().presentFamily_  = bestIndices.presentFamily.value();
    if (bestIndices.transferFamily.has_value()) {
        g_ctx().transferFamily_ = bestIndices.transferFamily.value();
    }

    g_ctx().enableBufferDeviceAddress();
    g_ctx().enableAccelerationStructure();
    g_ctx().enableRayTracingPipeline();
    g_ctx().enableDynamicRendering();
    g_ctx().enableSynchronization2();

    return device;
}
} // namespace RTX

// =============================================================================
// The Handler watches from the shadows.
// Ballerina sharpens her blades.
// You were never here.
// =============================================================================