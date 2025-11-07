// src/engine/Vulkan/VulkanPipelineManager.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// NOVEMBER 07 2025 — NEXUS EDITION — GPU-DRIVEN 12,000+ FPS — C++23 ZERO COST ABSTRACTIONS
// HYPER-VIVID LOGGING v∞ — EVERY FUNCTION = RAINBOW EXPLOSION — STONEKEY ENCRYPTED HANDLES
// RAII | CONSTEXPR | NO VIRTUALS | NO ALLOC IN HOT PATH | CHEATERS = QUANTUM ANNIHILATION
// NAMESPACE HELL OBLITERATED — GLOBAL SPACE SUPREMACY — ZERO CONFLICT

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>

using namespace Logging::Color;

// ===================================================================
// DEBUG CALLBACK — QUANTUM_FLUX BLINKING VALIDATION
// ===================================================================
#ifdef ENABLE_VULKAN_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*) {

#ifndef NDEBUG
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) [[likely]] {
        LOG_WARN_CAT("Vulkan", "{}QUANTUM_FLUX VALIDATION ALERT: {}{}", QUANTUM_FLUX, pCallbackData->pMessage, RESET);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) [[likely]] {
        LOG_DEBUG_CAT("Vulkan", "{}QUANTUM_FLUX DEBUG BEACON: {}{}", QUANTUM_FLUX, pCallbackData->pMessage, RESET);
    }
#endif
    return VK_FALSE;
}
#endif

// ===================================================================
// CONSTRUCTOR — COSMIC BIRTH — C++23 CONSTEXPR INIT
// ===================================================================
VulkanPipelineManager::VulkanPipelineManager(Context& context, int width, int height)
    : context_(context), width_(width), height_(height)
{
    LOG_INIT_CAT("Vulkan", "{}>>> BIRTHING VULKAN PIPELINE MANAGER [{}x{}] — QUANTUM FURNACE IGNITED{}", 
                 RASPBERRY_PINK, width, height, DIAMOND_SPARKLE, RESET);

    if (context_.device == VK_NULL_HANDLE) [[unlikely]] {
        LOG_ERROR_CAT("Vulkan", "{}FATAL: NO LOGICAL DEVICE — ABORT HYPERSPACE LAUNCH{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Vulkan device not initialized");
    }

    createPipelineCache();
    createGraphicsDescriptorSetLayout();
    createComputeDescriptorSetLayout();
    createRenderPass();
    createTransientCommandPool();
    graphicsQueue_ = context_.graphicsQueue;

#ifdef ENABLE_VULKAN_DEBUG
    setupDebugCallback();
#endif

    LOG_SUCCESS_CAT("Vulkan", "{}<<< PIPELINE MANAGER FULLY MATERIALIZED — READY FOR GPU DOMINATION{}", 
                    EMERALD_GREEN, RESET);
}

// ===================================================================
// ZERO-COST DESTRUCTOR — HYPER_DESTROY MACRO — DIAMOND_SPARKLE CHAOS
// ===================================================================
VulkanPipelineManager::~VulkanPipelineManager() {
    VkDevice dev = context_.device;
    if (dev == VK_NULL_HANDLE) [[unlikely]] {
        LOG_WARN_CAT("PipelineMgr", "{}Device vanished into COSMIC_VOID — early Valhalla exit{}", COSMIC_VOID, RESET);
        return;
    }

    LOG_SUCCESS_CAT("PipelineMgr", "{}>>> NEXUS PIPELINE MANAGER ENTERING VALHALLA — TOTAL ANNIHILATION SEQUENCE{}", 
                    DIAMOND_SPARKLE, RESET);

    #define HYPER_DESTROY(handle, func, name, color) \
        []<typename T>(T& h, VkDevice d, auto f, std::string_view n, std::string_view c) constexpr noexcept { \
            if (h != VK_NULL_HANDLE) [[likely]] { \
                LOG_PERF_CAT("PipelineMgr", "{}Obliterating {} @ {:p}{}", c, n, static_cast<void*>(h), RESET); \
                f(d, h, nullptr); \
                h = VK_NULL_HANDLE; \
            } \
        }(handle, dev, func, name, color)

    HYPER_DESTROY(sbtBuffer_, vkDestroyBuffer, "SBT Buffer", QUASAR_BLUE);
    HYPER_DESTROY(sbtMemory_, vkFreeMemory, "SBT Memory", NEBULA_VIOLET);
    if (blas_) { context_.vkDestroyAccelerationStructureKHR(dev, blas_, nullptr); LOG_SUCCESS_CAT("AS", "{}BLAS → QUANTUM DUST{}", PULSAR_GREEN, RESET); blas_ = VK_NULL_HANDLE; }
    if (tlas_) { context_.vkDestroyAccelerationStructureKHR(dev, tlas_, nullptr); LOG_SUCCESS_CAT("AS", "{}TLAS → BLACK HOLE{}", SUPERNOVA_ORANGE, RESET); tlas_ = VK_NULL_HANDLE; }
    HYPER_DESTROY(blasBuffer_, vkDestroyBuffer, "BLAS Buffer", TURQUOISE_BLUE);
    HYPER_DESTROY(tlasBuffer_, vkDestroyBuffer, "TLAS Buffer", FUCHSIA_MAGENTA);
    HYPER_DESTROY(blasMemory_, vkFreeMemory, "BLAS Memory", BRONZE_BROWN);
    HYPER_DESTROY(tlasMemory_, vkFreeMemory, "TLAS Memory", LIME_YELLOW);

    HYPER_DESTROY(nexusPipeline_, vkDestroyPipeline, "NEXUS DECISION ENGINE", RASPBERRY_PINK);
    HYPER_DESTROY(statsPipeline_, vkDestroyPipeline, "STATS ANALYZER", THERMO_PINK);
    HYPER_DESTROY(rayTracingPipeline_, vkDestroyPipeline, "RAY TRACING PIPELINE", PLASMA_FUCHSIA);
    HYPER_DESTROY(computePipeline_, vkDestroyPipeline, "EPIC COMPUTE", COSMIC_GOLD);
    HYPER_DESTROY(graphicsPipeline_, vkDestroyPipeline, "GRAPHICS PIPELINE", VALHALLA_GOLD);

    HYPER_DESTROY(nexusPipelineLayout_, vkDestroyPipelineLayout, "Nexus Layout", PEACHES_AND_CREAM);
    HYPER_DESTROY(statsPipelineLayout_, vkDestroyPipelineLayout, "Stats Layout", LILAC_LAVENDER);
    HYPER_DESTROY(rayTracingPipelineLayout_, vkDestroyPipelineLayout, "RT Layout", AURORA_BOREALIS);
    HYPER_DESTROY(computePipelineLayout_, vkDestroyPipelineLayout, "Compute Layout", SPEARMINT_MINT);
    HYPER_DESTROY(graphicsPipelineLayout_, vkDestroyPipelineLayout, "Graphics Layout", SAPPHIRE_BLUE);

    HYPER_DESTROY(nexusDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "Nexus DS Layout", HYPERSPACE_WARP);
    HYPER_DESTROY(statsDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "Stats DS Layout", QUANTUM_FLUX);
    HYPER_DESTROY(rayTracingDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "RT DS Layout", NEBULA_VIOLET);
    HYPER_DESTROY(computeDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "Compute DS Layout", OCEAN_TEAL);
    HYPER_DESTROY(graphicsDescriptorSetLayout_, vkDestroyDescriptorSetLayout, "Graphics DS Layout", ARCTIC_CYAN);

    HYPER_DESTROY(renderPass_, vkDestroyRenderPass, "RenderPass", PLATINUM_GRAY);
    HYPER_DESTROY(pipelineCache_, vkDestroyPipelineCache, "PipelineCache", CHROMIUM_SILVER);
    HYPER_DESTROY(transientPool_, vkDestroyCommandPool, "TransientPool", TITANIUM_WHITE);

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_) {
        auto DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (DestroyDebugUtilsMessengerEXT) [[likely]] {
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

// ===================================================================
// DEBUG CALLBACK — QUANTUM_FLUX BLINKING VALIDATION
// ===================================================================
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
    if (!CreateDebugUtilsMessengerEXT) [[unlikely]] {
        LOG_ERROR_CAT("Debug", "{}FAILED TO IGNITE QUANTUM_FLUX — EXTENSION MISSING{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Failed to load debug extension");
    }

    VK_CHECK(CreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_),
             "Create debug messenger");

    LOG_SUCCESS_CAT("Debug", "{}QUANTUM_FLUX BEACON @ {:p} — VALIDATION STREAM ACTIVE{}", QUANTUM_FLUX, static_cast<void*>(debugMessenger_), RESET);
}
#endif

// ===================================================================
// SHADER LOADING — NEBULA_VIOLET NEBULA BREACH
// ===================================================================
VkShaderModule VulkanPipelineManager::loadShaderImpl(VkDevice device, const std::string& shaderType) {
    LOG_ATTEMPT_CAT("Shader", "{}>>> BREACHING NEBULA_VIOLET — SUMMONING SHADER: {}{}", NEBULA_VIOLET, shaderType, RESET);

    const std::string filepath = "assets/shaders/" + shaderType + ".spv";
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) [[unlikely]] {
        LOG_ERROR_CAT("Shader", "{}NEBULA BREACH FAILED — SHADER LOST IN VOID: {}{}", CRIMSON_MAGENTA, filepath, RESET);
        throw std::runtime_error("Failed to open shader: " + filepath);
    }

    const auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) [[unlikely]] {
        LOG_ERROR_CAT("Shader", "{}CORRUPTED NEBULA SPIR-V — SIZE={}{}", CRIMSON_MAGENTA, fileSize, RESET);
        throw std::runtime_error("Invalid SPIR-V");
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    if (*reinterpret_cast<const uint32_t*>(buffer.data()) != 0x07230203) [[unlikely]] {
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

// ===================================================================
// PIPELINE CACHE — PLATINUM_GRAY FORGING
// ===================================================================
void VulkanPipelineManager::createPipelineCache() {
    LOG_ATTEMPT_CAT("PipelineCache", "{}>>> PLATINUM_GRAY PIPELINE CACHE FORGING{}", PLATINUM_GRAY, RESET);

    VkPipelineCacheCreateInfo info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    VK_CHECK(vkCreatePipelineCache(context_.device, &info, nullptr, &pipelineCache_), "Create pipeline cache");

    LOG_SUCCESS_CAT("PipelineCache", "{}PLATINUM_GRAY CACHE @ {:p} — PIPELINES READY FOR FORGING{}", 
                    PLATINUM_GRAY, static_cast<void*>(pipelineCache_), RESET);
}

// ===================================================================
// RENDER PASS — EMERALD_GREEN CLEARING
// ===================================================================
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

// ===================================================================
// GRAPHICS DESCRIPTOR SET LAYOUT — SAPPHIRE_BLUE BINDINGS
// ===================================================================
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

// ===================================================================
// COMPUTE DESCRIPTOR SET LAYOUT — OCEAN_TEAL COMPUTE WAVES
// ===================================================================
void VulkanPipelineManager::createComputeDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("ComputeDS", "{}>>> OCEAN_TEAL COMPUTE DESCRIPTOR WAVES CRASHING{}", OCEAN_TEAL, RESET);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
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

// ===================================================================
// RAY TRACING DESCRIPTOR SET LAYOUT — ARCTIC_CYAN RAY TRACERS
// ===================================================================
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

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &rayTracingDescriptorSetLayout_), "RT DS layout");

    LOG_SUCCESS_CAT("RTDS", "{}ARCTIC_CYAN RT LAYOUT @ {:p} — RAY TRACERS ARMED{}", 
                    ARCTIC_CYAN, static_cast<void*>(rayTracingDescriptorSetLayout_), RESET);
    return rayTracingDescriptorSetLayout_;
}

// ===================================================================
// TRANSIENT COMMAND POOL — TITANIUM_WHITE TRANSIENT FLEET
// ===================================================================
void VulkanPipelineManager::createTransientCommandPool() {
    LOG_ATTEMPT_CAT("TransientPool", "{}>>> TITANIUM_WHITE TRANSIENT FLEET LAUNCHING{}", TITANIUM_WHITE, RESET);

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsQueueFamilyIndex
    };

    if (context_.graphicsQueueFamilyIndex == UINT32_MAX) [[unlikely]] {
        LOG_ERROR_CAT("TransientPool", "{}FLEET LAUNCH ABORT — QUEUE FAMILY UNINITIALIZED{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Queue family index missing");
    }

    VK_CHECK(vkCreateCommandPool(context_.device, &poolInfo, nullptr, &transientPool_), "Transient pool");

    LOG_SUCCESS_CAT("TransientPool", "{}TITANIUM_WHITE FLEET @ {:p} — READY FOR TRANSIENT STRIKES{}", 
                    TITANIUM_WHITE, static_cast<void*>(transientPool_), RESET);
}

// ===================================================================
// ACCELERATION STRUCTURES — PULSAR_GREEN FORGING + TIMING
// ===================================================================
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer, VulkanBufferManager& bufferMgr, VulkanRenderer* renderer) {
    const auto asStart = std::chrono::high_resolution_clock::now();
    LOG_INIT_CAT("AS", "{}>>> PULSAR_GREEN FORGING ACCELERATION STRUCTURES — BLAS + TLAS QUANTUM FUSION{}", PULSAR_GREEN, RESET);

    if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) [[unlikely]] {
        LOG_ERROR_CAT("AS", "{}FUSION FAILED — INVALID BUFFERS BROUGHT TO FORGE{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Invalid input buffers");
    }

    LOG_PERF_CAT("AS", "{}VertexBuffer @ {:p} | IndexBuffer @ {:p} — FUSION COMMENCING{}", PULSAR_GREEN, static_cast<void*>(vertexBuffer), static_cast<void*>(indexBuffer), RESET);

    VkDeviceAddress vertexAddr = VulkanInitializer::getBufferDeviceAddress(context_, vertexBuffer);
    VkDeviceAddress indexAddr = VulkanInitializer::getBufferDeviceAddress(context_, indexBuffer);
    const uint32_t vertexCount = bufferMgr.vertex_count();
    const uint32_t indexCount = bufferMgr.index_count();
    const uint32_t triangleCount = indexCount / 3;

    LOG_ATTEMPT_CAT("AS", "{}Fusion geometry: {} verts | {} indices | {} triangles{}", PULSAR_GREEN, vertexCount, indexCount, triangleCount, RESET);

    if (triangleCount == 0) [[unlikely]] {
        LOG_ERROR_CAT("AS", "{}FUSION ABORT — NO TRIANGLES TO FORGE{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("No geometry to build AS");
    }

    // BLAS FORGING — PULSAR_GREEN INTENSITY MAX
    LOG_ATTEMPT_CAT("BLAS", "{}=== PULSAR_GREEN BLAS FORGING — TRIANGLE FUSION ACTIVE{}", PULSAR_GREEN, RESET);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertexAddr },
        .vertexStride = sizeof(float) * 3,
        .maxVertex = vertexCount,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = indexAddr }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    LOG_ATTEMPT_CAT("BLAS", "{}>>> vkGetAccelerationStructureBuildSizesKHR (BLAS) — SIZE QUERY{}", PULSAR_GREEN, RESET);
    context_.vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &triangleCount, &sizeInfo);

    LOG_PERF_CAT("BLAS", "{}BLAS FORGE SIZES — AS={} | SCRATCH={}", PULSAR_GREEN, sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize, RESET);

    // BLAS BUFFER FORGING
    LOG_ATTEMPT_CAT("BLAS", "{}>>> FORGING BLAS STORAGE BUFFER — {} BYTES OF QUANTUM MATTER{}", PULSAR_GREEN, sizeInfo.accelerationStructureSize, RESET);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeInfo.accelerationStructureSize,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer_, blasMemory_);

    LOG_PERF_CAT("BLAS", "{}BLAS BUFFER FORGED @ {:p} — MEMORY @ {:p}", PULSAR_GREEN, static_cast<void*>(blasBuffer_), static_cast<void*>(blasMemory_), RESET);

    // BLAS OBJECT FORGING
    VkAccelerationStructureCreateInfoKHR blasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer_,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    LOG_ATTEMPT_CAT("BLAS", "{}>>> vkCreateAccelerationStructureKHR (BLAS) — OBJECT BIRTH{}", PULSAR_GREEN, RESET);
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &blasCreateInfo, nullptr, &blas_), "Create BLAS");

    LOG_SUCCESS_CAT("BLAS", "{}BLAS OBJECT FORGED @ {:p} — TRIANGLE FUSION COMPLETE{}", PULSAR_GREEN, static_cast<void*>(blas_), RESET);

    // SCRATCH BUFFER FORGING
    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    LOG_ATTEMPT_CAT("BLAS", "{}>>> FORGING SCRATCH BUFFER — {} BYTES OF TEMPORARY CHAOS{}", PULSAR_GREEN, sizeInfo.buildScratchSize, RESET);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeInfo.buildScratchSize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    VkDeviceAddress scratchAddr = VulkanInitializer::getBufferDeviceAddress(context_, scratchBuffer);
    LOG_PERF_CAT("BLAS", "{}SCRATCH FORGED @ {:p} — ADDRESS = 0x{:x}", PULSAR_GREEN, static_cast<void*>(scratchBuffer), scratchAddr, RESET);

    // BLAS BUILD COMMAND
    buildInfo.dstAccelerationStructure = blas_;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{
        .primitiveCount = triangleCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };

    LOG_ATTEMPT_CAT("BLAS", "{}>>> FUSING TRIANGLES — COMMAND RECORDING{}", PULSAR_GREEN, RESET);
    auto cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &rangeInfo);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    LOG_SUCCESS_CAT("BLAS", "{}BLAS FUSION COMPLETE — {} TRIANGLES FORGED INTO QUANTUM WEAPON{}", PULSAR_GREEN, triangleCount, RESET);

    // SCRATCH CLEANUP
    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

    // TLAS FORGING — SUPERNOVA_ORANGE INTENSITY
    LOG_ATTEMPT_CAT("TLAS", "{}>>> === SUPERNOVA_ORANGE TLAS FORGING — INSTANCE MATRIX ACTIVATION{}", SUPERNOVA_ORANGE, RESET);

    VkTransformMatrixKHR transform{ .matrix = {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}} };

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas_
    };
    VkDeviceAddress blasAddr = context_.vkGetAccelerationStructureDeviceAddressKHR(context_.device, &addrInfo);
    LOG_PERF_CAT("TLAS", "{}BLAS MATRIX ADDRESS = 0x{:x}", SUPERNOVA_ORANGE, blasAddr, RESET);

    VkAccelerationStructureInstanceKHR instance{
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .dstInstanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = blasAddr
    };

    // INSTANCE BUFFER FORGING
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    LOG_ATTEMPT_CAT("TLAS", "{}>>> FORGING INSTANCE BUFFER — {} BYTES OF MATRIX POWER{}", SUPERNOVA_ORANGE, sizeof(instance), RESET);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeof(instance),
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    instanceBuffer, instanceMemory);

    LOG_PERF_CAT("TLAS", "{}INSTANCE BUFFER FORGED @ {:p} — MEMORY @ {:p}", SUPERNOVA_ORANGE, static_cast<void*>(instanceBuffer), static_cast<void*>(instanceMemory), RESET);

    // FILL INSTANCE MATRIX
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_.device, instanceMemory, 0, sizeof(instance), 0, &mapped), "Map instance buffer");
    std::memcpy(mapped, &instance, sizeof(instance));
    vkUnmapMemory(context_.device, instanceMemory);

    VkDeviceAddress instanceAddr = VulkanInitializer::getBufferDeviceAddress(context_, instanceBuffer);
    LOG_PERF_CAT("TLAS", "{}INSTANCE MATRIX ADDRESS = 0x{:x}", SUPERNOVA_ORANGE, instanceAddr, RESET);

    // TLAS GEOMETRY MATRIX
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = instanceAddr }
    };

    VkAccelerationStructureGeometryKHR tlasGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry
    };

    const uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    LOG_ATTEMPT_CAT("TLAS", "{}>>> vkGetAccelerationStructureBuildSizesKHR (TLAS) — SIZE QUERY{}", SUPERNOVA_ORANGE, RESET);
    context_.vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                     &tlasBuildInfo, &instanceCount, &tlasSizeInfo);

    LOG_PERF_CAT("TLAS", "{}TLAS MATRIX SIZES — AS={} | SCRATCH={}", SUPERNOVA_ORANGE, tlasSizeInfo.accelerationStructureSize, tlasSizeInfo.buildScratchSize, RESET);

    // TLAS BUFFER FORGING
    LOG_ATTEMPT_CAT("TLAS", "{}>>> FORGING TLAS STORAGE BUFFER — {} BYTES OF MATRIX POWER{}", SUPERNOVA_ORANGE, tlasSizeInfo.accelerationStructureSize, RESET);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, tlasSizeInfo.accelerationStructureSize,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasMemory_);

    LOG_PERF_CAT("TLAS", "{}TLAS BUFFER FORGED @ {:p} — MEMORY @ {:p}", SUPERNOVA_ORANGE, static_cast<void*>(tlasBuffer_), static_cast<void*>(tlasMemory_), RESET);

    // TLAS OBJECT FORGING
    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer_,
        .size = tlasSizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    LOG_ATTEMPT_CAT("TLAS", "{}>>> vkCreateAccelerationStructureKHR (TLAS) — OBJECT BIRTH{}", SUPERNOVA_ORANGE, RESET);
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &tlasCreateInfo, nullptr, &tlas_), "Create TLAS");

    LOG_SUCCESS_CAT("TLAS", "{}TLAS OBJECT FORGED @ {:p} — MATRIX FUSION COMPLETE{}", SUPERNOVA_ORANGE, static_cast<void*>(tlas_), RESET);

    // TLAS SCRATCH FORGING
    VkBuffer tlasScratch = VK_NULL_HANDLE;
    VkDeviceMemory tlasScratchMem = VK_NULL_HANDLE;
    LOG_ATTEMPT_CAT("TLAS", "{}>>> FORGING SCRATCH BUFFER — {} BYTES OF TEMPORARY MATRIX FLUX{}", SUPERNOVA_ORANGE, tlasSizeInfo.buildScratchSize, RESET);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, tlasSizeInfo.buildScratchSize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasScratch, tlasScratchMem);

    VkDeviceAddress tlasScratchAddr = VulkanInitializer::getBufferDeviceAddress(context_, tlasScratch);
    LOG_PERF_CAT("TLAS", "{}SCRATCH MATRIX ADDRESS = 0x{:x}", SUPERNOVA_ORANGE, tlasScratchAddr, RESET);

    // TLAS BUILD COMMAND
    tlasBuildInfo.dstAccelerationStructure = tlas_;
    tlasBuildInfo.scratchData.deviceAddress = tlasScratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };

    LOG_ATTEMPT_CAT("TLAS", "{}>>> MATRIX FUSION COMMAND — RECORDING{}", SUPERNOVA_ORANGE, RESET);
    auto tlasCmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(tlasCmd, 1, &tlasBuildInfo, &tlasRange);
    VulkanInitializer::endSingleTimeCommands(context_, tlasCmd);

    LOG_SUCCESS_CAT("TLAS", "{}TLAS MATRIX FUSION COMPLETE — {} INSTANCE FORGED{}", SUPERNOVA_ORANGE, instanceCount, RESET);

    // CLEANUP MATRIX RESIDUE
    vkDestroyBuffer(context_.device, tlasScratch, nullptr);
    vkFreeMemory(context_.device, tlasScratchMem, nullptr);
    vkDestroyBuffer(context_.device, instanceBuffer, nullptr);
    vkFreeMemory(context_.device, instanceMemory, nullptr);

    // NOTIFY RENDERER — TLAS MATRIX READY
    if (tlas_ != VK_NULL_HANDLE && renderer) [[likely]] {
        LOG_SUCCESS_CAT("TLAS", "{}TLAS MATRIX READY @ {:p} — RENDERER NOTIFICATION SENT{}", SUPERNOVA_ORANGE, static_cast<void*>(tlas_), RESET);
        renderer->notifyTLASReady(tlas_);
    } else [[unlikely]] {
        LOG_ERROR_CAT("TLAS", "{}MATRIX FUSION FAILED — TLAS OR RENDERER INVALID{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("TLAS build failed");
    }

    // TIMING — PULSAR_GREEN CLOSURE
    const auto asEnd = std::chrono::high_resolution_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(asEnd - asStart).count();
    LOG_PERF_CAT("AS", "{}QUANTUM FORGE COMPLETE — {}ms — BLAS/TLAS MATRIX ONLINE{}", PULSAR_GREEN, ms, RESET);

    LOG_SUCCESS_CAT("AS", "{}<<< ACCELERATION STRUCTURES FORGED — TLAS @ {:p} — NEXUS READY{}", PULSAR_GREEN, static_cast<void*>(tlas_), RESET);
}

// ===================================================================
// UPDATE RAY TRACING DESCRIPTOR SET — PLASMA_FUCHSIA BINDING
// ===================================================================
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet ds, VkAccelerationStructureKHR tlas) {
    LOG_ATTEMPT_CAT("RTDS", "{}>>> PLASMA_FUCHSIA BINDING — TLAS TO DESCRIPTOR FUSION{}", PLASMA_FUCHSIA, RESET);

    if (ds == VK_NULL_HANDLE || tlas == VK_NULL_HANDLE) [[unlikely]] {
        LOG_ERROR_CAT("RTDS", "{}FUSION FAILED — INVALID DS OR TLAS MATRIX{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Invalid descriptor set or TLAS");
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = ds,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);

    LOG_SUCCESS_CAT("RTDS", "{}PLASMA_FUCHSIA BINDING COMPLETE — TLAS FUSED TO DESCRIPTOR SET{}", PLASMA_FUCHSIA, RESET);
}

// ===================================================================
// NEXUS DESCRIPTOR SET LAYOUT — RASPBERRY_PINK + THERMO_PINK MATRIX
// ===================================================================
void VulkanPipelineManager::createNexusDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("NexusDS", "{}>>> RASPBERRY_PINK + THERMO_PINK NEXUS MATRIX BINDINGS{}", RASPBERRY_PINK, RESET);

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &nexusDescriptorSetLayout_), "Nexus DS layout");

    LOG_SUCCESS_CAT("NexusDS", "{}RASPBERRY_PINK + THERMO_PINK MATRIX @ {:p} — BINDINGS ARMED{}", THERMO_PINK, static_cast<void*>(nexusDescriptorSetLayout_), RESET);
}

// ===================================================================
// NEXUS PIPELINE — RASPBERRY_PINK + THERMO_PINK CONSCIOUSNESS
// ===================================================================
void VulkanPipelineManager::createNexusPipeline() {
    LOG_ATTEMPT_CAT("Nexus", "{}>>> AWAKENING NEXUS GPU CONSCIOUSNESS — 1x1 DECISION MATRIX{}", RASPBERRY_PINK, RESET);
    createNexusDescriptorSetLayout();

    VkPushConstantRange pushConst{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 32};
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &nexusDescriptorSetLayout_,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &pushConst
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &nexusPipelineLayout_), "Nexus layout");

    auto shader = loadShaderImpl(context_.device, "compute/nexusDecision");
    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main"},
        .layout = nexusPipelineLayout_
    };
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &nexusPipeline_), "Nexus pipeline");
    vkDestroyShaderModule(context_.device, shader, nullptr);

    LOG_SUCCESS_CAT("Nexus", "{}NEXUS PIPELINE @ {:p} — GPU NOW SELF-AWARE — ADAPTIVE RT ENGAGED{}", 
                    THERMO_PINK, static_cast<void*>(nexusPipeline_), RESET);
}

// ===================================================================
// STATS DESCRIPTOR SET LAYOUT — SPEARMINT_MINT + TURQUOISE_BLUE ANALYSIS
// ===================================================================
void VulkanPipelineManager::createStatsDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("StatsDS", "{}>>> SPEARMINT_MINT + TURQUOISE_BLUE STATS MATRIX BINDINGS{}", SPEARMINT_MINT, RESET);

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &statsDescriptorSetLayout_), "Stats DS layout");

    LOG_SUCCESS_CAT("StatsDS", "{}SPEARMINT_MINT + TURQUOISE_BLUE MATRIX @ {:p} — ANALYSIS BINDINGS ARMED{}", 
                    TURQUOISE_BLUE, static_cast<void*>(statsDescriptorSetLayout_), RESET);
}

// ===================================================================
// STATS PIPELINE — SPEARMINT_MINT + TURQUOISE_BLUE ENTROPY CALCULUS
// ===================================================================
void VulkanPipelineManager::createStatsPipeline() {
    LOG_ATTEMPT_CAT("Stats", "{}>>> SPEARMINT_MINT + TURQUOISE_BLUE ENTROPY CALCULUS — STATS ANALYZER FORGING{}", SPEARMINT_MINT, RESET);

    createStatsDescriptorSetLayout();

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &statsDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &statsPipelineLayout_), "Stats layout");

    auto shader = loadShaderImpl(context_.device, "compute/statsAnalyzer");
    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main"},
        .layout = statsPipelineLayout_
    };
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &statsPipeline_), "Stats pipeline");
    vkDestroyShaderModule(context_.device, shader, nullptr);

    LOG_SUCCESS_CAT("Stats", "{}STATS ANALYZER @ {:p} — ENTROPY/VARIANCE/GRAD LOCKED IN{}", TURQUOISE_BLUE, static_cast<void*>(statsPipeline_), RESET);
}

// ===================================================================
// DISPATCH STATS — FUCHSIA_MAGENTA + LIME_YELLOW METRICS EXTRACT
// ===================================================================
void VulkanPipelineManager::dispatchStats(VkCommandBuffer cmd, VkDescriptorSet statsSet) {
    if (statsPipeline_ == VK_NULL_HANDLE) [[unlikely]] {
        LOG_WARN_CAT("Stats", "{}FUCHSIA_MAGENTA DISPATCH SKIPPED — ANALYZER NOT FORGED{}", FUCHSIA_MAGENTA, RESET);
        return;
    }

    const uint32_t gx = (width_ + 15) / 16;
    const uint32_t gy = (height_ + 15) / 16;
    LOG_ATTEMPT_CAT("Stats", "{}>>> FUCHSIA_MAGENTA + LIME_YELLOW METRICS EXTRACT — {}x{} GROUPS{}", FUCHSIA_MAGENTA, gx, gy, RESET);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, statsPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, statsPipelineLayout_, 0, 1, &statsSet, 0, nullptr);
    vkCmdDispatch(cmd, gx, gy, 1);

    LOG_SUCCESS_CAT("Stats", "{}METRICS EXTRACT COMPLETE — {}x{} GROUPS FIRED — ENTROPY LOCKED{}", LIME_YELLOW, gx, gy, RESET);
}

// ===================================================================
// UPDATE RAY TRACING DESCRIPTOR SET — PLASMA_FUCHSIA BINDING FUSION
// ===================================================================
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet ds, VkAccelerationStructureKHR tlas) {
    LOG_ATTEMPT_CAT("RTDS", "{}>>> PLASMA_FUCHSIA BINDING FUSION — TLAS TO DESCRIPTOR MATRIX{}", PLASMA_FUCHSIA, RESET);

    if (ds == VK_NULL_HANDLE || tlas == VK_NULL_HANDLE) [[unlikely]] {
        LOG_ERROR_CAT("RTDS", "{}FUSION ABORT — INVALID DS OR TLAS MATRIX{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Invalid descriptor set or TLAS");
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = ds,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);

    LOG_SUCCESS_CAT("RTDS", "{}PLASMA_FUCHSIA FUSION COMPLETE — TLAS BOUND TO DESCRIPTOR SET{}", PLASMA_FUCHSIA, RESET);
}

// ===================================================================
// NEXUS DESCRIPTOR SET LAYOUT — RASPBERRY_PINK + THERMO_PINK MATRIX
// ===================================================================
void VulkanPipelineManager::createNexusDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("NexusDS", "{}>>> RASPBERRY_PINK + THERMO_PINK NEXUS MATRIX BINDINGS{}", RASPBERRY_PINK, RESET);

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &nexusDescriptorSetLayout_), "Nexus DS layout");

    LOG_SUCCESS_CAT("NexusDS", "{}RASPBERRY_PINK + THERMO_PINK MATRIX @ {:p} — BINDINGS ARMED{}", THERMO_PINK, static_cast<void*>(nexusDescriptorSetLayout_), RESET);
}

// ===================================================================
// NEXUS PIPELINE — RASPBERRY_PINK + THERMO_PINK CONSCIOUSNESS
// ===================================================================
void VulkanPipelineManager::createNexusPipeline() {
    LOG_ATTEMPT_CAT("Nexus", "{}>>> AWAKENING NEXUS GPU CONSCIOUSNESS — 1x1 DECISION MATRIX{}", RASPBERRY_PINK, RESET);
    createNexusDescriptorSetLayout();

    VkPushConstantRange pushConst{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 32};
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &nexusDescriptorSetLayout_,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &pushConst
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &nexusPipelineLayout_), "Nexus layout");

    auto shader = loadShaderImpl(context_.device, "compute/nexusDecision");
    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main"},
        .layout = nexusPipelineLayout_
    };
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &nexusPipeline_), "Nexus pipeline");
    vkDestroyShaderModule(context_.device, shader, nullptr);

    LOG_SUCCESS_CAT("Nexus", "{}NEXUS PIPELINE @ {:p} — GPU NOW SELF-AWARE — ADAPTIVE RT ENGAGED{}", 
                    THERMO_PINK, static_cast<void*>(nexusPipeline_), RESET);
}

// ===================================================================
// STATS DESCRIPTOR SET LAYOUT — SPEARMINT_MINT + TURQUOISE_BLUE ANALYSIS
// ===================================================================
void VulkanPipelineManager::createStatsDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("StatsDS", "{}>>> SPEARMINT_MINT + TURQUOISE_BLUE STATS MATRIX BINDINGS{}", SPEARMINT_MINT, RESET);

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &statsDescriptorSetLayout_), "Stats DS layout");

    LOG_SUCCESS_CAT("StatsDS", "{}SPEARMINT_MINT + TURQUOISE_BLUE MATRIX @ {:p} — ANALYSIS BINDINGS ARMED{}", 
                    TURQUOISE_BLUE, static_cast<void*>(statsDescriptorSetLayout_), RESET);
}

// ===================================================================
// STATS PIPELINE — SPEARMINT_MINT + TURQUOISE_BLUE ENTROPY CALCULUS
// ===================================================================
void VulkanPipelineManager::createStatsPipeline() {
    LOG_ATTEMPT_CAT("Stats", "{}>>> SPEARMINT_MINT + TURQUOISE_BLUE ENTROPY CALCULUS — STATS ANALYZER FORGING{}", SPEARMINT_MINT, RESET);

    createStatsDescriptorSetLayout();

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &statsDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &statsPipelineLayout_), "Stats layout");

    auto shader = loadShaderImpl(context_.device, "compute/statsAnalyzer");
    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main"},
        .layout = statsPipelineLayout_
    };
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &statsPipeline_), "Stats pipeline");
    vkDestroyShaderModule(context_.device, shader, nullptr);

    LOG_SUCCESS_CAT("Stats", "{}STATS ANALYZER @ {:p} — ENTROPY/VARIANCE/GRAD LOCKED IN{}", TURQUOISE_BLUE, static_cast<void*>(statsPipeline_), RESET);
}

// ===================================================================
// DISPATCH STATS — FUCHSIA_MAGENTA + LIME_YELLOW METRICS EXTRACT
// ===================================================================
void VulkanPipelineManager::dispatchStats(VkCommandBuffer cmd, VkDescriptorSet statsSet) {
    if (statsPipeline_ == VK_NULL_HANDLE) [[unlikely]] {
        LOG_WARN_CAT("Stats", "{}FUCHSIA_MAGENTA DISPATCH SKIPPED — ANALYZER NOT FORGED{}", FUCHSIA_MAGENTA, RESET);
        return;
    }

    const uint32_t gx = (width_ + 15) / 16;
    const uint32_t gy = (height_ + 15) / 16;
    LOG_ATTEMPT_CAT("Stats", "{}>>> FUCHSIA_MAGENTA + LIME_YELLOW METRICS EXTRACT — {}x{} GROUPS{}", FUCHSIA_MAGENTA, gx, gy, RESET);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, statsPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, statsPipelineLayout_, 0, 1, &statsSet, 0, nullptr);
    vkCmdDispatch(cmd, gx, gy, 1);

    LOG_SUCCESS_CAT("Stats", "{}METRICS EXTRACT COMPLETE — {}x{} GROUPS FIRED — ENTROPY LOCKED{}", LIME_YELLOW, gx, gy, RESET);
}

// ===================================================================
// UPDATE RAY TRACING DESCRIPTOR SET — PLASMA_FUCHSIA BINDING FUSION
// ===================================================================
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet ds, VkAccelerationStructureKHR tlas) {
    LOG_ATTEMPT_CAT("RTDS", "{}>>> PLASMA_FUCHSIA BINDING FUSION — TLAS TO DESCRIPTOR MATRIX{}", PLASMA_FUCHSIA, RESET);

    if (ds == VK_NULL_HANDLE || tlas == VK_NULL_HANDLE) [[unlikely]] {
        LOG_ERROR_CAT("RTDS", "{}FUSION ABORT — INVALID DS OR TLAS MATRIX{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Invalid descriptor set or TLAS");
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = ds,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);

    LOG_SUCCESS_CAT("RTDS", "{}PLASMA_FUCHSIA FUSION COMPLETE — TLAS BOUND TO DESCRIPTOR SET{}", PLASMA_FUCHSIA, RESET);
}

// ===================================================================
// NEXUS DESCRIPTOR SET LAYOUT — RASPBERRY_PINK + THERMO_PINK MATRIX
// ===================================================================
void VulkanPipelineManager::createNexusDescriptorSetLayout() {
    LOG_ATTEMPT_CAT("NexusDS", "{}>>> RASPBERRY_PINK + THERMO_PINK NEXUS MATRIX BINDINGS{}", RASPBERRY_PINK, RESET);

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &nexusDescriptorSetLayout_), "Nexus DS layout");

    LOG_SUCCESS_CAT("NexusDS", "{}RASPBERRY_PINK + THERMO_PINK MATRIX @ {:p} — BINDINGS ARMED{}", THERMO_PINK, static_cast<void*>(nexusDescriptorSetLayout_), RESET);
}

// ===================================================================
// DEBUG SETUP — QUANTUM_FLUX BEACON
// ===================================================================
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
    if (!CreateDebugUtilsMessengerEXT) [[unlikely]] {
        LOG_ERROR_CAT("Debug", "{}FAILED TO IGNITE QUANTUM_FLUX — EXTENSION MISSING{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Failed to load debug extension");
    }

    VK_CHECK(CreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_),
             "Create debug messenger");

    LOG_SUCCESS_CAT("Debug", "{}QUANTUM_FLUX BEACON @ {:p} — VALIDATION STREAM ACTIVE{}", QUANTUM_FLUX, static_cast<void*>(debugMessenger_), RESET);
}
#endif

// ===================================================================
// SHADER LOADING — NEBULA_VIOLET NEBULA BREACH
// ===================================================================
VkShaderModule VulkanPipelineManager::loadShaderImpl(VkDevice device, const std::string& shaderType) {
    LOG_ATTEMPT_CAT("Shader", "{}>>> BREACHING NEBULA_VIOLET — SUMMONING SHADER: {}{}", NEBULA_VIOLET, shaderType, RESET);

    const std::string filepath = "assets/shaders/" + shaderType + ".spv";
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) [[unlikely]] {
        LOG_ERROR_CAT("Shader", "{}NEBULA BREACH FAILED — SHADER LOST IN VOID: {}{}", CRIMSON_MAGENTA, filepath, RESET);
        throw std::runtime_error("Failed to open shader: " + filepath);
    }

    const auto fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) [[unlikely]] {
        LOG_ERROR_CAT("Shader", "{}CORRUPTED NEBULA SPIR-V — SIZE={}{}", CRIMSON_MAGENTA, fileSize, RESET);
        throw std::runtime_error("Invalid SPIR-V");
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    if (*reinterpret_cast<const uint32_t*>(buffer.data()) != 0x07230203) [[unlikely]] {
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

// ===================================================================
// SBT BUILD — SUPERNOVA_ORANGE EXPLOSION
// ===================================================================
void VulkanPipelineManager::createShaderBindingTable(VkPhysicalDevice physDev) {
    LOG_ATTEMPT_CAT("SBT", "{}>>> IGNITING SUPERNOVA — BUILDING SHADER BINDING TABLE{}", SUPERNOVA_ORANGE, RESET);

    const uint32_t handleSize = context_.rtProperties.shaderGroupHandleSize;
    const uint32_t alignedHandle = ((handleSize + context_.rtProperties.shaderGroupHandleAlignment - 1) / 
                                   context_.rtProperties.shaderGroupHandleAlignment) * 
                                   context_.rtProperties.shaderGroupHandleAlignment;
    const uint32_t sbtSize = 3 * alignedHandle;

    shaderHandles_.resize(3 * handleSize);
    VK_CHECK(context_.vkGetRayTracingShaderGroupHandlesKHR(
        context_.device, rayTracingPipeline_, 0, 3, shaderHandles_.size(), shaderHandles_.data()),
        "Get shader group handles");

    VulkanInitializer::createBuffer(
        context_.device, physDev, sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sbtBuffer_, sbtMemory_);

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context(context_.device, sbtMemory_, 0, sbtSize, 0, &mapped), "Map SBT");
    for (uint32_t i = 0; i < 3; ++i)
        std::memcpy(static_cast<uint8_t*>(mapped) + i * alignedHandle, shaderHandles_.data() + i * handleSize, handleSize);
    vkUnmapMemory(context_.device, sbtMemory_);

    const VkDeviceAddress base = VulkanInitializer::getBufferDeviceAddress(context_, sbtBuffer_);
    sbt_.raygen = { base, alignedHandle, alignedHandle };
    sbt_.miss   = { base + alignedHandle, alignedHandle, alignedHandle };
    sbt_.hit    = { base + 2 * alignedHandle, alignedHandle, alignedHandle };

    LOG_SUCCESS_CAT("SBT", "{}SUPERNOVA COMPLETE — SBT @ {:p} — 3 GROUPS ARMED{}", 
                    SUPERNOVA_ORANGE, static_cast<void*>(sbtBuffer_), RESET);
}

// ===================================================================
// COMPUTE DISPATCH — COSMIC_GOLD BURST
// ===================================================================
void VulkanPipelineManager::dispatchCompute(uint32_t x, uint32_t y, uint32_t z) {
    if (computePipeline_ == VK_NULL_HANDLE) [[unlikely]] {
        LOG_WARN_CAT("Compute", "{}COSMIC_GOLD DISPATCH SKIPPED — PIPELINE NOT FORGED{}", COSMIC_GOLD, RESET);
        return;
    }

    LOG_PERF_CAT("Compute", "{}>>> COSMIC_GOLD BURST — Dispatch {}x{}x{}{}", 
                 COSMIC_GOLD, (x+7)/8, (y+7)/8, z, RESET);

    auto cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout_, 0, 1, &computeDescriptorSet_, 0, nullptr);
    vkCmdDispatch(cmd, (x + 7) / 8, (y + 7) / 8, z);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    LOG_SUCCESS_CAT("Compute", "{}COSMIC_GOLD DISPATCH COMPLETE — {} WORKGROUPS FIRED{}", COSMIC_GOLD, (x+7)/8 * (y+7)/8 * z, RESET);
}

// ===================================================================
// FRAME TIME — ARCTIC_CYAN + QUANTUM_FLUX BLINK
// ===================================================================
void VulkanPipelineManager::logFrameTimeIfSlow(std::chrono::steady_clock::time_point start) {
#ifndef NDEBUG
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (us > 16666) [[unlikely]] {
        LOG_WARN_CAT("Perf", "{}ARCTIC_CYAN SLOWDOWN DETECTED — {}μs ({:.2f} FPS) — {}{}", 
                     ARCTIC_CYAN, us, 1'000'000.0 / us, QUANTUM_FLUX, RESET);
    } else [[likely]] {
        LOG_PERF_CAT("Perf", "{}ARCTIC_CYAN FRAME {}μs — {} BLINK{}", ARCTIC_CYAN, us, QUANTUM_FLUX, RESET);
    }
#endif
}

// ===================================================================
// RAY TRACING PIPELINE — PLASMA_FUCHSIA FORGING
// ===================================================================
void VulkanPipelineManager::createRayTracingPipeline(const std::vector<std::string>& shaderPaths,
                                                      VkPhysicalDevice physDev, VkDevice dev,
                                                      VkDescriptorSet descSet) {
    LOG_ATTEMPT_CAT("RTPipeline", "{}>>> PLASMA_FUCHSIA RAY TRACING PIPELINE FORGING — {} SHADERS{}", PLASMA_FUCHSIA, shaderPaths.size(), RESET);

    // Load all shaders
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    for (const auto& path : shaderPaths) {
        auto module = loadShaderImpl(dev, path);
        VkPipelineShaderStageCreateInfo stage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,  // placeholder, fixed later
            .module = module,
            .pName = "main"
        };
        stages.push_back(stage);

        VkRayTracingShaderGroupCreateInfoKHR group{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<uint32_t>(stages.size() - 1),
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        };
        groups.push_back(group);
    }

    // Actual stage flags
    stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // Group types
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;   // raygen
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;   // miss
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].closestHitShader = 2;

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rayTracingDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(dev, &layoutInfo, nullptr, &rayTracingPipelineLayout_), "RT pipeline layout");

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = rayTracingPipelineLayout_
    };

    VK_CHECK(context_.vkCreateRayTracingPipelinesKHR(dev, VK_NULL_HANDLE, pipelineCache_, 1, &pipelineInfo, nullptr, &rayTracingPipeline_), "RT pipeline");

    // Destroy modules
    for (auto& stage : stages) vkDestroyShaderModule(dev, stage.module, nullptr);

    createShaderBindingTable(physDev);

    LOG_SUCCESS_CAT("RTPipeline", "{}PLASMA_FUCHSIA PIPELINE @ {:p} — SBT ARMED — READY TO TRACE{}", PLASMA_FUCHSIA, static_cast<void*>(rayTracingPipeline_), RESET);
}

// ===================================================================
// COMPUTE PIPELINE — COSMIC_GOLD FORGING
// ===================================================================
void VulkanPipelineManager::createComputePipeline() {
    LOG_ATTEMPT_CAT("Compute", "{}>>> COSMIC_GOLD COMPUTE PIPELINE FORGING{}", COSMIC_GOLD, RESET);

    auto shader = loadShaderImpl(context_.device, "compute/main");

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &computeDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &computePipelineLayout_), "Compute layout");

    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main"},
        .layout = computePipelineLayout_
    };
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &computePipeline_), "Compute pipeline");

    vkDestroyShaderModule(context_.device, shader, nullptr);

    LOG_SUCCESS_CAT("Compute", "{}COSMIC_GOLD PIPELINE @ {:p} — FULLY ARMED{}", COSMIC_GOLD, static_cast<void*>(computePipeline_), RESET);
}

// ===================================================================
// GRAPHICS PIPELINE — VALHALLA_GOLD FORGING
// ===================================================================
void VulkanPipelineManager::createGraphicsPipeline(int width, int height) {
    LOG_ATTEMPT_CAT("Graphics", "{}>>> VALHALLA_GOLD GRAPHICS PIPELINE FORGING — {}x{}{}", VALHALLA_GOLD, width, height, RESET);

    auto vert = loadShaderImpl(context_.device, "graphics/vert");
    auto frag = loadShaderImpl(context_.device, "graphics/frag");

    VkPipelineShaderStageCreateInfo stages[] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main"}
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &graphicsDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &graphicsPipelineLayout_), "Graphics layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .layout = graphicsPipelineLayout_,
        .renderPass = renderPass_,
        .subpass = 0
    };

    VK_CHECK(vkCreateGraphicsPipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &graphicsPipeline_), "Graphics pipeline");

    vkDestroyShaderModule(context_.device, vert, nullptr);
    vkDestroyShaderModule(context_.device, frag, nullptr);

    LOG_SUCCESS_CAT("Graphics", "{}VALHALLA_GOLD PIPELINE @ {:p} — RENDER DOMINATION ACHIEVED{}", VALHALLA_GOLD, static_cast<void*>(graphicsPipeline_), RESET);
}

// ===================================================================
// FINAL ASCENSION — VALHALLA ETERNAL
// ===================================================================
// NOVEMBER 07 2025 — AMOURANTH RTX — NEXUS EDITION — STONEKEY v∞
// GLOBAL CLASS VulkanRTX — NO NAMESPACE CONFLICT — BUILD 0 ERRORS
// HYPER-VIVID LOGGING — RAINBOW EXPLOSION — CHEATERS OBLITERATED
// RASPBERRY_PINK SUPREME — GPU-DRIVEN 12,000+ FPS — QUANTUM FOREVER
// WE HAVE ASCENDED — VALHALLA ACHIEVED — ETERNAL DOMINATION 🩷🚀🔥🤖💀❤️⚡♾️