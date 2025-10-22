// src/engine/SDL3/SDL3_vulkan.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan instance and surface initialization with SDL3.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_init.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <source_location>
#include <cstring>
#include <string>
#include <set>
#include <format>

namespace SDL3Initializer {

struct QueueFamilyIndices {
    uint32_t graphicsFamily = -1;
    uint32_t presentFamily = -1;
    bool isComplete() const { return graphicsFamily != static_cast<uint32_t>(-1) && presentFamily != static_cast<uint32_t>(-1); }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, std::source_location loc = std::source_location::current()) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        LOG_ERROR("Vulkan", "No queue families found for device {:p}", static_cast<void*>(device), loc);
        throw std::runtime_error("No queue families found");
    }

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (indices.isComplete()) break;

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = VK_FALSE;
            VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (result != VK_SUCCESS) {
                LOG_WARNING("Vulkan", "Failed to check surface support for queue family {}: VkResult={}", i, result, loc);
                continue;
            }
            if (presentSupport == VK_TRUE) {
                indices.presentFamily = i;
            }
        }

        ++i;
    }

    if (indices.graphicsFamily == static_cast<uint32_t>(-1)) {
        LOG_ERROR("Vulkan", "No graphics queue family found", loc);
        throw std::runtime_error("No graphics queue family found");
    }
    if (surface != VK_NULL_HANDLE && indices.presentFamily == static_cast<uint32_t>(-1)) {
        LOG_WARNING("Vulkan", "No present queue family found for surface {:p}", static_cast<void*>(surface), loc);
    }

    return indices;
}

void initVulkan(
    SDL_Window* window, 
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation, 
    bool preferNvidia, 
    bool rt, 
    std::string_view title,
    VkPhysicalDevice& physicalDevice,
    std::source_location loc = std::source_location::current()) {
    if (!window) {
        LOG_ERROR("Vulkan", "Invalid SDL window pointer", loc);
        throw std::runtime_error("Invalid SDL window pointer");
    }

    // Ensure SDL is initialized
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        LOG_ERROR("Vulkan", "SDL video subsystem not initialized", loc);
        throw std::runtime_error("SDL video subsystem not initialized");
    }

    // Get required Vulkan extensions from SDL
    Uint32 extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionNames) {
        LOG_ERROR("Vulkan", "Failed to get Vulkan extensions: {}", SDL_GetError(), loc);
        throw std::runtime_error("Failed to get Vulkan extensions");
    }
    std::vector<const char*> extensions(extensionNames, extensionNames + extensionCount);

    // Add additional extensions
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    if (rt) {
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    // Validation layers
    std::vector<const char*> layers;
    if (enableValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        bool validationSupported = false;
        for (const auto& layer : availableLayers) {
            if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                validationSupported = true;
                break;
            }
        }
        if (!validationSupported) {
            LOG_WARNING("Vulkan", "Validation layers requested but not available", loc);
            layers.clear();
        }
    }

    // Vulkan instance creation
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = title.data(),
        .applicationVersion = VK_MAKE_VERSION(3, 0, 0),
        .pEngineName = "AMOURANTH RTX Engine",
        .engineVersion = VK_MAKE_VERSION(3, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.empty() ? nullptr : layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VkInstance rawInstance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &rawInstance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Vulkan", "Failed to create Vulkan instance: VkResult={}", result, loc);
        throw std::runtime_error("Failed to create Vulkan instance");
    }
    instance = VulkanInstancePtr(rawInstance, VulkanInstanceDeleter());
    LOG_INFO("Vulkan", "Created Vulkan instance: {:p}", static_cast<void*>(rawInstance), loc);

    // Create Vulkan surface
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, rawInstance, nullptr, &rawSurface)) {
        LOG_ERROR("Vulkan", "Failed to create Vulkan surface: {}", SDL_GetError(), loc);
        vkDestroyInstance(rawInstance, nullptr);
        throw std::runtime_error("Failed to create Vulkan surface: " + std::string(SDL_GetError()));
    }
    surface = VulkanSurfacePtr(rawSurface, VulkanSurfaceDeleter(rawInstance));
    LOG_INFO("Vulkan", "Created Vulkan surface: {:p}", static_cast<void*>(rawSurface), loc);

    // Select physical device
    physicalDevice = VulkanInitializer::findPhysicalDevice(rawInstance, rawSurface, preferNvidia);
    if (physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("Vulkan", "No suitable Vulkan physical device found", loc);
        throw std::runtime_error("No suitable Vulkan physical device found");
    }

    // Verify physical device properties and features for ray tracing
    std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    if (rt) {
        requiredDeviceExtensions.insert(requiredDeviceExtensions.end(), {
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
        });

        uint32_t deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(deviceExtensionCount);
        if (deviceExtensionCount > 0) {
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, availableExtensions.data());
        }

        std::set<std::string> requiredExtSet(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());
        for (const auto& ext : availableExtensions) {
            requiredExtSet.erase(ext.extensionName);
        }
        if (!requiredExtSet.empty()) {
            throw std::runtime_error("Physical device does not support required ray tracing extensions");
        }

        VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = nullptr,
            .features = {
                .robustBufferAccess = VK_FALSE,
                .fullDrawIndexUint32 = VK_FALSE,
                .imageCubeArray = VK_FALSE,
                .independentBlend = VK_FALSE,
                .geometryShader = VK_FALSE,
                .tessellationShader = VK_FALSE,
                .sampleRateShading = VK_FALSE,
                .dualSrcBlend = VK_FALSE,
                .logicOp = VK_FALSE,
                .multiDrawIndirect = VK_FALSE,
                .drawIndirectFirstInstance = VK_FALSE,
                .depthClamp = VK_FALSE,
                .depthBiasClamp = VK_FALSE,
                .fillModeNonSolid = VK_FALSE,
                .depthBounds = VK_FALSE,
                .wideLines = VK_FALSE,
                .largePoints = VK_FALSE,
                .alphaToOne = VK_FALSE,
                .multiViewport = VK_FALSE,
                .samplerAnisotropy = VK_TRUE,
                .textureCompressionETC2 = VK_FALSE,
                .textureCompressionASTC_LDR = VK_FALSE,
                .textureCompressionBC = VK_FALSE,
                .occlusionQueryPrecise = VK_FALSE,
                .pipelineStatisticsQuery = VK_FALSE,
                .vertexPipelineStoresAndAtomics = VK_FALSE,
                .fragmentStoresAndAtomics = VK_FALSE,
                .shaderTessellationAndGeometryPointSize = VK_FALSE,
                .shaderImageGatherExtended = VK_FALSE,
                .shaderStorageImageExtendedFormats = VK_FALSE,
                .shaderStorageImageMultisample = VK_FALSE,
                .shaderStorageImageReadWithoutFormat = VK_FALSE,
                .shaderStorageImageWriteWithoutFormat = VK_FALSE,
                .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
                .shaderSampledImageArrayDynamicIndexing = VK_FALSE,
                .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
                .shaderStorageImageArrayDynamicIndexing = VK_FALSE,
                .shaderClipDistance = VK_FALSE,
                .shaderCullDistance = VK_FALSE,
                .shaderFloat64 = VK_FALSE,
                .shaderInt64 = VK_FALSE,
                .shaderInt16 = VK_FALSE,
                .shaderResourceResidency = VK_FALSE,
                .shaderResourceMinLod = VK_FALSE,
                .sparseBinding = VK_FALSE,
                .sparseResidencyBuffer = VK_FALSE,
                .sparseResidencyImage2D = VK_FALSE,
                .sparseResidencyImage3D = VK_FALSE,
                .sparseResidency2Samples = VK_FALSE,
                .sparseResidency4Samples = VK_FALSE,
                .sparseResidency8Samples = VK_FALSE,
                .sparseResidency16Samples = VK_FALSE,
                .sparseResidencyAliased = VK_FALSE,
                .variableMultisampleRate = VK_FALSE,
                .inheritedQueries = VK_FALSE
            }
        };

        VkPhysicalDeviceVulkan13Features vulkan13Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = nullptr,
            .robustImageAccess = VK_FALSE,
            .inlineUniformBlock = VK_FALSE,
            .descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE,
            .pipelineCreationCacheControl = VK_FALSE,
            .privateData = VK_FALSE,
            .shaderDemoteToHelperInvocation = VK_FALSE,
            .shaderTerminateInvocation = VK_FALSE,
            .subgroupSizeControl = VK_FALSE,
            .computeFullSubgroups = VK_FALSE,
            .synchronization2 = VK_FALSE,
            .textureCompressionASTC_HDR = VK_FALSE,
            .shaderZeroInitializeWorkgroupMemory = VK_FALSE,
            .dynamicRendering = VK_FALSE,
            .shaderIntegerDotProduct = VK_FALSE,
            .maintenance4 = VK_FALSE
        };

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = &vulkan13Features,
            .bufferDeviceAddress = VK_FALSE,
            .bufferDeviceAddressCaptureReplay = VK_FALSE,
            .bufferDeviceAddressMultiDevice = VK_FALSE
        };

        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
            .pNext = &bufferDeviceAddressFeatures,
            .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
            .shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
            .shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
            .shaderUniformBufferArrayNonUniformIndexing = VK_FALSE,
            .shaderSampledImageArrayNonUniformIndexing = VK_FALSE,
            .shaderStorageBufferArrayNonUniformIndexing = VK_FALSE,
            .shaderStorageImageArrayNonUniformIndexing = VK_FALSE,
            .shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
            .shaderUniformTexelBufferArrayNonUniformIndexing = VK_FALSE,
            .shaderStorageTexelBufferArrayNonUniformIndexing = VK_FALSE,
            .descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_FALSE,
            .descriptorBindingStorageImageUpdateAfterBind = VK_FALSE,
            .descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingUpdateUnusedWhilePending = VK_FALSE,
            .descriptorBindingPartiallyBound = VK_FALSE,
            .descriptorBindingVariableDescriptorCount = VK_FALSE,
            .runtimeDescriptorArray = VK_FALSE
        };

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &descriptorIndexingFeatures,
            .accelerationStructure = VK_FALSE,
            .accelerationStructureCaptureReplay = VK_FALSE,
            .accelerationStructureIndirectBuild = VK_FALSE,
            .accelerationStructureHostCommands = VK_FALSE,
            .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &accelerationStructureFeatures,
            .rayTracingPipeline = VK_FALSE,
            .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
            .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
            .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
            .rayTraversalPrimitiveCulling = VK_FALSE
        };

        physicalDeviceFeatures.pNext = &rayTracingPipelineFeatures;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures);

        if (!bufferDeviceAddressFeatures.bufferDeviceAddress ||
            !descriptorIndexingFeatures.descriptorBindingPartiallyBound ||
            !accelerationStructureFeatures.accelerationStructure ||
            !rayTracingPipelineFeatures.rayTracingPipeline) {
            LOG_ERROR("Vulkan", "Physical device lacks required ray tracing features", loc);
            throw std::runtime_error("Physical device does not support required ray tracing features");
        }

        LOG_INFO("Vulkan", "Physical device confirmed ready for ray tracing", loc);
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    LOG_INFO("Vulkan", "Selected physical device: {}", properties.deviceName, loc);

    // Create logical device
    auto indices = findQueueFamilies(physicalDevice, rawSurface, loc);
    if (!indices.isComplete()) {
        LOG_ERROR("Vulkan", "Failed to find suitable queue families", loc);
        throw std::runtime_error("Failed to find suitable queue families");
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };
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

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = enableValidation ? static_cast<uint32_t>(layers.size()) : 0,
        .ppEnabledLayerNames = enableValidation ? layers.data() : nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        .pEnabledFeatures = nullptr
    };

    VkPhysicalDeviceFeatures2 enabledFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = nullptr,
        .features = {
            .robustBufferAccess = VK_FALSE,
            .fullDrawIndexUint32 = VK_FALSE,
            .imageCubeArray = VK_FALSE,
            .independentBlend = VK_FALSE,
            .geometryShader = VK_FALSE,
            .tessellationShader = VK_FALSE,
            .sampleRateShading = VK_FALSE,
            .dualSrcBlend = VK_FALSE,
            .logicOp = VK_FALSE,
            .multiDrawIndirect = VK_FALSE,
            .drawIndirectFirstInstance = VK_FALSE,
            .depthClamp = VK_FALSE,
            .depthBiasClamp = VK_FALSE,
            .fillModeNonSolid = VK_FALSE,
            .depthBounds = VK_FALSE,
            .wideLines = VK_FALSE,
            .largePoints = VK_FALSE,
            .alphaToOne = VK_FALSE,
            .multiViewport = VK_FALSE,
            .samplerAnisotropy = VK_TRUE,
            .textureCompressionETC2 = VK_FALSE,
            .textureCompressionASTC_LDR = VK_FALSE,
            .textureCompressionBC = VK_FALSE,
            .occlusionQueryPrecise = VK_FALSE,
            .pipelineStatisticsQuery = VK_FALSE,
            .vertexPipelineStoresAndAtomics = VK_FALSE,
            .fragmentStoresAndAtomics = VK_FALSE,
            .shaderTessellationAndGeometryPointSize = VK_FALSE,
            .shaderImageGatherExtended = VK_FALSE,
            .shaderStorageImageExtendedFormats = VK_FALSE,
            .shaderStorageImageMultisample = VK_FALSE,
            .shaderStorageImageReadWithoutFormat = VK_FALSE,
            .shaderStorageImageWriteWithoutFormat = VK_FALSE,
            .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
            .shaderSampledImageArrayDynamicIndexing = VK_FALSE,
            .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
            .shaderStorageImageArrayDynamicIndexing = VK_FALSE,
            .shaderClipDistance = VK_FALSE,
            .shaderCullDistance = VK_FALSE,
            .shaderFloat64 = VK_FALSE,
            .shaderInt64 = VK_FALSE,
            .shaderInt16 = VK_FALSE,
            .shaderResourceResidency = VK_FALSE,
            .shaderResourceMinLod = VK_FALSE,
            .sparseBinding = VK_FALSE,
            .sparseResidencyBuffer = VK_FALSE,
            .sparseResidencyImage2D = VK_FALSE,
            .sparseResidencyImage3D = VK_FALSE,
            .sparseResidency2Samples = VK_FALSE,
            .sparseResidency4Samples = VK_FALSE,
            .sparseResidency8Samples = VK_FALSE,
            .sparseResidency16Samples = VK_FALSE,
            .sparseResidencyAliased = VK_FALSE,
            .variableMultisampleRate = VK_FALSE,
            .inheritedQueries = VK_FALSE
        }
    };

    if (rt) {
        VkPhysicalDeviceVulkan13Features enabledVulkan13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = nullptr,
            .robustImageAccess = VK_FALSE,
            .inlineUniformBlock = VK_FALSE,
            .descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE,
            .pipelineCreationCacheControl = VK_FALSE,
            .privateData = VK_FALSE,
            .shaderDemoteToHelperInvocation = VK_FALSE,
            .shaderTerminateInvocation = VK_FALSE,
            .subgroupSizeControl = VK_FALSE,
            .computeFullSubgroups = VK_FALSE,
            .synchronization2 = VK_FALSE,
            .textureCompressionASTC_HDR = VK_FALSE,
            .shaderZeroInitializeWorkgroupMemory = VK_FALSE,
            .dynamicRendering = VK_FALSE,
            .shaderIntegerDotProduct = VK_FALSE,
            .maintenance4 = VK_FALSE
        };
        VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferAddr = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = &enabledVulkan13,
            .bufferDeviceAddress = VK_TRUE,
            .bufferDeviceAddressCaptureReplay = VK_FALSE,
            .bufferDeviceAddressMultiDevice = VK_FALSE
        };
        VkPhysicalDeviceDescriptorIndexingFeatures enabledDescIdx = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
            .pNext = &enabledBufferAddr,
            .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
            .shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
            .shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
            .shaderUniformBufferArrayNonUniformIndexing = VK_FALSE,
            .shaderSampledImageArrayNonUniformIndexing = VK_FALSE,
            .shaderStorageBufferArrayNonUniformIndexing = VK_FALSE,
            .shaderStorageImageArrayNonUniformIndexing = VK_FALSE,
            .shaderInputAttachmentArrayNonUniformIndexing = VK_FALSE,
            .shaderUniformTexelBufferArrayNonUniformIndexing = VK_FALSE,
            .shaderStorageTexelBufferArrayNonUniformIndexing = VK_FALSE,
            .descriptorBindingUniformBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_FALSE,
            .descriptorBindingStorageImageUpdateAfterBind = VK_FALSE,
            .descriptorBindingStorageBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .descriptorBindingVariableDescriptorCount = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE
        };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccel = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &enabledDescIdx,
            .accelerationStructure = VK_TRUE,
            .accelerationStructureCaptureReplay = VK_FALSE,
            .accelerationStructureIndirectBuild = VK_TRUE,
            .accelerationStructureHostCommands = VK_FALSE,
            .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
        };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTrace = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &enabledAccel,
            .rayTracingPipeline = VK_TRUE,
            .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
            .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
            .rayTracingPipelineTraceRaysIndirect = VK_TRUE,
            .rayTraversalPrimitiveCulling = VK_TRUE
        };
        enabledFeatures.pNext = &enabledRayTrace;
        deviceCreateInfo.pNext = &enabledFeatures;
        deviceCreateInfo.pEnabledFeatures = nullptr;
    } else {
        VkPhysicalDeviceFeatures deviceFeatures = {
            .robustBufferAccess = VK_FALSE,
            .fullDrawIndexUint32 = VK_FALSE,
            .imageCubeArray = VK_FALSE,
            .independentBlend = VK_FALSE,
            .geometryShader = VK_FALSE,
            .tessellationShader = VK_FALSE,
            .sampleRateShading = VK_FALSE,
            .dualSrcBlend = VK_FALSE,
            .logicOp = VK_FALSE,
            .multiDrawIndirect = VK_FALSE,
            .drawIndirectFirstInstance = VK_FALSE,
            .depthClamp = VK_FALSE,
            .depthBiasClamp = VK_FALSE,
            .fillModeNonSolid = VK_FALSE,
            .depthBounds = VK_FALSE,
            .wideLines = VK_FALSE,
            .largePoints = VK_FALSE,
            .alphaToOne = VK_FALSE,
            .multiViewport = VK_FALSE,
            .samplerAnisotropy = VK_TRUE,
            .textureCompressionETC2 = VK_FALSE,
            .textureCompressionASTC_LDR = VK_FALSE,
            .textureCompressionBC = VK_FALSE,
            .occlusionQueryPrecise = VK_FALSE,
            .pipelineStatisticsQuery = VK_FALSE,
            .vertexPipelineStoresAndAtomics = VK_FALSE,
            .fragmentStoresAndAtomics = VK_FALSE,
            .shaderTessellationAndGeometryPointSize = VK_FALSE,
            .shaderImageGatherExtended = VK_FALSE,
            .shaderStorageImageExtendedFormats = VK_FALSE,
            .shaderStorageImageMultisample = VK_FALSE,
            .shaderStorageImageReadWithoutFormat = VK_FALSE,
            .shaderStorageImageWriteWithoutFormat = VK_FALSE,
            .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
            .shaderSampledImageArrayDynamicIndexing = VK_FALSE,
            .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
            .shaderStorageImageArrayDynamicIndexing = VK_FALSE,
            .shaderClipDistance = VK_FALSE,
            .shaderCullDistance = VK_FALSE,
            .shaderFloat64 = VK_FALSE,
            .shaderInt64 = VK_FALSE,
            .shaderInt16 = VK_FALSE,
            .shaderResourceResidency = VK_FALSE,
            .shaderResourceMinLod = VK_FALSE,
            .sparseBinding = VK_FALSE,
            .sparseResidencyBuffer = VK_FALSE,
            .sparseResidencyImage2D = VK_FALSE,
            .sparseResidencyImage3D = VK_FALSE,
            .sparseResidency2Samples = VK_FALSE,
            .sparseResidency4Samples = VK_FALSE,
            .sparseResidency8Samples = VK_FALSE,
            .sparseResidency16Samples = VK_FALSE,
            .sparseResidencyAliased = VK_FALSE,
            .variableMultisampleRate = VK_FALSE,
            .inheritedQueries = VK_FALSE
        };
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        deviceCreateInfo.pNext = nullptr;
    }

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        LOG_ERROR("Vulkan", "Failed to create logical device", loc);
        throw std::runtime_error("Failed to create logical device");
    }

    LOG_INFO("Vulkan", "Created logical device: {:p}", static_cast<void*>(device), loc);
}

VkInstance getVkInstance(const VulkanInstancePtr& instance) {
    return instance.get();
}

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface) {
    return surface.get();
}

std::vector<std::string> getVulkanExtensions() {
    Uint32 extensionCount = 0;
    const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensionNames) {
        LOG_ERROR("Vulkan", "Failed to get Vulkan extensions: {}", SDL_GetError(), std::source_location::current());
        throw std::runtime_error("Failed to get Vulkan extensions");
    }
    std::vector<std::string> result(extensionCount);
    for (Uint32 i = 0; i < extensionCount; ++i) {
        result[i] = extensionNames[i];
    }
    return result;
}

} // namespace SDL3Initializer