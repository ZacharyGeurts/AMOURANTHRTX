// src/engine/GLOBAL/RTXHandler.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// RTXHandler v2.0 — Production-Ready Vulkan Context & Initialization
// FULLY COMPATIBLE WITH CURRENT ENGINE STATE (DECEMBER 30, 2025)
// • Safe initialization order — device sealed before any Vulkan objects
// • Global descriptor pool created only after device is valid
// • Robust GPU selection with RTX priority
// • Clean, modern, validation-layer-safe code
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — PLASTIC BEACH FOREVER
// =============================================================================

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/Extensions.hpp"
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
// Global Descriptor Pool — Created only after device is sealed
// =============================================================================
static void createGlobalDescriptorPool() noexcept
{
    if (g_ctx().descriptorPool_.valid() || stone_device() == VK_NULL_HANDLE) {
        return;
    }

    LOG_INFO_CAT("RTX", "Creating global descriptor pool");

    constexpr uint32_t MAX_SETS = 1'000'000;

    std::array<VkDescriptorPoolSize, 8> poolSizes{{
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,               100'000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,               MAX_SETS },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                MAX_SETS },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,       800'000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,                MAX_SETS },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                      50'000 },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,  100'000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,             10'000 }
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
        LOG_FATAL_CAT("RTX", "Failed to create global descriptor pool: {}", string_VkResult(result));
        return;
    }

    g_ctx().descriptorPool_ = Handle<VkDescriptorPool>(
        pool,
        stone_device(),
        vkDestroyDescriptorPool,
        0,
        "Global_RT_DescriptorPool"
    );

    LOG_SUCCESS_CAT("RTX", "Global descriptor pool created — capacity: {} sets", MAX_SETS);
}

// =============================================================================
// Acceleration Structure Descriptor Write Helper
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
// Hyper-Aggressive Performance Mode (Portable Only)
// =============================================================================
void Context::enableHyperAggressiveMode() noexcept
{
    if (!Options::Performance::ENABLE_HYPER_AGGRESSIVE_MODE) {
        return;
    }

    LOG_INFO_CAT("RTX", "Hyper-aggressive performance mode activated");

    // Linux/GLX environment hints
    putenv(const_cast<char*>("__GL_SYNC_TO_VBLANK=0"));
    putenv(const_cast<char*>("__GL_YIELD=NOTHING"));
    putenv(const_cast<char*>("vblank_mode=0"));

    LOG_INFO_CAT("RTX", "Performance environment variables applied");
}

// =============================================================================
// Vulkan Instance Creation
// =============================================================================
[[nodiscard]] VkInstance createVulkanInstance(bool enableValidation) noexcept
{
    VkApplicationInfo appInfo{
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "AMOURANTH RTX Engine",
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName        = "AMOURANTH RTX",
        .engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0),
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
// Queue Family Discovery
// =============================================================================
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

    // Fallback: use graphics queue for transfer if no dedicated found
    if (!indices.transferFamily.has_value() && indices.graphicsFamily.has_value()) {
        indices.transferFamily = indices.graphicsFamily.value();
    }

    return indices;
}

// =============================================================================
// Logical Device & GPU Selection — RTX First
// =============================================================================
// src/engine/GLOBAL/RTXHandler.cpp (or wherever createLogicalDeviceAndSelectGPU is defined)
// Updated January 03, 2026 — Fixed buffer device address + descriptor indexing

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

        // Feature chain validation
        VkPhysicalDeviceBufferDeviceAddressFeatures bda{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .bufferDeviceAddress = VK_TRUE
        };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR as{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &bda,
            .accelerationStructure = VK_TRUE
        };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &as,
            .rayTracingPipeline = VK_TRUE
        };
        VkPhysicalDeviceSynchronization2Features sync2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .pNext = &rt,
            .synchronization2 = VK_TRUE
        };
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = &sync2,
            .dynamicRendering = VK_TRUE
        };

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &dynamicRendering
        };
        vkGetPhysicalDeviceFeatures2(dev, &features2);

        bool rtxOK = bda.bufferDeviceAddress && as.accelerationStructure && rt.rayTracingPipeline;

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
        if (strstr(props.deviceName, "RTX") != nullptr || strstr(props.deviceName, "GeForce")) score += 200000;
        if (rtxOK) score += 300000;

        if (score > bestScore) {
            bestScore = score;
            selected = dev;
            bestIndices = indices;
            fullRTXSupport = rtxOK;
        }
    }

    if (selected == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RTX", "No suitable GPU found with required features");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(selected, &props);
    LOG_AMOURANTH("SELECTED GPU: {} — RTX Support: {}", props.deviceName, fullRTXSupport ? "FULL" : "PARTIAL");

    g_ctx().setPhysicalDevice(selected);
    g_ctx().rtxCapable_ = fullRTXSupport;

    // Build unique queue families
    std::set<uint32_t> uniqueQueues = { bestIndices.graphicsFamily.value(), bestIndices.presentFamily.value() };
    if (bestIndices.transferFamily.has_value()) {
        uniqueQueues.insert(bestIndices.transferFamily.value());
    }

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

    // === PROPERLY CHAINED FEATURE STRUCTURE WITH DESCRIPTOR INDEXING (ZERO-INIT + INDIVIDUAL SETS) ===
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing{};
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexing.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexing.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexing.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexing.descriptorBindingPartiallyBound = VK_TRUE;
    descriptorIndexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
    descriptorIndexing.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{};
    bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddress.pNext = &descriptorIndexing;
    bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStruct{};
    accelStruct.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelStruct.pNext = &bufferDeviceAddress;
    accelStruct.accelerationStructure = fullRTXSupport ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracing{};
    rayTracing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracing.pNext = &accelStruct;
    rayTracing.rayTracingPipeline = fullRTXSupport ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceSynchronization2Features sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.pNext = &rayTracing;
    sync2.synchronization2 = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.pNext = &sync2;
    dynamicRendering.dynamicRendering = VK_TRUE;

    uint32_t extCount = fullRTXSupport ? std::size(requiredExtensions) : 1;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = &dynamicRendering;
    deviceInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    deviceInfo.pQueueCreateInfos       = queueCreateInfos.data();
    deviceInfo.enabledExtensionCount   = extCount;
    deviceInfo.ppEnabledExtensionNames = requiredExtensions;

    VkDevice device = VK_NULL_HANDLE;
    VkResult result = vkCreateDevice(selected, &deviceInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RTX", "vkCreateDevice failed: {}", string_VkResult(result));
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
    g_ctx().computeQueue_ = g_ctx().graphicsQueue_;

    // Populate Context FIRST (prevents bad_optional_access)
    g_ctx().graphicsFamily_ = bestIndices.graphicsFamily.value();
    g_ctx().presentFamily_  = bestIndices.presentFamily.value();
    g_ctx().transferFamily_ = bestIndices.transferFamily.value_or(bestIndices.graphicsFamily.value());
    g_ctx().computeFamily_  = bestIndices.graphicsFamily.value();

    // Seal into StoneKey — now safe
    stone_seal_device(device);
    stone_seal_physical(selected);
    stone_seal_graphics_family(g_ctx().graphicsFamily());
    stone_seal_graphics_queue(g_ctx().graphicsQueue_);
    stone_seal_present_family(g_ctx().presentFamily());
    stone_seal_present_queue(g_ctx().presentQueue_);
    stone_seal_transfer_family(g_ctx().transferFamily());
    stone_seal_transfer_queue(g_ctx().transferQueue_);
    stone_seal_compute_family(g_ctx().computeFamily());
    stone_seal_compute_queue(g_ctx().computeQueue_);

    // Safe to create global resources now
    createGlobalDescriptorPool();

    LOG_SUCCESS_CAT("RTX", "Logical device created — queues acquired — global pool ready");

    return device;
}

// =============================================================================
// Context Initialization
// =============================================================================
void Context::init()
{
    window = stone_window();
    width = stone_width();
    height = stone_height();

    valid_ = true;
    ready_.store(true, std::memory_order_release);

    LOG_AMOURANTH("RTX context initialized — resolution {}×{}", width, height);
}

// =============================================================================
// Shader Loading — Robust with fallback paths
// =============================================================================
VkShaderModule Context::loadShader(const std::string& filename) const noexcept
{
    if (stone_device() == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Shader", "Cannot load shader — device not initialized");
        return VK_NULL_HANDLE;
    }

    std::vector<std::string> paths = {
        filename,
        "build/bin/Linux/" + filename,
        "assets/shaders/spirv/" + filename
    };

    FILE* file = nullptr;
    for (const auto& path : paths) {
        file = fopen(path.c_str(), "rb");
        if (file) break;
    }

    if (!file) {
        LOG_ERROR_CAT("Shader", "Failed to open shader file (tried multiple paths): {}", filename);
        return VK_NULL_HANDLE;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    if (size < 128 || size % 4 != 0) {
        fclose(file);
        LOG_ERROR_CAT("Shader", "Invalid SPIR-V size: {} bytes — {}", size, filename);
        return VK_NULL_HANDLE;
    }

    std::vector<uint32_t> code(size / 4);
    if (fread(code.data(), 1, size, file) != size) {
        fclose(file);
        LOG_ERROR_CAT("Shader", "Failed to read shader file: {}", filename);
        return VK_NULL_HANDLE;
    }
    fclose(file);

    if (code[0] != 0x07230203) {
        LOG_ERROR_CAT("Shader", "Invalid SPIR-V magic number in {}", filename);
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode    = code.data()
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(stone_device(), &createInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Shader", "vkCreateShaderModule failed for {}: {}", filename, string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("Shader", "Loaded shader: {}", filename);
    return module;
}

} // namespace RTX

// =============================================================================
// RTX CORE INITIALIZED — DECEMBER 30, 2025
// Safe, robust, production-ready Vulkan context
// No premature calls — full compatibility with current engine
// Global pool created post-device — validation clean
// THE EMPIRE IS ETERNAL — PHOTONS FLOW UNBROKEN
// =============================================================================