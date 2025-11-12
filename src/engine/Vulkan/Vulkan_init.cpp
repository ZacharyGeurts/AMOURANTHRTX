// src/engine/Vulkan/Vulkan_init.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// Updated to global RTX::ctx() | SWAPCHAIN globals | No constexpr obfuscate
// =============================================================================

#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include <SDL3/SDL_vulkan.h>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <iostream>

using namespace Logging::Color;

namespace RTX {

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
void initInstance(const std::vector<const char*>& extensions)
{
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AMOURANTH RTX",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "AMOURANTH",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> ext = extensions;
    ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(ext.size()),
        .ppEnabledExtensionNames = ext.data()
    };

    VkInstance rawInstance;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &rawInstance), "vkCreateInstance");
    ctx().instance_ = reinterpret_cast<VkInstance>(obfuscate(reinterpret_cast<uint64_t>(rawInstance)));

    LOG_INFO_CAT("Vulkan", "Vulkan instance created with {} extensions", ext.size());
}

// ---------------------------------------------------------------------
// SURFACE CREATION
// ---------------------------------------------------------------------
void initSurface(void* window)
{
    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);
    if (!sdlWindow) {
        LOG_ERROR_CAT("Vulkan", "initSurface called with null window");
        throw std::runtime_error("Window is null");
    }

    VkInstance deobfInstance = reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(ctx().instance_)));
    VkSurfaceKHR rawSurface;
    if (!SDL_Vulkan_CreateSurface(sdlWindow, deobfInstance, nullptr, &rawSurface)) {
        LOG_ERROR_CAT("Vulkan", "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    ctx().surface_ = reinterpret_cast<VkSurfaceKHR>(obfuscate(reinterpret_cast<uint64_t>(rawSurface)));

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

// ---------------------------------------------------------------------
// DEVICE CREATION + QUEUE SETUP
// ---------------------------------------------------------------------
void initDevice()
{
    VkInstance deobfInstance = reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(ctx().instance_)));
    VkSurfaceKHR deobfSurface = reinterpret_cast<VkSurfaceKHR>(deobfuscate(reinterpret_cast<uint64_t>(ctx().surface_)));
    if (deobfSurface == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "initDevice() called before surface creation!");
        throw std::runtime_error("Surface must be created before device");
    }

    VkPhysicalDevice rawPhysicalDevice = findPhysicalDevice(deobfInstance, deobfSurface, true);
    ctx().physicalDevice_ = reinterpret_cast<VkPhysicalDevice>(obfuscate(reinterpret_cast<uint64_t>(rawPhysicalDevice)));

    VkPhysicalDevice deobfPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().physicalDevice_)));

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(deobfPhysicalDevice, &memProps);

    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &ctx().rayTracingProps_
    };
    vkGetPhysicalDeviceProperties2(deobfPhysicalDevice, &props2);
    LOG_INFO_CAT("Vulkan", "RT Properties -> handleSize={}, align={}", ctx().rayTracingProps_.shaderGroupHandleSize, ctx().rayTracingProps_.shaderGroupHandleAlignment);

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(deobfPhysicalDevice, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(deobfPhysicalDevice, &qCount, families.data());

    int graphics = -1, present = -1, compute = -1;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics = i;
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) compute = i;

        VkBool32 supportsPresent = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(deobfPhysicalDevice, i, deobfSurface, &supportsPresent),
                 "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (supportsPresent && present == -1) present = i;
    }

    if (graphics == -1 || present == -1) {
        LOG_ERROR_CAT("Vulkan", "Missing required queue families: G={} P={}", graphics, present);
        throw std::runtime_error("Invalid queue family indices");
    }
    if (compute == -1) compute = graphics;

    ctx().graphicsFamily_ = static_cast<uint32_t>(graphics);
    ctx().presentFamily_  = static_cast<uint32_t>(present);

    LOG_INFO_CAT("Vulkan", "Queue families -> G: {} | P: {} | C: {}", graphics, present, compute);

    std::set<uint32_t> uniqueQueueFamilies = { ctx().graphicsFamily_, ctx().presentFamily_ };
    if (static_cast<uint32_t>(compute) != ctx().graphicsFamily_) {
        uniqueQueueFamilies.insert(static_cast<uint32_t>(compute));
    }
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float priority = 1.0f;
    for (uint32_t idx : uniqueQueueFamilies) {
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

    VkDevice rawDevice;
    VK_CHECK(vkCreateDevice(deobfPhysicalDevice, &deviceInfo, nullptr, &rawDevice), "vkCreateDevice");
    ctx().device_ = reinterpret_cast<VkDevice>(obfuscate(reinterpret_cast<uint64_t>(rawDevice)));

    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().device_)));

    VkQueue rawGraphicsQueue;
    vkGetDeviceQueue(deobfDevice, ctx().graphicsFamily_, 0, &rawGraphicsQueue);
    ctx().graphicsQueue_ = reinterpret_cast<VkQueue>(obfuscate(reinterpret_cast<uint64_t>(rawGraphicsQueue)));

    VkQueue rawPresentQueue;
    vkGetDeviceQueue(deobfDevice, ctx().presentFamily_, 0, &rawPresentQueue);
    ctx().presentQueue_ = reinterpret_cast<VkQueue>(obfuscate(reinterpret_cast<uint64_t>(rawPresentQueue)));

#define LOAD_KHR(name) \
    ctx().name##_ = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(deobfDevice, #name)); \
    if (!ctx().name##_) { \
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
#undef LOAD_KHR

    LOG_INFO_CAT("Vulkan", "Device + queues + KHR functions initialized");
}

// ---------------------------------------------------------------------
// FULL INITIALISATION
// ---------------------------------------------------------------------
void initializeVulkan(void* window)
{
    LOG_INFO_CAT("INIT", "initializeVulkan() — full Vulkan init");

    Uint32 count = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&count);
    std::vector<const char*> instanceExtensions(sdlExtensions, sdlExtensions + count);
    initInstance(instanceExtensions);
    initSurface(window);
    initDevice();

    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().device_)));

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx().graphicsFamily_
    };
    VkCommandPool rawCommandPool;
    VK_CHECK(vkCreateCommandPool(deobfDevice, &poolInfo, nullptr, &rawCommandPool), "vkCreateCommandPool");
    ctx().commandPool_ = reinterpret_cast<VkCommandPool>(obfuscate(reinterpret_cast<uint64_t>(rawCommandPool)));

    LOG_INFO_CAT("CMD", "Command pool created");

    createSwapchain(window);
    LOG_INFO_CAT("SWAP", "Swapchain created: {}x{} | {} images",
                 swapchainExtent().width, swapchainExtent().height,
                 swapchainImages().size());

    LOG_INFO_CAT("Vulkan", "initializeVulkan() complete");
}

// ---------------------------------------------------------------------
// SWAPCHAIN
// ---------------------------------------------------------------------
void createSwapchain(void* window)
{
    VkSurfaceKHR deobfSurface = reinterpret_cast<VkSurfaceKHR>(deobfuscate(reinterpret_cast<uint64_t>(ctx().surface_)));
    VkPhysicalDevice deobfPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().physicalDevice_)));
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().device_)));

    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);
    int w, h;
    SDL_GetWindowSize(sdlWindow, &w, &h);
    uint32_t width = static_cast<uint32_t>(w);
    uint32_t height = static_cast<uint32_t>(h);

    if (width == 0 || height == 0) {
        LOG_WARN_CAT("Vulkan", "Window minimized, skipping swapchain creation");
        return;
    }

    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(deobfPhysicalDevice, deobfSurface, &capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    VkExtent2D extent = {
        std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, width)),
        std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, height))
    };

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    uint32_t formatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(deobfPhysicalDevice, deobfSurface, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR count");
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(deobfPhysicalDevice, deobfSurface, &formatCount, surfaceFormats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");

    VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
    if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
        surfaceFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    uint32_t presentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(deobfPhysicalDevice, deobfSurface, &presentModeCount, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR count");
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(deobfPhysicalDevice, deobfSurface, &presentModeCount, presentModes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR");

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }

    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t queueFamilyIndices[] = {ctx().graphicsFamily_, ctx().presentFamily_};
    uint32_t queueFamilyIndexCount = 1;
    if (ctx().graphicsFamily_ != ctx().presentFamily_) {
        sharingMode = VK_SHARING_MODE_CONCURRENT;
        queueFamilyIndexCount = 2;
    } else {
        queueFamilyIndices[0] = ctx().graphicsFamily_;  // Single index
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = deobfSurface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = sharingMode;
    createInfo.queueFamilyIndexCount = queueFamilyIndexCount;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR rawSwapchain;
    VK_CHECK(vkCreateSwapchainKHR(deobfDevice, &createInfo, nullptr, &rawSwapchain), "vkCreateSwapchainKHR");
    swapchain() = RTX::MakeHandle(rawSwapchain, deobfDevice, vkDestroySwapchainKHR);

    swapchainFormat() = surfaceFormat.format;
    swapchainExtent() = extent;

    VK_CHECK(vkGetSwapchainImagesKHR(deobfDevice, *swapchain(), &imageCount, nullptr), "vkGetSwapchainImagesKHR count");
    swapchainImages().resize(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(deobfDevice, *swapchain(), &imageCount, swapchainImages().data()), "vkGetSwapchainImagesKHR");

    swapchainImageViews().resize(swapchainImages().size());

    for (size_t i = 0; i < swapchainImages().size(); i++) {
        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.image = swapchainImages()[i];
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = swapchainFormat();
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;

        VkImageView rawView;
        VK_CHECK(vkCreateImageView(deobfDevice, &viewCreateInfo, nullptr, &rawView), "vkCreateImageView");
        swapchainImageViews()[i] = RTX::MakeHandle(rawView, deobfDevice, vkDestroyImageView);
    }

    LOG_INFO_CAT("Context", "Swapchain created via direct Vulkan calls");
}

void destroySwapchain()
{
    for (auto& imageView : swapchainImageViews()) {
        imageView.reset();
    }

    swapchain().reset();

    swapchainImageViews().clear();
    swapchainImages().clear();

    LOG_INFO_CAT("Context", "Swapchain destroyed");
}

// ---------------------------------------------------------------------
// COMMAND BUFFER HELPERS
// ---------------------------------------------------------------------
VkCommandBuffer beginSingleTimeCommands()
{
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().device_)));
    VkCommandPool deobfPool = reinterpret_cast<VkCommandPool>(deobfuscate(reinterpret_cast<uint64_t>(ctx().commandPool_)));

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

void endSingleTimeCommands(VkCommandBuffer cmd)
{
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().device_)));
    VkCommandPool deobfPool = reinterpret_cast<VkCommandPool>(deobfuscate(reinterpret_cast<uint64_t>(ctx().commandPool_)));
    VkQueue deobfQueue = reinterpret_cast<VkQueue>(deobfuscate(reinterpret_cast<uint64_t>(ctx().graphicsQueue_)));

    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (single-time)");
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
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
                  const VkMemoryAllocateFlagsInfo* allocFlags)
{
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "vkCreateBuffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device, buffer, &reqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = allocFlags,
        .allocationSize = reqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, reqs.memoryTypeBits, properties)
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "vkAllocateMemory");
    VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");
}

// ---------------------------------------------------------------------
// IMAGE HELPERS
// ---------------------------------------------------------------------
void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkImageMemoryBarrier barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = oldLayout; barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = { (format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

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
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else {
        endSingleTimeCommands(cmd);
        throw std::runtime_error("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
}

void copyBuffer(VkDevice device,
                VkCommandPool commandPool,
                VkQueue queue,
                VkBuffer srcBuffer,
                VkBuffer dstBuffer,
                VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void copyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height)
{
    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = { width, height, 1 }
    };
    vkCmdCopyBufferToImage(cmd, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);
}

void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& image, VkDeviceMemory& memory, VkImageView& view,
                        uint32_t width, uint32_t height)
{
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

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "vkAllocateMemory (storage)");
    VK_CHECK(vkBindImageMemory(device, image, memory, 0), "vkBindImageMemory (storage)");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = imageInfo.format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &view), "vkCreateImageView (storage)");
}

// ---------------------------------------------------------------------
// DESCRIPTOR LAYOUTS
// ---------------------------------------------------------------------
void createDescriptorSetLayout(VkDevice device,
                               VkDescriptorSetLayout& rayTracingLayout,
                               VkDescriptorSetLayout& graphicsLayout)
{
    // TODO: Implement descriptor set layouts
    rayTracingLayout = VK_NULL_HANDLE;
    graphicsLayout = VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------
// DESCRIPTOR POOL + SET
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
    VkImageView gNormalView)
{
    // TODO: Implement descriptor pool and set creation
}

// ---------------------------------------------------------------------
// DEVICE ADDRESS HELPERS
// ---------------------------------------------------------------------
VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer)
{
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().device_)));
    VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
    return ctx().vkGetBufferDeviceAddressKHR()(deobfDevice, &info);
}

VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as)
{
    VkDevice deobfDevice = reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(ctx().device_)));
    VkAccelerationStructureDeviceAddressInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as
    };
    return ctx().vkGetAccelerationStructureDeviceAddressKHR()(deobfDevice, &info);
}

// ---------------------------------------------------------------------
// UTILITY
// ---------------------------------------------------------------------
bool hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace RTX