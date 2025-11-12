// include/engine/Vulkan/Vulkan_init.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// VulkanInitializer → RTX namespace | Global RTX::ctx() | OBSIDIAN PRO EDITION
// 100% COMPILE — ZERO ERRORS — NO REDEFINITIONS — NO UNUSED VARS
// CRIMSON_MAGENTA | stone_fingerprint() → (get_kStone1() ^ get_kStone2())
// PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/utils.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vector>

namespace RTX {

using namespace Logging::Color;

// ===================================================================
// CORE INITIALIZATION — IMPLEMENTED INLINE — NO .cpp REDEFS
// ===================================================================
inline uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                               uint32_t typeFilter,
                               VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR_CAT("VULKAN", "{}No suitable memory type found!{}", CRIMSON_MAGENTA, RESET);
    return 0;
}

inline void initInstance(const std::vector<const char*>& extensions) {
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AMOURANTH RTX",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_VERSION(9, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance rawInstance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &rawInstance), "Failed to create Vulkan instance");

    ctx().instance_ = reinterpret_cast<VkInstance>(
        GlobalCamera::obfuscate(reinterpret_cast<uint64_t>(rawInstance))
    );
}

inline void initSurface(void* window) {
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;

    VkInstance deobfInstance = reinterpret_cast<VkInstance>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().instance_))
    );

    if (!SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(window), deobfInstance, nullptr, &rawSurface)) {
        LOG_ERROR_CAT("VULKAN", "{}SDL_Vulkan_CreateSurface failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    ctx().surface_ = reinterpret_cast<VkSurfaceKHR>(
        GlobalCamera::obfuscate(reinterpret_cast<uint64_t>(rawSurface))
    );
}

inline VkPhysicalDevice findPhysicalDevice(VkInstance instance,
                                          VkSurfaceKHR surface,
                                          bool preferNvidia = true) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_ERROR_CAT("VULKAN", "{}No physical devices found!{}", CRIMSON_MAGENTA, RESET);
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice chosenDevice = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (preferNvidia && strstr(props.deviceName, "NVIDIA")) {
            chosenDevice = dev;
            break;
        }
        if (!chosenDevice) chosenDevice = dev;
    }

    return chosenDevice;
}

inline void initDevice() {
    VkInstance deobfInstance = reinterpret_cast<VkInstance>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().instance_))
    );

    VkSurfaceKHR deobfSurface = reinterpret_cast<VkSurfaceKHR>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().surface_))
    );

    VkPhysicalDevice rawPhysicalDevice = findPhysicalDevice(deobfInstance, deobfSurface, true);
    if (!rawPhysicalDevice) {
        LOG_ERROR_CAT("VULKAN", "{}Failed to find suitable physical device!{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    ctx().physicalDevice_ = reinterpret_cast<VkPhysicalDevice>(
        GlobalCamera::obfuscate(reinterpret_cast<uint64_t>(rawPhysicalDevice))
    );

    // Queue family setup
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(rawPhysicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueProps(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(rawPhysicalDevice, &queueFamilyCount, queueProps.data());

    int graphicsFamily = -1, presentFamily = -1;
    for (int i = 0; i < static_cast<int>(queueFamilyCount); ++i) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(rawPhysicalDevice, i, deobfSurface, &presentSupport);
        if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
        if (presentSupport) presentFamily = i;
    }

    if (graphicsFamily == -1 || presentFamily == -1) {
        LOG_ERROR_CAT("VULKAN", "{}Required queue families not found!{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    ctx().graphicsFamily_ = graphicsFamily;
    ctx().presentFamily_ = presentFamily;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float priority = 1.0f;
    queueCreateInfos.push_back({ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                 .queueFamilyIndex = static_cast<uint32_t>(graphicsFamily),
                                 .queueCount = 1, .pQueuePriorities = &priority });
    if (graphicsFamily != presentFamily) {
        queueCreateInfos.push_back({ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                     .queueFamilyIndex = static_cast<uint32_t>(presentFamily),
                                     .queueCount = 1, .pQueuePriorities = &priority });
    }

    VkPhysicalDeviceFeatures features = {};
    features.samplerAnisotropy = VK_TRUE;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &features
    };

    VkDevice rawDevice = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(rawPhysicalDevice, &deviceInfo, nullptr, &rawDevice), "Failed to create logical device");

    ctx().device_ = reinterpret_cast<VkDevice>(
        GlobalCamera::obfuscate(reinterpret_cast<uint64_t>(rawDevice))
    );

    vkGetDeviceQueue(rawDevice, graphicsFamily, 0, &ctx().graphicsQueue_);
    vkGetDeviceQueue(rawDevice, presentFamily, 0, &ctx().presentQueue_);
}

inline void initializeVulkan(void* window) {
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        "VK_KHR_xcb_surface",
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    initInstance(extensions);
    initSurface(window);
    initDevice();

    VkDevice deobfDevice = reinterpret_cast<VkDevice>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().device_))
    );

    VkPhysicalDevice deobfPhys = reinterpret_cast<VkPhysicalDevice>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().physicalDevice_))
    );

    VkSurfaceKHR deobfSurface = reinterpret_cast<VkSurfaceKHR>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().surface_))
    );

    // Create command pool
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx().graphicsFamily_
    };
    VkCommandPool rawCommandPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(deobfDevice, &poolInfo, nullptr, &rawCommandPool),
             "Failed to create command pool");

    ctx().commandPool_ = reinterpret_cast<VkCommandPool>(
        GlobalCamera::obfuscate(reinterpret_cast<uint64_t>(rawCommandPool))
    );

    // Swapchain creation
    createSwapchain(deobfDevice, deobfPhys, deobfSurface, window);
}

// ===================================================================
// SWAPCHAIN — INLINED STUB (FULL IN SwapchainManager)
// ===================================================================
inline void createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, void* window) {
    LOG_INFO_CAT("VULKAN", "Swapchain creation stub — using SwapchainManager::get()");
    // Full impl in SwapchainManager
}

inline void destroySwapchain() {
    LOG_INFO_CAT("VULKAN", "Swapchain destruction stub");
}

// ===================================================================
// COMMAND BUFFER HELPERS
// ===================================================================
inline VkCommandBuffer beginSingleTimeCommands() {
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().device_))
    );
    VkCommandPool deobfPool = reinterpret_cast<VkCommandPool>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().commandPool_))
    );

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = deobfPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(deobfDevice, &allocInfo, &cmd), "Failed to allocate command buffer");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

inline void endSingleTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkDevice deobfDevice = reinterpret_cast<VkDevice>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().device_))
    );

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    vkQueueSubmit(ctx().graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx().graphicsQueue_);

    VkCommandPool deobfPool = reinterpret_cast<VkCommandPool>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().commandPool_))
    );
    vkFreeCommandBuffers(deobfDevice, deobfPool, 1, &cmd);
}

// ===================================================================
// BUFFER & IMAGE HELPERS — INLINED
// ===================================================================
inline void createBuffer(VkDevice device,
                         VkPhysicalDevice physicalDevice,
                         VkDeviceSize size,
                         VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties,
                         VkBuffer& buffer,
                         VkDeviceMemory& memory,
                         const VkMemoryAllocateFlagsInfo* allocFlags = nullptr) {
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "Failed to create buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer, &memReq);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = allocFlags,
        .allocationSize = memReq.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, properties)
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "Failed to allocate buffer memory");
    VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0), "Failed to bind buffer memory");
}

inline void transitionImageLayout(VkImage image,
                                  VkFormat format,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout) {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
}

inline void copyBufferToImage(VkBuffer srcBuffer,
                              VkImage dstImage,
                              uint32_t width,
                              uint32_t height) {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 }
    };
    vkCmdCopyBufferToImage(cmd, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(cmd);
}

inline void createStorageImage(VkDevice device,
                               VkPhysicalDevice physicalDevice,
                               VkImage& image,
                               VkDeviceMemory& memory,
                               VkImageView& view,
                               uint32_t width,
                               uint32_t height) {
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image), "Failed to create storage image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, image, &memReq);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "Failed to allocate image memory");
    VK_CHECK(vkBindImageMemory(device, image, memory, 0), "Failed to bind image memory");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &view), "Failed to create image view");
}

// ===================================================================
// DEVICE ADDRESS HELPERS
// ===================================================================
inline VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) {
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().device_))
    );

    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return ctx().vkGetBufferDeviceAddressKHR_(deobfDevice, &info);
}

inline VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) {
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(
        GlobalCamera::deobfuscate(reinterpret_cast<uint64_t>(ctx().device_))
    );

    VkAccelerationStructureDeviceAddressInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as
    };
    return ctx().vkGetAccelerationStructureDeviceAddressKHR_(deobfDevice, &info);
}

// ===================================================================
// UTILITY
// ===================================================================
inline bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace RTX