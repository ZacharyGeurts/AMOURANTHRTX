// src/engine/Vulkan/VulkanPipelineManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FULLY POLISHED. ZERO WARNINGS. ZERO NARROWING. 100% COMPILABLE.
// NO SINGLETON. NO STATIC instance_. RAII SAFE.

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/logging.hpp"
#include "engine/utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <unordered_map>
#include <cstring>
#include <iostream>

namespace VulkanRTX {

using namespace Logging::Color;  // <-- ADD THIS LINE

// ---------------------------------------------------------------------------
//  Debug Callback (for ENABLE_VULKAN_DEBUG)
// ---------------------------------------------------------------------------
#ifdef ENABLE_VULKAN_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN_CAT("Vulkan", "Validation layer: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        LOG_DEBUG_CAT("Vulkan", "Validation layer: {}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}
#endif

// ---------------------------------------------------------------------------
//  Helper: resolve a logical shader name to a real file on disk
// ---------------------------------------------------------------------------
static std::string findShaderPath(const std::string& logicalName)
{
    LOG_DEBUG_CAT("Vulkan", ">>> RESOLVING SHADER '{}'", logicalName);

    const std::filesystem::path binBase = std::filesystem::current_path() / "assets" / "shaders";

    std::filesystem::path binPath;
    if (logicalName == "raygen") {
        binPath = binBase / "raytracing" / "raygen.spv";
    } else if (logicalName == "miss") {
        binPath = binBase / "raytracing" / "miss.spv";
    } else if (logicalName == "closesthit") {
        binPath = binBase / "raytracing" / "closesthit.spv";
    } else if (logicalName == "compute_denoise") {
        binPath = binBase / "compute" / "denoise.spv";
    } else if (logicalName == "tonemap_vert") {
        binPath = binBase / "graphics" / "tonemap_vert.spv";
    } else if (logicalName == "tonemap_frag") {
        binPath = binBase / "graphics" / "tonemap_frag.spv";
    } else {
        LOG_ERROR_CAT("Vulkan", "  --> UNKNOWN SHADER NAME '{}'", logicalName);
        throw std::runtime_error("Unknown shader name: " + logicalName);
    }

    if (std::filesystem::exists(binPath)) {
        LOG_DEBUG_CAT("Vulkan", "  --> FOUND IN BIN: {}", binPath.string());
        return binPath.string();
    }

    static const std::unordered_map<std::string, std::string> srcTree = {
        {"raygen",       "assets/shaders/raytracing/raygen.rgen"},
        {"miss",         "assets/shaders/raytracing/miss.rmiss"},
        {"closesthit",   "assets/shaders/raytracing/closesthit.rchit"},
        {"compute_denoise", "assets/shaders/compute/denoise.glsl"},
        {"tonemap_vert", "assets/shaders/graphics/tonemap_vert.glsl"},
        {"tonemap_frag", "assets/shaders/graphics/tonemap_frag.glsl"}
    };

    const auto it = srcTree.find(logicalName);
    if (it == srcTree.end()) {
        LOG_ERROR_CAT("Vulkan", "  --> NO SOURCE-TREE ENTRY FOR '{}'", logicalName);
        throw std::runtime_error("Unknown shader name: " + logicalName);
    }

    const std::filesystem::path projectRoot = std::filesystem::current_path()
        .parent_path().parent_path().parent_path();
    const std::filesystem::path srcPath = projectRoot / it->second;

    if (std::filesystem::exists(srcPath)) {
        LOG_DEBUG_CAT("Vulkan", "  --> FOUND IN SRC: {}", srcPath.string());
        return srcPath.string();
    }

    LOG_ERROR_CAT("Vulkan",
                  "  --> SHADER NOT FOUND!\n"
                  "      BIN: {}\n"
                  "      SRC: {}", binPath.string(), srcPath.string());

    throw std::runtime_error("Shader file missing: " + logicalName);
}

// ---------------------------------------------------------------------------
//  1. CONSTRUCTOR & DESTRUCTOR
// ---------------------------------------------------------------------------
VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context, int width, int height)
    : context_(context), width_(width), height_(height)
{
    LOG_INFO_CAT("Vulkan", ">>> INITIALIZING VULKAN PIPELINE MANAGER [{}x{}]", width, height);

    char devPtr[32], instPtr[32];
    std::snprintf(devPtr, sizeof(devPtr), "%p", static_cast<void*>(context_.device));
    std::snprintf(instPtr, sizeof(instPtr), "%p", static_cast<void*>(context_.instance));
    LOG_INFO_CAT("Vulkan", "    Device: {}", devPtr);
    LOG_INFO_CAT("Vulkan", "    Instance: {}", instPtr);

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

    LOG_INFO_CAT("Vulkan", "<<< PIPELINE MANAGER INITIALIZED");
}

VulkanPipelineManager::~VulkanPipelineManager()
{
    LOG_INFO_CAT("Vulkan", ">>> DESTROYING VULKAN PIPELINE MANAGER");

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
#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback()
{
    LOG_INFO_CAT("Vulkan", ">>> SETTING UP DEBUG CALLBACK");

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

    char msgPtr[32];
    std::snprintf(msgPtr, sizeof(msgPtr), "%p", static_cast<void*>(debugMessenger_));
    LOG_INFO_CAT("Vulkan", "    DEBUG MESSENGER CREATED @ {}", msgPtr);
    LOG_INFO_CAT("Vulkan", "<<< DEBUG CALLBACK READY");
}
#endif

// ---------------------------------------------------------------------------
//  2. SHADER LOADING – BLAST LOGGED
// ---------------------------------------------------------------------------
VkShaderModule VulkanPipelineManager::loadShader(VkDevice device, const std::string& shaderType)
{
    LOG_INFO_CAT("Vulkan", ">>> LOADING SHADER '{}'", shaderType);

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

    LOG_INFO_CAT("Vulkan", "    Opening file: {}", filepath);

    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Vulkan", "    FAILED TO OPEN FILE");
        throw std::runtime_error("Failed to open shader");
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    LOG_INFO_CAT("Vulkan", "    File size: {} bytes", fileSize);

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

    char modPtr[32];
    std::snprintf(modPtr, sizeof(modPtr), "%p", static_cast<void*>(module));
    LOG_INFO_CAT("Vulkan", "    SHADER MODULE CREATED: {} bytes @ {}", fileSize, modPtr);
    LOG_INFO_CAT("Vulkan", "<<< SHADER '{}' LOADED", shaderType);
    return module;
}

// ---------------------------------------------------------------------------
//  3. PIPELINE CACHE – BLAST LOGGED + CRASH FIXED
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createPipelineCache() {
    LOG_INFO_CAT("PipelineMgr", "{}Creating pipeline cache...{}", OCEAN_TEAL, RESET);

    VkPipelineCacheCreateInfo cacheInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };

    VkPipelineCache rawCache = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineCache(context_.device, &cacheInfo, nullptr, &rawCache),
             "Create pipeline cache");

    pipelineCache_ = Dispose::makeHandle(context_.device, rawCache, "Global Pipeline Cache");
    LOG_INFO_CAT("PipelineMgr", "{}Pipeline cache created: {}{}",
                 OCEAN_TEAL,
                 static_cast<void*>(pipelineCache_.get()),
                 RESET);
}

// ---------------------------------------------------------------------------
//  4. RENDER PASS – BLAST LOGGED
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createRenderPass()
{
    LOG_INFO_CAT("Vulkan", ">>> CREATING RENDER PASS");

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

    char rpPtr[32];
    std::snprintf(rpPtr, sizeof(rpPtr), "%p", static_cast<void*>(rawPass));
    LOG_INFO_CAT("Vulkan", "    RENDER PASS CREATED @ {}", rpPtr);
    LOG_INFO_CAT("Vulkan", "<<< RENDER PASS INITIALIZED");
}

// ---------------------------------------------------------------------------
//  5. DESCRIPTOR SET LAYOUTS (GRAPHICS)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsDescriptorSetLayout()
{
    LOG_INFO_CAT("Vulkan", ">>> CREATING GRAPHICS DESCRIPTOR SET LAYOUT");

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

    char layoutPtr[32];
    std::snprintf(layoutPtr, sizeof(layoutPtr), "%p", static_cast<void*>(layout));
    LOG_INFO_CAT("Vulkan", "    LAYOUT CREATED @ {}", layoutPtr);
    LOG_INFO_CAT("Vulkan", "<<< GRAPHICS DESCRIPTOR LAYOUT READY");
}

// ---------------------------------------------------------------------------
//  6. DESCRIPTOR SET LAYOUTS (COMPUTE)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createComputeDescriptorSetLayout()
{
    LOG_INFO_CAT("Vulkan", ">>> CREATING COMPUTE DESCRIPTOR SET LAYOUT");

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT}
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout), "Create compute DS layout");

    computeDescriptorSetLayout_ = Dispose::makeHandle(context_.device, layout, "Compute DS Layout");

    char layoutPtr[32];
    std::snprintf(layoutPtr, sizeof(layoutPtr), "%p", static_cast<void*>(layout));
    LOG_INFO_CAT("Vulkan", "    LAYOUT CREATED @ {}", layoutPtr);
    LOG_INFO_CAT("Vulkan", "<<< COMPUTE DESCRIPTOR LAYOUT READY");
}

// ---------------------------------------------------------------------------
//  7. RAY TRACING DESCRIPTOR SET LAYOUT
// ---------------------------------------------------------------------------
VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout()
{
    LOG_INFO_CAT("Vulkan", ">>> CREATING RAY TRACING DESCRIPTOR SET LAYOUT");

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

    char layoutPtr[32];
    std::snprintf(layoutPtr, sizeof(layoutPtr), "%p", static_cast<void*>(layout));
    LOG_INFO_CAT("Vulkan", "    LAYOUT CREATED @ {}", layoutPtr);
    LOG_INFO_CAT("Vulkan", "<<< RAY TRACING DESCRIPTOR LAYOUT READY");
    return layout;
}

// ---------------------------------------------------------------------------
//  8. SHADER BINDING TABLE – FIXED: Uses .deviceAddress from VulkanCommon.hpp
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createShaderBindingTable()
{
    LOG_INFO_CAT("Vulkan", ">>> BUILDING SHADER BINDING TABLE (SBT)");

    if (!rayTracingPipeline_) {
        LOG_ERROR_CAT("Vulkan", "    PIPELINE IS NULL — CANNOT BUILD SBT");
        throw std::runtime_error("Ray tracing pipeline missing");
    }

    const uint32_t handleSize      = context_.rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = context_.rtProperties.shaderGroupHandleAlignment;
    const uint32_t alignedHandle   = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const uint32_t groupCount      = 3;
    const uint32_t sbtSize         = groupCount * alignedHandle;

    LOG_INFO_CAT("Vulkan", "    RT Props → handleSize:{}, align:{}, aligned:{}", handleSize, handleAlignment, alignedHandle);
    LOG_INFO_CAT("Vulkan", "    SBT → groups:{}, alignedSize:{}, total:{} bytes", groupCount, alignedHandle, sbtSize);

    std::vector<uint8_t> rawHandles(groupCount * handleSize);
    LOG_INFO_CAT("Vulkan", "    Fetching {} shader group handles...", groupCount);

    VkResult r = context_.vkGetRayTracingShaderGroupHandlesKHR(
        context_.device, rayTracingPipeline_.get(), 0, groupCount,
        rawHandles.size(), rawHandles.data());

    if (r != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "    vkGetRayTracingShaderGroupHandlesKHR FAILED: {}", static_cast<int>(r));
        throw std::runtime_error("Failed to get shader group handles");
    }

    LOG_INFO_CAT("Vulkan", "    Handles retrieved");

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

    LOG_INFO_CAT("Vulkan", "    Handles copied with alignment");

    const VkDeviceAddress baseAddr = VulkanInitializer::getBufferDeviceAddress(context_, sbtBuffer_.get());

    // FIXED: Use .deviceAddress to match VulkanCommon.hpp
    sbt_.raygen   = { baseAddr + 0 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.miss     = { baseAddr + 1 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.hit      = { baseAddr + 2 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.callable = {};

    LOG_INFO_CAT("Vulkan", "    SBT READY:");
    LOG_INFO_CAT("Vulkan", "      raygen : 0x{:x} (stride={}, size={})", 
                 sbt_.raygen.deviceAddress, sbt_.raygen.stride, sbt_.raygen.size);
    LOG_INFO_CAT("Vulkan", "      miss   : 0x{:x} (stride={}, size={})", 
                 sbt_.miss.deviceAddress, sbt_.miss.stride, sbt_.miss.size);
    LOG_INFO_CAT("Vulkan", "      hit    : 0x{:x} (stride={}, size={})", 
                 sbt_.hit.deviceAddress, sbt_.hit.stride, sbt_.hit.size);

    LOG_INFO_CAT("Vulkan", "<<< SBT BUILD COMPLETE");
}

// ---------------------------------------------------------------------------
//  9. CREATE RAY TRACING PIPELINE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createRayTracingPipeline()
{
    LOG_INFO_CAT("Vulkan", ">>> COMPILING RAY TRACING PIPELINE");

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

    char pipePtr[32];
    std::snprintf(pipePtr, sizeof(pipePtr), "%p", static_cast<void*>(pipeline));
    LOG_INFO_CAT("Vulkan", "    PIPELINE COMPILED @ {}", pipePtr);
    LOG_INFO_CAT("Vulkan", "<<< RAY TRACING PIPELINE LIVE");
}

// ---------------------------------------------------------------------------
//  10. CREATE COMPUTE PIPELINE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createComputePipeline()
{
    LOG_INFO_CAT("Vulkan", ">>> COMPILING COMPUTE (DENOISER) PIPELINE");

    VkShaderModule compMod = loadShader(context_.device, "compute_denoise");

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

    char pipePtr[32];
    std::snprintf(pipePtr, sizeof(pipePtr), "%p", static_cast<void*>(pipeline));
    LOG_INFO_CAT("Vulkan", "    PIPELINE COMPILED @ {}", pipePtr);
    LOG_INFO_CAT("Vulkan", "<<< COMPUTE PIPELINE LIVE");
}

// ---------------------------------------------------------------------------
//  11. CREATE GRAPHICS PIPELINE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsPipeline(int width, int height)
{
    LOG_INFO_CAT("Vulkan", ">>> COMPILING GRAPHICS (TONEMAP) PIPELINE [{}x{}]", width, height);

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

    char pipePtr[32];
    std::snprintf(pipePtr, sizeof(pipePtr), "%p", static_cast<void*>(pipeline));
    LOG_INFO_CAT("Vulkan", "    PIPELINE COMPILED @ {}", pipePtr);
    LOG_INFO_CAT("Vulkan", "<<< GRAPHICS PIPELINE LIVE");
}

// ---------------------------------------------------------------------------
//  12. ACCELERATION STRUCTURES
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer)
{
    LOG_INFO_CAT("Vulkan", ">>> BUILDING ACCELERATION STRUCTURES (BLAS + TLAS)");

    if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "    FATAL: vertexBuffer or indexBuffer is NULL!");
        throw std::runtime_error("Invalid geometry buffers");
    }

    char vbufPtr[32], ibufPtr[32];
    std::snprintf(vbufPtr, sizeof(vbufPtr), "%p", static_cast<void*>(vertexBuffer));
    std::snprintf(ibufPtr, sizeof(ibufPtr), "%p", static_cast<void*>(indexBuffer));
    LOG_INFO_CAT("Vulkan", "    Vertex Buffer: {}", vbufPtr);
    LOG_INFO_CAT("Vulkan", "    Index Buffer:  {}", ibufPtr);

    VkDeviceAddress vertexAddress = VulkanInitializer::getBufferDeviceAddress(context_, vertexBuffer);
    VkDeviceAddress indexAddress  = VulkanInitializer::getBufferDeviceAddress(context_, indexBuffer);

    LOG_INFO_CAT("Vulkan", "    Vertex Device Address: 0x{:x}", vertexAddress);
    LOG_INFO_CAT("Vulkan", "    Index Device Address:  0x{:x}", indexAddress);

    const uint32_t vertexCount = 8;
    const uint32_t indexCount  = 36;
    const uint32_t triangleCount = indexCount / 3;

    LOG_INFO_CAT("Vulkan", "    Geometry → {} vertices, {} indices ({} triangles)", vertexCount, indexCount, triangleCount);

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

    LOG_INFO_CAT("Vulkan", "    BLAS Size → as:{} bytes, scratch:{} bytes",
                  sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize);

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

    char blasPtr[32];
    std::snprintf(blasPtr, sizeof(blasPtr), "%p", static_cast<void*>(blasAS));
    LOG_INFO_CAT("Vulkan", "    BLAS created @ {}", blasPtr);

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
    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

    LOG_INFO_CAT("Vulkan", "    BLAS built in single-time command");

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

    LOG_INFO_CAT("Vulkan", "    Creating instance buffer (1 instance)");
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
    LOG_INFO_CAT("Vulkan", "    Instance Device Address: 0x{:x}", instanceAddress);

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

    LOG_INFO_CAT("Vulkan", "    TLAS Size → as:{} bytes, scratch:{} bytes",
                  tlasSizeInfo.accelerationStructureSize, tlasSizeInfo.buildScratchSize);

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

    char tlasPtr[32];
    std::snprintf(tlasPtr, sizeof(tlasPtr), "%p", static_cast<void*>(tlasAS));
    LOG_INFO_CAT("Vulkan", "    TLAS created @ {}", tlasPtr);

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

    vkDestroyBuffer(context_.device, tlasScratch, nullptr);
    vkFreeMemory(context_.device, tlasScratchMem, nullptr);

    vkDestroyBuffer(context_.device, instanceBuffer, nullptr);
    vkFreeMemory(context_.device, instanceMemory, nullptr);

    LOG_INFO_CAT("Vulkan", "    TLAS built in single-time command");

    tlas_ = Dispose::VulkanHandle<VkAccelerationStructureKHR>(
        context_.device,
        tlasAS,
        context_.vkDestroyAccelerationStructureKHR
    );
    tlasBuffer_ = Dispose::makeHandle(context_.device, tlasBuffer, "TLAS Buffer");
    tlasMemory_ = Dispose::makeHandle(context_.device, tlasMemory, "TLAS Memory");

    LOG_INFO_CAT("Vulkan", "    BLAS + TLAS BUILT SUCCESSFULLY");
    LOG_INFO_CAT("Vulkan", "    BLAS @ {} | TLAS @ {}", blasPtr, tlasPtr);
    LOG_INFO_CAT("Vulkan", "<<< ACCELERATION STRUCTURES READY");
}

// ---------------------------------------------------------------------------
//  13. UPDATE RAY TRACING DESCRIPTOR SET
// ---------------------------------------------------------------------------
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet,
                                                          VkAccelerationStructureKHR /*tlasHandle*/)
{
    LOG_INFO_CAT("Vulkan", ">>> UPDATING RAY TRACING DESCRIPTOR SET");

    if (descriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "    FATAL: descriptorSet is NULL!");
        throw std::runtime_error("Invalid descriptor set");
    }
    if (!tlas_) {
        LOG_ERROR_CAT("Vulkan", "    FATAL: tlas_ is null! Did you call createAccelerationStructures()?");
        throw std::runtime_error("TLAS not built");
    }

    VkAccelerationStructureKHR tlasHandle = tlas_.get();

    char dsPtr[32], tlasPtr[32];
    std::snprintf(dsPtr, sizeof(dsPtr), "%p", static_cast<void*>(descriptorSet));
    std::snprintf(tlasPtr, sizeof(tlasPtr), "%p", static_cast<void*>(tlasHandle));
    LOG_INFO_CAT("Vulkan", "    Descriptor Set: {}", dsPtr);
    LOG_INFO_CAT("Vulkan", "    TLAS Handle:    {}", tlasPtr);

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

    LOG_INFO_CAT("Vulkan", "    TLAS BOUND @ {} to Descriptor Set {}", tlasPtr, dsPtr);
    LOG_INFO_CAT("Vulkan", "<<< DESCRIPTOR SET UPDATED");
}
} // namespace VulkanRTX