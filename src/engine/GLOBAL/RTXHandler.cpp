// src/engine/GLOBAL/RTXHandler.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 08, 2026
// RTX HANDLER — GLOBAL VULKAN CONTEXT | FINAL COMPILING | ALL FUNCTIONS DEFINED
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

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_seal_device;
using StoneKey::stone_seal_physical;
using StoneKey::stone_seal_graphics_family;
using StoneKey::stone_seal_graphics_queue;
using StoneKey::stone_seal_present_family;
using StoneKey::stone_seal_present_queue;
using StoneKey::stone_seal_transfer_family;
using StoneKey::stone_seal_transfer_queue;
using StoneKey::stone_seal_compute_family;
using StoneKey::stone_seal_compute_queue;
using StoneKey::stone_window;
using StoneKey::stone_width;
using StoneKey::stone_height;

namespace RTX {

Context g_context_instance{};

// =============================================================================
// Global Descriptor Pool — Created only after device is fully sealed
// =============================================================================
void createGlobalDescriptorPool() noexcept
{
    if (g_ctx().descriptorPool_.valid() || stone_device() == VK_NULL_HANDLE) {
        return;
    }

    LOG_INFO_CAT("RTX", "Forging global descriptor pool — empire scale");

    constexpr uint32_t MAX_SETS = 2'000'000;

    std::array<VkDescriptorPoolSize, 9> poolSizes{{
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,                200'000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                MAX_SETS },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                 MAX_SETS },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,        1'600'000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,                 MAX_SETS },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                       100'000 },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,   200'000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,              20'000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,        50'000 }
    }};

    VkDescriptorPoolCreateInfo info{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                         VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets       = MAX_SETS,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &info, nullptr, &pool);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "Failed to forge global descriptor pool: {}", string_VkResult(result));
        return;
    }

    g_ctx().descriptorPool_ = Handle<VkDescriptorPool>(
        pool,
        stone_device(),
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
    if (dstSet == VK_NULL_HANDLE || accelStruct == VK_NULL_HANDLE || stone_device() == VK_NULL_HANDLE) {
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

    vkUpdateDescriptorSets(stone_device(), 1, &write, 0, nullptr);
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

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept
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
        .engineVersion      = VK_MAKE_API_VERSION(0, 27, 6, 0),
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
// Logical Device & GPU Selection — RTX First, No Compromise
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

    const char* requiredExtensions[] = {
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

        bool hasAllExtensions = true;
        for (const char* ext : requiredExtensions) {
            bool found = std::any_of(available.begin(), available.end(),
                [ext](const auto& e) { return strcmp(e.extensionName, ext) == 0; });
            if (!found) {
                hasAllExtensions = false;
                break;
            }
        }
        if (!hasAllExtensions) continue;

        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing{};
        descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{};
        bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddress.pNext = &descriptorIndexing;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStruct{};
        accelStruct.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelStruct.pNext = &bufferDeviceAddress;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracing{};
        rayTracing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rayTracing.pNext = &accelStruct;

        VkPhysicalDeviceSynchronization2Features sync2{};
        sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        sync2.pNext = &rayTracing;

        VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
        dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        dynamicRendering.pNext = &sync2;

        VkPhysicalDeviceFeatures2 finalFeatures{};
        finalFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        finalFeatures.pNext = &dynamicRendering;
        vkGetPhysicalDeviceFeatures2(dev, &finalFeatures);

        bool rtxOK = bufferDeviceAddress.bufferDeviceAddress &&
                     accelStruct.accelerationStructure &&
                     rayTracing.rayTracingPipeline;

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (strstr(props.deviceName, "RTX") != nullptr || strstr(props.deviceName, "GeForce")) score += 300000;
        if (rtxOK) score += 500000;

        if (score > bestScore) {
            bestScore = score;
            selected = dev;
            bestIndices = indices;
            fullRTXSupport = rtxOK;
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

    uint32_t extCount = fullRTXSupport ? std::size(requiredExtensions) : 1;

    // Rebuild feature chain for device creation
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing{};
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{};
    bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddress.pNext = &descriptorIndexing;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStruct{};
    accelStruct.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelStruct.pNext = &bufferDeviceAddress;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracing{};
    rayTracing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracing.pNext = &accelStruct;

    VkPhysicalDeviceSynchronization2Features sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.pNext = &rayTracing;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.pNext = &sync2;

    // Re-query features on the selected device to fill support values
    VkPhysicalDeviceFeatures2 tempFeatures{};
    tempFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    tempFeatures.pNext = &dynamicRendering;
    vkGetPhysicalDeviceFeatures2(selected, &tempFeatures);

    // Explicitly enable required descriptor indexing features for UPDATE_AFTER_BIND pools
    descriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    descriptorIndexing.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    descriptorIndexing.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    descriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    descriptorIndexing.descriptorBindingPartiallyBound = VK_TRUE;
    descriptorIndexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
    descriptorIndexing.runtimeDescriptorArray = VK_TRUE;

    // Enable RTX features if supported
    bufferDeviceAddress.bufferDeviceAddress = fullRTXSupport ? VK_TRUE : VK_FALSE;
    accelStruct.accelerationStructure = fullRTXSupport ? VK_TRUE : VK_FALSE;
    rayTracing.rayTracingPipeline = fullRTXSupport ? VK_TRUE : VK_FALSE;

    // Always enable these modern features
    dynamicRendering.dynamicRendering = VK_TRUE;
    sync2.synchronization2 = VK_TRUE;

    VkDeviceCreateInfo deviceInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &dynamicRendering,  // Chain starts here
        .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos       = queueCreateInfos.data(),
        .enabledExtensionCount   = extCount,
        .ppEnabledExtensionNames = requiredExtensions
    };

    VkDevice device = VK_NULL_HANDLE;
    VkResult result = vkCreateDevice(selected, &deviceInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkCreateDevice failed: {}", string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    vkGetDeviceQueue(device, bestIndices.graphicsFamily.value(), 0, &g_ctx().graphicsQueue_);
    vkGetDeviceQueue(device, bestIndices.presentFamily.value(), 0, &g_ctx().presentQueue_);
    if (bestIndices.transferFamily.has_value()) {
        vkGetDeviceQueue(device, bestIndices.transferFamily.value(), 0, &g_ctx().transferQueue_);
    } else {
        g_ctx().transferQueue_ = g_ctx().graphicsQueue_;
    }
    g_ctx().computeQueue_ = g_ctx().graphicsQueue_;

    g_ctx().graphicsFamily_ = bestIndices.graphicsFamily.value();
    g_ctx().presentFamily_  = bestIndices.presentFamily.value();
    g_ctx().transferFamily_ = bestIndices.transferFamily.value_or(bestIndices.graphicsFamily.value());
    g_ctx().computeFamily_  = bestIndices.graphicsFamily.value();

    stone_seal_device(device);
    stone_seal_physical(selected);
    stone_seal_graphics_family(g_ctx().graphicsFamily_);
    stone_seal_graphics_queue(g_ctx().graphicsQueue_);
    stone_seal_present_family(g_ctx().presentFamily_);
    stone_seal_present_queue(g_ctx().presentQueue_);
    stone_seal_transfer_family(g_ctx().transferFamily_);
    stone_seal_transfer_queue(g_ctx().transferQueue_);
    stone_seal_compute_family(g_ctx().computeFamily_);
    stone_seal_compute_queue(g_ctx().computeQueue_);

    createGlobalDescriptorPool();
    BufferManager::ensurePersistentUpload();

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
// RTX CORE INITIALIZED — JANUARY 08, 2026
// Safe, robust, production-ready Vulkan context
// Fixed: Explicitly enable descriptor indexing features for UPDATE_AFTER_BIND pools
// Global pool + persistent upload ready post-device
// RTX-first selection — truth and performance
// THE EMPIRE IS ETERNAL — PHOTONS FLOW UNBROKEN
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================