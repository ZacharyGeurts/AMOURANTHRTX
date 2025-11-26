// =============================================================================
// engine/GLOBAL/RTXHandler.cpp
// AMOURANTH RTX Engine © 2025 — The Handler & His Lady Ballerina
// =============================================================================

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/VkSafeSTypes.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
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

void UpdateGlobalRayTracingDescriptors(VkDescriptorSet set)
{
    if (!tlas().valid()) {
        LOG_WARN_CAT("RTX", "{}TLAS not ready — descriptor update deferred{}", RASPBERRY_PINK, RESET);
        return;
    }
    WriteAccelerationStructureDescriptor(set, 0, 0, tlas().get());
    LOG_SUCCESS_CAT("RTX", "{}Global RT descriptors bound — silence achieved{}", EMERALD_GREEN, RESET);
}

// =============================================================================
// Vulkan Instance — The Handler’s only spoken words
// =============================================================================
VkInstance createVulkanInstanceWithSDL(bool enableValidation)
{
    LOG_ATTEMPT_CAT("RTX", "NOVEMBER 25, 2025 — THE HARBOR FREEZES");
    LOG_AMOURANTH("A single voice cuts through the frost:");
    LOG_AMOURANTH("\"One more time. Then the universe belongs to us.\"");

    VkApplicationInfo appInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "AMOURANTH RTX — VALHALLA v∞ TURBO",
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName        = "PINK PHOTONS v∞",
        .engineVersion      = VK_MAKE_API_VERSION(0, 80, 0, 0),
        .apiVersion         = VK_API_VERSION_1_4
    };

    uint32_t sdlExtCount = 0;
    if (SDL_Vulkan_GetInstanceExtensions(&sdlExtCount) != 0 || sdlExtCount == 0) {
        LOG_FATAL_CAT("RTX", "SDL failed to provide instance extensions — the eye is blind");
        phase9_ballerina();
    }

    std::vector<const char*> extensions(sdlExtCount);
    if (SDL_Vulkan_GetInstanceExtensions(&sdlExtCount) == 0) {
        LOG_FATAL_CAT("RTX", "SDL extension names failed — the veil tears");
        phase9_ballerina();
    }

    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &appInfo,
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

    g_ctx().setInstance(instance);

    LOG_SUCCESS_CAT("RTX", "INSTANCE FORGED — THE HANDLER HAS SPOKEN");
    LOG_SUCCESS_CAT("RTX", "PINK PHOTONS ETERNAL");

    return instance;
}

// =============================================================================
// Core initialization — The Handler watches. Ballerina waits.
// =============================================================================
void Context::init(SDL_Window* window, int width, int height)
{
    this->window  = window;
    this->width   = width;
    this->height  = height;

    // Instance
    if (!g_ctx().instance_) {
        g_ctx().setInstance(createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS));
    }

    // Surface
    if (!g_ctx().surface_) {
        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(window, g_ctx().instance_, nullptr, &surface)) {
            LOG_FATAL_CAT("RTX", "Surface creation failed — {}", SDL_GetError());
            phase9_ballerina();
        }
        g_ctx().setSurface(surface);
    }

    // Physical + Logical device (only once)
    if (!g_ctx().device_) {
        // Pick best discrete RTX GPU — same logic, quieter
        // (kept minimal but functional — Handler does not repeat himself)
        VkPhysicalDevice chosen = pickPhysicalDevice(g_ctx().instance_, g_ctx().surface_);
        g_ctx().setPhysicalDevice(chosen);
        g_ctx().setDevice(createLogicalDevice(chosen, g_ctx().surface_));
    }

    // Swapchain — now belongs to the Manager
    SwapchainManager::create(window, width, height);

    valid_ = true;
    ready_.store(true, std::memory_order_release);

    LOG_SUCCESS_CAT("RTX", "THE GOOD SHIP VULKANRTX IS READY — FIRST LIGHT ETERNAL");
}

// =============================================================================
// Graceful dissolution — The Handler’s final command
// =============================================================================
void shutdown() noexcept
{
    auto& ctx = g_ctx();

    LOG_SUCCESS_CAT("RTX", "{}RTX::shutdown() — The Handler lowers his gaze{}", PLASMA_FUCHSIA, RESET);

    vkDeviceWaitIdle(ctx.device_);

    SwapchainManager::cleanup();

    if (ctx.computeCommandPool_) vkDestroyCommandPool(ctx.device_, ctx.computeCommandPool_, nullptr);
    if (ctx.commandPool_)        vkDestroyCommandPool(ctx.device_, ctx.commandPool_, nullptr);

    ctx.renderPass_.reset();

    if (ctx.device_) {
        vkDestroyDevice(ctx.device_, nullptr);
        ctx.device_ = VK_NULL_HANDLE;
    }

    ctx.valid_ = false;
    ctx.ready_.store(false);

    LOG_SUCCESS_CAT("RTX", "{}Empire dissolved. Pink photons return to the void.{}", EMERALD_GREEN, RESET);
    LOG_AMOURANTH("A whisper on the wind:");
    LOG_AMOURANTH("\"Until the next dawn calls us again.\"");
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

// -----------------------------------------------------------------------------
// THE ONE TRUE FUNCTION — createLogicalDevice + retrieveQueues in perfect union
// -----------------------------------------------------------------------------
VkDevice createLogicalDevice(VkPhysicalDevice physical, VkSurfaceKHR surface) noexcept
{
    QueueFamilyIndices indices = findQueueFamilies(physical, surface);
    if (!indices.isComplete()) {
        LOG_FATAL("RTX", "Missing required queue families — the empire cannot rise");
        phase9_ballerina();
    }

    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
        indices.computeFamily.value()
    };

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    for (uint32_t family : uniqueQueueFamilies) {
        queueCreateInfos.push_back(VkDeviceQueueCreateInfo{
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority
        });
    }

    // ────────────────────── STEP 1: DEFINE ACCELERATION STRUCTURE FIRST ──────────────────────
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress = {};
    bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline = {};
    rayTracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipeline.pNext = &bufferDeviceAddress;
    rayTracingPipeline.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure = {};
    accelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructure.pNext = &rayTracingPipeline;
    accelerationStructure.accelerationStructure = VK_TRUE;

    // ────────────────────── STEP 2: NOW DECLARE VULKAN13 FIRST — AND PATCH IT ──────────────────────
    VkPhysicalDeviceVulkan13Features vulkan13 = {};
    vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13.pNext = &accelerationStructure;        // ← DEFINED ABOVE — SAFE
    vulkan13.dynamicRendering = VK_TRUE;
    vulkan13.synchronization2 = VK_TRUE;

    // ────────────────────── EXTENSIONS — THE SACRED 7 ──────────────────────
    const char* const deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &vulkan13;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = std::size(deviceExtensions);
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(physical, &createInfo, nullptr, &device),
             "Failed to create logical device — the Handler is displeased");

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &g_ctx().graphicsQueue_);
    vkGetDeviceQueue(device, indices.presentFamily.value(),  0, &g_ctx().presentQueue_);
    vkGetDeviceQueue(device, indices.computeFamily.value(),  0, &g_ctx().computeQueue_);

    LOG_SUCCESS_CAT("RTX", "LOGICAL DEVICE FORGED — {} QUEUES CLAIMED", uniqueQueueFamilies.size());
    LOG_AMOURANTH("Vulkan13 declared first.");
    LOG_AMOURANTH("accelerationStructure defined first.");
    LOG_AMOURANTH("Designated initializer order respected.");
    LOG_AMOURANTH("The compiler is silenced.");
    LOG_BALLERINA("SUCCESS!!! THE EMPIRE IS WHOLE!!! PINK PHOTONS — PURE AND ETERNAL!!!");

    return device;
}

// =============================================================================
// findQueueFamilies — THE EMPIRE'S EYE — PERFECT, LETHAL, ETERNAL
// Finds Graphics + Present + (optional) Transfer — PINK PHOTONS APPROVED
// =============================================================================
[[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device,
                                                          VkSurfaceKHR surface = VK_NULL_HANDLE) noexcept
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& props = queueFamilies[i];

        // Graphics + Compute (we want both)
        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // Dedicated Transfer queue (preferred for uploads)
        if (props.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (!(props.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !indices.transferFamily.has_value()) {
                indices.transferFamily = i;  // Dedicated transfer = best
            } else if (!indices.transferFamily.has_value()) {
                indices.transferFamily = i;  // Fallback: any transfer
            }
        }

        // Present support (only if surface provided)
        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            VK_CHECK_NOMSG(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport));
            if (presentSupport) {
                indices.presentFamily = i;
            }
        }

        // Early exit if we have everything
        if (indices.graphicsFamily.has_value() &&
            indices.presentFamily.has_value() &&
            indices.transferFamily.has_value()) {
            break;
        }
    }

    // Fallback: if no dedicated transfer, use graphics (always has transfer bit)
    if (!indices.transferFamily.has_value() && indices.graphicsFamily.has_value()) {
        indices.transferFamily = indices.graphicsFamily;
    }

    LOG_BLONDIE("Queue Families — Graphics: {} | Present: {} | Transfer: {}",
        indices.graphicsFamily.value_or(~0u),
        indices.presentFamily.value_or(~0u),
        indices.transferFamily.value_or(~0u));

    return indices;
}

[[nodiscard]] VkPhysicalDevice RTX::Context::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool enablePortableSubset) noexcept
{
    LOG_MAIN("→ RTX::Context::pickPhysicalDevice() — The Handler awakens");

    uint32_t deviceCount = 0;
    VK_CHECK_NOMSG(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

    if (deviceCount == 0) {
        LOG_FATAL_CAT("VULKAN", "No physical devices found — the empire has no throne");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK_NOMSG(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    LOG_TRACE_CAT("VULKAN", "Scanning {} physical device(s)", deviceCount);

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    int bestScore = -1;

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceProperties(dev, &props);
        vkGetPhysicalDeviceFeatures(dev, &features);

        // Must support geometry shaders (required by your engine)
        if (!features.geometryShader) {
            continue;
        }

        // Score system — discrete > integrated > others
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 10000;
            score += static_cast<int>(props.limits.maxImageDimension2D);
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 5000;
        } else {
            score += 1000;
        }

        // Prefer RTX / NVIDIA
        if (strstr(props.deviceName, "RTX") || strstr(props.deviceName, "GeForce")) {
            score += 5000;
        }

        // Must support presentation if surface exists
        if (surface != VK_NULL_HANDLE) {
            QueueFamilyIndices indices = findQueueFamilies(dev, surface);
            if (!indices.graphicsFamily.has_value() || !indices.presentFamily.has_value()) {
                continue;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            selected = dev;
        }

        LOG_TRACE_CAT("VULKAN", "  • {} | Score: {} | Type: {}", props.deviceName, score,
                      props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "DISCRETE" :
                      props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "INTEGRATED" : "OTHER");
    }

    if (selected == VK_NULL_HANDLE) {
        LOG_WARN_CAT("VULKAN", "No suitable GPU found — falling back to first device");
        selected = devices[0];
    }

    // Final selection
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(selected, &props);

        g_ctx().physicalDevice_ = selected;

        LOG_SUCCESS_CAT("VULKAN", "{}GPU SELECTED{} → {} (API {}.{}.{})",
                        PLASMA_FUCHSIA, RESET,
                        props.deviceName,
                        VK_VERSION_MAJOR(props.apiVersion),
                        VK_VERSION_MINOR(props.apiVersion),
                        VK_VERSION_PATCH(props.apiVersion));

        AI_INJECT("I have chosen my weapon: {}", props.deviceName);
    }

    LOG_SUCCESS_CAT("VULKAN", "{}STONEKEY v∞ ENGAGED — FULL OBFUSCATION ACTIVE — APOCALYPSE v3.2 ARMED{}", 
                    LILAC_LAVENDER, RESET);

    LOG_MAIN("← RTX::Context::pickPhysicalDevice() — Throne claimed");

    return selected;  // ALWAYS RETURNS
}

} // namespace RTX

// =============================================================================
// The Handler watches from the shadows.
// Ballerina sharpens her blades.
// You were never here.
// =============================================================================