// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// Vulkan initialization utilities implementation.
// Dependencies: Vulkan 1.3+, VulkanCore.hpp, VulkanRTX_Setup.hpp, logging.hpp, SDL3/SDL_vulkan.h.
// Supported platforms: Linux, Windows.
// Optimized for high-end GPUs (NVIDIA RTX 30/40-series, AMD RX 7900 XTX).
// Zachary Geurts 2025

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

namespace VulkanInitializer {

// Ray tracing and buffer device address function pointers
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
    LOG_DEBUG_CAT("VulkanInitializer", "vkCmdTraceRaysKHR address: {:p}", reinterpret_cast<void*>(vkCmdTraceRaysKHR));
    LOG_DEBUG_CAT("VulkanInitializer", "vkGetBufferDeviceAddressKHR address: {:p}", reinterpret_cast<void*>(vkGetBufferDeviceAddressKHR));
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
    LOG_DEBUG_CAT("VulkanInitializer", "Initializing Vulkan instance with {} extensions", instanceExtensions.size());

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
#endif

    if (vkCreateInstance(&createInfo, nullptr, &context.instance) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create Vulkan instance");
        throw std::runtime_error("Failed to create Vulkan instance");
    }
    LOG_INFO_CAT("VulkanInitializer", "Created Vulkan instance: {:p}", static_cast<void*>(context.instance));
}

void initSurface(Vulkan::Context& context, void* window, VkSurfaceKHR* rawsurface) {
    LOG_DEBUG_CAT("VulkanInitializer", "Initializing Vulkan surface");
    if (rawsurface && *rawsurface != VK_NULL_HANDLE) {
        context.surface = *rawsurface;
        LOG_INFO_CAT("VulkanInitializer", "Using provided raw surface: {:p}", static_cast<void*>(context.surface));
        return;
    }

    if (!window) {
        LOG_ERROR_CAT("VulkanInitializer", "Window pointer is null and no raw surface provided");
        throw std::runtime_error("Window pointer is null and no raw surface provided");
    }

    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(sdlWindow, context.instance, nullptr, &surface)) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create Vulkan surface: {}", SDL_GetError());
        throw std::runtime_error("Failed to create Vulkan surface");
    }
    context.surface = surface;
    LOG_INFO_CAT("VulkanInitializer", "Created Vulkan surface: {:p}", static_cast<void*>(surface));
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
        LOG_ERROR_CAT("VulkanInitializer", "Failed to find suitable queue families: graphics={}, compute={}, present={}",
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

    VkPhysicalDeviceVulkan12Features vulkan12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = nullptr,
        .samplerMirrorClampToEdge = VK_FALSE,
        .drawIndirectCount = VK_FALSE,
        .storageBuffer8BitAccess = VK_FALSE,
        .uniformAndStorageBuffer8BitAccess = VK_FALSE,
        .storagePushConstant8 = VK_FALSE,
        .shaderBufferInt64Atomics = VK_FALSE,
        .shaderSharedInt64Atomics = VK_FALSE,
        .shaderFloat16 = VK_FALSE,
        .shaderInt8 = VK_FALSE,
        .descriptorIndexing = VK_TRUE,
        .shaderInputAttachmentArrayDynamicIndexing = VK_FALSE,
        .shaderUniformTexelBufferArrayDynamicIndexing = VK_FALSE,
        .shaderStorageTexelBufferArrayDynamicIndexing = VK_FALSE,
        .shaderUniformBufferArrayNonUniformIndexing = VK_FALSE,
        .shaderSampledImageArrayNonUniformIndexing = VK_FALSE,
        .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
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
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .samplerFilterMinmax = VK_FALSE,
        .scalarBlockLayout = VK_FALSE,
        .imagelessFramebuffer = VK_FALSE,
        .uniformBufferStandardLayout = VK_FALSE,
        .shaderSubgroupExtendedTypes = VK_FALSE,
        .separateDepthStencilLayouts = VK_FALSE,
        .hostQueryReset = VK_FALSE,
        .timelineSemaphore = VK_FALSE,
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_FALSE,
        .bufferDeviceAddressMultiDevice = VK_FALSE,
        .vulkanMemoryModel = VK_FALSE,
        .vulkanMemoryModelDeviceScope = VK_FALSE,
        .vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE,
        .shaderOutputViewportIndex = VK_FALSE,
        .shaderOutputLayer = VK_FALSE,
        .subgroupBroadcastDynamicId = VK_FALSE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &vulkan12Features,
        .rayTracingPipeline = VK_TRUE,
        .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
        .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
        .rayTracingPipelineTraceRaysIndirect = VK_TRUE,
        .rayTraversalPrimitiveCulling = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructureFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &rayTracingFeatures,
        .accelerationStructure = VK_TRUE,
        .accelerationStructureCaptureReplay = VK_FALSE,
        .accelerationStructureIndirectBuild = VK_FALSE,
        .accelerationStructureHostCommands = VK_FALSE,
        .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
    };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .pNext = &accelStructureFeatures,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
        .pNext = &dynamicRenderingFeatures,
        .pipelineFragmentShadingRate = VK_FALSE,
        .primitiveFragmentShadingRate = VK_TRUE,
        .attachmentFragmentShadingRate = VK_FALSE
    };

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .pNext = &fragmentShadingFeatures,
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
        .multiviewMeshShader = VK_FALSE,
        .primitiveFragmentShadingRateMeshShader = VK_FALSE,
        .meshShaderQueries = VK_FALSE
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &meshShaderFeatures,
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures
    };

    if (vkCreateDevice(context.physicalDevice, &deviceCreateInfo, nullptr, &context.device) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create Vulkan device");
        throw std::runtime_error("Failed to create Vulkan device");
    }
    context.resourceManager.setDevice(context.device);
    LOG_INFO_CAT("VulkanInitializer", "Created Vulkan device: {:p}", static_cast<void*>(context.device));

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
    LOG_INFO_CAT("VulkanInitializer", "Selected physical device: {}", selectedDeviceName);
    return selectedDevice;
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory,
                  const VkMemoryAllocateFlagsInfo* allocFlagsInfo, VulkanResourceManager& resourceManager) {
    if (device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid device or physical device: device={:p}, physicalDevice={:p}",
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

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create buffer with size={}, usage={:x}", size, static_cast<uint32_t>(usage));
        throw std::runtime_error("Failed to create buffer");
    }
    resourceManager.addBuffer(buffer);
    LOG_INFO_CAT("VulkanInitializer", "Created buffer: {:p}", static_cast<void*>(buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
    VkDeviceSize alignedSize = (memRequirements.size + deviceProps.limits.minMemoryMapAlignment - 1) &
                              ~(deviceProps.limits.minMemoryMapAlignment - 1);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = allocFlagsInfo,
        .allocationSize = alignedSize,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate buffer memory for buffer={:p}, size={}",
                  static_cast<void*>(buffer), alignedSize);
        Dispose::destroySingleBuffer(device, buffer);
        throw std::runtime_error("Failed to allocate buffer memory");
    }
    resourceManager.addMemory(bufferMemory);

    if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to bind buffer memory for buffer={:p}, memory={:p}",
                  static_cast<void*>(buffer), static_cast<void*>(bufferMemory));
        Dispose::destroySingleBuffer(device, buffer);
        Dispose::freeSingleDeviceMemory(device, bufferMemory);
        throw std::runtime_error("Failed to bind buffer memory");
    }
    LOG_INFO_CAT("VulkanInitializer", "Allocated and bound buffer memory: {:p} for buffer: {:p}, alignedSize={}",
             static_cast<void*>(bufferMemory), static_cast<void*>(buffer), alignedSize);
}

VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    if (device == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid device or buffer for getBufferDeviceAddress: device={:p}, buffer={:p}",
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
        LOG_ERROR_CAT("VulkanInitializer", "Failed to get buffer device address: buffer={:p}", static_cast<void*>(buffer));
        throw std::runtime_error("Failed to get buffer device address");
    }
    LOG_DEBUG_CAT("VulkanInitializer", "Retrieved device address 0x{:x} for buffer {:p}", address, static_cast<void*>(buffer));
    return address;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            LOG_INFO_CAT("VulkanInitializer", "Selected memory type index: {} for properties: {}", i, properties);
            return i;
        }
    }
    LOG_ERROR_CAT("VulkanInitializer", "Failed to find suitable memory type for typeFilter={:x}, properties={}",
              typeFilter, properties);
    throw std::runtime_error("Failed to find suitable memory type");
}

void createDescriptorSetLayout(VkDevice device, VkPhysicalDevice physicalDevice,
                              VkDescriptorSetLayout& rayTracingLayout, VkDescriptorSetLayout& graphicsLayout) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    uint32_t maxDescriptorCount = deviceProperties.limits.maxPerStageDescriptorStorageBuffers;
    uint32_t descriptorCount = std::min(4096u, maxDescriptorCount);
    LOG_INFO_CAT("VulkanInitializer", "Selected descriptor count: {} (max allowed: {}) for storage buffer bindings",
             descriptorCount, maxDescriptorCount);

    // Ray-tracing descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> rayTracingBindings = {
        {
            .binding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::TLAS),
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::StorageImage),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::CameraUBO),
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::MaterialSSBO),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptorCount,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::DimensionDataSSBO),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptorCount,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::DenoiseImage),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::EnvMap),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
            .pImmutableSamplers = nullptr
        }
    };

    VkDescriptorSetLayoutCreateInfo rayTracingLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
        .bindingCount = static_cast<uint32_t>(rayTracingBindings.size()),
        .pBindings = rayTracingBindings.data()
    };

    if (vkCreateDescriptorSetLayout(device, &rayTracingLayoutInfo, nullptr, &rayTracingLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create ray-tracing descriptor set layout");
        throw std::runtime_error("Failed to create ray-tracing descriptor set layout");
    }
    LOG_INFO_CAT("VulkanInitializer", "Created ray-tracing descriptor set layout with {} bindings: {:p}",
             rayTracingBindings.size(), static_cast<void*>(rayTracingLayout));

    // Graphics descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> graphicsBindings = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        }
    };

    VkDescriptorSetLayoutCreateInfo graphicsLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
        .bindingCount = static_cast<uint32_t>(graphicsBindings.size()),
        .pBindings = graphicsBindings.data()
    };

    if (vkCreateDescriptorSetLayout(device, &graphicsLayoutInfo, nullptr, &graphicsLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create graphics descriptor set layout");
        vkDestroyDescriptorSetLayout(device, rayTracingLayout, nullptr);
        throw std::runtime_error("Failed to create graphics descriptor set layout");
    }
    LOG_INFO_CAT("VulkanInitializer", "Created graphics descriptor set layout with {} bindings: {:p}",
             graphicsBindings.size(), static_cast<void*>(graphicsLayout));
}

void initializeVulkan(Vulkan::Context& context) {
    if (context.instance == VK_NULL_HANDLE || context.surface == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid instance or surface: instance={:p}, surface={:p}",
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
    if (vkCreateCommandPool(context.device, &commandPoolInfo, nullptr, &context.commandPool) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create command pool for queue family {}",
                  context.graphicsQueueFamilyIndex);
        Dispose::destroyDevice(context.device);
        throw std::runtime_error("Failed to create command pool");
    }
    context.resourceManager.addCommandPool(context.commandPool);
    LOG_INFO_CAT("VulkanInitializer", "Created command pool: {:p}", static_cast<void*>(context.commandPool));

    // Create descriptor pool
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(context.physicalDevice, &deviceProperties);
    uint32_t maxDescriptorCount = deviceProperties.limits.maxPerStageDescriptorStorageBuffers;
    uint32_t descriptorCount = std::min(4096u, maxDescriptorCount);
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 16;
    uint32_t maxSets = context.enableRayTracing ? MAX_FRAMES_IN_FLIGHT * 2 : MAX_FRAMES_IN_FLIGHT;
    std::vector<VkDescriptorPoolSize> poolSizes = context.enableRayTracing ? std::vector<VkDescriptorPoolSize>{
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * descriptorCount * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT }
    } : std::vector<VkDescriptorPoolSize>{
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &context.descriptorPool) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create descriptor pool");
        Dispose::destroySingleCommandPool(context.device, context.commandPool);
        Dispose::destroyDevice(context.device);
        throw std::runtime_error("Failed to create descriptor pool");
    }
    context.resourceManager.addDescriptorPool(context.descriptorPool);
    LOG_INFO_CAT("VulkanInitializer", "Created descriptor pool: {:p} with maxSets={}", static_cast<void*>(context.descriptorPool), maxSets);

    createDescriptorSetLayout(context.device, context.physicalDevice, context.rayTracingDescriptorSetLayout, context.graphicsDescriptorSetLayout);
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &context.memoryProperties);
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

    if (vkCreateImage(device, &imageInfo, nullptr, &storageImage) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create storage image");
        throw std::runtime_error("Failed to create storage image");
    }
    resourceManager.addImage(storageImage);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, storageImage, &memRequirements);

    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
    VkDeviceSize alignedSize = (memRequirements.size + deviceProps.limits.minMemoryMapAlignment - 1) &
                              ~(deviceProps.limits.minMemoryMapAlignment - 1);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = alignedSize,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    if (vkAllocateMemory(device, &allocInfo, nullptr, &storageImageMemory) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate storage image memory");
        Dispose::destroySingleImage(device, storageImage);
        throw std::runtime_error("Failed to allocate storage image memory");
    }
    resourceManager.addMemory(storageImageMemory);

    if (vkBindImageMemory(device, storageImage, storageImageMemory, 0) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to bind storage image memory");
        Dispose::destroySingleImage(device, storageImage);
        Dispose::freeSingleDeviceMemory(device, storageImageMemory);
        throw std::runtime_error("Failed to bind storage image memory");
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

    if (vkCreateImageView(device, &viewInfo, nullptr, &storageImageView) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create storage image view");
        Dispose::destroySingleImage(device, storageImage);
        Dispose::freeSingleDeviceMemory(device, storageImageMemory);
        throw std::runtime_error("Failed to create storage image view");
    }
    resourceManager.addImageView(storageImageView);
    LOG_INFO_CAT("VulkanInitializer", "Created storage image {:p}, memory {:p}, view {:p}",
             static_cast<void*>(storageImage), static_cast<void*>(storageImageMemory), static_cast<void*>(storageImageView));
}

void createShaderBindingTable(Vulkan::Context& context) {
    if (!context.enableRayTracing) {
        LOG_INFO_CAT("VulkanInitializer", "Ray tracing disabled, skipping SBT creation");
        return;
    }
    if (context.rayTracingPipeline == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Ray tracing pipeline is null");
        throw std::runtime_error("Ray tracing pipeline is null");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        .pNext = nullptr
    };
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties
    };
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &properties);

    const uint32_t groupCount = 3; // Raygen, Miss, Hit
    const uint32_t sbtRecordSize = rtProperties.shaderGroupHandleSize;
    const uint32_t alignedSize = (sbtRecordSize + rtProperties.shaderGroupBaseAlignment - 1) & ~(rtProperties.shaderGroupBaseAlignment - 1);
    const VkDeviceSize sbtSize = alignedSize * groupCount;
    context.sbtRecordSize = alignedSize;

    VkBuffer raygenBuffer, missBuffer, hitBuffer;
    VkDeviceMemory raygenMemory, missMemory, hitMemory;
    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };

    createBuffer(context.device, context.physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 raygenBuffer, raygenMemory, &allocFlagsInfo, context.resourceManager);
    createBuffer(context.device, context.physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 missBuffer, missMemory, &allocFlagsInfo, context.resourceManager);
    createBuffer(context.device, context.physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 hitBuffer, hitMemory, &allocFlagsInfo, context.resourceManager);

    std::vector<uint8_t> shaderHandles(groupCount * sbtRecordSize);
    if (vkGetRayTracingShaderGroupHandlesKHR(context.device, context.rayTracingPipeline, 0, groupCount,
                                             groupCount * sbtRecordSize, shaderHandles.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to get shader group handles for pipeline {:p}",
                  static_cast<void*>(context.rayTracingPipeline));
        context.resourceManager.cleanup(context.device);
        throw std::runtime_error("Failed to get shader group handles");
    }

    void* data;
    if (vkMapMemory(context.device, raygenMemory, 0, alignedSize, 0, &data) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to map raygen SBT memory");
        context.resourceManager.cleanup(context.device);
        throw std::runtime_error("Failed to map raygen SBT memory");
    }
    memcpy(data, shaderHandles.data(), sbtRecordSize);
    vkUnmapMemory(context.device, raygenMemory);

    if (vkMapMemory(context.device, missMemory, 0, alignedSize, 0, &data) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to map miss SBT memory");
        context.resourceManager.cleanup(context.device);
        throw std::runtime_error("Failed to map miss SBT memory");
    }
    memcpy(data, shaderHandles.data() + sbtRecordSize, sbtRecordSize);
    vkUnmapMemory(context.device, missMemory);

    if (vkMapMemory(context.device, hitMemory, 0, alignedSize, 0, &data) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to map hit SBT memory");
        context.resourceManager.cleanup(context.device);
        throw std::runtime_error("Failed to map hit SBT memory");
    }
    memcpy(data, shaderHandles.data() + 2 * sbtRecordSize, sbtRecordSize);
    vkUnmapMemory(context.device, hitMemory);

    context.raygenSbtBuffer = raygenBuffer;
    context.missSbtBuffer = missBuffer;
    context.hitSbtBuffer = hitBuffer;
    context.raygenSbtMemory = raygenMemory;
    context.missSbtMemory = missMemory;
    context.hitSbtMemory = hitMemory;
    context.raygenSbtAddress = getBufferDeviceAddress(context.device, raygenBuffer);
    context.missSbtAddress = getBufferDeviceAddress(context.device, missBuffer);
    context.hitSbtAddress = getBufferDeviceAddress(context.device, hitBuffer);
    LOG_INFO_CAT("VulkanInitializer", "Created shader binding table with raygen: {:p}, miss: {:p}, hit: {:p}",
             static_cast<void*>(raygenBuffer), static_cast<void*>(missBuffer), static_cast<void*>(hitBuffer));
}

void createAccelerationStructures(Vulkan::Context& context, VulkanBufferManager& bufferManager,
                                 std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    if (!context.enableRayTracing) {
        LOG_INFO_CAT("VulkanInitializer", "Ray tracing disabled, skipping acceleration structure creation");
        return;
    }
    if (context.device == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid Vulkan device");
        throw std::runtime_error("Invalid Vulkan device");
    }
    if (vertices.empty() || indices.empty()) {
        LOG_ERROR_CAT("VulkanInitializer", "Empty vertex or index data provided");
        throw std::runtime_error("Vertex or index data cannot be empty");
    }

    vkDeviceWaitIdle(context.device);

    VkBuffer vertexBuffer = bufferManager.getVertexBuffer();
    VkBuffer indexBuffer = bufferManager.getIndexBuffer();
    if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid vertex or index buffer: vertexBuffer={:p}, indexBuffer={:p}",
                  static_cast<void*>(vertexBuffer), static_cast<void*>(indexBuffer));
        throw std::runtime_error("Invalid vertex or index buffer");
    }

    VkDeviceAddress vertexAddress = bufferManager.getVertexBufferAddress();
    VkDeviceAddress indexAddress = bufferManager.getIndexBufferAddress();
    if (vertexAddress == 0 || indexAddress == 0) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid vertex or index buffer address: vertexAddress=0x{:x}, indexAddress=0x{:x}",
                  vertexAddress, indexAddress);
        throw std::runtime_error("Invalid vertex or index buffer address");
    }

    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = nullptr
    };
    VkPhysicalDeviceProperties2 properties2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &accelProps
    };
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &properties2);

    // BLAS Geometry
    VkAccelerationStructureGeometryTrianglesDataKHR triangleData = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .pNext = nullptr,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertexAddress },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = static_cast<uint32_t>(vertices.size() - 1),
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = indexAddress },
        .transformData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangleData },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr
    };
    const uint32_t primitiveCount = static_cast<uint32_t>(indices.size() / 3);
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildGeometryInfo, &primitiveCount, &buildSizesInfo);

    // Create BLAS buffer
    VkBuffer blasBuffer;
    VkDeviceMemory blasMemory;
    createBuffer(context.device, context.physicalDevice, buildSizesInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer, blasMemory, nullptr, context.resourceManager);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = blasBuffer,
        .offset = 0,
        .size = buildSizesInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .deviceAddress = 0
    };
    if (vkCreateAccelerationStructureKHR(context.device, &blasCreateInfo, nullptr, &context.bottomLevelAS) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create BLAS");
        context.resourceManager.cleanup(context.device);
        throw std::runtime_error("Failed to create BLAS");
    }
    context.resourceManager.addAccelerationStructure(context.bottomLevelAS);
    context.bottomLevelASBuffer = blasBuffer;
    context.bottomLevelASMemory = blasMemory;
    LOG_INFO_CAT("VulkanInitializer", "Created BLAS: {:p}, buffer: {:p}, memory: {:p}",
             static_cast<void*>(context.bottomLevelAS), static_cast<void*>(blasBuffer), static_cast<void*>(blasMemory));

    // Create scratch buffer for BLAS
    VkBuffer blasScratchBuffer;
    VkDeviceMemory blasScratchMemory;
    createBuffer(context.device, context.physicalDevice, buildSizesInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasScratchBuffer, blasScratchMemory, nullptr, context.resourceManager);
    VkDeviceAddress blasScratchAddress = getBufferDeviceAddress(context.device, blasScratchBuffer);
    buildGeometryInfo.dstAccelerationStructure = context.bottomLevelAS;
    buildGeometryInfo.scratchData.deviceAddress = blasScratchAddress;

    // Build BLAS
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(context);
    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos[] = { &buildRangeInfo };
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, buildRangeInfos);
    endSingleTimeCommands(context, commandBuffer);
    context.resourceManager.removeBuffer(blasScratchBuffer);
    context.resourceManager.removeMemory(blasScratchMemory);
    Dispose::destroySingleBuffer(context.device, blasScratchBuffer);
    Dispose::freeSingleDeviceMemory(context.device, blasScratchMemory);

    // TLAS Geometry
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };
    VkAccelerationStructureInstanceKHR instanceData = {
        .transform = transformMatrix,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = 0
    };
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructure = context.bottomLevelAS
    };
    instanceData.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(context.device, &addressInfo);
    if (instanceData.accelerationStructureReference == 0) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to get BLAS device address");
        context.resourceManager.cleanup(context.device);
        throw std::runtime_error("Failed to get BLAS device address");
    }

    // Create instance buffer
    VkBuffer instanceBuffer;
    VkDeviceMemory instanceMemory;
    createBuffer(context.device, context.physicalDevice, sizeof(VkAccelerationStructureInstanceKHR),
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory, nullptr, context.resourceManager);
    void* instanceDataPtr;
    if (vkMapMemory(context.device, instanceMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &instanceDataPtr) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to map instance buffer memory");
        context.resourceManager.cleanup(context.device);
        throw std::runtime_error("Failed to map instance buffer memory");
    }
    memcpy(instanceDataPtr, &instanceData, sizeof(VkAccelerationStructureInstanceKHR));
    vkUnmapMemory(context.device, instanceMemory);

    VkDeviceAddress instanceBufferAddress = getBufferDeviceAddress(context.device, instanceBuffer);
    VkAccelerationStructureGeometryInstancesDataKHR instancesData = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .pNext = nullptr,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = instanceBufferAddress }
    };

    VkAccelerationStructureGeometryKHR tlasGeometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData },
        .flags = 0
    };

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry,
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizes = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr
    };
    const uint32_t instanceCount = 1;
    vkGetAccelerationStructureBuildSizesKHR(context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &tlasBuildInfo, &instanceCount, &tlasBuildSizes);

    // Create TLAS buffer
    VkBuffer tlasBuffer;
    VkDeviceMemory tlasMemory;
    createBuffer(context.device, context.physicalDevice, tlasBuildSizes.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer, tlasMemory, nullptr, context.resourceManager);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = tlasBuffer,
        .offset = 0,
        .size = tlasBuildSizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0
    };
    if (vkCreateAccelerationStructureKHR(context.device, &tlasCreateInfo, nullptr, &context.topLevelAS) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create TLAS");
        context.resourceManager.cleanup(context.device);
        throw std::runtime_error("Failed to create TLAS");
    }
    context.resourceManager.addAccelerationStructure(context.topLevelAS);
    context.topLevelASBuffer = tlasBuffer;
    context.topLevelASMemory = tlasMemory;
    LOG_INFO_CAT("VulkanInitializer", "Created TLAS: {:p}, buffer: {:p}, memory: {:p}",
             static_cast<void*>(context.topLevelAS), static_cast<void*>(tlasBuffer), static_cast<void*>(tlasMemory));

    // Create scratch buffer for TLAS
    VkBuffer tlasScratchBuffer;
    VkDeviceMemory tlasScratchMemory;
    createBuffer(context.device, context.physicalDevice, tlasBuildSizes.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasScratchBuffer, tlasScratchMemory, nullptr, context.resourceManager);
    VkDeviceAddress tlasScratchAddress = getBufferDeviceAddress(context.device, tlasScratchBuffer);
    tlasBuildInfo.dstAccelerationStructure = context.topLevelAS;
    tlasBuildInfo.scratchData.deviceAddress = tlasScratchAddress;

    // Build TLAS
    commandBuffer = beginSingleTimeCommands(context);
    VkAccelerationStructureBuildRangeInfoKHR tlasBuildRangeInfo = {
        .primitiveCount = instanceCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    VkAccelerationStructureBuildRangeInfoKHR* tlasBuildRangeInfos[] = { &tlasBuildRangeInfo };
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &tlasBuildInfo, tlasBuildRangeInfos);
    endSingleTimeCommands(context, commandBuffer);
    context.resourceManager.removeBuffer(tlasScratchBuffer);
    context.resourceManager.removeMemory(tlasScratchMemory);
    Dispose::destroySingleBuffer(context.device, tlasScratchBuffer);
    Dispose::freeSingleDeviceMemory(context.device, tlasScratchMemory);

    // Clean up instance buffer
    context.resourceManager.removeBuffer(instanceBuffer);
    context.resourceManager.removeMemory(instanceMemory);
    Dispose::destroySingleBuffer(context.device, instanceBuffer);
    Dispose::freeSingleDeviceMemory(context.device, instanceMemory);
    LOG_INFO_CAT("VulkanInitializer", "Acceleration structures created successfully");
}

void createDescriptorPoolAndSet(VkDevice device, VkPhysicalDevice physicalDevice, VkDescriptorSetLayout descriptorSetLayout,
                               VkDescriptorPool& descriptorPool, std::vector<VkDescriptorSet>& descriptorSets,
                               VkSampler& sampler, VkBuffer uniformBuffer, VkImageView storageImageView,
                               VkAccelerationStructureKHR topLevelAS, bool forRayTracing,
                               std::vector<VkBuffer> materialBuffers, std::vector<VkBuffer> dimensionBuffers,
                               VkImageView denoiseImageView) {
    if (device == VK_NULL_HANDLE || descriptorSetLayout == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid device or descriptor set layout: device={:p}, layout={:p}",
                  static_cast<void*>(device), static_cast<void*>(descriptorSetLayout));
        throw std::runtime_error("Invalid device or descriptor set layout");
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    uint32_t maxDescriptorCount = deviceProperties.limits.maxPerStageDescriptorStorageBuffers;
    uint32_t descriptorCount = std::min(4096u, maxDescriptorCount);
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 16;
    uint32_t maxSets = forRayTracing ? MAX_FRAMES_IN_FLIGHT * 2 : MAX_FRAMES_IN_FLIGHT;

    std::vector<VkDescriptorPoolSize> poolSizes = forRayTracing ? std::vector<VkDescriptorPoolSize>{
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * descriptorCount * MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT }
    } : std::vector<VkDescriptorPoolSize>{
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create descriptor pool");
        throw std::runtime_error("Failed to create descriptor pool");
    }
    LOG_INFO_CAT("VulkanInitializer", "Created descriptor pool: {:p}", static_cast<void*>(descriptorPool));

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()
    };
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate descriptor sets");
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        throw std::runtime_error("Failed to allocate descriptor sets");
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
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create sampler");
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        throw std::runtime_error("Failed to create sampler");
    }
    LOG_INFO_CAT("VulkanInitializer", "Created sampler: {:p}", static_cast<void*>(sampler));

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::vector<VkWriteDescriptorSet> descriptorWrites;
        VkDescriptorBufferInfo uniformBufferInfo = {
            .buffer = uniformBuffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
        VkDescriptorImageInfo imageInfo = {
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
        VkDescriptorImageInfo denoiseImageInfo = {
            .sampler = sampler,
            .imageView = denoiseImageView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        if (forRayTracing) {
            // TLAS descriptor
            VkWriteDescriptorSet tlasWrite = {};
            tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            tlasWrite.pNext = &asInfo;
            tlasWrite.dstSet = descriptorSets[i];
            tlasWrite.dstBinding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::TLAS);
            tlasWrite.dstArrayElement = 0;
            tlasWrite.descriptorCount = 1;
            tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            tlasWrite.pImageInfo = nullptr;
            tlasWrite.pBufferInfo = nullptr;
            tlasWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(tlasWrite);

            // Storage image descriptor
            VkWriteDescriptorSet storageImageWrite = {};
            storageImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            storageImageWrite.pNext = nullptr;
            storageImageWrite.dstSet = descriptorSets[i];
            storageImageWrite.dstBinding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::StorageImage);
            storageImageWrite.dstArrayElement = 0;
            storageImageWrite.descriptorCount = 1;
            storageImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            storageImageWrite.pImageInfo = &imageInfo;
            storageImageWrite.pBufferInfo = nullptr;
            storageImageWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(storageImageWrite);

            // Camera UBO descriptor
            VkWriteDescriptorSet cameraUboWrite = {};
            cameraUboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            cameraUboWrite.pNext = nullptr;
            cameraUboWrite.dstSet = descriptorSets[i];
            cameraUboWrite.dstBinding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::CameraUBO);
            cameraUboWrite.dstArrayElement = 0;
            cameraUboWrite.descriptorCount = 1;
            cameraUboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            cameraUboWrite.pImageInfo = nullptr;
            cameraUboWrite.pBufferInfo = &uniformBufferInfo;
            cameraUboWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(cameraUboWrite);

            // Material SSBO descriptor
            VkWriteDescriptorSet materialSsboWrite = {};
            materialSsboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            materialSsboWrite.pNext = nullptr;
            materialSsboWrite.dstSet = descriptorSets[i];
            materialSsboWrite.dstBinding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::MaterialSSBO);
            materialSsboWrite.dstArrayElement = 0;
            materialSsboWrite.descriptorCount = static_cast<uint32_t>(materialBufferInfos.size());
            materialSsboWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            materialSsboWrite.pImageInfo = nullptr;
            materialSsboWrite.pBufferInfo = materialBufferInfos.data();
            materialSsboWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(materialSsboWrite);

            // Dimension SSBO descriptor
            VkWriteDescriptorSet dimensionSsboWrite = {};
            dimensionSsboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            dimensionSsboWrite.pNext = nullptr;
            dimensionSsboWrite.dstSet = descriptorSets[i];
            dimensionSsboWrite.dstBinding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::DimensionDataSSBO);
            dimensionSsboWrite.dstArrayElement = 0;
            dimensionSsboWrite.descriptorCount = static_cast<uint32_t>(dimensionBufferInfos.size());
            dimensionSsboWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            dimensionSsboWrite.pImageInfo = nullptr;
            dimensionSsboWrite.pBufferInfo = dimensionBufferInfos.data();
            dimensionSsboWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(dimensionSsboWrite);

            // Denoise image descriptor
            VkWriteDescriptorSet denoiseImageWrite = {};
            denoiseImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            denoiseImageWrite.pNext = nullptr;
            denoiseImageWrite.dstSet = descriptorSets[i];
            denoiseImageWrite.dstBinding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::DenoiseImage);
            denoiseImageWrite.dstArrayElement = 0;
            denoiseImageWrite.descriptorCount = 1;
            denoiseImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            denoiseImageWrite.pImageInfo = &denoiseImageInfo;
            denoiseImageWrite.pBufferInfo = nullptr;
            denoiseImageWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(denoiseImageWrite);

            // Environment map descriptor
            VkWriteDescriptorSet envMapWrite = {};
            envMapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            envMapWrite.pNext = nullptr;
            envMapWrite.dstSet = descriptorSets[i];
            envMapWrite.dstBinding = static_cast<uint32_t>(VulkanRTX::DescriptorBindings::EnvMap);
            envMapWrite.dstArrayElement = 0;
            envMapWrite.descriptorCount = 1;
            envMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            envMapWrite.pImageInfo = &imageInfo;
            envMapWrite.pBufferInfo = nullptr;
            envMapWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(envMapWrite);
        } else {
            // Graphics combined image sampler descriptor
            VkWriteDescriptorSet imageSamplerWrite = {};
            imageSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            imageSamplerWrite.pNext = nullptr;
            imageSamplerWrite.dstSet = descriptorSets[i];
            imageSamplerWrite.dstBinding = 0;
            imageSamplerWrite.dstArrayElement = 0;
            imageSamplerWrite.descriptorCount = 1;
            imageSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            imageSamplerWrite.pImageInfo = &imageInfo;
            imageSamplerWrite.pBufferInfo = nullptr;
            imageSamplerWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(imageSamplerWrite);

            // Graphics uniform buffer descriptor
            VkWriteDescriptorSet uniformBufferWrite = {};
            uniformBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uniformBufferWrite.pNext = nullptr;
            uniformBufferWrite.dstSet = descriptorSets[i];
            uniformBufferWrite.dstBinding = 1;
            uniformBufferWrite.dstArrayElement = 0;
            uniformBufferWrite.descriptorCount = 1;
            uniformBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformBufferWrite.pImageInfo = nullptr;
            uniformBufferWrite.pBufferInfo = &uniformBufferInfo;
            uniformBufferWrite.pTexelBufferView = nullptr;
            descriptorWrites.push_back(uniformBufferWrite);
        }
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
    LOG_INFO_CAT("VulkanInitializer", "Created {} descriptor sets for {}", descriptorSets.size(), forRayTracing ? "ray tracing" : "graphics");
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
    if (vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate single-time command buffer");
        throw std::runtime_error("Failed to allocate single-time command buffer");
    }
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to begin single-time command buffer");
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to begin single-time command buffer");
    }
    LOG_DEBUG_CAT("VulkanInitializer", "Allocated and began single-time command buffer: {:p}", static_cast<void*>(commandBuffer));
    return commandBuffer;
}

void endSingleTimeCommands(Vulkan::Context& context, VkCommandBuffer commandBuffer) {
    if (commandBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid command buffer for endSingleTimeCommands");
        throw std::runtime_error("Invalid command buffer");
    }
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to end single-time command buffer: {:p}", static_cast<void*>(commandBuffer));
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to end single-time command buffer");
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
    if (vkCreateFence(context.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create fence for single-time command submission");
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to create fence");
    }

    if (vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to submit single-time command buffer to queue");
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        vkDestroyFence(context.device, fence, nullptr);
        throw std::runtime_error("Failed to submit single-time command buffer");
    }

    if (vkWaitForFences(context.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to wait for fence for single-time command buffer");
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        vkDestroyFence(context.device, fence, nullptr);
        throw std::runtime_error("Failed to wait for fence");
    }

    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
    vkDestroyFence(context.device, fence, nullptr);
    LOG_DEBUG_CAT("VulkanInitializer", "Submitted and freed single-time command buffer: {:p}", static_cast<void*>(commandBuffer));
}

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    if (device == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE ||
        srcBuffer == VK_NULL_HANDLE || dstBuffer == VK_NULL_HANDLE || size == 0) {
        LOG_ERROR_CAT("VulkanInitializer", "Invalid parameters for copyBuffer");
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
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to allocate command buffer for copyBuffer");
        throw std::runtime_error("Failed to allocate command buffer for copyBuffer");
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to begin command buffer for copyBuffer");
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to begin command buffer for copyBuffer");
    }

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to end command buffer for copyBuffer");
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to end command buffer for copyBuffer");
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
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to create fence for copyBuffer");
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to create fence for copyBuffer");
    }

    if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to submit command buffer for copyBuffer");
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to submit command buffer for copyBuffer");
    }

    if (vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanInitializer", "Failed to wait for fence in copyBuffer");
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("Failed to wait for fence in copyBuffer");
    }

    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    LOG_DEBUG_CAT("VulkanInitializer", "Buffer copy completed: src={:p}, dst={:p}, size={}",
             static_cast<void*>(srcBuffer), static_cast<void*>(dstBuffer), size);
}

} // namespace VulkanInitializer