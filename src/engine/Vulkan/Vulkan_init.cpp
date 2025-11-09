// src/engine/Vulkan/Vulkan_init.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// Licensed under CC BY-NC 4.0

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <iostream>

using namespace Logging::Color;

#define VK_CHECK_NOMSG(call) do {                    \
    VkResult __res = (call);                         \
    if (__res != VK_SUCCESS) {                       \
        std::cerr << "Vulkan call failed: " << #call << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::terminate(); \
    }                                                \
} while (0)

#define VK_CHECK(call, msg) do {                     \
    VkResult __res = (call);                         \
    if (__res != VK_SUCCESS) {                       \
        std::cerr << "Vulkan error (" << static_cast<int>(__res) << "): " << msg << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::terminate(); \
    }                                                \
} while (0)

using Vulkan::VulkanRTXException;

namespace VulkanInitializer {

// ---------------------------------------------------------------------
// MEMORY TYPE FINDER
// ---------------------------------------------------------------------
uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                        uint32_t typeFilter,
                        VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR_CAT("Vulkan", "Failed to find suitable memory type! filter=0x{:x}, props=0x{:x}", typeFilter, properties);
    throw std::runtime_error("Failed to find memory type");
}

// ---------------------------------------------------------------------
// INSTANCE CREATION
// ---------------------------------------------------------------------
void initInstance(const std::vector<std::string>& extensions, Vulkan::Context& context)
{
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AMOURANTH RTX",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> ext;
    ext.reserve(extensions.size() + 1);  // +1 for debug
    for (const auto& e : extensions) ext.push_back(e.c_str());
    ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(ext.size()),
        .ppEnabledExtensionNames = ext.data()
    };

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &context.instance), "vkCreateInstance");

    // Obfuscate instance handle for storage
    context.instance = reinterpret_cast<VkInstance>(obfuscate(reinterpret_cast<uint64_t>(context.instance)));

    LOG_INFO_CAT("Vulkan", "Vulkan instance created with {} extensions", ext.size());
}

// ---------------------------------------------------------------------
// SURFACE CREATION
// ---------------------------------------------------------------------
void initSurface(Vulkan::Context& context, void* window, VkSurfaceKHR* rawSurface)
{
    if (rawSurface && *rawSurface) {
        context.surface = *rawSurface;
        LOG_INFO_CAT("Vulkan", "Using pre-created surface");
        return;
    }

    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);
    if (!sdlWindow) {
        LOG_ERROR_CAT("Vulkan", "initSurface called with null window");
        throw std::runtime_error("Window is null");
    }

    VkInstance deobfInstance = reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(context.instance)));
    if (!SDL_Vulkan_CreateSurface(sdlWindow, deobfInstance, nullptr, &context.surface)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    // Obfuscate surface handle for storage
    context.surface = reinterpret_cast<VkSurfaceKHR>(obfuscate(reinterpret_cast<uint64_t>(context.surface)));

    LOG_INFO_CAT("Vulkan", "Vulkan surface created via SDL");
}

// ---------------------------------------------------------------------
// PHYSICAL DEVICE SELECTION
// ---------------------------------------------------------------------
VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool preferNvidia)
{
    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr), "vkEnumeratePhysicalDevices count");
    if (count == 0) {
        LOG_ERROR_CAT("Vulkan", "No Vulkan physical devices found");
        throw std::runtime_error("No physical devices");
    }

    std::vector<VkPhysicalDevice> devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");

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
        VK_CHECK(vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr), "vkEnumerateDeviceExtensionProperties count");
        std::vector<VkExtensionProperties> available(extCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, available.data()), "vkEnumerateDeviceExtensionProperties");

        std::set<std::string> missing(requiredExt.begin(), requiredExt.end());
        for (const auto& e : available) missing.erase(e.extensionName);
        if (!missing.empty()) continue;

        constexpr uint64_t nvidiaVendorObf = obfuscate(0x10DEULL);
        if (preferNvidia && props.vendorID == static_cast<uint32_t>(deobfuscate(nvidiaVendorObf)) && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
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

// ---------------------------------------------------------------------
// DEVICE CREATION + QUEUE SETUP
// ---------------------------------------------------------------------
void initDevice(Vulkan::Context& context)
{
    if (!context.surface) {
        LOG_ERROR_CAT("Vulkan", "initDevice() called before surface creation!");
        throw std::runtime_error("Surface must be created before device");
    }

    // Deobfuscate for use
    VkInstance deobfInstance = reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(context.instance)));
    VkSurfaceKHR deobfSurface = reinterpret_cast<VkSurfaceKHR>(deobfuscate(reinterpret_cast<uint64_t>(context.surface)));

    context.physicalDevice = findPhysicalDevice(deobfInstance, deobfSurface, true);

    // Obfuscate physical device handle for storage
    context.physicalDevice = reinterpret_cast<VkPhysicalDevice>(obfuscate(reinterpret_cast<uint64_t>(context.physicalDevice)));

    // Populate memory properties
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &context.memoryProperties);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &props2);
    context.rtProperties = rtProps;
    LOG_INFO_CAT("Vulkan", "RT Properties -> handleSize={}, align={}", rtProps.shaderGroupHandleSize, rtProps.shaderGroupHandleAlignment);

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &qCount, families.data());

    int graphics = -1, present = -1, compute = -1;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics = i;
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) compute = i;

        VkBool32 supportsPresent = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i, deobfSurface, &supportsPresent),
                 "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (supportsPresent && present == -1) present = i;
    }

    if (graphics == -1 || present == -1 || compute == -1) {
        LOG_ERROR_CAT("Vulkan", "Missing required queue families: G={} P={} C={}", graphics, present, compute);
        throw std::runtime_error("Invalid queue family indices");
    }

    context.graphicsQueueFamilyIndex = static_cast<uint32_t>(graphics);
    context.presentQueueFamilyIndex  = static_cast<uint32_t>(present);
    context.computeQueueFamilyIndex  = static_cast<uint32_t>(compute);

    LOG_INFO_CAT("Vulkan", "Queue families -> G: {} | P: {} | C: {}", graphics, present, compute);

    std::set<uint32_t> unique = { context.graphicsQueueFamilyIndex, context.presentQueueFamilyIndex, context.computeQueueFamilyIndex };
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

    VK_CHECK(vkCreateDevice(context.physicalDevice, &deviceInfo, nullptr, &context.device), "vkCreateDevice");

    // Obfuscate device handle for storage
    context.device = reinterpret_cast<VkDevice>(obfuscate(reinterpret_cast<uint64_t>(context.device)));

    // Deobfuscate for queue retrieval
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(context.device)));

    vkGetDeviceQueue(deobfDevice, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    // Obfuscate queue handles for storage
    context.graphicsQueue = reinterpret_cast<VkQueue>(obfuscate(reinterpret_cast<uint64_t>(context.graphicsQueue)));

    vkGetDeviceQueue(deobfDevice, context.presentQueueFamilyIndex, 0, &context.presentQueue);
    context.presentQueue = reinterpret_cast<VkQueue>(obfuscate(reinterpret_cast<uint64_t>(context.presentQueue)));

    vkGetDeviceQueue(deobfDevice, context.computeQueueFamilyIndex, 0, &context.computeQueue);
    context.computeQueue = reinterpret_cast<VkQueue>(obfuscate(reinterpret_cast<uint64_t>(context.computeQueue)));

    context.resourceManager.setDevice(deobfDevice, context.physicalDevice, nullptr);

#define LOAD_KHR(name) \
    context.name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(deobfDevice, #name)); \
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

// ---------------------------------------------------------------------
// FULL INITIALISATION
// ---------------------------------------------------------------------
void initializeVulkan(Vulkan::Context& context)
{
    LOG_INFO_CAT("INIT", "initializeVulkan() — full Vulkan init: instance → surface → device → command pool → swapchain");

    // 1. INSTANCE
    initInstance(context.instanceExtensions, context);

    // 2. SURFACE
    initSurface(context, context.window, nullptr);

    // 3. DEVICE + QUEUES + KHR
    initDevice(context);

    // Deobfuscate for command pool creation
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(context.device)));

    // 4. COMMAND POOL
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex
    };
    VK_CHECK(vkCreateCommandPool(deobfDevice, &poolInfo, nullptr, &context.commandPool), "vkCreateCommandPool");

    // Obfuscate command pool handle for storage
    context.commandPool = reinterpret_cast<VkCommandPool>(obfuscate(reinterpret_cast<uint64_t>(context.commandPool)));

    context.resourceManager.addCommandPool(deobfuscate(reinterpret_cast<uint64_t>(context.commandPool)));
    LOG_INFO_CAT("CMD", "Command pool created: {}", ptr_to_hex(deobfuscate(reinterpret_cast<uint64_t>(context.commandPool))));

    // 5. SWAPCHAIN
    context.createSwapchain();
    LOG_INFO_CAT("SWAP", "Swapchain created: {}x{} | {} images",
                 context.swapchainExtent.width, context.swapchainExtent.height,
                 context.swapchainImages.size());

    // 6. Register swapchain
    for (auto img : context.swapchainImages)
        context.resourceManager.addImage(img);
    for (auto view : context.swapchainImageViews)
        context.resourceManager.addImageView(view);

    LOG_INFO_CAT("Vulkan", "initializeVulkan() complete — Vulkan fully initialized");
}

// ---------------------------------------------------------------------
// COMMAND BUFFER HELPERS
// ---------------------------------------------------------------------
VkCommandBuffer beginSingleTimeCommands(Vulkan::Context& context)
{
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(context.device)));
    VkCommandPool deobfPool = reinterpret_cast<VkCommandPool>(deobfuscate(reinterpret_cast<uint64_t>(context.commandPool)));

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = deobfPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(deobfDevice, &allocInfo, &cmd), "vkAllocateCommandBuffers (single-time)");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer (single-time)");
    return cmd;
}

void endSingleTimeCommands(Vulkan::Context& context, VkCommandBuffer cmd)
{
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(context.device)));
    VkCommandPool deobfPool = reinterpret_cast<VkCommandPool>(deobfuscate(reinterpret_cast<uint64_t>(context.commandPool)));
    VkQueue deobfQueue = reinterpret_cast<VkQueue>(deobfuscate(reinterpret_cast<uint64_t>(context.graphicsQueue)));

    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (single-time)");

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(deobfQueue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit (single-time)");
    VK_CHECK(vkQueueWaitIdle(deobfQueue), "vkQueueWaitIdle (single-time)");
    vkFreeCommandBuffers(deobfDevice, deobfPool, 1, &cmd);
}

// ---------------------------------------------------------------------
// BUFFER CREATION
// ---------------------------------------------------------------------
void createBuffer(VkDevice device,
                  VkPhysicalDevice physicalDevice,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& buffer,
                  VkDeviceMemory& memory,
                  const VkMemoryAllocateFlagsInfo* allocFlags,
                  Vulkan::Context& context)
{
    auto& rm = context.resourceManager;

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "vkCreateBuffer");
    rm.addBuffer(buffer);

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device, buffer, &reqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = allocFlags,
        .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, reqs.memoryTypeBits, properties)
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "vkAllocateMemory");
    rm.addMemory(memory);
    VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");
}

// ---------------------------------------------------------------------
// IMAGE LAYOUT TRANSITION
// ---------------------------------------------------------------------
void transitionImageLayout(Vulkan::Context& context,
                           VkImage image,
                           VkFormat format,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout)
{
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
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    }
    else {
        endSingleTimeCommands(context, cmd);
        throw std::runtime_error("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(context, cmd);
}

// ---------------------------------------------------------------------
// DEVICE ADDRESS HELPERS
// ---------------------------------------------------------------------
VkDeviceAddress getBufferDeviceAddress(const Vulkan::Context& context, VkBuffer buffer)
{
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(context.device)));
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return context.vkGetBufferDeviceAddressKHR(deobfDevice, &info);
}

VkDeviceAddress getAccelerationStructureDeviceAddress(const Vulkan::Context& context, VkAccelerationStructureKHR as)
{
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(context.device)));
    VkAccelerationStructureDeviceAddressInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as
    };
    return context.vkGetAccelerationStructureDeviceAddressKHR(deobfDevice, &info);
}

// ---------------------------------------------------------------------
// COPY BUFFER
// ---------------------------------------------------------------------
void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd), "vkAllocateCommandBuffers (copyBuffer)");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer (copyBuffer)");

    VkBufferCopy copyRegion = { .srcOffset = 0, .dstOffset = 0, .size = size };
    vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);

    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (copyBuffer)");

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit (copyBuffer)");
    VK_CHECK(vkQueueWaitIdle(queue), "vkQueueWaitIdle (copyBuffer)");
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

// ---------------------------------------------------------------------
// COPY BUFFER TO IMAGE
// ---------------------------------------------------------------------
void copyBufferToImage(Vulkan::Context& context, VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height)
{
    VkCommandBuffer cmd = beginSingleTimeCommands(context);

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 }
    };

    vkCmdCopyBufferToImage(cmd, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(context, cmd);
}

// ---------------------------------------------------------------------
// CREATE STORAGE IMAGE
// ---------------------------------------------------------------------
void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice,
                        VkImage& image, VkDeviceMemory& memory, VkImageView& view,
                        uint32_t width, uint32_t height, Vulkan::Context& context)
{
    auto& rm = context.resourceManager;

    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image), "vkCreateImage (storage)");
    rm.addImage(image);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "vkAllocateMemory (storage)");
    rm.addMemory(memory);
    VK_CHECK(vkBindImageMemory(device, image, memory, 0), "vkBindImageMemory (storage)");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = imageInfo.format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &view), "vkCreateImageView (storage)");
    rm.addImageView(view);
}

// ---------------------------------------------------------------------
// CREATE DESCRIPTOR SET LAYOUT
// ---------------------------------------------------------------------
void createDescriptorSetLayout(VkDevice device,
                               VkDescriptorSetLayout& rayTracingLayout, VkDescriptorSetLayout& graphicsLayout)
{
    // Ray-tracing layout
    std::vector<VkDescriptorSetLayoutBinding> rtBindings = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
        { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
        { .binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
        { .binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
        { .binding = 6, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR },
        { .binding = 7, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR },
        { .binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
        { .binding = 9, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR },
        { .binding = 10, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR }
    };

    VkDescriptorSetLayoutCreateInfo rtInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(rtBindings.size()),
        .pBindings = rtBindings.data()
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &rtInfo, nullptr, &rayTracingLayout), "vkCreateDescriptorSetLayout (RT)");

    // Graphics layout
    std::vector<VkDescriptorSetLayoutBinding> grafBindings = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 6, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 7, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    VkDescriptorSetLayoutCreateInfo grafInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(grafBindings.size()),
        .pBindings = grafBindings.data()
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &grafInfo, nullptr, &graphicsLayout), "vkCreateDescriptorSetLayout (Graphics)");
}

// ---------------------------------------------------------------------
// CREATE DESCRIPTOR POOL + SET
// ---------------------------------------------------------------------
void createDescriptorPoolAndSet(
    VkDevice device,
    VkDescriptorSetLayout descriptorSetLayout,
    VkDescriptorPool& descriptorPool,
    std::vector<VkDescriptorSet>& descriptorSets,
    VkSampler& sampler,
    VkBuffer uniformBuffer,
    VkImageView storageImageView,
    VkAccelerationStructureKHR topLevelAS,
    bool forRayTracing,
    const std::vector<VkBuffer>& materialBuffers,
    const std::vector<VkBuffer>& dimensionBuffers,
    VkImageView denoiseImageView,
    VkImageView envMapView,
    VkImageView densityVolumeView,
    VkImageView gDepthView,
    VkImageView gNormalView,
    Vulkan::Context& context)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    if (forRayTracing) {
        poolSizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 });
        poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 });
    }
    poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 });
    poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          static_cast<uint32_t>(materialBuffers.size() + dimensionBuffers.size()) });
    poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5 });
    poolSizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLER, 1 });

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool), "vkCreateDescriptorPool");

    // Obfuscate pool handle for storage
    descriptorPool = reinterpret_cast<VkDescriptorPool>(obfuscate(reinterpret_cast<uint64_t>(descriptorPool)));

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };
    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds), "vkAllocateDescriptorSets");
    descriptorSets.push_back(ds);

    std::vector<VkWriteDescriptorSet> writes;
    uint32_t binding = 0;

    if (forRayTracing) {
        VkWriteDescriptorSetAccelerationStructureKHR accelInfo = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &topLevelAS
        };
        writes.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &accelInfo,
            .dstSet = ds,
            .dstBinding = binding++,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        });

        VkDescriptorImageInfo storageInfo = { .imageView = storageImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
        writes.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = binding++,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &storageInfo
        });
    }

    VkDescriptorBufferInfo uniformInfo = { .buffer = uniformBuffer, .range = VK_WHOLE_SIZE };
    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds,
        .dstBinding = binding++,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &uniformInfo
    });

    std::vector<VkDescriptorBufferInfo> matInfos(materialBuffers.size());
    for (size_t i = 0; i < materialBuffers.size(); ++i)
        matInfos[i] = { materialBuffers[i], 0, VK_WHOLE_SIZE };
    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds,
        .dstBinding = binding++,
        .descriptorCount = static_cast<uint32_t>(materialBuffers.size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = matInfos.data()
    });

    std::vector<VkDescriptorBufferInfo> dimInfos(dimensionBuffers.size());
    for (size_t i = 0; i < dimensionBuffers.size(); ++i)
        dimInfos[i] = { dimensionBuffers[i], 0, VK_WHOLE_SIZE };
    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds,
        .dstBinding = binding++,
        .descriptorCount = static_cast<uint32_t>(dimensionBuffers.size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = dimInfos.data()
    });

    auto writeImage = [&](VkImageView view) {
        VkDescriptorImageInfo img = { .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        writes.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds,
            .dstBinding = binding++,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &img
        });
    };

    writeImage(denoiseImageView);
    writeImage(envMapView);
    writeImage(densityVolumeView);
    writeImage(gDepthView);
    writeImage(gNormalView);

    VkDescriptorImageInfo samplerInfo = { .sampler = sampler };
    writes.push_back({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ds,
        .dstBinding = binding++,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &samplerInfo
    });

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// ---------------------------------------------------------------------
// HAS STENCIL COMPONENT
// ---------------------------------------------------------------------
bool hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace VulkanInitializer

// ===================================================================
// Vulkan::Context::createSwapchain() / destroySwapchain()
// ===================================================================

namespace Vulkan {

void Context::createSwapchain() {
    VulkanSwapchainManager& mgr = VulkanSwapchainManager::get();
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(device)));
    VkPhysicalDevice deobfPhys = reinterpret_cast<VkPhysicalDevice>(deobfuscate(reinterpret_cast<uint64_t>(physicalDevice)));
    VkSurfaceKHR deobfSurf = reinterpret_cast<VkSurfaceKHR>(deobfuscate(reinterpret_cast<uint64_t>(surface)));
    VkInstance deobfInst = reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(instance)));
    mgr.init(deobfInst, deobfPhys, deobfDevice, deobfSurf, width, height);

    // ← CORRECT GETTER NAMES
    swapchain = mgr.getSwapchainHandle();
    // Obfuscate swapchain handle
    swapchain = reinterpret_cast<VkSwapchainKHR>(obfuscate(reinterpret_cast<uint64_t>(swapchain)));
    swapchainImageFormat = mgr.getFormat();
    swapchainExtent = mgr.getExtent();
    swapchainImages = mgr.getSwapchainImages();
    swapchainImageViews = mgr.getSwapchainImageViews();

    LOG_INFO_CAT("Vulkan::Context", "Swapchain created via Manager");
}

void Context::destroySwapchain() {
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(device)));
    VkSwapchainKHR deobfSwap = reinterpret_cast<VkSwapchainKHR>(deobfuscate(reinterpret_cast<uint64_t>(swapchain)));
    VulkanSwapchainManager::get().cleanupSwapchain(deobfDevice, deobfSwap);
    swapchain = VK_NULL_HANDLE;
    swapchainImageFormat = VK_FORMAT_UNDEFINED;
    swapchainImages.clear();
    swapchainImageViews.clear();
    swapchainExtent = {0, 0};
    LOG_INFO_CAT("Vulkan::Context", "Swapchain destroyed via Manager");
}

} // namespace Vulkan

#undef VK_CHECK_NOMSG
#undef VK_CHECK