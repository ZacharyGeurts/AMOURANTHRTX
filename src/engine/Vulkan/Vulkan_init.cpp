// AMOURANTH RTX Engine, October 2025 - Vulkan initialization utilities implementation.
// Dependencies: Vulkan 1.3+, VulkanCore.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025
// Enhancements: Added comprehensive logging and error handling for fence waits in single-time commands.
// Includes timeout handling with vkDeviceWaitIdle fallback to isolate GPU hangs. Removed redundant SBT/AS creation
// (now in PipelineManager). Enabled support for validation layers via VK_VALIDATION define.
// Fixes: Corrected VkDeviceCreateInfo.enabledLayerCount; added <cinttypes>; changed LOG_WARN_CAT to LOG_WARNING_CAT.

#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/logging.hpp"
#include "engine/Dispose.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h> // For ray tracing and buffer device address extensions
#include <SDL3/SDL_vulkan.h>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>
#include <glm/glm.hpp>
#include <format>
#include <chrono>  // For timeouts
#include <cinttypes>  // For PRIx64

namespace VulkanInitializer {

// Ray tracing and buffer device address function pointers (global for convenience)
PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;

void initializeRayTracingFunctions(VkDevice device) {
    LOG_DEBUG_CAT("VulkanInitializer", "Loading ray tracing and buffer device address function pointers");
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
    if (!vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to load vkCmdTraceRaysKHR");
        throw std::runtime_error("Failed to load vkCmdTraceRaysKHR");
    }

    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
    if (!vkCreateRayTracingPipelinesKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to load vkCreateRayTracingPipelinesKHR");
        throw std::runtime_error("Failed to load vkCreateRayTracingPipelinesKHR");
    }

    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    if (!vkGetRayTracingShaderGroupHandlesKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to load vkGetRayTracingShaderGroupHandlesKHR");
        throw std::runtime_error("Failed to load vkGetRayTracingShaderGroupHandlesKHR");
    }

    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    if (!vkCreateAccelerationStructureKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to load vkCreateAccelerationStructureKHR");
        throw std::runtime_error("Failed to load vkCreateAccelerationStructureKHR");
    }

    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    if (!vkGetAccelerationStructureBuildSizesKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to load vkGetAccelerationStructureBuildSizesKHR");
        throw std::runtime_error("Failed to load vkGetAccelerationStructureBuildSizesKHR");
    }

    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    if (!vkCmdBuildAccelerationStructuresKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to load vkCmdBuildAccelerationStructuresKHR");
        throw std::runtime_error("Failed to load vkCmdBuildAccelerationStructuresKHR");
    }

    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    if (!vkGetAccelerationStructureDeviceAddressKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to load vkGetAccelerationStructureDeviceAddressKHR");
        throw std::runtime_error("Failed to load vkGetAccelerationStructureDeviceAddressKHR");
    }

    vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
        vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    if (!vkGetBufferDeviceAddressKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to load vkGetBufferDeviceAddressKHR");
        throw std::runtime_error("Failed to load vkGetBufferDeviceAddressKHR");
    }

    LOG_INFO_CAT("VulkanInitializer", "Successfully loaded ray tracing and buffer device address function pointers");
    LOG_DEBUG_CAT("VulkanInitializer", "vkCmdTraceRaysKHR address: %p", reinterpret_cast<void*>(vkCmdTraceRaysKHR));
    LOG_DEBUG_CAT("VulkanInitializer", "vkGetBufferDeviceAddressKHR address: %p", reinterpret_cast<void*>(vkGetBufferDeviceAddressKHR));
}

template<typename Container>
std::string join(const Container& items, const std::string& delimiter) {
    std::ostringstream oss;
    auto it = items.begin();
    if (it != items.end()) {
        oss << *it;
        ++it;
    }
    for (; it != items.end(); ++it) {
        oss << delimiter << *it;
    }
    return oss.str();
}

void initInstance(const std::vector<std::string>& instanceExtensions, Vulkan::Context& context) {
    LOG_DEBUG_CAT("VulkanInitializer", "Initializing Vulkan instance with %zu extensions", instanceExtensions.size());

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "AMOURANTH RTX Engine",
        .applicationVersion = VK_MAKE_VERSION(3, 0, 0),
        .pEngineName = "AMOURANTH",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> extensions;
    for (const auto& ext : instanceExtensions) {
        extensions.push_back(ext.c_str());
    }

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

#ifdef VK_VALIDATION
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;
    LOG_INFO_CAT("VulkanInitializer", "Enabled validation layers for debugging");
#endif

    VkResult result = vkCreateInstance(&createInfo, nullptr, &context.instance);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create Vulkan instance with result: %d", static_cast<int>(result));
        throw std::runtime_error(std::format("Failed to create Vulkan instance: {}", result));
    }
    LOG_INFO_CAT("VulkanInitializer", "Created Vulkan instance: %p", static_cast<void*>(context.instance));
}

void initSurface(Vulkan::Context& context, void* window, VkSurfaceKHR* rawsurface) {
    LOG_DEBUG_CAT("VulkanInitializer", "Initializing Vulkan surface");
    if (rawsurface && *rawsurface != VK_NULL_HANDLE) {
        context.surface = *rawsurface;
        LOG_INFO_CAT("VulkanInitializer", "Using provided raw surface: %p", static_cast<void*>(context.surface));
        return;
    }

    if (!window) {
        LOG_ERROR_CAT("VulkanInitializer", "Window pointer is null and no raw surface provided");
        throw std::runtime_error("Window pointer is null and no raw surface provided");
    }

    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(sdlWindow, context.instance, nullptr, &surface)) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create Vulkan surface: %s", SDL_GetError());
        throw std::runtime_error(std::format("Failed to create Vulkan surface: {}", SDL_GetError()));
    }
    context.surface = surface;
    LOG_INFO_CAT("VulkanInitializer", "Created Vulkan surface: %p", static_cast<void*>(surface));
}

void initDevice(Vulkan::Context& context) {
    LOG_DEBUG_CAT("VulkanInitializer", "Initializing Vulkan device");
    if (context.physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Physical device not set");
        throw std::runtime_error("Physical device not set");
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamilyCount, queueFamilies.data());

    context.graphicsQueueFamilyIndex = UINT32_MAX;
    context.computeQueueFamilyIndex = UINT32_MAX;
    context.presentQueueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            context.graphicsQueueFamilyIndex = i;
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            context.computeQueueFamilyIndex = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i, context.surface, &presentSupport);
        if (presentSupport) {
            context.presentQueueFamilyIndex = i;
        }
        if (context.graphicsQueueFamilyIndex != UINT32_MAX &&
            context.computeQueueFamilyIndex != UINT32_MAX &&
            context.presentQueueFamilyIndex != UINT32_MAX) {
            break;
        }
    }
    if (context.graphicsQueueFamilyIndex == UINT32_MAX ||
        context.computeQueueFamilyIndex == UINT32_MAX ||
        context.presentQueueFamilyIndex == UINT32_MAX) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to find suitable queue families: graphics=%u, compute=%u, present=%u",
                  context.graphicsQueueFamilyIndex, context.computeQueueFamilyIndex, context.presentQueueFamilyIndex);
        throw std::runtime_error("Failed to find suitable queue families");
    }

    std::set<uint32_t> uniqueQueueFamilies = {
        context.graphicsQueueFamilyIndex,
        context.computeQueueFamilyIndex,
        context.presentQueueFamilyIndex
    };
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.descriptorIndexing = VK_TRUE;
    vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
    rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingFeatures.pNext = &vulkan12Features;
    rayTracingFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingFeatures.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
    rayTracingFeatures.rayTraversalPrimitiveCulling = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructureFeatures{};
    accelStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelStructureFeatures.pNext = &rayTracingFeatures;
    accelStructureFeatures.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeatures.pNext = &accelStructureFeatures;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingFeatures{};
    fragmentShadingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
    fragmentShadingFeatures.pNext = &dynamicRenderingFeatures;
    fragmentShadingFeatures.primitiveFragmentShadingRate = VK_TRUE;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
    meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
    meshShaderFeatures.pNext = &fragmentShadingFeatures;
    meshShaderFeatures.taskShader = VK_TRUE;
    meshShaderFeatures.meshShader = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &meshShaderFeatures;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    VkResult deviceResult = vkCreateDevice(context.physicalDevice, &deviceCreateInfo, nullptr, &context.device);
    if (deviceResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create Vulkan device with result: %d", static_cast<int>(deviceResult));
        throw std::runtime_error(std::format("Failed to create Vulkan device: {}", deviceResult));
    }
    context.resourceManager.setDevice(context.device);
    LOG_INFO_CAT("VulkanInitializer", "Created Vulkan device: %p", static_cast<void*>(context.device));

    // Load ray tracing and buffer device address function pointers after device creation
    initializeRayTracingFunctions(context.device);

    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &context.graphicsQueue);
    vkGetDeviceQueue(context.device, context.computeQueueFamilyIndex, 0, &context.computeQueue);
    vkGetDeviceQueue(context.device, context.presentQueueFamilyIndex, 0, &context.presentQueue);
    LOG_DEBUG_CAT("VulkanInitializer", "Retrieved graphics, compute, and present queues");
}

VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool preferNvidia) {
    uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
        LOG_ERROR_CAT("VulkanInitializer", "No Vulkan physical devices found");
        throw std::runtime_error("No Vulkan physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::vector<const char*> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME
    };

    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
    std::string selectedDeviceName;
    for (const auto& device : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        bool hasGraphicsQueue = false, hasComputeQueue = false, hasPresentQueue = false;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) hasGraphicsQueue = true;
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) hasComputeQueue = true;
            if (surface != VK_NULL_HANDLE) {
                VkBool32 presentSupport = VK_FALSE;
                if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport) == VK_SUCCESS && presentSupport) {
                    hasPresentQueue = true;
                }
            }
            if (hasGraphicsQueue && hasComputeQueue && (surface == VK_NULL_HANDLE || hasPresentQueue)) break;
        }

        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
        std::set<std::string> requiredExtSet(requiredExtensions.begin(), requiredExtensions.end());
        for (const auto& ext : availableExtensions) {
            requiredExtSet.erase(ext.extensionName);
        }

        if (hasGraphicsQueue && hasComputeQueue && (surface == VK_NULL_HANDLE || hasPresentQueue) && requiredExtSet.empty()) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            if (preferNvidia && deviceProperties.vendorID == 0x10DE) { // NVIDIA vendor ID
                selectedDevice = device;
                selectedDeviceName = deviceProperties.deviceName;
                break;
            } else if (!selectedDevice) {
                selectedDevice = device;
                selectedDeviceName = deviceProperties.deviceName;
            }
        }
    }

    if (selectedDevice == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "No suitable Vulkan physical device found");
        throw std::runtime_error("No suitable Vulkan physical device found");
    }
    LOG_INFO_CAT("VulkanInitializer", "Selected physical device: %s", selectedDeviceName.c_str());
    return selectedDevice;
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory,
                  const VkMemoryAllocateFlagsInfo* allocFlagsInfo, VulkanResourceManager& resourceManager) {
    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid device or physical device: device=%p, physicalDevice=%p",
                  static_cast<void*>(device), static_cast<void*>(physicalDevice));
        throw std::runtime_error("Invalid device or physical device");
    }
    if (size == 0) {
        LOG_ERROR_CAT("VulkanInitializer", "Buffer size cannot be zero");
        throw std::invalid_argument("Buffer size cannot be zero");
    }

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VkResult bufferResult = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (bufferResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create buffer with size=%zu, usage=0x%x, result=%d",
                  size, static_cast<uint32_t>(usage), static_cast<int>(bufferResult));
        throw std::runtime_error(std::format("Failed to create buffer: {}", bufferResult));
    }
    resourceManager.addBuffer(buffer);
    LOG_INFO_CAT("VulkanInitializer", "Created buffer: %p (size=%zu, usage=0x%x)", static_cast<void*>(buffer), size, static_cast<uint32_t>(usage));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
    VkDeviceSize alignedSize = (memRequirements.size + deviceProps.limits.minMemoryMapAlignment - 1) &
                              ~(deviceProps.limits.minMemoryMapAlignment - 1);

    uint32_t memType = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = allocFlagsInfo,
        .allocationSize = alignedSize,
        .memoryTypeIndex = memType
    };

    VkResult allocResult = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    if (allocResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate buffer memory for buffer=%p, size=%zu, result=%d",
                  static_cast<void*>(buffer), alignedSize, static_cast<int>(allocResult));
        Dispose::destroySingleBuffer(device, buffer);
        resourceManager.removeBuffer(buffer);
        throw std::runtime_error(std::format("Failed to allocate buffer memory: {}", allocResult));
    }
    resourceManager.addMemory(bufferMemory);

    VkResult bindResult = vkBindBufferMemory(device, buffer, bufferMemory, 0);
    if (bindResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to bind buffer memory for buffer=%p, memory=%p, result=%d",
                  static_cast<void*>(buffer), static_cast<void*>(bufferMemory), static_cast<int>(bindResult));
        Dispose::destroySingleBuffer(device, buffer);
        Dispose::freeSingleDeviceMemory(device, bufferMemory);
        resourceManager.removeBuffer(buffer);
        resourceManager.removeMemory(bufferMemory);
        throw std::runtime_error(std::format("Failed to bind buffer memory: {}", bindResult));
    }
    LOG_INFO_CAT("VulkanInitializer", "Allocated and bound buffer memory: %p for buffer: %p, alignedSize=%zu (memType=%u)",
             static_cast<void*>(bufferMemory), static_cast<void*>(buffer), alignedSize, memType);
}

VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    if (device == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid device or buffer for getBufferDeviceAddress: device=%p, buffer=%p",
                  static_cast<void*>(device), static_cast<void*>(buffer));
        throw std::runtime_error("Invalid device or buffer for getBufferDeviceAddress");
    }

    if (!vkGetBufferDeviceAddressKHR) {
        LOG_ERROR_CAT("VulkanInitializer", "vkGetBufferDeviceAddressKHR function pointer is null");
        throw std::runtime_error("vkGetBufferDeviceAddressKHR not initialized");
    }

    VkBufferDeviceAddressInfo addressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer
    };
    VkDeviceAddress address = vkGetBufferDeviceAddressKHR(device, &addressInfo);
    if (address == 0) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to get buffer device address: buffer=%p", static_cast<void*>(buffer));
        throw std::runtime_error("Failed to get buffer device address");
    }
    LOG_DEBUG_CAT("VulkanInitializer", "Retrieved device address 0x%" PRIx64 " for buffer %p", static_cast<uint64_t>(address), static_cast<void*>(buffer));
    return address;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            LOG_INFO_CAT("VulkanInitializer", "Selected memory type index: %u for properties: 0x%x", i, static_cast<uint32_t>(properties));
            return i;
        }
    }
    LOG_ERROR_CAT("VulkanInitializer", "Failed to find suitable memory type for typeFilter=0x%x, properties=0x%x",
              static_cast<uint32_t>(typeFilter), static_cast<uint32_t>(properties));
    throw std::runtime_error("Failed to find suitable memory type");
}

void createDescriptorSetLayout(VkDevice device, VkPhysicalDevice physicalDevice,
                              VkDescriptorSetLayout& rayTracingLayout, VkDescriptorSetLayout& graphicsLayout) {
    // Ray-tracing descriptor set layout (matches manager exactly: 10 bindings)
    std::array<VkDescriptorSetLayoutBinding, 10> rayTracingBindings = {};
    rayTracingBindings[0] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::TLAS), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                              VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                              VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR };
    rayTracingBindings[1] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::StorageImage), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                              VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT };
    rayTracingBindings[2] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::CameraUBO), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                              VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    rayTracingBindings[3] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::MaterialSSBO), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 26,
                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR };
    rayTracingBindings[4] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::DimensionDataSSBO), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
    rayTracingBindings[5] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::AlphaTex), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                              VK_SHADER_STAGE_ANY_HIT_BIT_KHR };
    rayTracingBindings[6] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::EnvMap), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                              VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                              VK_SHADER_STAGE_CALLABLE_BIT_KHR };
    rayTracingBindings[7] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::DensityVolume), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                              VK_SHADER_STAGE_ANY_HIT_BIT_KHR };
    rayTracingBindings[8] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::GDepth), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                              VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT };
    rayTracingBindings[9] = { static_cast<uint32_t>(VulkanRTX::DescriptorBindings::GNormal), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                              VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT };

    VkDescriptorSetLayoutCreateInfo rayTracingLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
        .bindingCount = static_cast<uint32_t>(rayTracingBindings.size()),
        .pBindings = rayTracingBindings.data()
    };

    VkResult rtLayoutResult = vkCreateDescriptorSetLayout(device, &rayTracingLayoutInfo, nullptr, &rayTracingLayout);
    if (rtLayoutResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create ray-tracing descriptor set layout with result: %d", static_cast<int>(rtLayoutResult));
        throw std::runtime_error(std::format("Failed to create ray-tracing descriptor set layout: {}", rtLayoutResult));
    }
    LOG_INFO_CAT("VulkanInitializer", "Created ray-tracing descriptor set layout with %zu bindings: %p",
             rayTracingBindings.size(), static_cast<void*>(rayTracingLayout));

    // Graphics descriptor set layout (unchanged)
    std::array<VkDescriptorSetLayoutBinding, 3> graphicsBindings = {{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,        1, VK_SHADER_STAGE_FRAGMENT_BIT}
    }};

    VkDescriptorSetLayoutCreateInfo graphicsLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
        .bindingCount = static_cast<uint32_t>(graphicsBindings.size()),
        .pBindings = graphicsBindings.data()
    };

    VkResult graphicsLayoutResult = vkCreateDescriptorSetLayout(device, &graphicsLayoutInfo, nullptr, &graphicsLayout);
    if (graphicsLayoutResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create graphics descriptor set layout with result: %d", static_cast<int>(graphicsLayoutResult));
        vkDestroyDescriptorSetLayout(device, rayTracingLayout, nullptr);
        throw std::runtime_error(std::format("Failed to create graphics descriptor set layout: {}", graphicsLayoutResult));
    }
    LOG_INFO_CAT("VulkanInitializer", "Created graphics descriptor set layout with %zu bindings: %p",
             graphicsBindings.size(), static_cast<void*>(graphicsLayout));
}

void initializeVulkan(Vulkan::Context& context) {
    if (context.instance == VK_NULL_HANDLE || context.surface == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid instance or surface: instance=%p, surface=%p",
                  static_cast<void*>(context.instance), static_cast<void*>(context.surface));
        throw std::runtime_error("Invalid Vulkan instance or surface");
    }

    context.physicalDevice = findPhysicalDevice(context.instance, context.surface, true);
    initDevice(context);

    // Create command pool
    VkCommandPoolCreateInfo commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphicsQueueFamilyIndex
    };
    VkResult poolResult = vkCreateCommandPool(context.device, &commandPoolInfo, nullptr, &context.commandPool);
    if (poolResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create command pool for queue family %u with result: %d",
                  context.graphicsQueueFamilyIndex, static_cast<int>(poolResult));
        Dispose::destroyDevice(context.device);
        throw std::runtime_error(std::format("Failed to create command pool: {}", poolResult));
    }
    context.resourceManager.addCommandPool(context.commandPool);
    LOG_INFO_CAT("VulkanInitializer", "Created command pool: %p and added to resource manager", static_cast<void*>(context.commandPool));

    // Create descriptor pool (updated for 10 RT bindings: 3 combined_sampler, 3 storage_image, 27 storage_buffer approx)
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 16;
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 * MAX_FRAMES_IN_FLIGHT },  // StorageImage + GDepth + GNormal
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 27 * MAX_FRAMES_IN_FLIGHT },  // Material(26) + Dimension(1)
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 * MAX_FRAMES_IN_FLIGHT }  // AlphaTex + EnvMap + DensityVolume
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    VkResult descPoolResult = vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &context.descriptorPool);
    if (descPoolResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create descriptor pool with result: %d", static_cast<int>(descPoolResult));
        Dispose::destroySingleCommandPool(context.device, context.commandPool);
        Dispose::destroyDevice(context.device);
        throw std::runtime_error(std::format("Failed to create descriptor pool: {}", descPoolResult));
    }
    context.resourceManager.addDescriptorPool(context.descriptorPool);
    LOG_INFO_CAT("VulkanInitializer", "Created descriptor pool: %p with maxSets=%u", static_cast<void*>(context.descriptorPool), MAX_FRAMES_IN_FLIGHT);

    createDescriptorSetLayout(context.device, context.physicalDevice, context.rayTracingDescriptorSetLayout, context.graphicsDescriptorSetLayout);
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &context.memoryProperties);
    LOG_INFO_CAT("VulkanInitializer", "Vulkan initialization complete");
}

void createStorageImage(VkDevice device, VkPhysicalDevice physicalDevice, VkImage& storageImage,
                       VkDeviceMemory& storageImageMemory, VkImageView& storageImageView,
                       uint32_t width, uint32_t height, VulkanResourceManager& resourceManager) {
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkResult imageResult = vkCreateImage(device, &imageInfo, nullptr, &storageImage);
    if (imageResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create storage image with result: %d (width=%u, height=%u)", static_cast<int>(imageResult), width, height);
        throw std::runtime_error(std::format("Failed to create storage image: {}", imageResult));
    }
    resourceManager.addImage(storageImage);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, storageImage, &memRequirements);

    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
    VkDeviceSize alignedSize = (memRequirements.size + deviceProps.limits.minMemoryMapAlignment - 1) &
                              ~(deviceProps.limits.minMemoryMapAlignment - 1);

    uint32_t memType = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = alignedSize,
        .memoryTypeIndex = memType
    };

    VkResult allocResult = vkAllocateMemory(device, &allocInfo, nullptr, &storageImageMemory);
    if (allocResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate storage image memory with result: %d", static_cast<int>(allocResult));
        Dispose::destroySingleImage(device, storageImage);
        resourceManager.removeImage(storageImage);
        throw std::runtime_error(std::format("Failed to allocate storage image memory: {}", allocResult));
    }
    resourceManager.addMemory(storageImageMemory);

    VkResult bindResult = vkBindImageMemory(device, storageImage, storageImageMemory, 0);
    if (bindResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to bind storage image memory with result: %d", static_cast<int>(bindResult));
        Dispose::destroySingleImage(device, storageImage);
        Dispose::freeSingleDeviceMemory(device, storageImageMemory);
        resourceManager.removeImage(storageImage);
        resourceManager.removeMemory(storageImageMemory);
        throw std::runtime_error(std::format("Failed to bind storage image memory: {}", bindResult));
    }

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = storageImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkResult viewResult = vkCreateImageView(device, &viewInfo, nullptr, &storageImageView);
    if (viewResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create storage image view with result: %d", static_cast<int>(viewResult));
        Dispose::destroySingleImage(device, storageImage);
        Dispose::freeSingleDeviceMemory(device, storageImageMemory);
        resourceManager.removeImage(storageImage);
        resourceManager.removeMemory(storageImageMemory);
        throw std::runtime_error(std::format("Failed to create storage image view: {}", viewResult));
    }
    resourceManager.addImageView(storageImageView);
    LOG_INFO_CAT("VulkanInitializer", "Created storage image %p, memory %p, view %p (width=%u, height=%u)",
             static_cast<void*>(storageImage), static_cast<void*>(storageImageMemory), static_cast<void*>(storageImageView), width, height);
}

// Removed: createShaderBindingTable (now in PipelineManager)
// Removed: createAccelerationStructures (now in PipelineManager)

void createDescriptorPoolAndSet(VkDevice device, VkPhysicalDevice physicalDevice, VkDescriptorSetLayout descriptorSetLayout,
                               VkDescriptorPool& descriptorPool, std::vector<VkDescriptorSet>& descriptorSets,
                               VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                               VkAccelerationStructureKHR topLevelAS, bool forRayTracing,
                               std::vector<VkBuffer> materialBuffers, std::vector<VkBuffer> dimensionBuffers,
                               VkImageView alphaTexView, VkImageView envMapView, VkImageView densityVolumeView,
                               VkImageView gDepthView, VkImageView gNormalView) {  // Updated params to match 10 bindings
    if (device == VK_NULL_HANDLE || descriptorSetLayout == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid device or descriptor set layout: device=%p, layout=%p",
                  static_cast<void*>(device), static_cast<void*>(descriptorSetLayout));
        throw std::runtime_error("Invalid device or descriptor set layout");
    }

    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 16;

    std::vector<VkDescriptorPoolSize> poolSizes = forRayTracing ? std::vector<VkDescriptorPoolSize>{
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 27 * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 * MAX_FRAMES_IN_FLIGHT }
    } : std::vector<VkDescriptorPoolSize>{
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    VkResult poolResult = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (poolResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create descriptor pool with result: %d", static_cast<int>(poolResult));
        throw std::runtime_error(std::format("Failed to create descriptor pool: {}", poolResult));
    }
    LOG_INFO_CAT("VulkanInitializer", "Created descriptor pool: %p", static_cast<void*>(descriptorPool));

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()
    };
    VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data());
    if (allocResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate descriptor sets with result: %d", static_cast<int>(allocResult));
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        throw std::runtime_error(std::format("Failed to allocate descriptor sets: {}", allocResult));
    }

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    VkResult samplerResult = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
    if (samplerResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create sampler with result: %d", static_cast<int>(samplerResult));
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        throw std::runtime_error(std::format("Failed to create sampler: {}", samplerResult));
    }
    LOG_INFO_CAT("VulkanInitializer", "Created sampler: %p", static_cast<void*>(sampler));

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::vector<VkWriteDescriptorSet> descriptorWrites;
        VkDescriptorBufferInfo uniformBufferInfo = {
            .buffer = uniformBuffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
        VkDescriptorImageInfo storageImageInfo = {
            .sampler = sampler,
            .imageView = storageImageView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = nullptr,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &topLevelAS
        };
        std::vector<VkDescriptorBufferInfo> materialBufferInfos;
        for (const auto& buffer : materialBuffers) {
            materialBufferInfos.push_back({
                .buffer = buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            });
        }
        std::vector<VkDescriptorBufferInfo> dimensionBufferInfos;
        for (const auto& buffer : dimensionBuffers) {
            dimensionBufferInfos.push_back({
                .buffer = buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            });
        }
        VkDescriptorImageInfo alphaTexInfo = { .sampler = sampler, .imageView = alphaTexView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkDescriptorImageInfo envMapInfo = { .sampler = sampler, .imageView = envMapView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkDescriptorImageInfo densityVolumeInfo = { .sampler = sampler, .imageView = densityVolumeView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkDescriptorImageInfo gDepthInfo = { .imageView = gDepthView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };  // No sampler for storage_image
        VkDescriptorImageInfo gNormalInfo = { .imageView = gNormalView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };

        if (forRayTracing) {
            // TLAS (0)
            VkWriteDescriptorSet tlasWrite = {};
            tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            tlasWrite.pNext = &asInfo;
            tlasWrite.dstSet = descriptorSets[i];
            tlasWrite.dstBinding = 0;
            tlasWrite.descriptorCount = 1;
            tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            descriptorWrites.push_back(tlasWrite);

            // StorageImage (1)
            VkWriteDescriptorSet storageWrite = {};
            storageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            storageWrite.dstSet = descriptorSets[i];
            storageWrite.dstBinding = 1;
            storageWrite.descriptorCount = 1;
            storageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            storageWrite.pImageInfo = &storageImageInfo;
            descriptorWrites.push_back(storageWrite);

            // CameraUBO (2)
            VkWriteDescriptorSet cameraWrite = {};
            cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            cameraWrite.dstSet = descriptorSets[i];
            cameraWrite.dstBinding = 2;
            cameraWrite.descriptorCount = 1;
            cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            cameraWrite.pBufferInfo = &uniformBufferInfo;
            descriptorWrites.push_back(cameraWrite);

            // MaterialSSBO (3)
            VkWriteDescriptorSet materialWrite = {};
            materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            materialWrite.dstSet = descriptorSets[i];
            materialWrite.dstBinding = 3;
            materialWrite.descriptorCount = static_cast<uint32_t>(materialBufferInfos.size());
            materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            materialWrite.pBufferInfo = materialBufferInfos.data();
            descriptorWrites.push_back(materialWrite);

            // DimensionDataSSBO (4)
            VkWriteDescriptorSet dimensionWrite = {};
            dimensionWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            dimensionWrite.dstSet = descriptorSets[i];
            dimensionWrite.dstBinding = 4;
            dimensionWrite.descriptorCount = static_cast<uint32_t>(dimensionBufferInfos.size());
            dimensionWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            dimensionWrite.pBufferInfo = dimensionBufferInfos.data();
            descriptorWrites.push_back(dimensionWrite);

            // AlphaTex (5)
            VkWriteDescriptorSet alphaWrite = {};
            alphaWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            alphaWrite.dstSet = descriptorSets[i];
            alphaWrite.dstBinding = 5;
            alphaWrite.descriptorCount = 1;
            alphaWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            alphaWrite.pImageInfo = &alphaTexInfo;
            descriptorWrites.push_back(alphaWrite);

            // EnvMap (6)
            VkWriteDescriptorSet envWrite = {};
            envWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            envWrite.dstSet = descriptorSets[i];
            envWrite.dstBinding = 6;
            envWrite.descriptorCount = 1;
            envWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            envWrite.pImageInfo = &envMapInfo;
            descriptorWrites.push_back(envWrite);

            // DensityVolume (7)
            VkWriteDescriptorSet densityWrite = {};
            densityWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            densityWrite.dstSet = descriptorSets[i];
            densityWrite.dstBinding = 7;
            densityWrite.descriptorCount = 1;
            densityWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            densityWrite.pImageInfo = &densityVolumeInfo;
            descriptorWrites.push_back(densityWrite);

            // GDepth (8)
            VkWriteDescriptorSet gDepthWrite = {};
            gDepthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            gDepthWrite.dstSet = descriptorSets[i];
            gDepthWrite.dstBinding = 8;
            gDepthWrite.descriptorCount = 1;
            gDepthWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            gDepthWrite.pImageInfo = &gDepthInfo;
            descriptorWrites.push_back(gDepthWrite);

            // GNormal (9)
            VkWriteDescriptorSet gNormalWrite = {};
            gNormalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            gNormalWrite.dstSet = descriptorSets[i];
            gNormalWrite.dstBinding = 9;
            gNormalWrite.descriptorCount = 1;
            gNormalWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            gNormalWrite.pImageInfo = &gNormalInfo;
            descriptorWrites.push_back(gNormalWrite);
        } else {
            // Graphics: unchanged (binding 0 image, 1 uniform)
            VkWriteDescriptorSet imageWrite = {};
            imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            imageWrite.dstSet = descriptorSets[i];
            imageWrite.dstBinding = 0;
            imageWrite.descriptorCount = 1;
            imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            imageWrite.pImageInfo = &storageImageInfo;
            descriptorWrites.push_back(imageWrite);

            VkWriteDescriptorSet uniformWrite = {};
            uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uniformWrite.dstSet = descriptorSets[i];
            uniformWrite.dstBinding = 1;
            uniformWrite.descriptorCount = 1;
            uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformWrite.pBufferInfo = &uniformBufferInfo;
            descriptorWrites.push_back(uniformWrite);
        }
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
    LOG_INFO_CAT("VulkanInitializer", "Updated %u descriptor sets for %s", static_cast<uint32_t>(descriptorSets.size()), forRayTracing ? "ray tracing" : "graphics");
}

VkCommandBuffer beginSingleTimeCommands(Vulkan::Context& context) {
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    VkResult allocResult = vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);
    if (allocResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate single-time command buffer with result: %d", static_cast<int>(allocResult));
        throw std::runtime_error(std::format("Failed to allocate single-time command buffer: {}", allocResult));
    }
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VkResult beginResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (beginResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to begin single-time command buffer with result: %d", static_cast<int>(beginResult));
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to begin single-time command buffer: {}", beginResult));
    }
    LOG_DEBUG_CAT("VulkanInitializer", "Allocated and began single-time command buffer: %p", static_cast<void*>(commandBuffer));
    return commandBuffer;
}

void endSingleTimeCommands(Vulkan::Context& context, VkCommandBuffer commandBuffer) {
    if (commandBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid command buffer for endSingleTimeCommands");
        throw std::runtime_error("Invalid command buffer");
    }
    VkResult endResult = vkEndCommandBuffer(commandBuffer);
    if (endResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "vkEndCommandBuffer failed with result: %d (0x%x) - invalid commands?", 
                  static_cast<int>(endResult), static_cast<uint32_t>(endResult));
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to end single-time command buffer: {}", endResult));
    }

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    VkFence fence;
    VkResult fenceResult = vkCreateFence(context.device, &fenceInfo, nullptr, &fence);
    if (fenceResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "vkCreateFence failed with result: %d (0x%x)", 
                  static_cast<int>(fenceResult), static_cast<uint32_t>(fenceResult));
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to create fence: {}", fenceResult));
    }

    VkResult submitResult = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, fence);
    if (submitResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "vkQueueSubmit failed with result: %d (0x%x) - check queue family or command validity", 
                  static_cast<int>(submitResult), static_cast<uint32_t>(submitResult));
        vkDestroyFence(context.device, fence, nullptr);
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to submit single-time command buffer: {}", submitResult));
    }
    LOG_DEBUG_CAT("VulkanInitializer", "Submitted single-time commands to graphics queue (fence: %p)", static_cast<void*>(fence));

    // Wait with timeout to detect hangs early
    uint64_t timeoutNs = 5000000000ULL;  // 5 seconds
    VkResult waitResult = vkWaitForFences(context.device, 1, &fence, VK_TRUE, timeoutNs);
    if (waitResult == VK_TIMEOUT) {
        LOG_ERROR_CAT("VulkanInitializer", "vkWaitForFences timed out after 5s - GPU hang suspected (possible invalid copy/transfer); idling device");
        VkResult idleResult = vkDeviceWaitIdle(context.device);
        LOG_WARNING_CAT("VulkanInitializer", "vkDeviceWaitIdle after timeout: %d", static_cast<int>(idleResult));
        waitResult = vkWaitForFences(context.device, 1, &fence, VK_TRUE, UINT64_MAX);  // Retry infinite
        if (waitResult != VK_SUCCESS) {
            LOG_ERROR_CAT("VulkanInitializer", "Retry vkWaitForFences failed: %d (0x%x) - device lost?", 
                      static_cast<int>(waitResult), static_cast<uint32_t>(waitResult));
            if (waitResult == VK_ERROR_DEVICE_LOST) {
                LOG_ERROR_CAT("VulkanInitializer", "Device lost detected - recommend full context recreate");
            }
        }
    } else if (waitResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "vkWaitForFences failed: %d (0x%x) - possible device lost or invalid fence", 
                  static_cast<int>(waitResult), static_cast<uint32_t>(waitResult));
        VkResult idleResult = vkDeviceWaitIdle(context.device);
        LOG_WARNING_CAT("VulkanInitializer", "vkDeviceWaitIdle after error: %d", static_cast<int>(idleResult));
        if (waitResult == VK_ERROR_DEVICE_LOST) {
            LOG_ERROR_CAT("VulkanInitializer", "Device lost - recreate Vulkan device/context in caller");
        }
    }

    if (waitResult != VK_SUCCESS) {
        vkDestroyFence(context.device, fence, nullptr);
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to wait for fence (final): {}", waitResult));
    }

    vkDestroyFence(context.device, fence, nullptr);
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
    LOG_DEBUG_CAT("VulkanInitializer", "Single-time commands completed successfully (fence signaled)");
}

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    if (device == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE ||
        srcBuffer == VK_NULL_HANDLE || dstBuffer == VK_NULL_HANDLE || size == 0) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid parameters for copyBuffer (src=%p, dst=%p, size=%zu)", 
                  static_cast<void*>(srcBuffer), static_cast<void*>(dstBuffer), size);
        throw std::invalid_argument("Invalid parameters for copyBuffer");
    }

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    VkResult allocResult = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    if (allocResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate command buffer for copyBuffer with result: %d", static_cast<int>(allocResult));
        throw std::runtime_error(std::format("Failed to allocate command buffer for copyBuffer: {}", allocResult));
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VkResult beginResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (beginResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to begin command buffer for copyBuffer with result: %d", static_cast<int>(beginResult));
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to begin command buffer for copyBuffer: {}", beginResult));
    }

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    LOG_DEBUG_CAT("VulkanInitializer", "Recorded vkCmdCopyBuffer: src=%p -> dst=%p, size=%zu", 
              static_cast<void*>(srcBuffer), static_cast<void*>(dstBuffer), size);

    VkResult endResult = vkEndCommandBuffer(commandBuffer);
    if (endResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "vkEndCommandBuffer in copyBuffer failed with result: %d (0x%x)", 
                  static_cast<int>(endResult), static_cast<uint32_t>(endResult));
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to end command buffer for copyBuffer: {}", endResult));
    }

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    VkFence fence;
    VkResult fenceResult = vkCreateFence(device, &fenceInfo, nullptr, &fence);
    if (fenceResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "vkCreateFence for copyBuffer failed with result: %d", static_cast<int>(fenceResult));
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to create fence for copyBuffer: {}", fenceResult));
    }

    VkResult submitResult = vkQueueSubmit(queue, 1, &submitInfo, fence);
    if (submitResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "vkQueueSubmit in copyBuffer failed with result: %d (0x%x)", 
                  static_cast<int>(submitResult), static_cast<uint32_t>(submitResult));
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to submit command buffer for copyBuffer: {}", submitResult));
    }
    LOG_DEBUG_CAT("VulkanInitializer", "Submitted copyBuffer commands to queue (fence: %p)", static_cast<void*>(fence));

    // Wait with timeout (same as endSingleTimeCommands)
    uint64_t timeoutNs = 5000000000ULL;  // 5 seconds
    VkResult waitResult = vkWaitForFences(device, 1, &fence, VK_TRUE, timeoutNs);
    if (waitResult == VK_TIMEOUT) {
        LOG_ERROR_CAT("VulkanInitializer", "copyBuffer vkWaitForFences timed out after 5s - GPU hang (check buffer usage flags/alignment); idling device");
        VkResult idleResult = vkDeviceWaitIdle(device);
        LOG_WARNING_CAT("VulkanInitializer", "vkDeviceWaitIdle after copyBuffer timeout: %d", static_cast<int>(idleResult));
        waitResult = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        if (waitResult != VK_SUCCESS) {
            LOG_ERROR_CAT("VulkanInitializer", "Retry vkWaitForFences in copyBuffer failed: %d", static_cast<int>(waitResult));
        }
    } else if (waitResult != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "copyBuffer vkWaitForFences failed: %d (0x%x)", 
                  static_cast<int>(waitResult), static_cast<uint32_t>(waitResult));
        VkResult idleResult = vkDeviceWaitIdle(device);
        LOG_WARNING_CAT("VulkanInitializer", "vkDeviceWaitIdle after copyBuffer error: %d", static_cast<int>(idleResult));
    }

    if (waitResult != VK_SUCCESS) {
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error(std::format("Failed to wait for fence in copyBuffer: {}", waitResult));
    }

    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    LOG_DEBUG_CAT("VulkanInitializer", "copyBuffer completed successfully: src=%p, dst=%p, size=%zu", 
              static_cast<void*>(srcBuffer), static_cast<void*>(dstBuffer), size);
}

} // namespace VulkanInitializer