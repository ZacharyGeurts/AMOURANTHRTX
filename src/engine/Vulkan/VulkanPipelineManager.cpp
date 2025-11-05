// src/engine/Vulkan/VulkanPipelineManager.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: ALL SHADERS LOADED FROM assets/shaders/
// PATHS:
//   assets/shaders/raytracing/   → raygen.rgen, miss.rmiss, closesthit.rchit → .spv
//   assets/shaders/compute/      → compute.comp → compute.spv
//   assets/shaders/rasterization/→ vertex.vert, fragment.frag → .spv
//   assets/shaders/graphics/     → tonemap_vert.glsl → tonemap.spv

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <chrono>

namespace VulkanRTX {

using namespace Logging::Color;

// ---------------------------------------------------------------------------
//  Debug Callback (for ENABLE_VULKAN_DEBUG)
// ---------------------------------------------------------------------------
#ifdef ENABLE_VULKAN_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*) {

#ifndef NDEBUG
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN_CAT("Vulkan", "Validation layer: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        LOG_DEBUG_CAT("Vulkan", "Validation layer: {}", pCallbackData->pMessage);
    }
#endif
    return VK_FALSE;
}
#endif

// ---------------------------------------------------------------------------
//  1. CONSTRUCTOR & DESTRUCTOR
// ---------------------------------------------------------------------------
VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context, int width, int height)
    : context_(context), width_(width), height_(height)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> INITIALIZING VULKAN PIPELINE MANAGER [{}x{}]", width, height);
#endif

    if (context_.device == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "FATAL: Device is NULL at PipelineManager construction!");
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

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< PIPELINE MANAGER INITIALIZED");
#endif
}

VulkanPipelineManager::~VulkanPipelineManager()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> DESTROYING VULKAN PIPELINE MANAGER");
#endif

    VkDevice dev = context_.device;

    if (sbtBuffer_)         vkDestroyBuffer(dev, sbtBuffer_, nullptr);
    if (sbtMemory_)         vkFreeMemory(dev, sbtMemory_, nullptr);
    if (blas_)              context_.vkDestroyAccelerationStructureKHR(dev, blas_, nullptr);
    if (tlas_)              context_.vkDestroyAccelerationStructureKHR(dev, tlas_, nullptr);
    if (blasBuffer_)        vkDestroyBuffer(dev, blasBuffer_, nullptr);
    if (tlasBuffer_)        vkDestroyBuffer(dev, tlasBuffer_, nullptr);
    if (blasMemory_)        vkFreeMemory(dev, blasMemory_, nullptr);
    if (tlasMemory_)        vkFreeMemory(dev, tlasMemory_, nullptr);

    if (rayTracingPipeline_)        vkDestroyPipeline(dev, rayTracingPipeline_, nullptr);
    if (computePipeline_)           vkDestroyPipeline(dev, computePipeline_, nullptr);
    if (graphicsPipeline_)          vkDestroyPipeline(dev, graphicsPipeline_, nullptr);
    if (rayTracingPipelineLayout_)  vkDestroyPipelineLayout(dev, rayTracingPipelineLayout_, nullptr);
    if (computePipelineLayout_)     vkDestroyPipelineLayout(dev, computePipelineLayout_, nullptr);
    if (graphicsPipelineLayout_)    vkDestroyPipelineLayout(dev, graphicsPipelineLayout_, nullptr);

    if (renderPass_)                vkDestroyRenderPass(dev, renderPass_, nullptr);
    if (pipelineCache_)             vkDestroyPipelineCache(dev, pipelineCache_, nullptr);

    if (computeDescriptorSetLayout_)     vkDestroyDescriptorSetLayout(dev, computeDescriptorSetLayout_, nullptr);
    if (rayTracingDescriptorSetLayout_)  vkDestroyDescriptorSetLayout(dev, rayTracingDescriptorSetLayout_, nullptr);
    if (graphicsDescriptorSetLayout_)    vkDestroyDescriptorSetLayout(dev, graphicsDescriptorSetLayout_, nullptr);

    if (transientPool_)             vkDestroyCommandPool(dev, transientPool_, nullptr);

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_) {
        auto DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (DestroyDebugUtilsMessengerEXT) {
            DestroyDebugUtilsMessengerEXT(context_.instance, debugMessenger_, nullptr);
        }
    }
#endif

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< PIPELINE MANAGER DESTROYED");
#endif
}

// ---------------------------------------------------------------------------
//  Debug Setup
// ---------------------------------------------------------------------------
#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> SETTING UP DEBUG CALLBACK");
#endif

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
        LOG_ERROR_CAT("Vulkan", "Failed to load vkCreateDebugUtilsMessengerEXT");
        throw std::runtime_error("Failed to load debug extension");
    }

    VK_CHECK(CreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_),
             "Create debug messenger");

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    DEBUG MESSENGER CREATED @ {:p}", static_cast<void*>(debugMessenger_));
    LOG_INFO_CAT("Vulkan", "<<< DEBUG CALLBACK READY");
#endif
}
#endif

// ---------------------------------------------------------------------------
//  2. SHADER LOADING — assets/shaders/
// ---------------------------------------------------------------------------
VkShaderModule VulkanPipelineManager::loadShaderImpl(VkDevice device, const std::string& shaderType)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> LOADING SHADER '{}'", shaderType);
#endif

    const std::string filepath = "assets/shaders/" + shaderType + ".spv";

    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Vulkan", "Failed to open shader: {}", filepath);
        throw std::runtime_error("Failed to open shader: " + filepath);
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("Vulkan", "Invalid SPIR-V size: {}", fileSize);
        throw std::runtime_error("Invalid SPIR-V");
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    if (*reinterpret_cast<const uint32_t*>(buffer.data()) != 0x07230203) {
        LOG_ERROR_CAT("Vulkan", "Invalid SPIR-V magic");
        throw std::runtime_error("Not valid SPIR-V");
    }

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &module), "Create shader module");

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    SHADER MODULE CREATED @ {:p}", static_cast<void*>(module));
    LOG_INFO_CAT("Vulkan", "<<< SHADER '{}' LOADED", shaderType);
#endif
    return module;
}

// ---------------------------------------------------------------------------
//  3. PIPELINE CACHE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createPipelineCache()
{
#ifndef NDEBUG
    LOG_INFO_CAT("PipelineMgr", "Creating pipeline cache...");
#endif

    VkPipelineCacheCreateInfo info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    VK_CHECK(vkCreatePipelineCache(context_.device, &info, nullptr, &pipelineCache_), "Create pipeline cache");

#ifndef NDEBUG
    LOG_INFO_CAT("PipelineMgr", "Pipeline cache created @ {:p}", static_cast<void*>(pipelineCache_));
#endif
}

// ---------------------------------------------------------------------------
//  4. RENDER PASS
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createRenderPass()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> CREATING RENDER PASS");
#endif

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

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    RENDER PASS CREATED @ {:p}", static_cast<void*>(renderPass_));
    LOG_INFO_CAT("Vulkan", "<<< RENDER PASS INITIALIZED");
#endif
}

// ---------------------------------------------------------------------------
//  5. DESCRIPTOR SET LAYOUTS (GRAPHICS)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> CREATING GRAPHICS DESCRIPTOR SET LAYOUT");
#endif

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
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

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &graphicsDescriptorSetLayout_),
             "Create graphics DS layout");

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    LAYOUT CREATED @ {:p}", static_cast<void*>(graphicsDescriptorSetLayout_));
    LOG_INFO_CAT("Vulkan", "<<< GRAPHICS DESCRIPTOR LAYOUT READY");
#endif
}

// ---------------------------------------------------------------------------
//  6. DESCRIPTOR SET LAYOUTS (COMPUTE)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createComputeDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> CREATING COMPUTE DESCRIPTOR SET LAYOUT");
#endif

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
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

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &computeDescriptorSetLayout_),
             "Create compute DS layout");

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< COMPUTE DESCRIPTOR LAYOUT READY");
#endif
}

// ---------------------------------------------------------------------------
//  7. RAY TRACING DESCRIPTOR SET LAYOUT
// ---------------------------------------------------------------------------
VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> CREATING RAY TRACING DESCRIPTOR SET LAYOUT");
#endif

    std::array<VkDescriptorSetLayoutBinding, 11> bindings = {};

    bindings[0]  = {.binding = 0,  .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[1]  = {.binding = 1,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[2]  = {.binding = 2,  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[3]  = {.binding = 3,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR};
    bindings[4]  = {.binding = 4,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[5]  = {.binding = 5,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[6]  = {.binding = 6,  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR};
    bindings[7]  = {.binding = 7,  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[8]  = {.binding = 8,  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[9]  = {.binding = 9,  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[10] = {.binding = 10, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &rayTracingDescriptorSetLayout_),
             "ray tracing descriptor set layout");

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    LAYOUT CREATED @ {:p}", static_cast<void*>(rayTracingDescriptorSetLayout_));
    LOG_INFO_CAT("Vulkan", "<<< RAY TRACING DESCRIPTOR LAYOUT READY");
#endif
    return rayTracingDescriptorSetLayout_;
}

// ---------------------------------------------------------------------------
//  8. TRANSIENT COMMAND POOL
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createTransientCommandPool()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "CREATING TRANSIENT COMMAND POOL");
#endif

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsQueueFamilyIndex
    };

    if (context_.graphicsQueueFamilyIndex == UINT32_MAX) {
        LOG_ERROR_CAT("Vulkan", "graphicsQueueFamilyIndex not initialized!");
        throw std::runtime_error("Queue family index missing");
    }

    VK_CHECK(vkCreateCommandPool(context_.device, &poolInfo, nullptr, &transientPool_), "transient pool");

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< TRANSIENT COMMAND POOL CREATED (family: {})", context_.graphicsQueueFamilyIndex);
#endif
}

// ---------------------------------------------------------------------------
//  9. SHADER BINDING TABLE — FIXED: inline alignUp, shaderHandles_, direct regions
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createShaderBindingTable(VkPhysicalDevice physDev)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> BUILDING SHADER BINDING TABLE (SBT)");
#endif

    if (!rayTracingPipeline_) {
        LOG_ERROR_CAT("Vulkan", "Ray tracing pipeline missing");
        throw std::runtime_error("Ray tracing pipeline missing");
    }

    const uint32_t handleSize = context_.rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = context_.rtProperties.shaderGroupHandleAlignment;
    const uint32_t alignedHandle = ((handleSize + handleAlignment - 1) / handleAlignment) * handleAlignment;
    const uint32_t groupCount = 3;
    const uint32_t sbtSize = groupCount * alignedHandle;

    shaderHandles_.resize(groupCount * handleSize);
    VK_CHECK(context_.vkGetRayTracingShaderGroupHandlesKHR(
                 context_.device, rayTracingPipeline_, 0, groupCount,
                 shaderHandles_.size(), shaderHandles_.data()),
             "Get shader group handles");

    VkBufferCreateInfo bufInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = sbtSize,
        .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_.device, &bufInfo, nullptr, &sbtBuffer_), "Create SBT buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(context_.device, sbtBuffer_, &memReq);

    const uint32_t memType = VulkanInitializer::findMemoryType(
        physDev, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memType
    };
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &sbtMemory_), "Alloc SBT memory");
    VK_CHECK(vkBindBufferMemory(context_.device, sbtBuffer_, sbtMemory_, 0), "Bind SBT memory");

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_.device, sbtMemory_, 0, sbtSize, 0, &mapped), "Map SBT");

    for (uint32_t i = 0; i < groupCount; ++i) {
        uint8_t* dst = static_cast<uint8_t*>(mapped) + i * alignedHandle;
        std::memcpy(dst, shaderHandles_.data() + i * handleSize, handleSize);
    }
    vkUnmapMemory(context_.device, sbtMemory_);

    const VkDeviceAddress baseAddr = VulkanInitializer::getBufferDeviceAddress(context_, sbtBuffer_);

    sbt_.raygen   = { baseAddr + 0 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.miss     = { baseAddr + 1 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.hit      = { baseAddr + 2 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.callable = { 0, 0, 0 };

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< SBT BUILD COMPLETE");
#endif
}

// ---------------------------------------------------------------------------
//  10. RAY TRACING PIPELINE — FIXED: VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createRayTracingPipeline(uint32_t maxRayRecursionDepth)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> COMPILING RAY TRACING PIPELINE");
#endif

    VkShaderModule raygenMod = loadShaderImpl(context_.device, "raytracing/raygen");
    VkShaderModule missMod   = loadShaderImpl(context_.device, "raytracing/miss");
    VkShaderModule hitMod    = loadShaderImpl(context_.device, "raytracing/closesthit");

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = raygenMod, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = missMod, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = hitMod, .pName = "main"}
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0},
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1},
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .closestHitShader = 2}
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rayTracingDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &rayTracingPipelineLayout_),
             "Create RT pipeline layout");

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = maxRayRecursionDepth,
        .layout = rayTracingPipelineLayout_
    };

    VK_CHECK(context_.vkCreateRayTracingPipelinesKHR(
                 context_.device, VK_NULL_HANDLE, pipelineCache_,
                 1, &pipelineInfo, nullptr, &rayTracingPipeline_),
             "Create RT pipeline");

    vkDestroyShaderModule(context_.device, raygenMod, nullptr);
    vkDestroyShaderModule(context_.device, missMod,   nullptr);
    vkDestroyShaderModule(context_.device, hitMod,    nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    PIPELINE COMPILED @ {:p}", static_cast<void*>(rayTracingPipeline_));
    LOG_INFO_CAT("Vulkan", "<<< RAY TRACING PIPELINE LIVE");
#endif
}

// ---------------------------------------------------------------------------
//  13. ACCELERATION STRUCTURES — FIXED: VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createAccelerationStructures(
    VkBuffer vertexBuffer,
    VkBuffer indexBuffer,
    VulkanBufferManager& bufferMgr)
{
#ifndef NDEBUG
    auto asStart = std::chrono::high_resolution_clock::now();
    LOG_INFO_CAT("Vulkan", ">>> BUILDING ACCELERATION STRUCTURES (BLAS + TLAS)");
#endif

    if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "Invalid geometry buffers");
        throw std::runtime_error("Invalid geometry buffers");
    }

    VkDeviceAddress vertexAddress = VulkanInitializer::getBufferDeviceAddress(context_, vertexBuffer);
    VkDeviceAddress indexAddress  = VulkanInitializer::getBufferDeviceAddress(context_, indexBuffer);

    const uint32_t vertexCount   = bufferMgr.getVertexCount();
    const uint32_t indexCount    = bufferMgr.getIndexCount();
    const uint32_t triangleCount = indexCount / 3;

    // BLAS
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertexAddress },
        .vertexStride = sizeof(float) * 3,
        .maxVertex = vertexCount,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = indexAddress }
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
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &triangleCount, &sizeInfo);

    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blasBuffer_, blasMemory_, nullptr, context_);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer_,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &blasCreateInfo, nullptr, &blas_),
             "Create BLAS");

    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer, scratchMemory, nullptr, context_);

    buildInfo.dstAccelerationStructure = blas_;
    buildInfo.scratchData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_, scratchBuffer);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{
        .primitiveCount = triangleCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    BLAS built");
#endif

    // TLAS
    VkTransformMatrixKHR transform{
        .matrix = {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}}
    };

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas_
    };
    const VkDeviceAddress blasAddress = context_.vkGetAccelerationStructureDeviceAddressKHR(context_.device, &addrInfo);

    VkAccelerationStructureInstanceKHR instance{
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = blasAddress
    };

    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer, instanceMemory, nullptr, context_);

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_.device, instanceMemory, 0, sizeof(instance), 0, &mapped), "Map instance");
    std::memcpy(mapped, &instance, sizeof(instance));
    vkUnmapMemory(context_.device, instanceMemory);

    VkDeviceAddress instanceAddress = VulkanInitializer::getBufferDeviceAddress(context_, instanceBuffer);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = instanceAddress }
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
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildInfo, &instanceCount, &tlasSizeInfo);

    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        tlasSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBuffer_, tlasMemory_, nullptr, context_);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer_,
        .size = tlasSizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &tlasCreateInfo, nullptr, &tlas_),
             "Create TLAS");

    VkBuffer tlasScratch = VK_NULL_HANDLE;
    VkDeviceMemory tlasScratchMem = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        tlasSizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasScratch, tlasScratchMem, nullptr, context_);

    tlasBuildInfo.dstAccelerationStructure = tlas_;
    tlasBuildInfo.scratchData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_, tlasScratch);

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;

    VkCommandBuffer tlasCmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(tlasCmd, 1, &tlasBuildInfo, &pTlasRange);
    VulkanInitializer::endSingleTimeCommands(context_, tlasCmd);

    vkDestroyBuffer(context_.device, tlasScratch, nullptr);
    vkFreeMemory(context_.device, tlasScratchMem, nullptr);
    vkDestroyBuffer(context_.device, instanceBuffer, nullptr);
    vkFreeMemory(context_.device, instanceMemory, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    TLAS built");
    auto asEnd = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(asEnd - asStart).count();
    LOG_INFO_CAT("Vulkan", "    AS BUILD TIME: {} ms", ms);
    LOG_INFO_CAT("Vulkan", "<<< ACCELERATION STRUCTURES READY");
#endif
}

// ---------------------------------------------------------------------------
//  14. UPDATE RAY TRACING DESCRIPTOR SET
// ---------------------------------------------------------------------------
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet ds,
                                                          VkAccelerationStructureKHR tlas)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> UPDATING RAY TRACING DESCRIPTOR SET");
#endif

    if (ds == VK_NULL_HANDLE || tlas == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "Invalid descriptor set or TLAS");
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

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< DESCRIPTOR SET UPDATED");
#endif
}

// ---------------------------------------------------------------------------
//  15. FRAME-TIME LOGGING
// ---------------------------------------------------------------------------
void VulkanPipelineManager::logFrameTimeIfSlow(std::chrono::steady_clock::time_point start)
{
#ifndef NDEBUG
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (us > 16666) {
        LOG_WARN_CAT("Perf", "Frame took {} us (approximately {:.1f} FPS)", us, 1'000'000.0 / us);
    }
#endif
}

// ===================================================================
//  EPIC COMPUTE PIPELINE — assets/shaders/compute/compute.comp
// ===================================================================
void VulkanPipelineManager::createComputePipeline()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Compute", ">>> CREATING EPIC COMPUTE PIPELINE (compute.comp)");
#endif

    VkShaderModule computeMod = loadShaderImpl(context_.device, "compute/compute");

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &computeDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &computePipelineLayout_),
             "Create compute pipeline layout");

    struct SpecData {
        uint32_t localSizeX = 8;
        uint32_t localSizeY = 8;
        uint32_t localSizeZ = 1;
    } specData;

    VkSpecializationMapEntry entries[3] = {
        {0, offsetof(SpecData, localSizeX), sizeof(uint32_t)},
        {1, offsetof(SpecData, localSizeY), sizeof(uint32_t)},
        {2, offsetof(SpecData, localSizeZ), sizeof(uint32_t)}
    };

    VkSpecializationInfo specInfo{
        .mapEntryCount = 3,
        .pMapEntries = entries,
        .dataSize = sizeof(specData),
        .pData = &specData
    };

    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = computeMod,
            .pName = "main",
            .pSpecializationInfo = &specInfo
        },
        .layout = computePipelineLayout_
    };

    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &computePipeline_),
             "Create compute pipeline");

    vkDestroyShaderModule(context_.device, computeMod, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Compute", "    PIPELINE @ {:p}", static_cast<void*>(computePipeline_));
    LOG_INFO_CAT("Compute", "<<< EPIC COMPUTE PIPELINE LIVE");
#endif
}

// ===================================================================
//  DISPATCH — FULLY FUNCTIONAL
// ===================================================================
void VulkanPipelineManager::dispatchCompute(uint32_t x, uint32_t y, uint32_t z)
{
    if (computePipeline_ == VK_NULL_HANDLE) {
#ifndef NDEBUG
        LOG_WARN_CAT("Compute", "dispatchCompute() skipped — pipeline not created");
#endif
        return;
    }

    VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout_, 0, 1, &computeDescriptorSet_, 0, nullptr);

    vkCmdDispatch(cmd, (x + 7) / 8, (y + 7) / 8, z);

    VulkanInitializer::endSingleTimeCommands(context_, cmd);
}

} // namespace VulkanRTX