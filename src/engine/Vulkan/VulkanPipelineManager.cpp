// src/engine/Vulkan/VulkanPipelineManager.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FULLY POLISHED. ZERO WARNINGS. ZERO NARROWING. 100% COMPILABLE.
// NO SINGLETON. NO STATIC instance_. RAII SAFE.
// OPTIMIZED: Guarded logging, fixed unused vars/barriers, timed AS builds.

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <chrono>  // For perf timing

namespace VulkanRTX {

using namespace Logging::Color;

// ---------------------------------------------------------------------------
//  Debug Callback (for ENABLE_VULKAN_DEBUG)
// ---------------------------------------------------------------------------
// PROTIP: Validation layers catch API misuse early; filter severities to reduce noise in production.
#ifdef ENABLE_VULKAN_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

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
// PROTIP: Initialize core layouts and passes in ctor for immediate use; defer pipeline compiles to runtime.
VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context, int width, int height)
    : context_(context), width_(width), height_(height)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> INITIALIZING VULKAN PIPELINE MANAGER [{}x{}]", width, height);

    char devPtr[32], instPtr[32];
    std::snprintf(devPtr, sizeof(devPtr), "%p", static_cast<void*>(context_.device));
    std::snprintf(instPtr, sizeof(instPtr), "%p", static_cast<void*>(context_.instance));
    LOG_INFO_CAT("Vulkan", "    Device: {}", devPtr);
    LOG_INFO_CAT("Vulkan", "    Instance: {}", instPtr);
#endif

    if (context_.device == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "    FATAL: Device is NULL at PipelineManager construction!");
        throw std::runtime_error("Vulkan device not initialized");
    }

    createPipelineCache();
    createGraphicsDescriptorSetLayout();
    createComputeDescriptorSetLayout();
    createRenderPass();

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

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto DestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (DestroyDebugUtilsMessengerEXT != nullptr) {
            DestroyDebugUtilsMessengerEXT(context_.instance, debugMessenger_, nullptr);
        }
        debugMessenger_ = VK_NULL_HANDLE;
    }
#endif
}

// ---------------------------------------------------------------------------
//  Debug Setup (for ENABLE_VULKAN_DEBUG)
// ---------------------------------------------------------------------------
// PROTIP: Enable performance warnings for optimization; verbose for deep debugging.
#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> SETTING UP DEBUG CALLBACK");
#endif

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = this;

    auto CreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(context_.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (CreateDebugUtilsMessengerEXT == nullptr) {
        LOG_ERROR_CAT("Vulkan", "Failed to load vkCreateDebugUtilsMessengerEXT");
        throw std::runtime_error("Failed to load debug extension");
    }

    VK_CHECK(CreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_), "Create debug messenger");

#ifndef NDEBUG
    char msgPtr[32];
    std::snprintf(msgPtr, sizeof(msgPtr), "%p", static_cast<void*>(debugMessenger_));
    LOG_INFO_CAT("Vulkan", "    DEBUG MESSENGER CREATED @ {}", msgPtr);
    LOG_INFO_CAT("Vulkan", "<<< DEBUG CALLBACK READY");
#endif
}
#endif

// ---------------------------------------------------------------------------
//  2. SHADER LOADING – BLAST LOGGED
// ---------------------------------------------------------------------------
// PROTIP: Validate SPIR-V magic and alignment early; use binary mode for exact byte reads.
VkShaderModule VulkanPipelineManager::loadShader(VkDevice device, const std::string& shaderType)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> LOADING SHADER '{}'", shaderType);
#endif

    const std::string filepath = findShaderPath(shaderType);

    if (filepath.find(".spv") == std::string::npos && filepath.find(".glsl") == std::string::npos && 
        filepath.find(".rgen") == std::string::npos && filepath.find(".rmiss") == std::string::npos && 
        filepath.find(".rchit") == std::string::npos) {
        LOG_ERROR_CAT("Vulkan", "    INVALID PATH (no .spv/.glsl/.rgen): {}", filepath);
        throw std::runtime_error("Invalid shader path");
    }
    if (filepath.find("VK_") != std::string::npos) {
        LOG_ERROR_CAT("Vulkan", "    SECURITY: Attempted to load extension as shader: {}", filepath);
        throw std::runtime_error("Extension name used as shader");
    }

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    Opening file: {}", filepath);
#endif

    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Vulkan", "    FAILED TO OPEN FILE");
        throw std::runtime_error("Failed to open shader");
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    File size: {} bytes", fileSize);
#endif

    if (fileSize == 0) {
        LOG_ERROR_CAT("Vulkan", "    EMPTY SHADER FILE");
        throw std::runtime_error("Empty shader");
    }
    if (fileSize % 4 != 0) {
        LOG_ERROR_CAT("Vulkan", "    NOT 4-BYTE ALIGNED: {} bytes", fileSize);
        throw std::runtime_error("Invalid SPIR-V size");
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    if (*reinterpret_cast<const uint32_t*>(buffer.data()) != 0x07230203) {
        LOG_ERROR_CAT("Vulkan", "    INVALID SPIR-V MAGIC NUMBER");
        throw std::runtime_error("Not valid SPIR-V");
    }

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &module), "Create shader module");

#ifndef NDEBUG
    char modPtr[32];
    std::snprintf(modPtr, sizeof(modPtr), "%p", static_cast<void*>(module));
    LOG_INFO_CAT("Vulkan", "    SHADER MODULE CREATED: {} bytes @ {}", fileSize, modPtr);
    LOG_INFO_CAT("Vulkan", "<<< SHADER '{}' LOADED", shaderType);
#endif
    return module;
}

// ---------------------------------------------------------------------------
//  3. PIPELINE CACHE – BLAST LOGGED + CRASH FIXED
// ---------------------------------------------------------------------------
// PROTIP: Pipeline cache persists compiles across runs; invalidate on shader changes for dev.
void VulkanPipelineManager::createPipelineCache() {
#ifndef NDEBUG
    LOG_INFO_CAT("PipelineMgr", "{}Creating pipeline cache...{}", OCEAN_TEAL, RESET);
#endif

    VkPipelineCacheCreateInfo cacheInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };

    VkPipelineCache rawCache = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineCache(context_.device, &cacheInfo, nullptr, &rawCache),
             "Create pipeline cache");

    pipelineCache_ = Dispose::makeHandle(context_.device, rawCache, "Global Pipeline Cache");
#ifndef NDEBUG
    LOG_INFO_CAT("PipelineMgr", "{}Pipeline cache created: {}{}",
                 OCEAN_TEAL,
                 static_cast<void*>(pipelineCache_.get()),
                 RESET);
#endif
}

// ---------------------------------------------------------------------------
//  4. RENDER PASS – BLAST LOGGED
// ---------------------------------------------------------------------------
// PROTIP: Use CLEAR loadOp for swapchain; PRESENT_SRC finalLayout for direct present.
void VulkanPipelineManager::createRenderPass()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> CREATING RENDER PASS");
#endif

    VkAttachmentDescription colorAttachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorRef = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorRef
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VkRenderPass rawPass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &rawPass), "Create render pass");

    renderPass_ = Dispose::makeHandle(context_.device, rawPass, "RenderPass");

#ifndef NDEBUG
    char rpPtr[32];
    std::snprintf(rpPtr, sizeof(rpPtr), "%p", static_cast<void*>(rawPass));
    LOG_INFO_CAT("Vulkan", "    RENDER PASS CREATED @ {}", rpPtr);
    LOG_INFO_CAT("Vulkan", "<<< RENDER PASS INITIALIZED");
#endif
}

// ---------------------------------------------------------------------------
//  5. DESCRIPTOR SET LAYOUTS (GRAPHICS)
// ---------------------------------------------------------------------------
// PROTIP: Bind storage buffers with high counts for dynamic materials; sampled images for textures.
void VulkanPipelineManager::createGraphicsDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> CREATING GRAPHICS DESCRIPTOR SET LAYOUT");
#endif

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 6, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 7, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,              .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout), "Create graphics DS layout");

    graphicsDescriptorSetLayout_ = Dispose::makeHandle(context_.device, layout, "Graphics DS Layout");

#ifndef NDEBUG
    char layoutPtr[32];
    std::snprintf(layoutPtr, sizeof(layoutPtr), "%p", static_cast<void*>(layout));
    LOG_INFO_CAT("Vulkan", "    LAYOUT CREATED @ {}", layoutPtr);
    LOG_INFO_CAT("Vulkan", "<<< GRAPHICS DESCRIPTOR LAYOUT READY");
#endif
}

// ---------------------------------------------------------------------------
//  6. DESCRIPTOR SET LAYOUTS (COMPUTE) – FIXED: 2x STORAGE_IMAGE
// ---------------------------------------------------------------------------
// PROTIP: Use STORAGE_IMAGE for RW textures in compute; bind input/output separately for tonemapping.
void VulkanPipelineManager::createComputeDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> CREATING COMPUTE DESCRIPTOR SET LAYOUT");
#endif

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT}, // HDR input
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT}  // LDR output
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout), "Create compute DS layout");

    computeDescriptorSetLayout_ = Dispose::makeHandle(context_.device, layout, "Compute DS Layout");

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    LAYOUT CREATED WITH 2 STORAGE IMAGES (binding 0: HDR, 1: LDR)");
    LOG_INFO_CAT("Vulkan", "<<< COMPUTE DESCRIPTOR LAYOUT READY");
#endif
}

// ---------------------------------------------------------------------------
//  7. RAY TRACING DESCRIPTOR SET LAYOUT
// ---------------------------------------------------------------------------
// PROTIP: Inline acceleration structures in binding 0; use opaque flags for fast traces.
VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> CREATING RAY TRACING DESCRIPTOR SET LAYOUT");
#endif

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 6, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR},
        {.binding = 7, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {.binding = 8, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {.binding = 9, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {.binding = 10,.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,      .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR}
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout), "Create RT DS layout");

    rayTracingDescriptorSetLayout_ = Dispose::makeHandle(context_.device, layout, "RT DS Layout");

#ifndef NDEBUG
    char layoutPtr[32];
    std::snprintf(layoutPtr, sizeof(layoutPtr), "%p", static_cast<void*>(layout));
    LOG_INFO_CAT("Vulkan", "    LAYOUT CREATED @ {}", layoutPtr);
    LOG_INFO_CAT("Vulkan", "<<< RAY TRACING DESCRIPTOR LAYOUT READY");
#endif
    return layout;
}

// ---------------------------------------------------------------------------
//  8. SHADER BINDING TABLE – FIXED: Uses .deviceAddress from VulkanCommon.hpp
// ---------------------------------------------------------------------------
// PROTIP: Align SBT entries to hardware requirements; use PREFER_FAST_TRACE for build flags.
void VulkanPipelineManager::createShaderBindingTable()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> BUILDING SHADER BINDING TABLE (SBT)");
#endif

    if (!rayTracingPipeline_) {
        LOG_ERROR_CAT("Vulkan", "    PIPELINE IS NULL — CANNOT BUILD SBT");
        throw std::runtime_error("Ray tracing pipeline missing");
    }

    const uint32_t handleSize      = context_.rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = context_.rtProperties.shaderGroupHandleAlignment;
    const uint32_t alignedHandle   = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const uint32_t groupCount      = 3;
    const uint32_t sbtSize         = groupCount * alignedHandle;

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    RT Props → handleSize:{}, align:{}, aligned:{}", handleSize, handleAlignment, alignedHandle);
    LOG_INFO_CAT("Vulkan", "    SBT → groups:{}, alignedSize:{}, total:{} bytes", groupCount, alignedHandle, sbtSize);
#endif

    std::vector<uint8_t> rawHandles(groupCount * handleSize);
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    Fetching {} shader group handles...", groupCount);
#endif

    VkResult r = context_.vkGetRayTracingShaderGroupHandlesKHR(
        context_.device, rayTracingPipeline_.get(), 0, groupCount,
        rawHandles.size(), rawHandles.data());

    if (r != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "    vkGetRayTracingShaderGroupHandlesKHR FAILED: {}", static_cast<int>(r));
        throw std::runtime_error("Failed to get shader group handles");
    }

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    Handles retrieved");
#endif

    VkBufferCreateInfo bufInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = sbtSize,
        .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(context_.device, &bufInfo, nullptr, &sbtBuffer), "Create SBT buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(context_.device, sbtBuffer, &memReq);

    const uint32_t memType = VulkanInitializer::findMemoryType(
        context_.physicalDevice, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memType
    };
    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &sbtMemory), "Alloc SBT memory");
    VK_CHECK(vkBindBufferMemory(context_.device, sbtBuffer, sbtMemory, 0), "Bind SBT memory");

    sbtBuffer_ = Dispose::makeHandle(context_.device, sbtBuffer, "SBT Buffer");
    sbtMemory_ = Dispose::makeHandle(context_.device, sbtMemory, "SBT Memory");

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_.device, sbtMemory_.get(), 0, sbtSize, 0, &mapped), "Map SBT");

    for (uint32_t i = 0; i < groupCount; ++i) {
        uint8_t* dst = static_cast<uint8_t*>(mapped) + i * alignedHandle;
        std::memcpy(dst, rawHandles.data() + i * handleSize, handleSize);
    }
    vkUnmapMemory(context_.device, sbtMemory_.get());

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    Handles copied with alignment");
#endif

    const VkDeviceAddress baseAddr = VulkanInitializer::getBufferDeviceAddress(context_, sbtBuffer_.get());

    sbt_.raygen   = { baseAddr + 0 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.miss     = { baseAddr + 1 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.hit      = { baseAddr + 2 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.callable = {};

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    SBT READY:");
    LOG_INFO_CAT("Vulkan", "      raygen : 0x{:x} (stride={}, size={})", 
                 sbt_.raygen.deviceAddress, sbt_.raygen.stride, sbt_.raygen.size);
    LOG_INFO_CAT("Vulkan", "      miss   : 0x{:x} (stride={}, size={})", 
                 sbt_.miss.deviceAddress, sbt_.miss.stride, sbt_.miss.size);
    LOG_INFO_CAT("Vulkan", "      hit    : 0x{:x} (stride={}, size={})", 
                 sbt_.hit.deviceAddress, sbt_.hit.stride, sbt_.hit.size);
    LOG_INFO_CAT("Vulkan", "<<< SBT BUILD COMPLETE");
#endif
}

// ---------------------------------------------------------------------------
//  9. CREATE RAY TRACING PIPELINE
// ---------------------------------------------------------------------------
// PROTIP: Set recursion depth low (1) for primary rays; group shaders explicitly for SBT control.
void VulkanPipelineManager::createRayTracingPipeline()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> COMPILING RAY TRACING PIPELINE");
#endif

    VkShaderModule raygenMod   = loadShader(context_.device, "raygen");
    VkShaderModule missMod     = loadShader(context_.device, "miss");
    VkShaderModule hitMod      = loadShader(context_.device, "closesthit");

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(3);

    stages.emplace_back(VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        .module              = raygenMod,
        .pName               = "main"
    });
    stages.emplace_back(VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_MISS_BIT_KHR,
        .module              = missMod,
        .pName               = "main"
    });
    stages.emplace_back(VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage               = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .module              = hitMod,
        .pName               = "main"
    });

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    groups.reserve(3);

    groups.emplace_back(VkRayTracingShaderGroupCreateInfoKHR{
        .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader      = 0
    });
    groups.emplace_back(VkRayTracingShaderGroupCreateInfoKHR{
        .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader      = 1
    });
    groups.emplace_back(VkRayTracingShaderGroupCreateInfoKHR{
        .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .closestHitShader   = 2
    });

    VkDescriptorSetLayout dsLayout = createRayTracingDescriptorSetLayout();

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &dsLayout
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout), "Create RT pipeline layout");

    rayTracingPipelineLayout_ = Dispose::makeHandle(context_.device, layout, "RT Pipeline Layout");

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
        .sType                       = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                  = static_cast<uint32_t>(stages.size()),
        .pStages                     = stages.data(),
        .groupCount                  = static_cast<uint32_t>(groups.size()),
        .pGroups                     = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout                      = layout
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(context_.vkCreateRayTracingPipelinesKHR(
        context_.device, VK_NULL_HANDLE, pipelineCache_.get(),
        1, &pipelineInfo, nullptr, &pipeline), "Create RT pipeline");

    rayTracingPipeline_ = Dispose::makeHandle(context_.device, pipeline, "RT Pipeline");

    vkDestroyShaderModule(context_.device, raygenMod, nullptr);
    vkDestroyShaderModule(context_.device, missMod, nullptr);
    vkDestroyShaderModule(context_.device, hitMod, nullptr);

#ifndef NDEBUG
    char pipePtr[32];
    std::snprintf(pipePtr, sizeof(pipePtr), "%p", static_cast<void*>(pipeline));
    LOG_INFO_CAT("Vulkan", "    PIPELINE COMPILED @ {}", pipePtr);
    LOG_INFO_CAT("Vulkan", "<<< RAY TRACING PIPELINE LIVE");
#endif
}

// ---------------------------------------------------------------------------
//  10. CREATE COMPUTE PIPELINE – FIXED: tonemap_compute
// ---------------------------------------------------------------------------
// PROTIP: Compute pipelines excel for post-process like tonemapping; dispatch in workgroups of 16x16 for GPU efficiency.
void VulkanPipelineManager::createComputePipeline()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> COMPILING COMPUTE (TONEMAP) PIPELINE");
#endif

    VkShaderModule compMod = loadShader(context_.device, "tonemap_compute");  // FIXED

    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = compMod,
        .pName = "main"
    };

    VkDescriptorSetLayout dsLayout = computeDescriptorSetLayout_.get();
    if (dsLayout == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "    COMPUTE LAYOUT IS NULL!");
        throw std::runtime_error("Compute layout missing");
    }

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &dsLayout
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout), "Create compute pipeline layout");

    computePipelineLayout_ = Dispose::makeHandle(context_.device, layout, "Compute Pipeline Layout");

    VkComputePipelineCreateInfo pipeInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = layout
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_.get(), 1, &pipeInfo, nullptr, &pipeline), "Create compute pipeline");

    computePipeline_ = Dispose::makeHandle(context_.device, pipeline, "Compute Pipeline");

    vkDestroyShaderModule(context_.device, compMod, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    TONEMAP COMPUTE PIPELINE LIVE");
#endif
}

// ---------------------------------------------------------------------------
//  11. CREATE GRAPHICS PIPELINE
// ---------------------------------------------------------------------------
// PROTIP: Fullscreen quad for graphics tonemap as fallback; prefer compute for parallel pixel ops.
void VulkanPipelineManager::createGraphicsPipeline(int width, int height)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> COMPILING GRAPHICS (TONEMAP) PIPELINE [{}x{}]", width, height);
#endif

    VkShaderModule vertMod = loadShader(context_.device, "tonemap_vert");
    VkShaderModule fragMod = loadShader(context_.device, "tonemap_frag");

    const VkPipelineShaderStageCreateInfo stages[] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertMod, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragMod, .pName = "main"}
    };

    const VkVertexInputBindingDescription binding = {
        .binding = 0, .stride = sizeof(float) * 4, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    const VkVertexInputAttributeDescription attr = {
        .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0
    };

    const VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 1, .pVertexAttributeDescriptions = &attr
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    const VkViewport viewport = {0, 0, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f};
    const VkRect2D scissor = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

    const VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &viewport,
        .scissorCount = 1, .pScissors = &scissor
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE, .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.f
    };

    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    const VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    const VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &blendAttachment
    };

    VkDescriptorSetLayout dsLayout = graphicsDescriptorSetLayout_.get();

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &dsLayout
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout), "Create graphics pipeline layout");

    graphicsPipelineLayout_ = Dispose::makeHandle(context_.device, layout, "Graphics Pipeline Layout");

    VkGraphicsPipelineCreateInfo pipeInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .layout = layout,
        .renderPass = renderPass_.get(),
        .subpass = 0
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(context_.device, pipelineCache_.get(), 1, &pipeInfo, nullptr, &pipeline), "Create graphics pipeline");

    graphicsPipeline_ = Dispose::makeHandle(context_.device, pipeline, "Graphics Pipeline");

    vkDestroyShaderModule(context_.device, vertMod, nullptr);
    vkDestroyShaderModule(context_.device, fragMod, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    GRAPHICS PIPELINE LIVE");
#endif
}

// ---------------------------------------------------------------------------
//  12. ACCELERATION STRUCTURES – FIXED: Full ownership transfer + cleanup
// ---------------------------------------------------------------------------
// PROTIP: Use PREFER_FAST_TRACE build flag; time AS builds to profile geometry complexity.
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer, VulkanBufferManager& bufferMgr)
{
#ifndef NDEBUG
    auto asStart = std::chrono::high_resolution_clock::now();
    LOG_INFO_CAT("Vulkan", ">>> BUILDING ACCELERATION STRUCTURES (BLAS + TLAS)");
#endif

    if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "    FATAL: vertexBuffer or indexBuffer is NULL!");
        throw std::runtime_error("Invalid geometry buffers");
    }

#ifndef NDEBUG
    char vbufPtr[32], ibufPtr[32];
    std::snprintf(vbufPtr, sizeof(vbufPtr), "%p", static_cast<void*>(vertexBuffer));
    std::snprintf(ibufPtr, sizeof(ibufPtr), "%p", static_cast<void*>(indexBuffer));
    LOG_INFO_CAT("Vulkan", "    Vertex Buffer: {}", vbufPtr);
    LOG_INFO_CAT("Vulkan", "    Index Buffer:  {}", ibufPtr);
#endif

    VkDeviceAddress vertexAddress = VulkanInitializer::getBufferDeviceAddress(context_, vertexBuffer);
    VkDeviceAddress indexAddress  = VulkanInitializer::getBufferDeviceAddress(context_, indexBuffer);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    Vertex Device Address: 0x{:x}", vertexAddress);
    LOG_INFO_CAT("Vulkan", "    Index Device Address:  0x{:x}", indexAddress);
#endif

    const uint32_t vertexCount = bufferMgr.getVertexCount();
    const uint32_t indexCount  = bufferMgr.getIndexCount();
    const uint32_t triangleCount = indexCount / 3;

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    Geometry → {} vertices, {} indices ({} triangles)", vertexCount, indexCount, triangleCount);
#endif

    // Fetch real transfer family
    uint32_t transferQueueFamily = bufferMgr.getTransferQueueFamily();
    if (transferQueueFamily == VK_QUEUE_FAMILY_IGNORED) {
        transferQueueFamily = context_.graphicsQueueFamilyIndex;
    }
#ifndef NDEBUG
    LOG_DEBUG_CAT("PipelineMgr", "Using transfer queue family: {} (graphics: {})", transferQueueFamily, context_.graphicsQueueFamilyIndex);
#endif

    // BLAS
    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertexAddress },
        .vertexStride = sizeof(float) * 3,
        .maxVertex = vertexCount,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = indexAddress }
    };

    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &triangleCount, &sizeInfo);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    BLAS Size → as:{} bytes, scratch:{} bytes",
                  sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize);
#endif

    VkBuffer blasBuffer = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blasBuffer, blasMemory, nullptr, context_);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VkAccelerationStructureKHR blasAS = VK_NULL_HANDLE;
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &blasCreateInfo, nullptr, &blasAS), "Create BLAS");

#ifndef NDEBUG
    char blasPtr[32];
    std::snprintf(blasPtr, sizeof(blasPtr), "%p", static_cast<void*>(blasAS));
    LOG_INFO_CAT("Vulkan", "    BLAS created @ {}", blasPtr);
#endif

    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer, scratchMemory, nullptr, context_);

    buildInfo.dstAccelerationStructure = blasAS;
    buildInfo.scratchData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_, scratchBuffer);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {
        .primitiveCount = triangleCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);

    // Ownership transfer: Transfer → Graphics (if different)
    if (transferQueueFamily != VK_QUEUE_FAMILY_IGNORED && transferQueueFamily != context_.graphicsQueueFamilyIndex) {
#ifndef NDEBUG
        LOG_INFO_CAT("Vulkan", "Inserting ownership transfer barriers (transfer {} → graphics {})",
                     transferQueueFamily, context_.graphicsQueueFamilyIndex);
#endif

        VkBufferMemoryBarrier vertexBarrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .srcQueueFamilyIndex = transferQueueFamily,
            .dstQueueFamilyIndex = context_.graphicsQueueFamilyIndex,
            .buffer = vertexBuffer,
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };

        VkBufferMemoryBarrier indexBarrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .srcQueueFamilyIndex = transferQueueFamily,
            .dstQueueFamilyIndex = context_.graphicsQueueFamilyIndex,
            .buffer = indexBuffer,
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };

        std::array<VkBufferMemoryBarrier, 2> barriers = {vertexBarrier, indexBarrier};
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             0, 0, nullptr, 2, barriers.data(), 0, nullptr);
    }

    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);

    // Ownership transfer back: Graphics → Transfer (optional but complete)
    if (transferQueueFamily != VK_QUEUE_FAMILY_IGNORED && transferQueueFamily != context_.graphicsQueueFamilyIndex) {
#ifndef NDEBUG
        LOG_INFO_CAT("Vulkan", "Inserting ownership release barriers (graphics {} → transfer {})",
                     context_.graphicsQueueFamilyIndex, transferQueueFamily);
#endif

        VkBufferMemoryBarrier vertexRev = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .srcQueueFamilyIndex = context_.graphicsQueueFamilyIndex,
            .dstQueueFamilyIndex = transferQueueFamily,
            .buffer = vertexBuffer,
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };

        VkBufferMemoryBarrier indexRev = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .srcQueueFamilyIndex = context_.graphicsQueueFamilyIndex,
            .dstQueueFamilyIndex = transferQueueFamily,
            .buffer = indexBuffer,
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };

        std::array<VkBufferMemoryBarrier, 2> revBarriers = {vertexRev, indexRev};
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 2, revBarriers.data(), 0, nullptr);
    }

    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    // Cleanup scratch
    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    BLAS built in single-time command");
#endif

    blas_ = Dispose::VulkanHandle<VkAccelerationStructureKHR>(
        context_.device,
        blasAS,
        context_.vkDestroyAccelerationStructureKHR
    );
    blasBuffer_ = Dispose::makeHandle(context_.device, blasBuffer, "BLAS Buffer");
    blasMemory_ = Dispose::makeHandle(context_.device, blasMemory, "BLAS Memory");

    // TLAS
    VkTransformMatrixKHR transform = {
        .matrix = {
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f}
        }
    };

    VkAccelerationStructureInstanceKHR instance = {
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = VulkanInitializer::getAccelerationStructureDeviceAddress(context_, blasAS)
    };

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    Creating instance buffer (1 instance)");
#endif
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer, instanceMemory, nullptr, context_);

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_.device, instanceMemory, 0, sizeof(instance), 0, &mapped), "Map instance buffer");
    std::memcpy(mapped, &instance, sizeof(instance));
    vkUnmapMemory(context_.device, instanceMemory);

    VkDeviceAddress instanceAddress = VulkanInitializer::getBufferDeviceAddress(context_, instanceBuffer);
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    Instance Device Address: 0x{:x}", instanceAddress);
#endif

    VkAccelerationStructureGeometryInstancesDataKHR instancesData = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = instanceAddress }
    };

    VkAccelerationStructureGeometryKHR tlasGeometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry
    };

    const uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildInfo, &instanceCount, &tlasSizeInfo);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    TLAS Size → as:{} bytes, scratch:{} bytes",
                  tlasSizeInfo.accelerationStructureSize, tlasSizeInfo.buildScratchSize);
#endif

    VkBuffer tlasBuffer = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        tlasSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBuffer, tlasMemory, nullptr, context_);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer,
        .size = tlasSizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VkAccelerationStructureKHR tlasAS = VK_NULL_HANDLE;
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &tlasCreateInfo, nullptr, &tlasAS), "Create TLAS");

#ifndef NDEBUG
    char tlasPtr[32];
    std::snprintf(tlasPtr, sizeof(tlasPtr), "%p", static_cast<void*>(tlasAS));
    LOG_INFO_CAT("Vulkan", "    TLAS created @ {}", tlasPtr);
#endif

    VkBuffer tlasScratch = VK_NULL_HANDLE;
    VkDeviceMemory tlasScratchMem = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        tlasSizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasScratch, tlasScratchMem, nullptr, context_);

    tlasBuildInfo.dstAccelerationStructure = tlasAS;
    tlasBuildInfo.scratchData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_, tlasScratch);

    VkAccelerationStructureBuildRangeInfoKHR tlasRange = {
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;

    VkCommandBuffer tlasCmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(tlasCmd, 1, &tlasBuildInfo, &pTlasRange);
    VulkanInitializer::endSingleTimeCommands(context_, tlasCmd);

    // Cleanup TLAS scratch + instance buffer
    vkDestroyBuffer(context_.device, tlasScratch, nullptr);
    vkFreeMemory(context_.device, tlasScratchMem, nullptr);
    vkDestroyBuffer(context_.device, instanceBuffer, nullptr);
    vkFreeMemory(context_.device, instanceMemory, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    TLAS built in single-time command");
#endif

    tlas_ = Dispose::VulkanHandle<VkAccelerationStructureKHR>(
        context_.device,
        tlasAS,
        context_.vkDestroyAccelerationStructureKHR
    );
    tlasBuffer_ = Dispose::makeHandle(context_.device, tlasBuffer, "TLAS Buffer");
    tlasMemory_ = Dispose::makeHandle(context_.device, tlasMemory, "TLAS Memory");

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    BLAS + TLAS BUILT SUCCESSFULLY");
    LOG_INFO_CAT("Vulkan", "    BLAS @ {} | TLAS @ {}", blasPtr, tlasPtr);
    auto asEnd = std::chrono::high_resolution_clock::now();
    auto asDuration = std::chrono::duration_cast<std::chrono::milliseconds>(asEnd - asStart).count();
    LOG_INFO_CAT("Vulkan", "    AS BUILD TIME: {} ms", asDuration);
    LOG_INFO_CAT("Vulkan", "<<< ACCELERATION STRUCTURES READY");
#endif
}

// ---------------------------------------------------------------------------
//  13. UPDATE RAY TRACING DESCRIPTOR SET
// ---------------------------------------------------------------------------
// PROTIP: Bind AS via pNext chain; update only on geometry changes to avoid stalls.
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet,
                                                          VkAccelerationStructureKHR /*tlasHandle*/)
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> UPDATING RAY TRACING DESCRIPTOR SET");
#endif

    if (descriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "    FATAL: descriptorSet is NULL!");
        throw std::runtime_error("Invalid descriptor set");
    }
    if (!tlas_) {
        LOG_ERROR_CAT("Vulkan", "    FATAL: tlas_ is null! Did you call createAccelerationStructures()?");
        throw std::runtime_error("TLAS not built");
    }

    VkAccelerationStructureKHR tlasHandle = tlas_.get();

#ifndef NDEBUG
    char dsPtr[32], tlasPtr[32];
    std::snprintf(dsPtr, sizeof(dsPtr), "%p", static_cast<void*>(descriptorSet));
    std::snprintf(tlasPtr, sizeof(tlasPtr), "%p", static_cast<void*>(tlasHandle));
    LOG_INFO_CAT("Vulkan", "    Descriptor Set: {}", dsPtr);
    LOG_INFO_CAT("Vulkan", "    TLAS Handle:    {}", tlasPtr);
#endif

    VkWriteDescriptorSetAccelerationStructureKHR asWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlasHandle
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "    TLAS BOUND @ {} to Descriptor Set {}", tlasPtr, dsPtr);
    LOG_INFO_CAT("Vulkan", "<<< DESCRIPTOR SET UPDATED");
#endif
}

// ---------------------------------------------------------------------------
//  14. FRAME TIME LOGGING (PERFORMANCE)
// ---------------------------------------------------------------------------
// PROTIP: Log slow frames (>16ms) for real-time perf; use chrono for precise cross-platform timing.
void VulkanPipelineManager::logFrameTimeIfSlow(std::chrono::steady_clock::time_point start)
{
#ifndef NDEBUG
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (duration > 16666) {  // >60 FPS
        LOG_WARN_CAT("Perf", "Frame took {} microseconds ({} FPS)", duration, 1000000.0 / duration);
    }
#endif
}

} // namespace VulkanRTX