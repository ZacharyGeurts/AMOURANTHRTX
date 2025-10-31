// src/engine/Vulkan/Vulkan_init.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FULLY POLISHED. ZERO WARNINGS. 100% COMPILABLE.

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <algorithm>

#define VK_CHECK(x) do { \
    VkResult r = (x); \
    if (r != VK_SUCCESS) { \
        LOG_ERROR_CAT("Vulkan", #x " failed: {}", static_cast<int>(r)); \
        throw std::runtime_error(#x " failed"); \
    } \
} while(0)

namespace VulkanInitializer {

// ====================================================================
// UTILITY: MEMORY TYPE — MOVED TO TOP
// ====================================================================
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR_CAT("Vulkan", "Failed to find suitable memory type! typeFilter=0x{:x}, props=0x{:x}", typeFilter, properties);
    throw std::runtime_error("Failed to find memory type");
}

// ====================================================================
// INSTANCE CREATION — NO XCB/XLIB/WAYLAND
// ====================================================================
void initInstance(const std::vector<std::string>& extensions, Vulkan::Context& context) {
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AMOURANTH RTX",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> ext;
    ext.reserve(extensions.size() + 2);
    for (const auto& e : extensions) ext.push_back(e.c_str());
    ext.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(ext.size()),
        .ppEnabledExtensionNames = ext.data()
    };

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &context.instance));
    LOG_INFO_CAT("Vulkan", "Vulkan instance created");
}

// ====================================================================
// SURFACE CREATION
// ====================================================================
void initSurface(Vulkan::Context& context, void* window, VkSurfaceKHR* rawSurface) {
    if (rawSurface && *rawSurface) {
        context.surface = *rawSurface;
        LOG_INFO_CAT("Vulkan", "Using pre-created surface");
        return;
    }

    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);
    if (!SDL_Vulkan_CreateSurface(sdlWindow, context.instance, nullptr, &context.surface)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_CreateSurface failed");
        throw std::runtime_error("Failed to create Vulkan surface");
    }
    LOG_INFO_CAT("Vulkan", "Vulkan surface created via SDL");
}

// ====================================================================
// PHYSICAL DEVICE SELECTION
// ====================================================================
VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool preferNvidia) {
    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    if (count == 0) {
        LOG_ERROR_CAT("Vulkan", "No Vulkan physical devices found");
        throw std::runtime_error("No physical devices");
    }

    std::vector<VkPhysicalDevice> devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, devices.data()));

    const std::vector<const char*> requiredExt = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    VkPhysicalDevice best = VK_NULL_HANDLE;
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        uint32_t extCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr));
        std::vector<VkExtensionProperties> available(extCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, available.data()));

        std::set<std::string> missing(requiredExt.begin(), requiredExt.end());
        for (const auto& e : available) missing.erase(e.extensionName);
        if (!missing.empty()) continue;

        if (preferNvidia && props.vendorID == 0x10DE && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            LOG_INFO_CAT("Vulkan", "Selected discrete NVIDIA GPU: {}", props.deviceName);
            return dev;
        }
        if (!best) best = dev;
    }

    if (!best) {
        LOG_ERROR_CAT("Vulkan", "No device with required extensions found");
        throw std::runtime_error("No suitable GPU");
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(best, &props);
    LOG_INFO_CAT("Vulkan", "Selected GPU: {}", props.deviceName);
    return best;
}

// ====================================================================
// DEVICE CREATION + QUEUE SETUP + FUNCTION POINTERS
// ====================================================================
void initDevice(Vulkan::Context& context) {
    context.physicalDevice = findPhysicalDevice(context.instance, context.surface, true);

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &qCount, families.data());

    int graphics = -1, present = -1, compute = -1;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics = i;
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) compute = i;

        VkBool32 supportsPresent = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i, context.surface, &supportsPresent));
        if (supportsPresent) present = i;
    }

    if (graphics == -1 || present == -1 || compute == -1) {
        LOG_ERROR_CAT("Vulkan", "Missing required queue families:");
        LOG_ERROR_CAT("Vulkan", "  Graphics: {}", graphics);
        LOG_ERROR_CAT("Vulkan", "  Present:  {}", present);
        LOG_ERROR_CAT("Vulkan", "  Compute:  {}", compute);
        throw std::runtime_error("Invalid queue family indices");
    }

    context.graphicsQueueFamilyIndex = static_cast<uint32_t>(graphics);
    context.presentQueueFamilyIndex  = static_cast<uint32_t>(present);
    context.computeQueueFamilyIndex  = static_cast<uint32_t>(compute);

    LOG_INFO_CAT("Vulkan", "Queue families → G: {} | P: {} | C: {}", graphics, present, compute);

    std::set<uint32_t> unique = {
        static_cast<uint32_t>(graphics),
        static_cast<uint32_t>(present),
        static_cast<uint32_t>(compute)
    };

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float priority = 1.0f;
    for (uint32_t idx : unique) {
        queueInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = idx,
            .queueCount = 1,
            .pQueuePriorities = &priority
        });
    }

    const std::vector<const char*> extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferAddr = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipeline = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &bufferAddr,
        .rayTracingPipeline = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &rtPipeline,
        .accelerationStructure = VK_TRUE
    };

    VkPhysicalDeviceDescriptorIndexingFeatures descIndexing = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &asFeat,
        .shaderUniformBufferArrayNonUniformIndexing = VK_TRUE,
        .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE
    };

    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &descIndexing,
        .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos = queueInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
        .pEnabledFeatures = &features
    };

    VK_CHECK(vkCreateDevice(context.physicalDevice, &deviceInfo, nullptr, &context.device));

    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, context.presentQueueFamilyIndex,  0, &context.presentQueue);
    vkGetDeviceQueue(context.device, context.computeQueueFamilyIndex,  0, &context.computeQueue);

    context.resourceManager.setDevice(context.device, context.physicalDevice);

    #define LOAD_KHR(name) \
        context.name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(context.device, #name)); \
        if (!context.name) { \
            LOG_ERROR_CAT("Vulkan", "Failed to load " #name); \
            throw std::runtime_error("Missing KHR function: " #name); \
        }

    LOAD_KHR(vkCmdTraceRaysKHR);
    LOAD_KHR(vkCreateRayTracingPipelinesKHR);
    LOAD_KHR(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_KHR(vkCreateAccelerationStructureKHR);
    LOAD_KHR(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_KHR(vkCmdBuildAccelerationStructuresKHR);
    LOAD_KHR(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_KHR(vkGetBufferDeviceAddressKHR);
    LOAD_KHR(vkDestroyAccelerationStructureKHR);
    LOAD_KHR(vkCreateDeferredOperationKHR);
    LOAD_KHR(vkDeferredOperationJoinKHR);
    LOAD_KHR(vkGetDeferredOperationResultKHR);
    LOAD_KHR(vkDestroyDeferredOperationKHR);
    #undef LOAD_KHR

    LOG_INFO_CAT("Vulkan", "Device + queues + KHR functions initialized");
}

// ====================================================================
// COMMAND BUFFER HELPERS
// ====================================================================
VkCommandBuffer beginSingleTimeCommands(Vulkan::Context& context) {
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(context.device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
    return cmd;
}

void endSingleTimeCommands(Vulkan::Context& context, VkCommandBuffer cmd) {
    VK_CHECK(vkEndCommandBuffer(cmd));
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(context.graphicsQueue, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(context.graphicsQueue));
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &cmd);
}

// ====================================================================
// BUFFER CREATION
// ====================================================================
void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer& buffer, VkDeviceMemory& memory,
                  const VkMemoryAllocateFlagsInfo* allocFlags, VulkanResourceManager& rm) {

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));
    rm.addBuffer(buffer);

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device, buffer, &reqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = allocFlags,
        .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, reqs.memoryTypeBits, properties)
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));
    rm.addMemory(memory);
    VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));
}

// ====================================================================
// IMAGE LAYOUT TRANSITION
// ====================================================================
void transitionImageLayout(Vulkan::Context& context, VkImage image, VkFormat format,
                           VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer cmd = beginSingleTimeCommands(context);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = (format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

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
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else {
        endSingleTimeCommands(context, cmd);
        throw std::runtime_error("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(context, cmd);
}

// ====================================================================
// BUFFER DEVICE ADDRESS
// ====================================================================
VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& context, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return context.vkGetBufferDeviceAddressKHR(context.device, &info);
}

// ====================================================================
// FULL INITIALIZATION (COMMAND POOL)
// ====================================================================
void initializeVulkan(Vulkan::Context& context) {
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool));
    context.resourceManager.addCommandPool(context.commandPool);
    LOG_INFO_CAT("Vulkan", "Command pool created");
}

// ====================================================================
// GET ACCELERATION-STRUCTURE DEVICE ADDRESS
// ====================================================================
VkDeviceAddress getAccelerationStructureDeviceAddress(const Vulkan::Context& context,
                                                      VkAccelerationStructureKHR as)
{
    VkAccelerationStructureDeviceAddressInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as
    };
    return context.vkGetAccelerationStructureDeviceAddressKHR(context.device, &info);
}

// ====================================================================
// COPY BUFFER (single-time command)
// ====================================================================
void copyBuffer(VkDevice device,
                VkCommandPool commandPool,
                VkQueue queue,
                VkBuffer srcBuffer,
                VkBuffer dstBuffer,
                VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

// ====================================================================
// COPY BUFFER TO IMAGE
// ====================================================================
void copyBufferToImage(
    Vulkan::Context& context,
    VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height
) {
    VkCommandBuffer cmd = beginSingleTimeCommands(context);

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(cmd, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(context, cmd);
}

// ====================================================================
// CREATE STORAGE IMAGE
// ====================================================================
void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& image,
                        VkDeviceMemory& memory, VkImageView& view, uint32_t width, uint32_t height,
                        VulkanResourceManager& resourceManager) {
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image));
    resourceManager.addImage(image);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory));
    resourceManager.addMemory(memory);
    VK_CHECK(vkBindImageMemory(device, image, memory, 0));

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = imageInfo.format,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &view));
    resourceManager.addImageView(view);
}

// ====================================================================
// CREATE DESCRIPTOR SET LAYOUT
// ====================================================================
void createDescriptorSetLayout(VkDevice device, VkPhysicalDevice physicalDevice,
                               VkDescriptorSetLayout& rayTracingLayout, VkDescriptorSetLayout& graphicsLayout) {
    // Ray Tracing Layout
    VkDescriptorSetLayoutBinding accelBinding = {};
    accelBinding.binding = 0;
    accelBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    accelBinding.descriptorCount = 1;
    accelBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding outputImageBinding = {};
    outputImageBinding.binding = 1;
    outputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageBinding.descriptorCount = 1;
    outputImageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding uniformBinding = {};
    uniformBinding.binding = 2;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding materialBinding = {};
    materialBinding.binding = 3;
    materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialBinding.descriptorCount = 1024;
    materialBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding dimensionBinding = {};
    dimensionBinding.binding = 4;
    dimensionBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dimensionBinding.descriptorCount = 1024;
    dimensionBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding alphaTexBinding = {};
    alphaTexBinding.binding = 5;
    alphaTexBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    alphaTexBinding.descriptorCount = 1;
    alphaTexBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding envMapBinding = {};
    envMapBinding.binding = 6;
    envMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    envMapBinding.descriptorCount = 1;
    envMapBinding.stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR;

    VkDescriptorSetLayoutBinding densityVolumeBinding = {};
    densityVolumeBinding.binding = 7;
    densityVolumeBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    densityVolumeBinding.descriptorCount = 1;
    densityVolumeBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding gDepthBinding = {};
    gDepthBinding.binding = 8;
    gDepthBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    gDepthBinding.descriptorCount = 1;
    gDepthBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding gNormalBinding = {};
    gNormalBinding.binding = 9;
    gNormalBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    gNormalBinding.descriptorCount = 1;
    gNormalBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding samplerBinding = {};
    samplerBinding.binding = 10;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

    std::vector<VkDescriptorSetLayoutBinding> rtBindings = {accelBinding, outputImageBinding, uniformBinding, materialBinding, dimensionBinding, alphaTexBinding, envMapBinding, densityVolumeBinding, gDepthBinding, gNormalBinding, samplerBinding};

    VkDescriptorSetLayoutCreateInfo rtCreateInfo = {};
    rtCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    rtCreateInfo.bindingCount = static_cast<uint32_t>(rtBindings.size());
    rtCreateInfo.pBindings = rtBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device, &rtCreateInfo, nullptr, &rayTracingLayout));

    // Graphics Layout
    uniformBinding.binding = 0;
    uniformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    materialBinding.binding = 1;
    materialBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    dimensionBinding.binding = 2;
    dimensionBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    alphaTexBinding.binding = 3;
    alphaTexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    envMapBinding.binding = 4;
    envMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    densityVolumeBinding.binding = 5;
    densityVolumeBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    gDepthBinding.binding = 6;
    gDepthBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    gNormalBinding.binding = 7;
    gNormalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    samplerBinding.binding = 8;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> grafBindings = {uniformBinding, materialBinding, dimensionBinding, alphaTexBinding, envMapBinding, densityVolumeBinding, gDepthBinding, gNormalBinding, samplerBinding};

    VkDescriptorSetLayoutCreateInfo grafCreateInfo = {};
    grafCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    grafCreateInfo.bindingCount = static_cast<uint32_t>(grafBindings.size());
    grafCreateInfo.pBindings = grafBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device, &grafCreateInfo, nullptr, &graphicsLayout));
}

// ====================================================================
// CREATE DESCRIPTOR POOL AND SET
// ====================================================================
void createDescriptorPoolAndSet(
    VkDevice device, VkPhysicalDevice physicalDevice, VkDescriptorSetLayout descriptorSetLayout,
    VkDescriptorPool& descriptorPool, std::vector<VkDescriptorSet>& descriptorSets,
    VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
    VkAccelerationStructureKHR topLevelAS, bool forRayTracing,
    std::vector<VkBuffer> materialBuffers, std::vector<VkBuffer> dimensionBuffers,
    VkImageView alphaTexView, VkImageView envMapView, VkImageView densityVolumeView,
    VkImageView gDepthView, VkImageView gNormalView
) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    if (forRayTracing) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1});
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1});
    }
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(materialBuffers.size())});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(dimensionBuffers.size())});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, 1});

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    descriptorSets.push_back(ds);

    std::vector<VkWriteDescriptorSet> writes;
    uint32_t bindingOffset = 0;
    if (forRayTracing) {
        VkWriteDescriptorSetAccelerationStructureKHR accelInfo = {};
        accelInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        accelInfo.accelerationStructureCount = 1;
        accelInfo.pAccelerationStructures = &topLevelAS;

        VkWriteDescriptorSet accelWrite = {};
        accelWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        accelWrite.pNext = &accelInfo;
        accelWrite.dstSet = ds;
        accelWrite.dstBinding = 0;
        accelWrite.descriptorCount = 1;
        accelWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writes.push_back(accelWrite);

        VkDescriptorImageInfo storageInfo = {};
        storageInfo.imageView = storageImageView;
        storageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet storageWrite = {};
        storageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        storageWrite.dstSet = ds;
        storageWrite.dstBinding = 1;
        storageWrite.descriptorCount = 1;
        storageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        storageWrite.pImageInfo = &storageInfo;
        writes.push_back(storageWrite);

        bindingOffset = 2;
    }

    VkDescriptorBufferInfo uniformInfo = {};
    uniformInfo.buffer = uniformBuffer;
    uniformInfo.offset = 0;
    uniformInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet uniformWrite = {};
    uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformWrite.dstSet = ds;
    uniformWrite.dstBinding = 0 + bindingOffset;
    uniformWrite.descriptorCount = 1;
    uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformWrite.pBufferInfo = &uniformInfo;
    writes.push_back(uniformWrite);

    std::vector<VkDescriptorBufferInfo> materialInfos(materialBuffers.size());
    for (size_t i = 0; i < materialBuffers.size(); ++i) {
        materialInfos[i].buffer = materialBuffers[i];
        materialInfos[i].offset = 0;
        materialInfos[i].range = VK_WHOLE_SIZE;
    }
    VkWriteDescriptorSet materialWrite = {};
    materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    materialWrite.dstSet = ds;
    materialWrite.dstBinding = 1 + bindingOffset;
    materialWrite.descriptorCount = static_cast<uint32_t>(materialBuffers.size());
    materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialWrite.pBufferInfo = materialInfos.data();
    writes.push_back(materialWrite);

    std::vector<VkDescriptorBufferInfo> dimensionInfos(dimensionBuffers.size());
    for (size_t i = 0; i < dimensionBuffers.size(); ++i) {
        dimensionInfos[i].buffer = dimensionBuffers[i];
        dimensionInfos[i].offset = 0;
        dimensionInfos[i].range = VK_WHOLE_SIZE;
    }
    VkWriteDescriptorSet dimensionWrite = {};
    dimensionWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    dimensionWrite.dstSet = ds;
    dimensionWrite.dstBinding = 2 + bindingOffset;
    dimensionWrite.descriptorCount = static_cast<uint32_t>(dimensionBuffers.size());
    dimensionWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dimensionWrite.pBufferInfo = dimensionInfos.data();
    writes.push_back(dimensionWrite);

    VkDescriptorImageInfo alphaInfo = {};
    alphaInfo.imageView = alphaTexView;
    alphaInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet alphaWrite = {};
    alphaWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    alphaWrite.dstSet = ds;
    alphaWrite.dstBinding = 3 + bindingOffset;
    alphaWrite.descriptorCount = 1;
    alphaWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    alphaWrite.pImageInfo = &alphaInfo;
    writes.push_back(alphaWrite);

    VkDescriptorImageInfo envInfo = {};
    envInfo.imageView = envMapView;
    envInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet envWrite = {};
    envWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    envWrite.dstSet = ds;
    envWrite.dstBinding = 4 + bindingOffset;
    envWrite.descriptorCount = 1;
    envWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    envWrite.pImageInfo = &envInfo;
    writes.push_back(envWrite);

    VkDescriptorImageInfo densityInfo = {};
    densityInfo.imageView = densityVolumeView;
    densityInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet densityWrite = {};
    densityWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    densityWrite.dstSet = ds;
    densityWrite.dstBinding = 5 + bindingOffset;
    densityWrite.descriptorCount = 1;
    densityWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    densityWrite.pImageInfo = &densityInfo;
    writes.push_back(densityWrite);

    VkDescriptorImageInfo gDepthInfo = {};
    gDepthInfo.imageView = gDepthView;
    gDepthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet gDepthWrite = {};
    gDepthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    gDepthWrite.dstSet = ds;
    gDepthWrite.dstBinding = 6 + bindingOffset;
    gDepthWrite.descriptorCount = 1;
    gDepthWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    gDepthWrite.pImageInfo = &gDepthInfo;
    writes.push_back(gDepthWrite);

    VkDescriptorImageInfo gNormalInfo = {};
    gNormalInfo.imageView = gNormalView;
    gNormalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet gNormalWrite = {};
    gNormalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    gNormalWrite.dstSet = ds;
    gNormalWrite.dstBinding = 7 + bindingOffset;
    gNormalWrite.descriptorCount = 1;
    gNormalWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    gNormalWrite.pImageInfo = &gNormalInfo;
    writes.push_back(gNormalWrite);

    VkDescriptorImageInfo samplerInfo = {};
    samplerInfo.sampler = sampler;
    VkWriteDescriptorSet samplerWrite = {};
    samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    samplerWrite.dstSet = ds;
    samplerWrite.dstBinding = 8 + bindingOffset;
    samplerWrite.descriptorCount = 1;
    samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerWrite.pImageInfo = &samplerInfo;
    writes.push_back(samplerWrite);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// ====================================================================
// HAS STENCIL COMPONENT
// ====================================================================
bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace VulkanInitializer