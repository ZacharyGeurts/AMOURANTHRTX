// src/engine/Vulkan/VulkanPipelineManager.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: ALL SHADERS LOADED FROM assets/shaders/
// FIXED: SIGSEGV in ~VulkanPipelineManager() → null-check ALL handles
//        No double destroy | RAII-safe | Device valid until end
// ADDED: Stats Pipeline for Nexus (variance/entropy/grad analysis from prev output)
//        Destructor now cleans stats handles

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
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
    class VulkanRenderer;
}

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
        LOG_WARN_CAT("Vulkan", "{}Validation layer: {}{}", VIOLET_PURPLE, pCallbackData->pMessage, RESET);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        LOG_DEBUG_CAT("Vulkan", "{}Validation layer: {}{}", VIOLET_PURPLE, pCallbackData->pMessage, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}>>> INITIALIZING VULKAN PIPELINE MANAGER [{}x{}]{}{}", VIOLET_PURPLE, width, height, RESET, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}<<< PIPELINE MANAGER INITIALIZED{}", VIOLET_PURPLE, RESET);
#endif
}

VulkanPipelineManager::~VulkanPipelineManager()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> DESTROYING VULKAN PIPELINE MANAGER{}", VIOLET_PURPLE, RESET);
#endif

    VkDevice dev = context_.device;
    if (dev == VK_NULL_HANDLE) {
        LOG_WARN_CAT("Vulkan", "{}Device already destroyed — early exit from ~VulkanPipelineManager(){}", VIOLET_PURPLE, RESET);
        return;
    }

    // === SAFE DESTROY: null-check every handle ===
    if (sbtBuffer_)         { vkDestroyBuffer(dev, sbtBuffer_, nullptr); sbtBuffer_ = VK_NULL_HANDLE; }
    if (sbtMemory_)         { vkFreeMemory(dev, sbtMemory_, nullptr); sbtMemory_ = VK_NULL_HANDLE; }
    if (blas_)              { context_.vkDestroyAccelerationStructureKHR(dev, blas_, nullptr); blas_ = VK_NULL_HANDLE; }
    if (tlas_)              { context_.vkDestroyAccelerationStructureKHR(dev, tlas_, nullptr); tlas_ = VK_NULL_HANDLE; }
    if (blasBuffer_)        { vkDestroyBuffer(dev, blasBuffer_, nullptr); blasBuffer_ = VK_NULL_HANDLE; }
    if (tlasBuffer_)        { vkDestroyBuffer(dev, tlasBuffer_, nullptr); tlasBuffer_ = VK_NULL_HANDLE; }
    if (blasMemory_)        { vkFreeMemory(dev, blasMemory_, nullptr); blasMemory_ = VK_NULL_HANDLE; }
    if (tlasMemory_)        { vkFreeMemory(dev, tlasMemory_, nullptr); tlasMemory_ = VK_NULL_HANDLE; }

    if (rayTracingPipeline_)        { vkDestroyPipeline(dev, rayTracingPipeline_, nullptr); rayTracingPipeline_ = VK_NULL_HANDLE; }
    if (computePipeline_)           { vkDestroyPipeline(dev, computePipeline_, nullptr); computePipeline_ = VK_NULL_HANDLE; }
    if (graphicsPipeline_)          { vkDestroyPipeline(dev, graphicsPipeline_, nullptr); graphicsPipeline_ = VK_NULL_HANDLE; }
    if (rayTracingPipelineLayout_)  { vkDestroyPipelineLayout(dev, rayTracingPipelineLayout_, nullptr); rayTracingPipelineLayout_ = VK_NULL_HANDLE; }
    if (computePipelineLayout_)     { vkDestroyPipelineLayout(dev, computePipelineLayout_, nullptr); computePipelineLayout_ = VK_NULL_HANDLE; }
    if (graphicsPipelineLayout_)    { vkDestroyPipelineLayout(dev, graphicsPipelineLayout_, nullptr); graphicsPipelineLayout_ = VK_NULL_HANDLE; }

    if (renderPass_)                { vkDestroyRenderPass(dev, renderPass_, nullptr); renderPass_ = VK_NULL_HANDLE; }
    if (pipelineCache_)             { vkDestroyPipelineCache(dev, pipelineCache_, nullptr); pipelineCache_ = VK_NULL_HANDLE; }

    if (computeDescriptorSetLayout_)     { vkDestroyDescriptorSetLayout(dev, computeDescriptorSetLayout_, nullptr); computeDescriptorSetLayout_ = VK_NULL_HANDLE; }
    if (rayTracingDescriptorSetLayout_)  { vkDestroyDescriptorSetLayout(dev, rayTracingDescriptorSetLayout_, nullptr); rayTracingDescriptorSetLayout_ = VK_NULL_HANDLE; }
    if (graphicsDescriptorSetLayout_)    { vkDestroyDescriptorSetLayout(dev, graphicsDescriptorSetLayout_, nullptr); graphicsDescriptorSetLayout_ = VK_NULL_HANDLE; }

    if (transientPool_)             { vkDestroyCommandPool(dev, transientPool_, nullptr); transientPool_ = VK_NULL_HANDLE; }

    if (nexusPipeline_)             { vkDestroyPipeline(dev, nexusPipeline_, nullptr); nexusPipeline_ = VK_NULL_HANDLE; }
    if (nexusPipelineLayout_)       { vkDestroyPipelineLayout(dev, nexusPipelineLayout_, nullptr); nexusPipelineLayout_ = VK_NULL_HANDLE; }
    if (nexusDescriptorSetLayout_)  { vkDestroyDescriptorSetLayout(dev, nexusDescriptorSetLayout_, nullptr); nexusDescriptorSetLayout_ = VK_NULL_HANDLE; }

    // Stats pipeline cleanup
    if (statsPipeline_)             { vkDestroyPipeline(dev, statsPipeline_, nullptr); statsPipeline_ = VK_NULL_HANDLE; }
    if (statsPipelineLayout_)       { vkDestroyPipelineLayout(dev, statsPipelineLayout_, nullptr); statsPipelineLayout_ = VK_NULL_HANDLE; }
    if (statsDescriptorSetLayout_)  { vkDestroyDescriptorSetLayout(dev, statsDescriptorSetLayout_, nullptr); statsDescriptorSetLayout_ = VK_NULL_HANDLE; }

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_) {
        auto DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (DestroyDebugUtilsMessengerEXT) {
            DestroyDebugUtilsMessengerEXT(context_.instance, debugMessenger_, nullptr);
        }
        debugMessenger_ = VK_NULL_HANDLE;
    }
#endif

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}<<< PIPELINE MANAGER DESTROYED — ALL HANDLES CLEARED{}", VIOLET_PURPLE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  Debug Setup
// ---------------------------------------------------------------------------
#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> SETTING UP DEBUG CALLBACK{}", VIOLET_PURPLE, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}    DEBUG MESSENGER CREATED @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(debugMessenger_), RESET);
    LOG_INFO_CAT("Vulkan", "{}<<< DEBUG CALLBACK READY{}", VIOLET_PURPLE, RESET);
#endif
}
#endif

// ---------------------------------------------------------------------------
//  2. SHADER LOADING — assets/shaders/
// ---------------------------------------------------------------------------
VkShaderModule VulkanPipelineManager::loadShaderImpl(VkDevice device, const std::string& shaderType)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> LOADING SHADER '{}'{}{}", VIOLET_PURPLE, shaderType, RESET, RESET);
#endif

    const std::string filepath = shaderType;

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
    LOG_INFO_CAT("Vulkan", "{}    SHADER MODULE CREATED @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(module), RESET);
    LOG_INFO_CAT("Vulkan", "{}<<< SHADER '{}' LOADED{}", VIOLET_PURPLE, shaderType, RESET);
#endif
    return module;
}

// ---------------------------------------------------------------------------
//  3. PIPELINE CACHE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createPipelineCache()
{
#ifndef NDEBUG
    LOG_INFO_CAT("PipelineMgr", "{}Creating pipeline cache...{}", VIOLET_PURPLE, RESET);
#endif

    VkPipelineCacheCreateInfo info{ .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    VK_CHECK(vkCreatePipelineCache(context_.device, &info, nullptr, &pipelineCache_), "Create pipeline cache");

#ifndef NDEBUG
    LOG_INFO_CAT("PipelineMgr", "{}Pipeline cache created @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(pipelineCache_), RESET);
#endif
}

// ---------------------------------------------------------------------------
//  4. RENDER PASS
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createRenderPass()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> CREATING RENDER PASS{}", VIOLET_PURPLE, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}    RENDER PASS CREATED @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(renderPass_), RESET);
    LOG_INFO_CAT("Vulkan", "{}<<< RENDER PASS INITIALIZED{}", VIOLET_PURPLE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  5. DESCRIPTOR SET LAYOUTS (GRAPHICS)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> CREATING GRAPHICS DESCRIPTOR SET LAYOUT{}", VIOLET_PURPLE, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}    LAYOUT CREATED @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(graphicsDescriptorSetLayout_), RESET);
    LOG_INFO_CAT("Vulkan", "{}<<< GRAPHICS DESCRIPTOR LAYOUT READY{}", VIOLET_PURPLE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  6. DESCRIPTOR SET LAYOUTS (COMPUTE)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createComputeDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> CREATING COMPUTE DESCRIPTOR SET LAYOUT{}", VIOLET_PURPLE, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}<<< COMPUTE DESCRIPTOR LAYOUT READY{}", VIOLET_PURPLE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  7. RAY TRACING DESCRIPTOR SET LAYOUT
// ---------------------------------------------------------------------------
VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> CREATING RAY TRACING DESCRIPTOR SET LAYOUT{}", VIOLET_PURPLE, RESET);
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
    bindings[5]  = {.binding = 5,  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR};
    bindings[6]  = {.binding = 6,  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR};
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
    LOG_INFO_CAT("Vulkan", "{}    LAYOUT CREATED @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(rayTracingDescriptorSetLayout_), RESET);
    LOG_INFO_CAT("Vulkan", "{}<<< RAY TRACING DESCRIPTOR LAYOUT READY{}", VIOLET_PURPLE, RESET);
#endif
    return rayTracingDescriptorSetLayout_;
}

// ---------------------------------------------------------------------------
//  8. TRANSIENT COMMAND POOL
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createTransientCommandPool()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}CREATING TRANSIENT COMMAND POOL{}", VIOLET_PURPLE, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}<<< TRANSIENT COMMAND POOL CREATED (family: {}){}", VIOLET_PURPLE, context_.graphicsQueueFamilyIndex, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  9. SHADER BINDING TABLE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createShaderBindingTable(VkPhysicalDevice physDev)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> BUILDING SHADER BINDING TABLE (SBT){}", VIOLET_PURPLE, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}<<< SBT BUILD COMPLETE{}", VIOLET_PURPLE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  10. RAY TRACING PIPELINE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createRayTracingPipeline(
    const std::vector<std::string>& shaderPaths,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDescriptorSet descriptorSet)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> CREATING RAY TRACING PIPELINE (dynamic shaders){}", VIOLET_PURPLE, RESET);
#endif

    if (shaderPaths.size() < 3) {
        LOG_ERROR_CAT("Vulkan", "Expected at least 3 shaders (raygen, miss, closesthit)");
        throw std::runtime_error("Insufficient shader paths");
    }

    // Load shaders dynamically
    VkShaderModule raygenMod = loadShaderImpl(device, shaderPaths[0]);
    VkShaderModule missMod   = loadShaderImpl(device, shaderPaths[1]);
    VkShaderModule hitMod    = loadShaderImpl(device, shaderPaths[2]);

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

    // Reuse or create descriptor set layout
    if (!rayTracingDescriptorSetLayout_) {
        rayTracingDescriptorSetLayout_ = createRayTracingDescriptorSetLayout();
    }

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rayTracingDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &rayTracingPipelineLayout_),
             "Create RT pipeline layout");

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = rayTracingPipelineLayout_
    };

    VK_CHECK(context_.vkCreateRayTracingPipelinesKHR(
                 device, VK_NULL_HANDLE, pipelineCache_,
                 1, &pipelineInfo, nullptr, &rayTracingPipeline_),
             "Create RT pipeline");

    // Destroy modules after pipeline creation
    vkDestroyShaderModule(device, raygenMod, nullptr);
    vkDestroyShaderModule(device, missMod, nullptr);
    vkDestroyShaderModule(device, hitMod, nullptr);

    // Build SBT
    createShaderBindingTable(physicalDevice);

    // Update descriptor set with TLAS
    if (tlas_ && descriptorSet) {
        updateRayTracingDescriptorSet(descriptorSet, tlas_);
    }

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}    RT PIPELINE @ {:p}, SBT READY{}", VIOLET_PURPLE, static_cast<void*>(rayTracingPipeline_), RESET);
    LOG_INFO_CAT("Vulkan", "{}<<< RAY TRACING PIPELINE CREATED SUCCESSFULLY{}", VIOLET_PURPLE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  13. ACCELERATION STRUCTURES
//  EXTREME LOGGING: Every step, every pointer, every address, every size
//  COLOR: VIOLET_PURPLE for all VulkanRTX path
//  FIX: Notify renderer after TLAS build via notifyTLASReady()
//  SBT BUILT AFTER TLAS — ORDER ENFORCED
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createAccelerationStructures(
    VkBuffer vertexBuffer,
    VkBuffer indexBuffer,
    VulkanBufferManager& bufferMgr,
    VulkanRenderer* renderer)  // renderer for notifyTLASReady
{
    const auto asStart = std::chrono::high_resolution_clock::now();
    LOG_INFO_CAT("Vulkan", "{}>>> BUILDING ACCELERATION STRUCTURES (BLAS + TLAS){}{}", 
                 VIOLET_PURPLE, Logging::Color::RESET, RESET);

    // -----------------------------------------------------------------
    // 1. VALIDATE INPUT BUFFERS
    // -----------------------------------------------------------------
    if (vertexBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "FATAL: vertexBuffer is VK_NULL_HANDLE");
        throw std::runtime_error("Invalid vertex buffer");
    }
    if (indexBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "FATAL: indexBuffer is VK_NULL_HANDLE");
        throw std::runtime_error("Invalid index buffer");
    }

    LOG_INFO_CAT("Vulkan", "{}    vertexBuffer @ {:p}, indexBuffer @ {:p}{}", 
                 VIOLET_PURPLE,
                 static_cast<void*>(vertexBuffer), static_cast<void*>(indexBuffer),
                 RESET);

    VkDeviceAddress vertexAddress = VulkanInitializer::getBufferDeviceAddress(context_, vertexBuffer);
    VkDeviceAddress indexAddress  = VulkanInitializer::getBufferDeviceAddress(context_, indexBuffer);

    LOG_INFO_CAT("Vulkan", "{}    vertexAddress = 0x{:x}, indexAddress = 0x{:x}{}", 
                 VIOLET_PURPLE, vertexAddress, indexAddress, RESET);

    const uint32_t vertexCount   = bufferMgr.getVertexCount();
    const uint32_t indexCount    = bufferMgr.getIndexCount();
    const uint32_t triangleCount = indexCount / 3;

    LOG_INFO_CAT("Vulkan", "{}    Geometry: {} verts, {} indices ({} triangles){}", 
                 VIOLET_PURPLE, vertexCount, indexCount, triangleCount, RESET);

    if (triangleCount == 0) {
        LOG_ERROR_CAT("Vulkan", "FATAL: No triangles (indexCount={})", indexCount);
        throw std::runtime_error("No geometry to build AS");
    }

    // -----------------------------------------------------------------
    // 2. BLAS: BUILD GEOMETRY + SIZES
    // -----------------------------------------------------------------
    LOG_INFO_CAT("Vulkan", "{}    === BUILDING BOTTOM-LEVEL AS (BLAS) ==={}", 
                 VIOLET_PURPLE, RESET);

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

    LOG_INFO_CAT("Vulkan", "{}    vkGetAccelerationStructureBuildSizesKHR (BLAS)...{}", 
                 VIOLET_PURPLE, RESET);
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &triangleCount, &sizeInfo);

    LOG_INFO_CAT("Vulkan", "{}    BLAS sizes: AS={} bytes, scratch={} bytes{}", 
                 VIOLET_PURPLE,
                 sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize,
                 RESET);

    // -----------------------------------------------------------------
    // 3. CREATE BLAS BUFFER + MEMORY
    // -----------------------------------------------------------------
    LOG_INFO_CAT("Vulkan", "{}    Creating BLAS storage buffer: {} bytes{}", 
                 VIOLET_PURPLE, sizeInfo.accelerationStructureSize, RESET);
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blasBuffer_, blasMemory_, nullptr, context_);

    LOG_INFO_CAT("Vulkan", "{}    BLAS buffer @ {:p}, memory @ {:p}{}", 
                 VIOLET_PURPLE,
                 static_cast<void*>(blasBuffer_), static_cast<void*>(blasMemory_),
                 RESET);

    // -----------------------------------------------------------------
    // 4. CREATE BLAS OBJECT
    // -----------------------------------------------------------------
    VkAccelerationStructureCreateInfoKHR blasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer_,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    LOG_INFO_CAT("Vulkan", "{}    vkCreateAccelerationStructureKHR (BLAS)...{}", 
                 VIOLET_PURPLE, RESET);
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &blasCreateInfo, nullptr, &blas_),
             "vkCreateAccelerationStructureKHR (BLAS)");

    LOG_INFO_CAT("Vulkan", "{}    BLAS OBJECT CREATED @ {:p}{}", 
                 VIOLET_PURPLE, static_cast<void*>(blas_), RESET);

    // -----------------------------------------------------------------
    // 5. CREATE SCRATCH BUFFER
    // -----------------------------------------------------------------
    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;

    LOG_INFO_CAT("Vulkan", "{}    Creating scratch buffer: {} bytes{}", 
                 VIOLET_PURPLE, sizeInfo.buildScratchSize, RESET);
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer, scratchMemory, nullptr, context_);

    VkDeviceAddress scratchAddr = VulkanInitializer::getBufferDeviceAddress(context_, scratchBuffer);
    LOG_INFO_CAT("Vulkan", "{}    Scratch buffer @ {:p}, address = 0x{:x}{}", 
                 VIOLET_PURPLE,
                 static_cast<void*>(scratchBuffer), scratchAddr, RESET);

    // -----------------------------------------------------------------
    // 6. BUILD BLAS
    // -----------------------------------------------------------------
    buildInfo.dstAccelerationStructure = blas_;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{
        .primitiveCount = triangleCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    LOG_INFO_CAT("Vulkan", "{}    Recording BLAS build command...{}", 
                 VIOLET_PURPLE, RESET);
    VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    LOG_INFO_CAT("Vulkan", "{}    BLAS BUILD SUBMITTED AND SYNCHRONIZED{}", 
                 VIOLET_PURPLE, RESET);

    // Cleanup scratch
    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

    LOG_INFO_CAT("Vulkan", "{}    BLAS built @ {:p}{}", 
                 VIOLET_PURPLE, static_cast<void*>(blas_), RESET);

    // -----------------------------------------------------------------
    // 7. TLAS: BUILD INSTANCE + GEOMETRY
    // -----------------------------------------------------------------
    LOG_INFO_CAT("Vulkan", "{}    === BUILDING TOP-LEVEL AS (TLAS) ==={}", 
                 VIOLET_PURPLE, RESET);

    VkTransformMatrixKHR transform{
        .matrix = {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}}
    };

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas_
    };
    VkDeviceAddress blasAddress = context_.vkGetAccelerationStructureDeviceAddressKHR(context_.device, &addrInfo);
    LOG_INFO_CAT("Vulkan", "{}    BLAS device address = 0x{:x}{}", 
                 VIOLET_PURPLE, blasAddress, RESET);

    VkAccelerationStructureInstanceKHR instance{
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = blasAddress
    };

    // -----------------------------------------------------------------
    // 8. CREATE INSTANCE BUFFER
    // -----------------------------------------------------------------
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;

    LOG_INFO_CAT("Vulkan", "{}    Creating instance buffer: {} bytes{}", 
                 VIOLET_PURPLE, sizeof(instance), RESET);
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer, instanceMemory, nullptr, context_);

    LOG_INFO_CAT("Vulkan", "{}    Instance buffer @ {:p}, memory @ {:p}{}", 
                 VIOLET_PURPLE,
                 static_cast<void*>(instanceBuffer), static_cast<void*>(instanceMemory),
                 RESET);

    // -----------------------------------------------------------------
    // 9. MAP + FILL INSTANCE
    // -----------------------------------------------------------------
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_.device, instanceMemory, 0, sizeof(instance), 0, &mapped), "Map instance buffer");
    std::memcpy(mapped, &instance, sizeof(instance));
    vkUnmapMemory(context_.device, instanceMemory);
    LOG_INFO_CAT("Vulkan", "{}    Instance data written and unmapped{}", 
                 VIOLET_PURPLE, RESET);

    VkDeviceAddress instanceAddress = VulkanInitializer::getBufferDeviceAddress(context_, instanceBuffer);
    LOG_INFO_CAT("Vulkan", "{}    Instance buffer address = 0x{:x}{}", 
                 VIOLET_PURPLE, instanceAddress, RESET);

    // -----------------------------------------------------------------
    // 10. TLAS GEOMETRY + SIZES
    // -----------------------------------------------------------------
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

    LOG_INFO_CAT("Vulkan", "{}    vkGetAccelerationStructureBuildSizesKHR (TLAS)...{}", 
                 VIOLET_PURPLE, RESET);
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildInfo, &instanceCount, &tlasSizeInfo);

    LOG_INFO_CAT("Vulkan", "{}    TLAS sizes: AS={} bytes, scratch={} bytes{}", 
                 VIOLET_PURPLE,
                 tlasSizeInfo.accelerationStructureSize, tlasSizeInfo.buildScratchSize,
                 RESET);

    // -----------------------------------------------------------------
    // 11. CREATE TLAS BUFFER
    // -----------------------------------------------------------------
    LOG_INFO_CAT("Vulkan", "{}    Creating TLAS storage buffer: {} bytes{}", 
                 VIOLET_PURPLE, tlasSizeInfo.accelerationStructureSize, RESET);
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        tlasSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBuffer_, tlasMemory_, nullptr, context_);

    LOG_INFO_CAT("Vulkan", "{}    TLAS buffer @ {:p}, memory @ {:p}{}", 
                 VIOLET_PURPLE,
                 static_cast<void*>(tlasBuffer_), static_cast<void*>(tlasMemory_),
                 RESET);

    // -----------------------------------------------------------------
    // 12. CREATE TLAS OBJECT
    // -----------------------------------------------------------------
    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer_,
        .size = tlasSizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    LOG_INFO_CAT("Vulkan", "{}    vkCreateAccelerationStructureKHR (TLAS)...{}", 
                 VIOLET_PURPLE, RESET);
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &tlasCreateInfo, nullptr, &tlas_),
             "vkCreateAccelerationStructureKHR (TLAS)");

    LOG_INFO_CAT("Vulkan", "{}    TLAS OBJECT CREATED @ {:p}{}", 
                 VIOLET_PURPLE, static_cast<void*>(tlas_), RESET);

    // -----------------------------------------------------------------
    // 13. CREATE TLAS SCRATCH
    // -----------------------------------------------------------------
    VkBuffer tlasScratch = VK_NULL_HANDLE;
    VkDeviceMemory tlasScratchMem = VK_NULL_HANDLE;

    LOG_INFO_CAT("Vulkan", "{}    Creating TLAS scratch buffer: {} bytes{}", 
                 VIOLET_PURPLE, tlasSizeInfo.buildScratchSize, RESET);
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        tlasSizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasScratch, tlasScratchMem, nullptr, context_);

    VkDeviceAddress tlasScratchAddr = VulkanInitializer::getBufferDeviceAddress(context_, tlasScratch);
    LOG_INFO_CAT("Vulkan", "{}    TLAS scratch address = 0x{:x}{}", 
                 VIOLET_PURPLE, tlasScratchAddr, RESET);

    // -----------------------------------------------------------------
    // 14. BUILD TLAS
    // -----------------------------------------------------------------
    tlasBuildInfo.dstAccelerationStructure = tlas_;
    tlasBuildInfo.scratchData.deviceAddress = tlasScratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;

    LOG_INFO_CAT("Vulkan", "{}    Recording TLAS build command...{}", 
                 VIOLET_PURPLE, RESET);
    VkCommandBuffer tlasCmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(tlasCmd, 1, &tlasBuildInfo, &pTlasRange);
    VulkanInitializer::endSingleTimeCommands(context_, tlasCmd);

    LOG_INFO_CAT("Vulkan", "{}    TLAS BUILD SUBMITTED AND SYNCHRONIZED{}", 
                 VIOLET_PURPLE, RESET);

    // Cleanup
    vkDestroyBuffer(context_.device, tlasScratch, nullptr);
    vkFreeMemory(context_.device, tlasScratchMem, nullptr);
    vkDestroyBuffer(context_.device, instanceBuffer, nullptr);
    vkFreeMemory(context_.device, instanceMemory, nullptr);

    LOG_INFO_CAT("Vulkan", "{}    TLAS built @ {:p}{}", 
                 VIOLET_PURPLE, static_cast<void*>(tlas_), RESET);

    // -----------------------------------------------------------------
    // 15. NOTIFY RENDERER — SBT BUILT AFTER TLAS
    // -----------------------------------------------------------------
    if (tlas_ != VK_NULL_HANDLE && renderer) {
        LOG_INFO_CAT("Vulkan", "{}    TLAS READY @ {:p} — NOTIFYING RENDERER{}", 
                     VIOLET_PURPLE, static_cast<void*>(tlas_), RESET);
        renderer->notifyTLASReady(tlas_);
    } else {
        LOG_ERROR_CAT("Vulkan", "FATAL: TLAS build failed or renderer is null");
        throw std::runtime_error("TLAS build failed");
    }

    // -----------------------------------------------------------------
    // 16. FINAL TIMING
    // -----------------------------------------------------------------
    const auto asEnd = std::chrono::high_resolution_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(asEnd - asStart).count();

    LOG_INFO_CAT("Vulkan", "{}    AS BUILD TIME: {} ms{}", 
                 VIOLET_PURPLE, ms, RESET);
    LOG_INFO_CAT("Vulkan", "{}<<< ACCELERATION STRUCTURES READY — TLAS @ {:p}{}", 
                 VIOLET_PURPLE, static_cast<void*>(tlas_), RESET);
}

// ---------------------------------------------------------------------------
//  14. UPDATE RAY TRACING DESCRIPTOR SET
// ---------------------------------------------------------------------------
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet ds,
                                                          VkAccelerationStructureKHR tlas)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> UPDATING RAY TRACING DESCRIPTOR SET{}", VIOLET_PURPLE, RESET);
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
    LOG_INFO_CAT("Vulkan", "{}<<< DESCRIPTOR SET UPDATED{}", VIOLET_PURPLE, RESET);
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
        LOG_WARN_CAT("Perf", "{}Frame took {} us (approximately {:.1f} FPS){}", VIOLET_PURPLE, us, 1'000'000.0 / us, RESET);
    }
#endif
}

// ===================================================================
//  EPIC COMPUTE PIPELINE — assets/shaders/compute/compute.comp
// ===================================================================
void VulkanPipelineManager::createComputePipeline()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Compute", "{}>>> CREATING EPIC COMPUTE PIPELINE (compute.comp){}", VIOLET_PURPLE, RESET);
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
    LOG_INFO_CAT("Compute", "{}    PIPELINE @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(computePipeline_), RESET);
    LOG_INFO_CAT("Compute", "{}<<< EPIC COMPUTE PIPELINE LIVE{}", VIOLET_PURPLE, RESET);
#endif
}

// ===================================================================
//  DISPATCH — FULLY FUNCTIONAL
// ===================================================================
void VulkanPipelineManager::dispatchCompute(uint32_t x, uint32_t y, uint32_t z)
{
    if (computePipeline_ == VK_NULL_HANDLE) {
#ifndef NDEBUG
        LOG_WARN_CAT("Compute", "{}dispatchCompute() skipped — pipeline not created{}", VIOLET_PURPLE, RESET);
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

// ---------------------------------------------------------------------------
//  NEXUS: DESCRIPTOR SET LAYOUT (4 bindings)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createNexusDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Nexus", "{}>>> CREATING NEXUS DESCRIPTOR SET LAYOUT{}", VIOLET_PURPLE, RESET);
#endif

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

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &nexusDescriptorSetLayout_),
             "Create Nexus descriptor set layout");

#ifndef NDEBUG
    LOG_INFO_CAT("Nexus", "{}    LAYOUT @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(nexusDescriptorSetLayout_), RESET);
    LOG_INFO_CAT("Nexus", "{}<<< NEXUS DESCRIPTOR LAYOUT READY{}", VIOLET_PURPLE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  NEXUS: COMPUTE PIPELINE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createNexusPipeline()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Nexus", "{}>>> CREATING NEXUS DECISION PIPELINE (nexusDecision.comp){}", VIOLET_PURPLE, RESET);
#endif

    createNexusDescriptorSetLayout();

    VkPushConstantRange pushConst{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 32  // Exact size for NexusPushConsts (5f + uint + 2f = 32 bytes)
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &nexusDescriptorSetLayout_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConst
    };

    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &nexusPipelineLayout_),
             "Create Nexus pipeline layout");

    VkShaderModule shader = loadShaderImpl(context_.device, "compute/nexusDecision");

    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main"
        },
        .layout = nexusPipelineLayout_
    };

    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &nexusPipeline_),
             "Create Nexus compute pipeline");

    vkDestroyShaderModule(context_.device, shader, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Nexus", "{}    PIPELINE @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(nexusPipeline_), RESET);
    LOG_INFO_CAT("Nexus", "{}<<< NEXUS DECISION PIPELINE LIVE{}", VIOLET_PURPLE, RESET);
#endif
}

// ===================================================================
//  STATS ANALYZER PIPELINE — assets/shaders/compute/statsAnalyzer.comp
//  Analyzes prev output for variance/entropy/grad → writes to BufferStats
// ===================================================================
void VulkanPipelineManager::createStatsPipeline()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Stats", "{}>>> CREATING STATS ANALYZER PIPELINE (statsAnalyzer.comp){}", VIOLET_PURPLE, RESET);
#endif

    // Descriptor set layout: binding 0=storage_image (prev output), binding 1=storage_buffer (stats out)
    std::array<VkDescriptorSetLayoutBinding, 2> statsBindings = {{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},  // prevOutput (readonly)
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}  // stats buffer (writeonly)
    }};

    VkDescriptorSetLayoutCreateInfo statsLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(statsBindings.size()),
        .pBindings = statsBindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &statsLayoutInfo, nullptr, &statsDescriptorSetLayout_),
             "Create Stats descriptor set layout");

#ifndef NDEBUG
    LOG_INFO_CAT("Stats", "{}    STATS LAYOUT @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(statsDescriptorSetLayout_), RESET);
#endif

    // Pipeline layout (no push constants needed)
    VkPipelineLayoutCreateInfo statsPipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &statsDescriptorSetLayout_
    };

    VK_CHECK(vkCreatePipelineLayout(context_.device, &statsPipelineLayoutInfo, nullptr, &statsPipelineLayout_),
             "Create Stats pipeline layout");

#ifndef NDEBUG
    LOG_INFO_CAT("Stats", "{}    STATS PIPELINE LAYOUT @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(statsPipelineLayout_), RESET);
#endif

    // Load shader
    VkShaderModule statsShader = loadShaderImpl(context_.device, "compute/statsAnalyzer");

    // Specialization for local size (16x16 tiles for image analysis)
    struct StatsSpecData {
        uint32_t localSizeX = 16;
        uint32_t localSizeY = 16;
        uint32_t localSizeZ = 1;
    } statsSpecData;

    VkSpecializationMapEntry statsEntries[3] = {
        {0, offsetof(StatsSpecData, localSizeX), sizeof(uint32_t)},
        {1, offsetof(StatsSpecData, localSizeY), sizeof(uint32_t)},
        {2, offsetof(StatsSpecData, localSizeZ), sizeof(uint32_t)}
    };

    VkSpecializationInfo statsSpecInfo{
        .mapEntryCount = 3,
        .pMapEntries = statsEntries,
        .dataSize = sizeof(statsSpecData),
        .pData = &statsSpecData
    };

    VkComputePipelineCreateInfo statsPipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = statsShader,
            .pName = "main",
            .pSpecializationInfo = &statsSpecInfo
        },
        .layout = statsPipelineLayout_
    };

    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &statsPipelineInfo, nullptr, &statsPipeline_),
             "Create Stats compute pipeline");

    vkDestroyShaderModule(context_.device, statsShader, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Stats", "{}    STATS PIPELINE @ {:p}{}", VIOLET_PURPLE, static_cast<void*>(statsPipeline_), RESET);
    LOG_INFO_CAT("Stats", "{}<<< STATS ANALYZER PIPELINE LIVE{}", VIOLET_PURPLE, RESET);
#endif
}

// ===================================================================
//  STATS DISPATCH — Call after RT to fill BufferStats
// ===================================================================
void VulkanPipelineManager::dispatchStats(VkCommandBuffer cmd, VkDescriptorSet statsSet)
{
    if (statsPipeline_ == VK_NULL_HANDLE) {
#ifndef NDEBUG
        LOG_WARN_CAT("Stats", "{}dispatchStats() skipped — pipeline not created{}", VIOLET_PURPLE, RESET);
#endif
        return;
    }

    // Bind and dispatch (full image: groups = (width+15)/16, (height+15)/16, 1)
    uint32_t gx = (width_ + 15) / 16;
    uint32_t gy = (height_ + 15) / 16;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, statsPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, statsPipelineLayout_, 0, 1, &statsSet, 0, nullptr);
    vkCmdDispatch(cmd, gx, gy, 1);

#ifndef NDEBUG
    LOG_DEBUG_CAT("Stats", "{}Dispatched stats analyzer: {}x{} groups{}", VIOLET_PURPLE, gx, gy, RESET);
#endif
}

} // namespace VulkanRTX