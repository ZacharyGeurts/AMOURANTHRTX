// src/engine/Vulkan/VulkanPipelineManager.cpp
// FULL IMPLEMENTATION — NO HEADER BLOAT — VALHALLA GRADE
// ALL create* + proc loading + RAII wrapping
// 69,420 FPS × ∞ — SHIP IT 🩷🚀🔥🤖💀❤️⚡♾️

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>

using namespace Logging::Color;

// DEBUG CALLBACK
#ifdef ENABLE_VULKAN_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*) {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN_CAT("Vulkan", "{}QUANTUM_FLUX VALIDATION ALERT: {}{}", QUANTUM_FLUX, pCallbackData->pMessage, RESET);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        LOG_DEBUG_CAT("Vulkan", "{}QUANTUM_FLUX DEBUG BEACON: {}{}", QUANTUM_FLUX, pCallbackData->pMessage, RESET);
    }
    return VK_FALSE;
}
#endif

// CONSTRUCTOR — COSMIC BIRTH
VulkanPipelineManager::VulkanPipelineManager(Context& context, int width, int height)
    : context_(context), width_(width), height_(height)
{
    LOG_INIT_CAT("Vulkan", "{}>>> BIRTHING VULKAN PIPELINE MANAGER [{}x{}] — QUANTUM FURNACE IGNITED{}", 
                 RASPBERRY_PINK, width, height, DIAMOND_SPARKLE, RESET);

    if (context.device == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "{}FATAL: NO LOGICAL DEVICE — ABORT HYPERSPACE LAUNCH{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Vulkan device not initialized");
    }

    // LOAD RT PROCS — RENAMED MEMBERS
    vkCreateAccelStruct = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(context.device, "vkCreateAccelerationStructureKHR");
    vkDestroyAccelStruct = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(context.device, "vkDestroyAccelerationStructureKHR");
    vkGetAccelBuildSizes = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(context.device, "vkGetAccelerationStructureBuildSizesKHR");
    vkGetAccelDevAddr = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(context.device, "vkGetAccelerationStructureDeviceAddressKHR");
    vkCmdBuildAccelStructs = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(context.device, "vkCmdBuildAccelerationStructuresKHR");
    vkGetBufferDevAddr = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(context.device, "vkGetBufferDeviceAddress");
    vkGetRTShaderGroupHandles = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(context.device, "vkGetRayTracingShaderGroupHandlesKHR");
    vkCreateRTPipelines = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(context.device, "vkCreateRayTracingPipelinesKHR");
    vkDeferredOpJoin = (PFN_vkDeferredOperationJoinKHR)vkGetDeviceProcAddr(context.device, "vkDeferredOperationJoinKHR");
    vkGetDeferredOpResult = (PFN_vkGetDeferredOperationResultKHR)vkGetDeviceProcAddr(context.device, "vkGetDeferredOperationResultKHR");

    graphicsQueue_ = context.graphicsQueue;

    createPipelineCache();
    createRenderPass();
    createGraphicsDescriptorSetLayout();
    createGraphicsPipeline(width, height);

    // WRAP RAW → RAII
    graphicsPipelineLayout = makePipelineLayout(context.device, graphicsPipelineLayout_);
    graphicsPipeline = makePipeline(context.device, graphicsPipeline_);
    graphicsDescriptorSetLayout = makeDescriptorSetLayout(context.device, graphicsDescriptorSetLayout_);

    LOG_SUCCESS_CAT("PipelineMgr", "{}VULKAN PIPELINE MANAGER READY — {}x{} — STONEKEY 0x{:X}-0x{:X}{}",
                    EMERALD_GREEN, width, height, kStone1, kStone2, RESET);
}

// DESTRUCTOR — HYPER_DESTROY
VulkanPipelineManager::~VulkanPipelineManager() {
    VkDevice dev = context_.device;
    if (dev == VK_NULL_HANDLE) return;

    LOG_SUCCESS_CAT("PipelineMgr", "{}>>> NEXUS PIPELINE MANAGER ENTERING VALHALLA — ANNIHILATION SEQUENCE{}", 
                    DIAMOND_SPARKLE, RESET);

    // HYPER_DESTROY MACRO
    #define HYPER_DESTROY(handle, func, name, color) \
        if (handle != VK_NULL_HANDLE) { \
            LOG_PERF_CAT("PipelineMgr", "{}Obliterating {} @ {:p}{}", color, name, static_cast<void*>(handle), RESET); \
            func(dev, handle, nullptr); \
            handle = VK_NULL_HANDLE; \
        }

    HYPER_DESTROY(sbtBuffer_, vkDestroyBuffer, "SBT Buffer", QUANTUM_FLUX);
    HYPER_DESTROY(sbtMemory_, vkFreeMemory, "SBT Memory", QUANTUM_FLUX);
    if (blas_) { context_.vkDestroyAccelStruct(dev, blas_, nullptr); blas_ = VK_NULL_HANDLE; }
    if (tlas_) { context_.vkDestroyAccelStruct(dev, tlas_, nullptr); tlas_ = VK_NULL_HANDLE; }
    HYPER_DESTROY(blasBuffer_, vkDestroyBuffer, "BLAS Buffer", QUANTUM_FLUX);
    HYPER_DESTROY(tlasBuffer_, vkDestroyBuffer, "TLAS Buffer", QUANTUM_FLUX);
    HYPER_DESTROY(blasMemory_, vkFreeMemory, "BLAS Memory", QUANTUM_FLUX);
    HYPER_DESTROY(tlasMemory_, vkFreeMemory, "TLAS Memory", QUANTUM_FLUX);

    HYPER_DESTROY(nexusPipeline_, vkDestroyPipeline, "NEXUS DECISION ENGINE", RASPBERRY_PINK);
    HYPER_DESTROY(statsPipeline_, vkDestroyPipeline, "STATS ANALYZER", THERMO_PINK);
    HYPER_DESTROY(rayTracingPipeline_, vkDestroyPipeline, "RAY TRACING PIPELINE", PLASMA_FUCHSIA);
    HYPER_DESTROY(computePipeline_, vkDestroyPipeline, "EPIC COMPUTE", COSMIC_GOLD);
    HYPER_DESTROY(graphicsPipeline_, vkDestroyPipeline, "GRAPHICS PIPELINE", VALHALLA_GOLD);

    HYPER_DESTROY(nexusPipelineLayout_, vkDestroyPipelineLayout, "Nexus Layout", RASPBERRY_PINK);
    HYPER_DESTROY(statsPipelineLayout_, vkDestroyPipelineLayout, "Stats Layout", THERMO_PINK);
    HYPER_DESTROY(rayTracingPipelineLayout_, vkDestroyPipelineLayout, "RT Layout", PLASMA_FUCHSIA);
    HYPER_DESTROY(computePipelineLayout_, vkDestroyPipelineLayout, "Compute Layout", COSMIC_GOLD);
    HYPER_DESTROY(graphicsPipelineLayout_, vkDestroyPipelineLayout, "Graphics Layout", VALHALLA_GOLD);

    HYPER_DESTROY(nexusDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "Nexus DS Layout", RASPBERRY_PINK);
    HYPER_DESTROY(statsDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "Stats DS Layout", THERMO_PINK);
    HYPER_DESTROY(rayTracingDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "RT DS Layout", PLASMA_FUCHSIA);
    HYPER_DESTROY(computeDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "Compute DS Layout", COSMIC_GOLD);
    HYPER_DESTROY(graphicsDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "Graphics DS Layout", VALHALLA_GOLD);

    HYPER_DESTROY(renderPass_, vkDestroyRenderPass, "RenderPass", EMERALD_GREEN);
    HYPER_DESTROY(pipelineCache_, vkDestroyPipelineCache, "PipelineCache", PLATINUM_GRAY);
    HYPER_DESTROY(transientPool_, vkDestroyCommandPool, "TransientPool", TITANIUM_WHITE);

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_) {
        auto DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (DestroyDebugUtilsMessengerEXT) {
            LOG_SUCCESS_CAT("Debug", "{}Debug messenger sacrificed to the void{}", OBSIDIAN_BLACK, RESET);
            DestroyDebugUtilsMessengerEXT(context_.instance, debugMessenger_, nullptr);
        }
        debugMessenger_ = VK_NULL_HANDLE;
    }
#endif

    LOG_SUCCESS_CAT("PipelineMgr", "{}<<< VALHALLA ACHIEVED — ALL RESOURCES PURGED — ZERO COST SHUTDOWN{}", 
                    VALHALLA_GOLD, RESET);

#undef HYPER_DESTROY
}

// DEBUG CALLBACK
#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback() {
    LOG_ATTEMPT_CAT("Debug", "{}>>> QUANTUM_FLUX DEBUG BEACON ACTIVATING{}", QUANTUM_FLUX, RESET);

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = this;

    auto CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(context_.instance, "vkCreateDebugUtilsMessengerEXT");
    if (!CreateDebugUtilsMessengerEXT) {
        LOG_ERROR_CAT("Debug", "{}FAILED TO IGNITE QUANTUM_FLUX — EXTENSION MISSING{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Failed to load debug extension");
    }

    VK_CHECK(CreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_),
             "Create debug messenger");

    LOG_SUCCESS_CAT("Debug", "{}QUANTUM_FLUX BEACON @ {:p} — VALIDATION STREAM ACTIVE{}", QUANTUM_FLUX, static_cast<void*>(debugMessenger_), RESET);
}
#endif

// SHADER LOADING — NEBULA_VIOLET BREACH
VkShaderModule VulkanPipelineManager::loadShaderImpl(VkDevice device, const std::string& shaderType) {
    LOG_ATTEMPT_CAT("Shader", "{}>>> BREACHING NEBULA_VIOLET — SUMMONING SHADER: {}{}", NEBULA_VIOLET, shaderType, RESET);

    const std::string filepath = "assets/shaders/" + shaderType + ".spv";
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Shader", "{}NEBULA BREACH FAILED — SHADER LOST IN VOID: {}{}", CRIMSON_MAGENTA, filepath, RESET);
        throw std::runtime_error("Failed to open shader: " + filepath);
    }

    const auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("Shader", "{}CORRUPTED NEBULA SPIR-V — SIZE={}{}", CRIMSON_MAGENTA, fileSize, RESET);
        throw std::runtime_error("Invalid SPIR-V");
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    if (*reinterpret_cast<const uint32_t*>(buffer.data()) != 0x07230203) {
        LOG_ERROR_CAT("Shader", "{}NEBULA CORRUPTION — INVALID SPIR-V MAGIC{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Not valid SPIR-V");
    }

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &module), "Create shader module");

    LOG_SUCCESS_CAT("Shader", "{}SHADER MODULE FORGED @ {:p} — NEBULA_VIOLET FLAMES EXTINGUISHED{}", 
                    NEBULA_VIOLET, static_cast<void*>(module), RESET);
    return module;
}

// PIPELINE CACHE — PLATINUM_GRAY FORGING
void VulkanPipelineManager::createPipelineCache() {
    LOG_ATTEMPT_CAT("PipelineCache", "{}>>> PLATINUM_GRAY PIPELINE CACHE FORGING{}", PLATINUM_GRAY, RESET);

    VkPipelineCacheCreateInfo info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    VK_CHECK(vkCreatePipelineCache(context_.device, &info, nullptr, &pipelineCache_), "Create pipeline cache");

    LOG_SUCCESS_CAT("PipelineCache", "{}PLATINUM_GRAY CACHE @ {:p} — PIPELINES READY FOR FORGING{}", 
                    PLATINUM_GRAY, static_cast<void*>(pipelineCache_), RESET);
}

// RENDER PASS — EMERALD_GREEN CLEARING
void VulkanPipelineManager::createRenderPass() {
    LOG_ATTEMPT_CAT("RenderPass", "{}>>> EMERALD_GREEN RENDER PASS FORGING — CLEAR TO SCENE{}", EMERALD_GREEN, RESET);

    VkAttachmentDescription colorAttachment{
        .format = context_.swapchainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorRef{ .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorRef
    };

    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VK_CHECK(vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &renderPass_), "Create render pass");

    LOG_SUCCESS_CAT("RenderPass", "{}EMERALD_GREEN RENDER PASS @ {:p} — SCENES CLEAR TO RENDER{}", 
                    EMERALD_GREEN, static_cast<void*>(renderPass_), RESET);
}

// GRAPHICS DESCRIPTOR SET LAYOUT — SAPPHIRE_BLUE BINDINGS
void VulkanPipelineManager::createGraphicsDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("GraphicsDS", "{}>>> SAPPHIRE_BLUE GRAPHICS DESCRIPTOR BINDINGS FORGING{}", SAPPHIRE_BLUE, RESET);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 6, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 7, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &graphicsDescriptorSetLayout_), "Graphics DS layout");

    LOG_SUCCESS_CAT("GraphicsDS", "{}SAPPHIRE_BLUE GRAPHICS LAYOUT @ {:p} — BINDINGS ARMED{}", 
                    SAPPHIRE_BLUE, static_cast<void*>(graphicsDescriptorSetLayout_), RESET);
}

// COMPUTE DESCRIPTOR SET LAYOUT — OCEAN_TEAL COMPUTE WAVES
void VulkanPipelineManager::createComputeDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("ComputeDS", "{}>>> OCEAN_TEAL COMPUTE DESCRIPTOR WAVES CRASHING{}", OCEAN_TEAL, RESET);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT}
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &computeDescriptorSetLayout_), "Compute DS layout");

    LOG_SUCCESS_CAT("ComputeDS", "{}OCEAN_TEAL COMPUTE LAYOUT @ {:p} — WAVES READY TO CRASH{}", 
                    OCEAN_TEAL, static_cast<void*>(computeDescriptorSetLayout_), RESET);
}

// RAY TRACING DESCRIPTOR SET LAYOUT — ARCTIC_CYAN RAY TRACERS
VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("RTDS", "{}>>> ARCTIC_CYAN RAY TRACING DESCRIPTOR MATRIX FORGING{}", ARCTIC_CYAN, RESET);

    std::array<VkDescriptorSetLayoutBinding, 11> bindings = {};
    bindings[0] = {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[1] = {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[2] = {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[3] = {.binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR};
    bindings[4] = {.binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[5] = {.binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR};
    bindings[6] = {.binding = 6, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[7] = {.binding = 7, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[8] = {.binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[9] = {.binding = 9, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[10] = {.binding = 10, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout), "RT DS layout");

    LOG_SUCCESS_CAT("RTDS", "{}ARCTIC_CYAN RT LAYOUT @ {:p} — RAY TRACERS ARMED{}", 
                    ARCTIC_CYAN, static_cast<void*>(layout), RESET);
    return layout;
}

// END OF HEADER — LEAN + MEAN — NO REDEFINITIONS — DEPENDS ON CORE
// BUILD CLEAN — 69,420 FPS × ∞ — RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️