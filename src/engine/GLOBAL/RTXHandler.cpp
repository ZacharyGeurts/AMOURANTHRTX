// =============================================================================
// AMOURANTH RTX Engine - RTX Handler Implementation
// Global Vulkan context, instance, device, queues
// RTX feature enablement | Descriptor indexing | totalTime driven
// Version 30.76 — January 30, 2026
// - Removed timeline semaphores completely — totalTime monolith drives timing
// - Descriptor pool delayed (called externally after first AS build)
// - All sealing centralized via StoneKey grouped sealers
// - Instance, device, physical, surface, queues, families sealed here
// - Minimal, production-stable
// - VK_EXT_descriptor_buffer enabled explicitly
// - VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure = VK_TRUE
// - No fences/semaphores — fire-and-forget submits + totalTime progression
// =============================================================================

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <set>
#include <cstring>
#include <format>
#include <vector>

// StoneKey accessors & sealers — using at top, no qualification
using StoneKey::stone_seal_device_resources;
using StoneKey::stone_seal_queues;
using StoneKey::stone_seal_families;

namespace RTX {

Context g_context_instance{};

// =============================================================================
// Queue Family Indices
// =============================================================================
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    std::optional<uint32_t> compute;
    std::optional<uint32_t> transfer;

    [[nodiscard]] bool complete() const noexcept {
        return graphics.has_value() && present.has_value();
    }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface) noexcept {
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        const auto& f = families[i];

        if (f.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphics = i;
        if (f.queueFlags & VK_QUEUE_COMPUTE_BIT)  indices.compute  = i;

        if (surface != VK_NULL_HANDLE) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if (present) indices.present = i;
        }

        if ((f.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(f.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.transfer = i;
        }
    }

    if (!indices.compute.has_value())  indices.compute  = indices.graphics;
    if (!indices.transfer.has_value()) indices.transfer = indices.graphics;

    return indices;
}

// =============================================================================
// Instance Creation — validation toggleable via Options::Debug
// =============================================================================
VkInstance createVulkanInstance() noexcept {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "AMOURANTH RTX";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 30, 0, 0);
    appInfo.pEngineName        = "VALHALLA";
    appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 30, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    uint32_t sdlCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlCount);

    std::vector<const char*> extensions(sdlExts, sdlExts + sdlCount);
    if (Options::Debug::ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    std::vector<const char*> layers;
    if (Options::Debug::ENABLE_VALIDATION_LAYERS) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames     = layers.data();
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    VkInstance inst = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&ci, nullptr, &inst);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkCreateInstance failed: {}", string_VkResult(res));
        return VK_NULL_HANDLE;
    }

    RTX::loadInstanceExtensions(inst);

    LOG_SUCCESS_CAT("RTX", "Instance created — {} extensions, validation {}",
                    extensions.size(), Options::Debug::ENABLE_VALIDATION_LAYERS ? "ON" : "OFF");
    return inst;
}

// =============================================================================
// Logical Device & GPU Selection — RTX mandatory + sealing
// =============================================================================
VkDevice createLogicalDeviceAndSelectGPU(VkInstance inst, VkSurfaceKHR surf) noexcept {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(inst, &count, nullptr);
    if (count == 0) {
        LOG_FATAL_CAT("RTX", "No Vulkan GPUs found");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(inst, &count, devices.data());

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    QueueFamilyIndices best;
    int bestScore = -1;

    for (VkPhysicalDevice pd : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        if (props.apiVersion < VK_API_VERSION_1_3) continue;

        QueueFamilyIndices indices = findQueueFamilies(pd, surf);
        if (!indices.complete()) continue;

        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());

        bool hasAll = std::all_of(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end(),
            [&exts](const char* need) {
                return std::any_of(exts.begin(), exts.end(),
                    [need](const auto& e) { return strcmp(e.extensionName, need) == 0; });
            });

        if (!hasAll) continue;

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (strstr(props.deviceName, "RTX") || strstr(props.deviceName, "GeForce")) score += 300000;

        if (score > bestScore) {
            bestScore = score;
            selected = pd;
            best = indices;
        }
    }

    if (selected == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "No RTX-capable GPU found");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(selected, &props);
    LOG_INFO_CAT("RTX", "Selected GPU: {}", props.deviceName);

    // Set in context before sealing
    g_ctx().setPhysicalDevice(selected);

    std::set<uint32_t> uniqueQ = {best.graphics.value(), best.present.value()};
    if (best.transfer.has_value()) uniqueQ.insert(best.transfer.value());

    float prio = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qInfos;
    for (uint32_t fam : uniqueQ) {
        qInfos.push_back({
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = fam,
            .queueCount       = 1,
            .pQueuePriorities = &prio
        });
    }

    // Enable required features — acceleration structure + descriptor buffer
    VkPhysicalDeviceVulkan12Features vk12{};
    vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12.bufferDeviceAddress = VK_TRUE;
    vk12.descriptorIndexing = VK_TRUE;
    vk12.runtimeDescriptorArray = VK_TRUE;  // required for materials[] etc.

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel{};
    accel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accel.accelerationStructure = VK_TRUE;
    accel.pNext = &vk12;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipe{};
    rtPipe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipe.rayTracingPipeline = VK_TRUE;
    rtPipe.pNext = &accel;

    // Enable descriptor buffer feature explicitly
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descBufFeatures{};
    descBufFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descBufFeatures.descriptorBuffer = VK_TRUE;
    descBufFeatures.descriptorBufferCaptureReplay = VK_FALSE;
    descBufFeatures.descriptorBufferImageLayoutIgnored = VK_FALSE;
    descBufFeatures.pNext = &rtPipe;

    VkDeviceCreateInfo devInfo{};
    devInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.pNext                   = &descBufFeatures;
    devInfo.queueCreateInfoCount    = static_cast<uint32_t>(qInfos.size());
    devInfo.pQueueCreateInfos       = qInfos.data();
    devInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtensions.size());
    devInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

    VkDevice dev = VK_NULL_HANDLE;
    VkResult res = vkCreateDevice(selected, &devInfo, nullptr, &dev);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkCreateDevice failed: {}", string_VkResult(res));
        return VK_NULL_HANDLE;
    }

    VkQueue graphicsQ, presentQ, computeQ, transferQ;
    vkGetDeviceQueue(dev, best.graphics.value(), 0, &graphicsQ);
    vkGetDeviceQueue(dev, best.present.value(), 0, &presentQ);
    computeQ = graphicsQ;  // compute on graphics for simplicity
    transferQ = best.transfer.has_value() ? [&](){ VkQueue q; vkGetDeviceQueue(dev, best.transfer.value(), 0, &q); return q; }() : graphicsQ;

    // Seal the empire — unbreakable lockdown
    stone_seal_device_resources(inst, dev, selected, surf, VK_NULL_HANDLE);  // swapchain sealed later
    stone_seal_queues(graphicsQ, presentQ, computeQ, transferQ);
    stone_seal_families(best.graphics.value(), best.present.value(),
                        best.transfer.value_or(best.graphics.value()), best.graphics.value());

    g_ctx().graphicsQueue = graphicsQ;
    g_ctx().presentQueue = presentQ;
    g_ctx().computeQueue = computeQ;
    g_ctx().transferQueue = transferQ;

    g_ctx().graphicsFamily = best.graphics.value();
    g_ctx().presentFamily  = best.present.value();
    g_ctx().transferFamily = best.transfer.value_or(best.graphics.value());
    g_ctx().computeFamily  = best.graphics.value();

    LOG_SUCCESS_CAT("RTX", "Logical device created — full RTX + descriptor buffer + acceleration structure enabled");

    return dev;
}

// =============================================================================
// Context init — minimal
// =============================================================================
void Context::init() noexcept {
    valid = true;
    ready.store(true, std::memory_order_release);
    LOG_INFO_CAT("RTX", "Context initialized");
}

// =============================================================================
// Acceleration structure descriptor write helper
// =============================================================================
void writeAccelerationStructureDescriptor(
    VkDescriptorSet set,
    uint32_t binding,
    uint32_t arrayElement,
    VkAccelerationStructureKHR accel) noexcept {
    VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
    asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures = &accel;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = &asWrite;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = arrayElement;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    vkUpdateDescriptorSets(StoneKey::stone_device(), 1, &write, 0, nullptr);
}

} // namespace RTX