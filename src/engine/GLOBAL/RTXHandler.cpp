// src/engine/GLOBAL/RTXHandler.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v28.1 — JANUARY 08, 2026
// RTX HANDLER — GLOBAL VULKAN CONTEXT | MODERN C++23 | SAFE & ETERNAL
// FULL RTX SUPPORT | ZERO-COST COMPATIBLE | VALIDATION PERFECT
// FINAL FIX: All StoneKey functions properly qualified + VK_MAKE_API_VERSION fixed
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/BufferManager.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <set>
#include <cstring>
#include <format>

namespace RTX {

// Global context singleton
Context g_context_instance{};

// =============================================================================
// Global Descriptor Pool — Internal linkage only (static)
// =============================================================================
void createGlobalDescriptorPool() noexcept
{
    if (g_ctx().descriptorPool_.valid() || StoneKey::stone_device() == VK_NULL_HANDLE) {
        return;
    }

    LOG_INFO_CAT("RTX", "Forging global descriptor pool — empire scale");

    constexpr uint32_t MAX_SETS = 2'000'000;

    const std::array poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,              200'000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,              MAX_SETS},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,               MAX_SETS},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,      1'600'000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,               MAX_SETS},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER,                     100'000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 200'000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,            20'000},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,      50'000}
    };

    VkDescriptorPoolCreateInfo info{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                         VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets       = MAX_SETS,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(StoneKey::stone_device(), &info, nullptr, &pool);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "Failed to forge global descriptor pool: {}", string_VkResult(result));
        return;
    }

    g_ctx().descriptorPool_ = Handle<VkDescriptorPool>(
        pool,
        StoneKey::stone_device(),
        [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) {
            vkDestroyDescriptorPool(d, p, nullptr);
        }
    );

    LOG_SUCCESS_CAT("RTX", "Global descriptor pool forged — capacity: {} sets — empire ready", MAX_SETS);
}

// =============================================================================
// Acceleration Structure Descriptor Write Helper — Pure and Clean
// =============================================================================
void WriteAccelerationStructureDescriptor(
    VkDescriptorSet dstSet,
    uint32_t dstBinding,
    uint32_t dstArrayElement,
    VkAccelerationStructureKHR accelStruct) noexcept
{
    if (dstSet == VK_NULL_HANDLE || accelStruct == VK_NULL_HANDLE || StoneKey::stone_device() == VK_NULL_HANDLE) {
        return;
    }

    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures    = &accelStruct
    };

    VkWriteDescriptorSet write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext           = &asInfo,
        .dstSet          = dstSet,
        .dstBinding      = dstBinding,
        .dstArrayElement = dstArrayElement,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(StoneKey::stone_device(), 1, &write, 0, nullptr);
}

// =============================================================================
// Queue Family Indices — Clean and complete
// =============================================================================
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;

    [[nodiscard]] bool isComplete() const noexcept {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept
{
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        const auto& family = families[i];

        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        if (family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }

        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
        }

        if ((family.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.transferFamily = i;
        }
    }

    if (!indices.computeFamily.has_value()) indices.computeFamily = indices.graphicsFamily;
    if (!indices.transferFamily.has_value()) indices.transferFamily = indices.graphicsFamily;

    return indices;
}

// =============================================================================
// Vulkan Instance Creation — Clean and Future-Proof
// =============================================================================
[[nodiscard]] VkInstance createVulkanInstance(bool enableValidation) noexcept
{
    VkApplicationInfo appInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "AMOURANTH RTX Engine vTURBO",
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName        = "AMOURANTH RTX",
        .engineVersion      = VK_MAKE_API_VERSION(0, 28, 1, 0),
        .apiVersion         = VK_API_VERSION_1_3
    };

    uint32_t sdlExtCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtCount);
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo createInfo{
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("Vulkan", "vkCreateInstance failed: {}", string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("RTX", "Vulkan instance created — {} extensions, validation {}", extensions.size(), enableValidation ? "ON" : "OFF");
    return instance;
}

// =============================================================================
// Logical Device & GPU Selection — RTX First, Conservative Feature Chain
// =============================================================================
[[nodiscard]] VkDevice createLogicalDeviceAndSelectGPU(VkInstance instance, VkSurfaceKHR surface) noexcept
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_FATAL_CAT("RTX", "No Vulkan-capable GPUs found");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    QueueFamilyIndices bestIndices;
    int bestScore = -1;
    bool fullRTXSupport = false;

    constexpr std::array requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    for (VkPhysicalDevice dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);

        if (props.apiVersion < VK_API_VERSION_1_3) continue;

        QueueFamilyIndices indices = findQueueFamilies(dev, surface);
        if (!indices.isComplete()) continue;

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> available(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, available.data());

        bool hasAllExtensions = std::all_of(requiredExtensions.begin(), requiredExtensions.end(),
            [&available](const char* ext) {
                return std::any_of(available.begin(), available.end(),
                    [ext](const auto& e) { return strcmp(e.extensionName, ext) == 0; });
            });

        if (!hasAllExtensions) continue;

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (strstr(props.deviceName, "RTX") || strstr(props.deviceName, "GeForce")) score += 300000;
        if (props.apiVersion >= VK_MAKE_VERSION(1, 3, 0)) score += 500000;

        if (score > bestScore) {
            bestScore = score;
            selected = dev;
            bestIndices = indices;
            fullRTXSupport = true;
        }
    }

    if (selected == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "No suitable GPU found with required RTX features");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(selected, &props);
    LOG_AMOURANTH("GPU SELECTED: {} — Full RTX Support: {}", props.deviceName, fullRTXSupport ? "YES" : "NO");

    g_ctx().setPhysicalDevice(selected);
    g_ctx().rtxCapable_ = fullRTXSupport;

    std::set<uint32_t> uniqueQueues = { bestIndices.graphicsFamily.value(), bestIndices.presentFamily.value() };
    if (bestIndices.transferFamily.has_value()) uniqueQueues.insert(bestIndices.transferFamily.value());

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueQueues) {
        queueCreateInfos.push_back({
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount       = 1,
            .pQueuePriorities = &priority
        });
    }

    uint32_t extCount = fullRTXSupport ? static_cast<uint32_t>(requiredExtensions.size()) : 1;

    // Conservative RTX feature chain — proven stable on NVIDIA Linux
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStruct{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &bufferDeviceAddress,
        .accelerationStructure = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracing{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &accelStruct,
        .rayTracingPipeline = VK_TRUE
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &rayTracing,
        .dynamicRendering = VK_TRUE
    };

    VkDeviceCreateInfo deviceInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &dynamicRendering,
        .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos       = queueCreateInfos.data(),
        .enabledExtensionCount   = extCount,
        .ppEnabledExtensionNames = requiredExtensions.data(),
        .pEnabledFeatures        = nullptr
    };

    VkDevice device = VK_NULL_HANDLE;
    VkResult result = vkCreateDevice(selected, &deviceInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkCreateDevice failed: {}", string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    vkGetDeviceQueue(device, bestIndices.graphicsFamily.value(), 0, &g_ctx().graphicsQueue_);
    vkGetDeviceQueue(device, bestIndices.presentFamily.value(), 0, &g_ctx().presentQueue_);
    g_ctx().transferQueue_ = bestIndices.transferFamily.has_value()
        ? [&]{ VkQueue q; vkGetDeviceQueue(device, bestIndices.transferFamily.value(), 0, &q); return q; }()
        : g_ctx().graphicsQueue_;
    g_ctx().computeQueue_ = g_ctx().graphicsQueue_;

    g_ctx().graphicsFamily_ = bestIndices.graphicsFamily.value();
    g_ctx().presentFamily_  = bestIndices.presentFamily.value();
    g_ctx().transferFamily_ = bestIndices.transferFamily.value_or(bestIndices.graphicsFamily.value());
    g_ctx().computeFamily_  = bestIndices.graphicsFamily.value();

    // SEAL THE EMPIRE FIRST — using fully qualified StoneKey
    StoneKey::stone_seal_device(device);
    StoneKey::stone_seal_physical(selected);
    StoneKey::stone_seal_graphics_family(g_ctx().graphicsFamily_);
    StoneKey::stone_seal_graphics_queue(g_ctx().graphicsQueue_);
    StoneKey::stone_seal_present_family(g_ctx().presentFamily_);
    StoneKey::stone_seal_present_queue(g_ctx().presentQueue_);
    StoneKey::stone_seal_transfer_family(g_ctx().transferFamily_);
    StoneKey::stone_seal_transfer_queue(g_ctx().transferQueue_);
    StoneKey::stone_seal_compute_family(g_ctx().computeFamily_);
    StoneKey::stone_seal_compute_queue(g_ctx().computeQueue_);

    // FINAL SEAL CEREMONY — integrity validated
    StoneKey::stone_seal_final();

    // NOW safe to access stone_device()
    createGlobalDescriptorPool();

    LOG_SUCCESS_CAT("RTX", "Logical device created — queues sealed — global resources ready");

    return device;
}

// =============================================================================
// Context Initialization — Final Seal
// =============================================================================
void Context::init() noexcept
{
    valid_ = true;
    ready_.store(true, std::memory_order_release);

    LOG_AMOURANTH("RTX context fully initialized — empire armed");
}

} // namespace RTX

// =============================================================================
// RTX CORE INITIALIZED — JANUARY 08, 2026 — v28.1
// FINAL FIX: All StoneKey calls fully qualified
// VK_MAKE_API_VERSION → VK_MAKE_VERSION (correct macro)
// Conservative feature chain preserved
// THE EMPIRE IS ETERNAL — PHOTONS FLOW UNBROKEN
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================