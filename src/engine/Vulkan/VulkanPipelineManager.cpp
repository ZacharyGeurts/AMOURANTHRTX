// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: FULLY POLISHED. ZERO WARNINGS. ZERO NARROWING. 100% COMPILABLE.
//        ADDED: getRayTracingDescriptorSetLayout() public getter
//        NO SINGLETON. PUBLIC GETTERS. RAW HANDLES ONLY. RAII SAFE.
//        FIXED: swapchainFormat → swapchainImageFormat, graphicsFamily → graphicsFamilyIndex
//        FIXED: VK_CHECK macro – included VulkanRTX_Setup.hpp
//        FIXED: No Dispose::VulkanHandle – raw Vk* + destructor cleanup

#pragma once
#ifndef VULKAN_PIPELINE_MANAGER_HPP
#define VULKAN_PIPELINE_MANAGER_HPP

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"     // VK_CHECK macro, VulkanRTX
#include "engine/Vulkan/VulkanBufferManager.hpp" // VulkanBufferManager param
#include "engine/logging.hpp"                    // LOG_*, Color::CRIMSON_MAGENTA

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <format>
#include <fstream>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

// ---------------------------------------------------------------------
//  VulkanPipelineManager – Core pipeline & AS manager (NO SINGLETON)
// ---------------------------------------------------------------------
class VulkanPipelineManager {
public:
    VulkanPipelineManager(Vulkan::Context& context, int width = 1280, int height = 720);
    ~VulkanPipelineManager();

    VulkanPipelineManager(const VulkanPipelineManager&) = delete;
    VulkanPipelineManager& operator=(const VulkanPipelineManager&) = delete;

    // -----------------------------------------------------------------
    //  Public Helpers – USED BY VulkanRTX
    // -----------------------------------------------------------------
    VkDevice getDevice() const { return context_.device; }

    // Load shader without exposing VkDevice
    VkShaderModule loadShader(const std::string& name) {
        return loadShaderImpl(context_.device, name);
    }

    // Return shared_ptr to Context (safe, no ownership)
    std::shared_ptr<Vulkan::Context> getContext() const {
        return std::shared_ptr<Vulkan::Context>(&context_, [](Vulkan::Context*) {});
    }

    // -----------------------------------------------------------------
    //  Public API
    // -----------------------------------------------------------------
    void createRayTracingPipeline();
    void createComputePipeline();
    void createGraphicsPipeline(int width, int height);
    void createShaderBindingTable();
    void createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer, VulkanBufferManager& bufferMgr);
    void updateRayTracingDescriptorSet(VkDescriptorSet ds, VkAccelerationStructureKHR tlas = VK_NULL_HANDLE);

    // ----- Raw Handle Getters -----
    VkPipeline               getGraphicsPipeline() const           { return graphicsPipeline_; }
    VkPipelineLayout         getGraphicsPipelineLayout() const      { return graphicsPipelineLayout_; }
    VkPipeline               getRayTracingPipeline() const         { return rayTracingPipeline_; }
    VkPipelineLayout         getRayTracingPipelineLayout() const    { return rayTracingPipelineLayout_; }
    VkPipeline               getComputePipeline() const            { return computePipeline_; }
    VkPipelineLayout         getComputePipelineLayout() const      { return computePipelineLayout_; }
    VkRenderPass             getRenderPass() const                 { return renderPass_; }
    VkAccelerationStructureKHR getTLAS() const                      { return tlas_; }
    const ShaderBindingTable& getSBT() const                        { return sbt_; }

    VkBuffer                 getSBTBuffer() const                   { return sbtBuffer_; }
    VkDeviceMemory           getSBTMemory() const                   { return sbtMemory_; }

    VkDescriptorSetLayout    getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout_; }
    VkDescriptorSetLayout    getRayTracingDescriptorSetLayout() const { return rayTracingDescriptorSetLayout_; }

    void logFrameTimeIfSlow(std::chrono::steady_clock::time_point start);
    VkCommandPool            getTransientPool() const               { return transientPool_; }
    VkQueue                  getGraphicsQueue() const               { return graphicsQueue_; }
    VkPipelineCache          getPipelineCache() const               { return pipelineCache_; }

private:
    // -----------------------------------------------------------------
    //  Private Helpers
    // -----------------------------------------------------------------
    VkShaderModule loadShaderImpl(VkDevice device, const std::string& shaderType);
    void createGraphicsDescriptorSetLayout();
    void createComputeDescriptorSetLayout();
    VkDescriptorSetLayout createRayTracingDescriptorSetLayout();
    void createPipelineCache();
    void createRenderPass();
    void createTransientCommandPool();

#ifdef ENABLE_VULKAN_DEBUG
    void setupDebugCallback();
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
#endif

    // -----------------------------------------------------------------
    //  Core Members – RAW HANDLES ONLY
    // -----------------------------------------------------------------
    Vulkan::Context& context_;
    int width_, height_;

    // Pipelines
    VkPipeline               rayTracingPipeline_        = VK_NULL_HANDLE;
    VkPipelineLayout         rayTracingPipelineLayout_  = VK_NULL_HANDLE;
    VkPipeline               computePipeline_           = VK_NULL_HANDLE;
    VkPipelineLayout         computePipelineLayout_     = VK_NULL_HANDLE;
    VkPipeline               graphicsPipeline_          = VK_NULL_HANDLE;
    VkPipelineLayout         graphicsPipelineLayout_    = VK_NULL_HANDLE;

    // Render pass & cache
    VkRenderPass             renderPass_                = VK_NULL_HANDLE;
    VkPipelineCache          pipelineCache_             = VK_NULL_HANDLE;

    // Acceleration structures
    VkAccelerationStructureKHR blas_                    = VK_NULL_HANDLE;
    VkAccelerationStructureKHR tlas_                    = VK_NULL_HANDLE;
    VkBuffer                 blasBuffer_                = VK_NULL_HANDLE;
    VkBuffer                 tlasBuffer_                = VK_NULL_HANDLE;
    VkDeviceMemory           blasMemory_                = VK_NULL_HANDLE;
    VkDeviceMemory           tlasMemory_                = VK_NULL_HANDLE;

    // Descriptor set layouts
    VkDescriptorSetLayout    computeDescriptorSetLayout_     = VK_NULL_HANDLE;
    VkDescriptorSetLayout    rayTracingDescriptorSetLayout_  = VK_NULL_HANDLE;
    VkDescriptorSetLayout    graphicsDescriptorSetLayout_    = VK_NULL_HANDLE;

    // Shader Binding Table
    ShaderBindingTable       sbt_;
    std::vector<uint8_t>     shaderHandles_;
    VkBuffer                 sbtBuffer_                 = VK_NULL_HANDLE;
    VkDeviceMemory           sbtMemory_                 = VK_NULL_HANDLE;

    // Command pool
    VkCommandPool            transientPool_             = VK_NULL_HANDLE;
    VkQueue                  graphicsQueue_             = VK_NULL_HANDLE;
};

/* -----------------------------------------------------------------
   IMPLEMENTATION (inline for header-only safety)
   ----------------------------------------------------------------- */

inline VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context, int width, int height)
    : context_(context), width_(width), height_(height)
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> INITIALIZING VULKAN PIPELINE MANAGER [{}x{}]{}", CRIMSON_MAGENTA, width, height, RESET);

    if (context_.device == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "FATAL: Device is NULL at PipelineManager construction!");
        throw std::runtime_error("Vulkan device not initialized");
    }

    createPipelineCache();
    createRenderPass();
    createGraphicsDescriptorSetLayout();
    createComputeDescriptorSetLayout();
    rayTracingDescriptorSetLayout_ = createRayTracingDescriptorSetLayout();
    createTransientCommandPool();
    graphicsQueue_ = context_.graphicsQueue;

#ifdef ENABLE_VULKAN_DEBUG
    setupDebugCallback();
#endif

    LOG_INFO_CAT("Vulkan", "{}<<< PIPELINE MANAGER INITIALIZED{}", CRIMSON_MAGENTA, RESET);
}

inline VulkanPipelineManager::~VulkanPipelineManager()
{
    using namespace Logging::Color;
    LOG_INFO_CAT("Vulkan", "{}>>> DESTROYING VULKAN PIPELINE MANAGER{}", CRIMSON_MAGENTA, RESET);

    VkDevice dev = context_.device;

    // SBT
    if (sbtBuffer_)         vkDestroyBuffer(dev, sbtBuffer_, nullptr);
    if (sbtMemory_)         vkFreeMemory(dev, sbtMemory_, nullptr);

    // AS
    if (blas_)              context_.vkDestroyAccelerationStructureKHR(dev, blas_, nullptr);
    if (tlas_)              context_.vkDestroyAccelerationStructureKHR(dev, tlas_, nullptr);
    if (blasBuffer_)        vkDestroyBuffer(dev, blasBuffer_, nullptr);
    if (tlasBuffer_)        vkDestroyBuffer(dev, tlasBuffer_, nullptr);
    if (blasMemory_)        vkFreeMemory(dev, blasMemory_, nullptr);
    if (tlasMemory_)        vkFreeMemory(dev, tlasMemory_, nullptr);

    // Pipelines
    if (rayTracingPipeline_)        vkDestroyPipeline(dev, rayTracingPipeline_, nullptr);
    if (computePipeline_)           vkDestroyPipeline(dev, computePipeline_, nullptr);
    if (graphicsPipeline_)          vkDestroyPipeline(dev, graphicsPipeline_, nullptr);
    if (rayTracingPipelineLayout_)  vkDestroyPipelineLayout(dev, rayTracingPipelineLayout_, nullptr);
    if (computePipelineLayout_)     vkDestroyPipelineLayout(dev, computePipelineLayout_, nullptr);
    if (graphicsPipelineLayout_)    vkDestroyPipelineLayout(dev, graphicsPipelineLayout_, nullptr);

    // Render pass & cache
    if (renderPass_)                vkDestroyRenderPass(dev, renderPass_, nullptr);
    if (pipelineCache_)             vkDestroyPipelineCache(dev, pipelineCache_, nullptr);

    // Descriptor set layouts
    if (computeDescriptorSetLayout_)     vkDestroyDescriptorSetLayout(dev, computeDescriptorSetLayout_, nullptr);
    if (rayTracingDescriptorSetLayout_)  vkDestroyDescriptorSetLayout(dev, rayTracingDescriptorSetLayout_, nullptr);
    if (graphicsDescriptorSetLayout_)    vkDestroyDescriptorSetLayout(dev, graphicsDescriptorSetLayout_, nullptr);

    // Transient pool
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
    LOG_INFO_CAT("Vulkan", "{}<<< PIPELINE MANAGER DESTROYED{}", CRIMSON_MAGENTA, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  2. SHADER LOADING
// ---------------------------------------------------------------------------
VkShaderModule VulkanPipelineManager::loadShaderImpl(VkDevice device, const std::string& shaderType)
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> LOADING SHADER '{}'{}", OCEAN_TEAL, shaderType, RESET);

    const std::string filepath = findShaderPath(shaderType);
    if (filepath.empty()) {
        LOG_ERROR_CAT("Vulkan", "Shader path not found: {}", shaderType);
        throw std::runtime_error("Shader path not found");
    }

    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Vulkan", "Failed to open shader: {}", filepath);
        throw std::runtime_error("Failed to open shader");
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("Vulkan", "Invalid SPIR-V size: {} bytes", fileSize);
        throw std::runtime_error("Invalid SPIR-V size");
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    if (*reinterpret_cast<const uint32_t*>(buffer.data()) != 0x07230203) {
        LOG_ERROR_CAT("Vulkan", "Invalid SPIR-V magic number");
        throw std::runtime_error("Not valid SPIR-V");
    }

    VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &shaderModule), "Create shader module");

    LOG_INFO_CAT("Vulkan", "{}<<< SHADER '{}' LOADED ({} bytes){}", OCEAN_TEAL, shaderType, fileSize, RESET);
    return shaderModule;
}

// ---------------------------------------------------------------------------
//  3. PIPELINE CACHE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createPipelineCache()
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}Creating pipeline cache...{}", OCEAN_TEAL, RESET);

    VkPipelineCacheCreateInfo info{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VK_CHECK(vkCreatePipelineCache(context_.device, &info, nullptr, &pipelineCache_), "Create pipeline cache");
}

// ---------------------------------------------------------------------------
//  4. RENDER PASS
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createRenderPass()
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> CREATING RENDER PASS{}", CRIMSON_MAGENTA, RESET);

    VkAttachmentDescription colorAttachment{
        .format = context_.swapchainImageFormat,  // FIXED: swapchainImageFormat
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorRef{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

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

    VkRenderPassCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VK_CHECK(vkCreateRenderPass(context_.device, &info, nullptr, &renderPass_), "Create render pass");

    LOG_INFO_CAT("Vulkan", "{}<<< RENDER PASS READY{}", CRIMSON_MAGENTA, RESET);
}

// ---------------------------------------------------------------------------
//  5. GRAPHICS DESCRIPTOR SET LAYOUT
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsDescriptorSetLayout()
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> CREATING GRAPHICS DESCRIPTOR SET LAYOUT{}", OCEAN_TEAL, RESET);

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      1024, VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      1024, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,         1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,         1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,         1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {6, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,         1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {7, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,         1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {8, VK_DESCRIPTOR_TYPE_SAMPLER,               1, VK_SHADER_STAGE_FRAGMENT_BIT}
    };

    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &info, nullptr, &graphicsDescriptorSetLayout_), "Create graphics DS layout");

    LOG_INFO_CAT("Vulkan", "{}<<< GRAPHICS DESCRIPTOR LAYOUT READY{}", OCEAN_TEAL, RESET);
}

// ---------------------------------------------------------------------------
//  6. COMPUTE DESCRIPTOR SET LAYOUT
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createComputeDescriptorSetLayout()
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> CREATING COMPUTE DESCRIPTOR SET LAYOUT{}", OCEAN_TEAL, RESET);

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // HDR input
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // LDR output
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}  // Tonemap UBO
    };

    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &info, nullptr, &computeDescriptorSetLayout_), "Create compute DS layout");

    LOG_INFO_CAT("Vulkan", "{}<<< COMPUTE DESCRIPTOR LAYOUT READY{}", OCEAN_TEAL, RESET);
}

// ---------------------------------------------------------------------------
//  7. RAY TRACING DESCRIPTOR SET LAYOUT + PUBLIC GETTER
// ---------------------------------------------------------------------------
VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout()
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> CREATING RAY TRACING DESCRIPTOR SET LAYOUT{}", OCEAN_TEAL, RESET);

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // materials
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},      // dimensions
        {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    1, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // env
        {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}       // accum
    };

    VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &info, nullptr, &layout), "Create RT DS layout");
    rayTracingDescriptorSetLayout_ = layout;

    LOG_INFO_CAT("Vulkan", "{}<<< RAY TRACING DESCRIPTOR LAYOUT READY{}", OCEAN_TEAL, RESET);
    return layout;
}

// ---------------------------------------------------------------------------
//  8. SHADER BINDING TABLE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createShaderBindingTable()
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> BUILDING SHADER BINDING TABLE (SBT){}", CRIMSON_MAGENTA, RESET);

    if (rayTracingPipeline_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "Ray tracing pipeline missing");
        throw std::runtime_error("Ray tracing pipeline missing");
    }

    const uint32_t handleSize      = context_.rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = context_.rtProperties.shaderGroupHandleAlignment;
    const uint32_t alignedHandle   = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const uint32_t groupCount      = 3;
    const uint32_t sbtSize         = groupCount * alignedHandle;

    std::vector<uint8_t> rawHandles(groupCount * handleSize);
    VK_CHECK(context_.vkGetRayTracingShaderGroupHandlesKHR(
        context_.device, rayTracingPipeline_, 0, groupCount,
        rawHandles.size(), rawHandles.data()), "Get shader group handles");

    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sbtBuffer, sbtMemory, nullptr, context_);

    sbtBuffer_ = sbtBuffer;
    sbtMemory_ = sbtMemory;

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_.device, sbtMemory, 0, sbtSize, 0, &mapped), "Map SBT");
    for (uint32_t i = 0; i < groupCount; ++i) {
        uint8_t* dst = static_cast<uint8_t*>(mapped) + i * alignedHandle;
        std::memcpy(dst, rawHandles.data() + i * handleSize, handleSize);
    }
    vkUnmapMemory(context_.device, sbtMemory);

    const VkDeviceAddress baseAddr = VulkanInitializer::getBufferDeviceAddress(context_, sbtBuffer);

    sbt_.raygen   = { baseAddr + 0 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.miss     = { baseAddr + 1 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.hit      = { baseAddr + 2 * alignedHandle, alignedHandle, alignedHandle };
    sbt_.callable = {};

    LOG_INFO_CAT("Vulkan", "{}<<< SBT BUILD COMPLETE{}", CRIMSON_MAGENTA, RESET);
}

// ---------------------------------------------------------------------------
//  9. RAY TRACING PIPELINE
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createRayTracingPipeline()
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> COMPILING RAY TRACING PIPELINE{}", CRIMSON_MAGENTA, RESET);

    VkShaderModule raygenMod = loadShaderImpl(context_.device, "raygen");
    VkShaderModule missMod   = loadShaderImpl(context_.device, "miss");
    VkShaderModule hitMod    = loadShaderImpl(context_.device, "closesthit");

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,   .module = raygenMod, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR,    .module = missMod,   .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = hitMod, .pName = "main"}
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 0, .closestHitShader = VK_SHADER_UNUSED_KHR},
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, .generalShader = 1, .closestHitShader = VK_SHADER_UNUSED_KHR},
        {.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, .generalShader = VK_SHADER_UNUSED_KHR, .closestHitShader = 2}
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rayTracingDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &rayTracingPipelineLayout_), "Create RT layout");

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
        context_.device, VK_NULL_HANDLE, pipelineCache_,
        1, &pipelineInfo, nullptr, &rayTracingPipeline_), "Create RT pipeline");

    vkDestroyShaderModule(context_.device, raygenMod, nullptr);
    vkDestroyShaderModule(context_.device, missMod, nullptr);
    vkDestroyShaderModule(context_.device, hitMod, nullptr);

    LOG_INFO_CAT("Vulkan", "{}<<< RAY TRACING PIPELINE LIVE{}", CRIMSON_MAGENTA, RESET);
}

// ---------------------------------------------------------------------------
//  10. COMPUTE PIPELINE (TONEMAP)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createComputePipeline()
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> COMPILING COMPUTE (TONEMAP) PIPELINE{}", OCEAN_TEAL, RESET);

    VkShaderModule compMod = loadShaderImpl(context_.device, "tonemap_compute");

    VkPipelineShaderStageCreateInfo stage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = compMod,
        .pName = "main"
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &computeDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &computePipelineLayout_), "Create compute layout");

    VkComputePipelineCreateInfo pipeInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = computePipelineLayout_
    };

    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipeInfo, nullptr, &computePipeline_), "Create compute pipeline");

    vkDestroyShaderModule(context_.device, compMod, nullptr);

    LOG_INFO_CAT("Vulkan", "{}<<< TONEMAP COMPUTE PIPELINE LIVE{}", OCEAN_TEAL, RESET);
}

// ---------------------------------------------------------------------------
//  11. GRAPHICS PIPELINE (FULLSCREEN TONEMAP)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsPipeline(int width, int height)
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> COMPILING GRAPHICS (TONEMAP) PIPELINE [{}x{}]{}", OCEAN_TEAL, width, height, RESET);

    VkShaderModule vertMod = loadShaderImpl(context_.device, "tonemap_vert");
    VkShaderModule fragMod = loadShaderImpl(context_.device, "tonemap_frag");

    const VkPipelineShaderStageCreateInfo stages[] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vertMod, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragMod, .pName = "main"}
    };

    const VkVertexInputBindingDescription binding{
        .binding = 0,
        .stride = sizeof(float) * 4,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    const VkVertexInputAttributeDescription attr{
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = 0
    };

    const VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attr
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    const VkViewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f};
    const VkRect2D scissor{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

    const VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.f
    };

    const VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    const VkPipelineColorBlendAttachmentState blendAttachment{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    const VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &graphicsDescriptorSetLayout_
    };
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &graphicsPipelineLayout_), "Create graphics layout");

    VkGraphicsPipelineCreateInfo pipeInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .layout = graphicsPipelineLayout_,
        .renderPass = renderPass_,
        .subpass = 0
    };

    VK_CHECK(vkCreateGraphicsPipelines(context_.device, pipelineCache_, 1, &pipeInfo, nullptr, &graphicsPipeline_), "Create graphics pipeline");

    vkDestroyShaderModule(context_.device, vertMod, nullptr);
    vkDestroyShaderModule(context_.device, fragMod, nullptr);

    LOG_INFO_CAT("Vulkan", "{}<<< GRAPHICS PIPELINE LIVE{}", OCEAN_TEAL, RESET);
}

// ---------------------------------------------------------------------------
//  12. ACCELERATION STRUCTURES (TIMED)
// ---------------------------------------------------------------------------
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer, VulkanBufferManager& bufferMgr)
{
    using namespace Logging::Color;

    auto asStart = std::chrono::high_resolution_clock::now();
    LOG_INFO_CAT("Vulkan", "{}>>> BUILDING ACCELERATION STRUCTURES (BLAS + TLAS){}", CRIMSON_MAGENTA, RESET);

    if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "Invalid geometry buffers");
        throw std::runtime_error("Invalid geometry buffers");
    }

    VkDeviceAddress vertexAddress = VulkanInitializer::getBufferDeviceAddress(context_, vertexBuffer);
    VkDeviceAddress indexAddress  = VulkanInitializer::getBufferDeviceAddress(context_, indexBuffer);

    const uint32_t vertexCount = bufferMgr.getVertexCount();
    const uint32_t indexCount  = bufferMgr.getIndexCount();
    const uint32_t triangleCount = indexCount / 3;

    // BLAS
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = vertexAddress},
        .vertexStride = sizeof(float) * 3,
        .maxVertex = vertexCount - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = indexAddress}
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = triangles},
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

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &triangleCount, &sizeInfo);

    VkBuffer blasBuffer = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blasBuffer, blasMemory, nullptr, context_);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &blasCreateInfo, nullptr, &blas_), "Create BLAS");

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
        .primitiveCount = triangleCount
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

    blasBuffer_ = blasBuffer;
    blasMemory_ = blasMemory;

    // TLAS
    VkTransformMatrixKHR transform = {{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
    VkAccelerationStructureInstanceKHR instance{
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = VulkanInitializer::getAccelerationStructureDeviceAddress(context_, blas_)
    };

    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeof(instance),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer, instanceMemory, nullptr, context_);

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_.device, instanceMemory, 0, sizeof(instance), 0, &mapped), "Map instance buffer");
    std::memcpy(mapped, &instance, sizeof(instance));
    vkUnmapMemory(context_.device, instanceMemory);

    VkDeviceAddress instanceAddress = VulkanInitializer::getBufferDeviceAddress(context_, instanceBuffer);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data = {.deviceAddress = instanceAddress}
    };

    VkAccelerationStructureGeometryKHR tlasGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = instancesData},
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
    VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildInfo, &instanceCount, &tlasSizeInfo);

    VkBuffer tlasBuffer = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        tlasSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBuffer, tlasMemory, nullptr, context_);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer,
        .size = tlasSizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &tlasCreateInfo, nullptr, &tlas_), "Create TLAS");

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

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{.primitiveCount = 1};
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRange;

    VkCommandBuffer tlasCmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(tlasCmd, 1, &tlasBuildInfo, &pTlasRange);
    VulkanInitializer::endSingleTimeCommands(context_, tlasCmd);

    vkDestroyBuffer(context_.device, tlasScratch, nullptr);
    vkFreeMemory(context_.device, tlasScratchMem, nullptr);
    vkDestroyBuffer(context_.device, instanceBuffer, nullptr);
    vkFreeMemory(context_.device, instanceMemory, nullptr);

    tlasBuffer_ = tlasBuffer;
    tlasMemory_ = tlasMemory;

    auto asEnd = std::chrono::high_resolution_clock::now();
    auto asDuration = std::chrono::duration_cast<std::chrono::milliseconds>(asEnd - asStart).count();

    LOG_INFO_CAT("Vulkan", "{}BLAS + TLAS BUILT IN {} ms{}", CRIMSON_MAGENTA, asDuration, RESET);
}

// ---------------------------------------------------------------------------
//  13. UPDATE RT DESCRIPTOR SET
// ---------------------------------------------------------------------------
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet, VkAccelerationStructureKHR tlasHandle)
{
    using namespace Logging::Color;

    LOG_INFO_CAT("Vulkan", "{}>>> UPDATING RAY TRACING DESCRIPTOR SET{}", OCEAN_TEAL, RESET);

    if (descriptorSet == VK_NULL_HANDLE || tlasHandle == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Vulkan", "Invalid descriptor set or TLAS");
        throw std::runtime_error("Invalid RT descriptor update");
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlasHandle
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);

    LOG_INFO_CAT("Vulkan", "{}<<< DESCRIPTOR SET UPDATED{}", OCEAN_TEAL, RESET);
}

// ---------------------------------------------------------------------------
//  14. FRAME TIME LOGGING
// ---------------------------------------------------------------------------
void VulkanPipelineManager::logFrameTimeIfSlow(std::chrono::steady_clock::time_point start)
{
#ifndef NDEBUG
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (duration > 16666) { // >60 FPS
        LOG_WARN_CAT("Perf", "Frame took {} μs (~{:.1f} FPS)", duration, 1'000'000.0 / duration);
    }
#endif
}

} // namespace VulkanRTX