// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan pipeline management implementation (common and graphics).
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, VulkanRTX_Setup.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows, Consoles (PS5, Xbox Series X).
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <array>
#include <fstream>
#include <filesystem>
#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>
#include <cstring>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

// -----------------------------------------------------------------------------
//  Helper: create a VkPipelineShaderStageCreateInfo from a module + stage
// -----------------------------------------------------------------------------
static VkPipelineShaderStageCreateInfo makeShaderStage(VkShaderModule mod,
                                                       VkShaderStageFlagBits stage)
{
    return VkPipelineShaderStageCreateInfo{
        .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stage               = stage,
        .module              = mod,
        .pName               = "main",
        .pSpecializationInfo = nullptr
    };
}

// -----------------------------------------------------------------------------
//  Shader loading (unchanged)
// -----------------------------------------------------------------------------
VkShaderModule VulkanPipelineManager::loadShader(VkDevice device, const std::string& shaderType)
{
    auto it = shaderPaths_.find(shaderType);
    if (it == shaderPaths_.end())
        throw std::runtime_error("Shader type not found: " + shaderType);
    const std::string& filename = it->second;

    std::filesystem::path shaderPath = std::filesystem::absolute(filename);
    if (!std::filesystem::exists(shaderPath))
        throw std::runtime_error("Shader file does not exist: " + shaderPath.string());
    if (!std::filesystem::is_regular_file(shaderPath))
        throw std::runtime_error("Shader path is not a regular file: " + shaderPath.string());

    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader file: " + shaderPath.string());

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0)
        throw std::runtime_error("Invalid SPIR-V size for: " + shaderPath.string());

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    if (fileSize >= 4 && *reinterpret_cast<const uint32_t*>(buffer.data()) != 0x07230203)
        throw std::runtime_error("Invalid SPIR-V magic for: " + shaderPath.string());

    VkShaderModuleCreateInfo ci{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = buffer.size(),
        .pCode    = reinterpret_cast<const uint32_t*>(buffer.data())
    };
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module for: " + shaderPath.string());
    return mod;
}

// -----------------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------------
VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context, int width, int height)
    : context_(context),
      width_(width),
      height_(height),
      rayTracingPipeline_(nullptr),
      rayTracingPipelineLayout_(nullptr),
      computePipeline_(nullptr),
      computePipelineLayout_(nullptr),
      graphicsPipeline_(nullptr),
      graphicsPipelineLayout_(nullptr),
      renderPass_(VK_NULL_HANDLE),
      pipelineCache_(VK_NULL_HANDLE),
      rasterPrepassPipeline_(VK_NULL_HANDLE),
      denoiserPostPipeline_(VK_NULL_HANDLE),
      shaderPaths_({
          {"vertex",            "assets/shaders/rasterization/vertex.spv"},
          {"fragment",          "assets/shaders/rasterization/fragment.spv"},
          {"raygen",            "assets/shaders/raytracing/raygen.spv"},
          {"miss",              "assets/shaders/raytracing/miss.spv"},
          {"closesthit",        "assets/shaders/raytracing/closesthit.spv"},
          {"shadowmiss",        "assets/shaders/raytracing/shadowmiss.spv"},
          {"anyhit",            "assets/shaders/raytracing/anyhit.spv"},
          {"intersection",      "assets/shaders/raytracing/intersection.spv"},
          {"callable",          "assets/shaders/raytracing/callable.spv"},
          {"shadow_anyhit",     "assets/shaders/raytracing/shadow_anyhit.spv"},
          {"mid_anyhit",        "assets/shaders/raytracing/mid_anyhit.spv"},
          {"volumetric_anyhit", "assets/shaders/raytracing/volumetric_anyhit.spv"},
          {"compute",           "assets/shaders/compute/compute.spv"},
          {"raster_prepass",    "assets/shaders/compute/raster_prepass.spv"},
          {"denoiser_post",     "assets/shaders/compute/denoiser_post.spv"}
      }),
      sbt_(),
      platformConfig_({.graphicsQueueFamily = context.graphicsQueueFamilyIndex,
                      .computeQueueFamily  = context.computeQueueFamilyIndex,
                      .preferDeviceLocalMemory = true}),
      createAsFunc_(nullptr),
      destroyAsFunc_(nullptr)
{
#ifdef ENABLE_VULKAN_DEBUG
    debugMessenger_ = VK_NULL_HANDLE;
    setupDebugCallback();
#endif

    if (!context_.device || !context_.physicalDevice || !context_.graphicsQueue)
        throw std::runtime_error("Invalid Vulkan context");

    // Load RT extension functions
    createAsFunc_ = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(context_.device, "vkCreateAccelerationStructureKHR"));
    if (!createAsFunc_) {
        throw std::runtime_error("Failed to load vkCreateAccelerationStructureKHR");
    }
    destroyAsFunc_ = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(context_.device, "vkDestroyAccelerationStructureKHR"));
    if (!destroyAsFunc_) {
        throw std::runtime_error("Failed to load vkDestroyAccelerationStructureKHR");
    }

    // allocate a fallback command buffer if none exist
    if (context_.commandBuffers.empty() && context_.commandPool != VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo ai{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = context_.commandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        VkCommandBuffer cb = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(context_.device, &ai, &cb) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate fallback command buffer");
        context_.commandBuffers.push_back(cb);
    }

    createPipelineCache();
    createRayTracingDescriptorSetLayout();
    createGraphicsDescriptorSetLayout();

    // render pass
    VkAttachmentDescription colorAttachment{
        .format         = context_.swapchainImageFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorRef
    };
    VkSubpassDependency dep{
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };
    VkRenderPassCreateInfo rpInfo{
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &colorAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dep
    };
    if (vkCreateRenderPass(context_.device, &rpInfo, nullptr, &renderPass_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
    context_.resourceManager.addRenderPass(renderPass_);

    // pipeline creation order
    createRayTracingPipeline();   // defined in VulkanRayTracingPipeline.cpp
    createComputePipeline();      // defined in VulkanComputePipeline.cpp
    createShaderBindingTable();   // defined in VulkanRayTracingPipeline.cpp
    createGraphicsPipeline(width_, height_);
}

// -----------------------------------------------------------------------------
//  Destructor
// -----------------------------------------------------------------------------
VulkanPipelineManager::~VulkanPipelineManager()
{
    if (blasHandle_ != VK_NULL_HANDLE && destroyAsFunc_) {
        destroyAsFunc_(context_.device, blasHandle_, nullptr);
        blasHandle_ = VK_NULL_HANDLE;
    }
    if (tlasHandle_ != VK_NULL_HANDLE && destroyAsFunc_) {
        destroyAsFunc_(context_.device, tlasHandle_, nullptr);
        tlasHandle_ = VK_NULL_HANDLE;
    }

    if (graphicsPipeline_)       context_.resourceManager.removePipeline(graphicsPipeline_->get());
    if (graphicsPipelineLayout_) context_.resourceManager.removePipelineLayout(graphicsPipelineLayout_->get());
    if (rayTracingPipeline_)     context_.resourceManager.removePipeline(rayTracingPipeline_->get());
    if (rayTracingPipelineLayout_) context_.resourceManager.removePipelineLayout(rayTracingPipelineLayout_->get());
    if (computePipeline_)        context_.resourceManager.removePipeline(computePipeline_->get());
    if (computePipelineLayout_)  context_.resourceManager.removePipelineLayout(computePipelineLayout_->get());

    if (rasterPrepassPipeline_ != VK_NULL_HANDLE) {
        context_.resourceManager.removePipeline(rasterPrepassPipeline_);
        vkDestroyPipeline(context_.device, rasterPrepassPipeline_, nullptr);
    }
    if (denoiserPostPipeline_ != VK_NULL_HANDLE) {
        context_.resourceManager.removePipeline(denoiserPostPipeline_);
        vkDestroyPipeline(context_.device, denoiserPostPipeline_, nullptr);
    }

    if (pipelineCache_ != VK_NULL_HANDLE) vkDestroyPipelineCache(context_.device, pipelineCache_, nullptr);
    if (renderPass_ != VK_NULL_HANDLE) {
        context_.resourceManager.removeRenderPass(renderPass_);
        vkDestroyRenderPass(context_.device, renderPass_, nullptr);
    }

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (fn) fn(context_.instance, debugMessenger_, nullptr);
    }
#endif
}

// -----------------------------------------------------------------------------
//  Debug callback
// -----------------------------------------------------------------------------
#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback()
{
    VkDebugUtilsMessengerCreateInfoEXT ci{
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT,
                              VkDebugUtilsMessageTypeFlagsEXT,
                              const VkDebugUtilsMessengerCallbackDataEXT* data,
                              void*) -> VkBool32 {
            return VK_FALSE;
        }
    };
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(context_.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (fn) fn(context_.instance, &ci, nullptr, &debugMessenger_);
}
#endif

// -----------------------------------------------------------------------------
//  Pipeline cache
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createPipelineCache()
{
    VkPipelineCacheCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };
    if (vkCreatePipelineCache(context_.device, &ci, nullptr, &pipelineCache_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline cache");
}

// -----------------------------------------------------------------------------
//  Descriptor set layouts
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createRayTracingDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 10> bindings = {};

    bindings[0].binding         = static_cast<uint32_t>(DescriptorBindings::TLAS);
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                  VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                  VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                  VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

    bindings[1].binding         = static_cast<uint32_t>(DescriptorBindings::StorageImage);
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[2].binding         = static_cast<uint32_t>(DescriptorBindings::CameraUBO);
    bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    bindings[3].binding         = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO);
    bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 26;
    bindings[3].stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    bindings[4].binding         = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO);
    bindings[4].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags      = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    bindings[5].binding         = static_cast<uint32_t>(DescriptorBindings::AlphaTex);
    bindings[5].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags      = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    bindings[6].binding         = static_cast<uint32_t>(DescriptorBindings::EnvMap);
    bindings[6].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                  VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                  VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    bindings[7].binding         = static_cast<uint32_t>(DescriptorBindings::DensityVolume);
    bindings[7].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[7].descriptorCount = 1;
    bindings[7].stageFlags      = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

    bindings[8].binding         = static_cast<uint32_t>(DescriptorBindings::GDepth);
    bindings[8].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[8].descriptorCount = 1;
    bindings[8].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[9].binding         = static_cast<uint32_t>(DescriptorBindings::GNormal);
    bindings[9].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[9].descriptorCount = 1;
    bindings[9].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo ci{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };
    if (vkCreateDescriptorSetLayout(context_.device, &ci, nullptr, &context_.rayTracingDescriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create ray-tracing descriptor set layout");
    context_.resourceManager.addDescriptorSetLayout(context_.rayTracingDescriptorSetLayout);
}

void VulkanPipelineManager::createGraphicsDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};

    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding         = 2;
    bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };
    if (vkCreateDescriptorSetLayout(context_.device, &ci, nullptr, &context_.graphicsDescriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics descriptor set layout");
    context_.resourceManager.addDescriptorSetLayout(context_.graphicsDescriptorSetLayout);
}

// -----------------------------------------------------------------------------
//  Graphics pipeline
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsPipeline(int /*width*/, int /*height*/)
{
    auto vert = loadShader(context_.device, "vertex");
    auto frag = loadShader(context_.device, "fragment");

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {
        makeShaderStage(vert, VK_SHADER_STAGE_VERTEX_BIT),
        makeShaderStage(frag, VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates    = dynStates
    };
    VkPipelineViewportStateCreateInfo viewport{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1
    };
    VkPipelineRasterizationStateCreateInfo raster{
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_NONE,
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth   = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo ms{
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    VkPipelineColorBlendAttachmentState blend{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo blending{
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &blend
    };
    VkPushConstantRange pc{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size       = sizeof(MaterialData::PushConstants)
    };
    VkPipelineLayoutCreateInfo layoutCI{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &context_.graphicsDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pc
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(context_.device, &layoutCI, nullptr, &layout) != VK_SUCCESS) {
        vkDestroyShaderModule(context_.device, vert, nullptr);
        vkDestroyShaderModule(context_.device, frag, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline layout");
    }
    graphicsPipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout,
                                                             PFN_vkDestroyPipelineLayout>>(
        context_.device, layout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(layout);

    VkGraphicsPipelineCreateInfo pipeCI{
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = static_cast<uint32_t>(stages.size()),
        .pStages             = stages.data(),
        .pVertexInputState   = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewport,
        .pRasterizationState = &raster,
        .pMultisampleState   = &ms,
        .pColorBlendState    = &blending,
        .pDynamicState       = &dyn,
        .layout              = layout,
        .renderPass          = renderPass_,
        .subpass             = 0
    };
    VkPipeline pipe = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(context_.device, pipelineCache_, 1, &pipeCI, nullptr, &pipe) != VK_SUCCESS) {
        vkDestroyShaderModule(context_.device, vert, nullptr);
        vkDestroyShaderModule(context_.device, frag, nullptr);
        vkDestroyPipelineLayout(context_.device, layout, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    graphicsPipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
        context_.device, pipe, vkDestroyPipeline);
    context_.resourceManager.addPipeline(pipe);

    vkDestroyShaderModule(context_.device, vert, nullptr);
    vkDestroyShaderModule(context_.device, frag, nullptr);
}

// -----------------------------------------------------------------------------
//  Record graphics commands
// -----------------------------------------------------------------------------
void VulkanPipelineManager::recordGraphicsCommands(VkCommandBuffer cmd,
                                                   VkFramebuffer fb,
                                                   VkDescriptorSet ds,
                                                   uint32_t w, uint32_t h,
                                                   VkImage /*denoiseImage*/)
{
    VkCommandBufferBeginInfo bi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clear{{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo rp{
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = renderPass_,
        .framebuffer = fb,
        .renderArea  = {{0, 0}, {w, h}},
        .clearValueCount = 1,
        .pClearValues    = &clear
    };
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, getGraphicsPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            getGraphicsPipelineLayout(), 0, 1, &ds, 0, nullptr);

    VkViewport vp{0, 0, static_cast<float>(w), static_cast<float>(h), 0, 1};
    VkRect2D scissor{{0, 0}, {w, h}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    MaterialData::PushConstants pc{};
    pc.resolution = glm::vec4(static_cast<float>(w), static_cast<float>(h), 0.0f, 0.0f);
    vkCmdPushConstants(cmd, getGraphicsPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

// -----------------------------------------------------------------------------
//  NEW: Create Acceleration Structures for Ray Tracing (Fix for Blue Screen)
//  Call this after creating vertex/index buffers in your init code.
//  Assumes vertex buffer contains 3 vec3 positions (stride 12 bytes), index buffer 3 uint32 indices.
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer) {
    VkDeviceAddress vertexAddr = VulkanInitializer::getBufferDeviceAddress(context_.device, vertexBuffer);
    VkDeviceAddress indexAddr = VulkanInitializer::getBufferDeviceAddress(context_.device, indexBuffer);

    // Query AS properties
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{};
    asProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &asProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);

    // BLAS Geometry Setup
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddr;
    triangles.vertexStride = sizeof(glm::vec3);
    triangles.maxVertex = 2;  // 3 vertices: 0,1,2
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddr;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR | VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = 1;  // One triangle

    // Get Build Sizes
    auto vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureBuildSizesKHR"));
    if (!vkGetAccelerationStructureBuildSizesKHR) {
        throw std::runtime_error("Failed to get vkGetAccelerationStructureBuildSizesKHR");
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    std::vector<uint32_t> maxPrimCounts = {1};
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, maxPrimCounts.data(), &sizeInfo);

    // Create AS buffer for BLAS
    VkBuffer blasBuffer;
    VkDeviceMemory blasMemory;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeInfo.accelerationStructureSize,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer, blasMemory, nullptr, context_.resourceManager);

    // Create BLAS
    VkAccelerationStructureCreateInfoKHR asCreateInfo{};
    asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asCreateInfo.buffer = blasBuffer;
    asCreateInfo.offset = 0;
    asCreateInfo.size = sizeInfo.accelerationStructureSize;
    asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR blasHandleLocal = VK_NULL_HANDLE;
    if (createAsFunc_(context_.device, &asCreateInfo, nullptr, &blasHandleLocal) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, blasBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, blasMemory);
        context_.resourceManager.removeBuffer(blasBuffer);
        context_.resourceManager.removeMemory(blasMemory);
        throw std::runtime_error("Failed to create BLAS");
    }
    blasHandle_ = blasHandleLocal;

    // Create scratch buffer
    VkDeviceSize scratchSizeAligned = (sizeInfo.buildScratchSize + asProps.minAccelerationStructureScratchOffsetAlignment - 1) &
                                      ~(asProps.minAccelerationStructureScratchOffsetAlignment - 1);
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, scratchSizeAligned,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory, nullptr, context_.resourceManager);

    // Temp command buffer for build
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = context_.commandPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(context_.device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Build BLAS
    buildInfo.dstAccelerationStructure = blasHandle_;
    buildInfo.scratchData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, scratchBuffer);
    VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &buildRangeInfo;
    auto vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCmdBuildAccelerationStructuresKHR"));
    if (!vkCmdBuildAccelerationStructuresKHR) {
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        throw std::runtime_error("Failed to get vkCmdBuildAccelerationStructuresKHR");
    }
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);

    vkEndCommandBuffer(cmd);

    // Submit
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    vkCreateFence(context_.device, &fenceInfo, nullptr, &fence);
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submit, fence) != VK_SUCCESS) {
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        throw std::runtime_error("Failed to submit BLAS build");
    }
    if (vkWaitForFences(context_.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        throw std::runtime_error("Failed to wait for BLAS build");
    }
    vkDestroyFence(context_.device, fence, nullptr);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);

    // Clean scratch
    Dispose::destroySingleBuffer(context_.device, scratchBuffer);
    Dispose::freeSingleDeviceMemory(context_.device, scratchMemory);
    context_.resourceManager.removeBuffer(scratchBuffer);
    context_.resourceManager.removeMemory(scratchMemory);

    // Get BLAS device address
    auto vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureDeviceAddressKHR"));
    if (!vkGetAccelerationStructureDeviceAddressKHR) {
        throw std::runtime_error("Failed to get vkGetAccelerationStructureDeviceAddressKHR");
    }
    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = blasHandle_;
    VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(context_.device, &addrInfo);

    // Instance buffer for TLAS (one instance)
    VkBuffer instanceBuffer;
    VkDeviceMemory instanceMemory;
    VkDeviceSize instanceSize = sizeof(VkAccelerationStructureInstanceKHR);
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, instanceSize,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instanceBuffer, instanceMemory, nullptr, context_.resourceManager);

    // Staging for instance
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, instanceSize,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    stagingBuffer, stagingMemory, nullptr, context_.resourceManager);

    // Fill instance
    VkAccelerationStructureInstanceKHR instance{};
    // Identity transform (row-major 3x4)
    float identity[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    memcpy(&instance.transform, identity, sizeof(VkTransformMatrixKHR));
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = blasAddr;

    void* data;
    vkMapMemory(context_.device, stagingMemory, 0, instanceSize, 0, &data);
    memcpy(data, &instance, instanceSize);
    vkUnmapMemory(context_.device, stagingMemory);

    // Copy to device
    VkCommandBuffer cmdCopy;
    vkAllocateCommandBuffers(context_.device, &cmdAlloc, &cmdCopy);
    vkBeginCommandBuffer(cmdCopy, &beginInfo);
    VkBufferCopy copy{};
    copy.size = instanceSize;
    vkCmdCopyBuffer(cmdCopy, stagingBuffer, instanceBuffer, 1, &copy);
    vkEndCommandBuffer(cmdCopy);
    vkQueueSubmit(context_.graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmdCopy);

    // Clean staging
    Dispose::destroySingleBuffer(context_.device, stagingBuffer);
    Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
    context_.resourceManager.removeBuffer(stagingBuffer);
    context_.resourceManager.removeMemory(stagingMemory);

    // TLAS Geometry
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.data.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, instanceBuffer);

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeometry.geometry.instances = instancesData;
    tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR | VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
    tlasRange.primitiveCount = 1;  // One instance

    // Reuse buildInfo for TLAS
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &tlasGeometry;

    std::vector<uint32_t> tlasMaxPrimCounts = {1};
    VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{};
    tlasSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, tlasMaxPrimCounts.data(), &tlasSizeInfo);

    // Create AS buffer for TLAS
    VkBuffer tlasBuffer;
    VkDeviceMemory tlasMemory;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, tlasSizeInfo.accelerationStructureSize,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer, tlasMemory, nullptr, context_.resourceManager);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
    tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasCreateInfo.buffer = tlasBuffer;
    tlasCreateInfo.offset = 0;
    tlasCreateInfo.size = tlasSizeInfo.accelerationStructureSize;
    tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR tlasHandleLocal = VK_NULL_HANDLE;
    if (createAsFunc_(context_.device, &tlasCreateInfo, nullptr, &tlasHandleLocal) != VK_SUCCESS) {
        Dispose::destroySingleBuffer(context_.device, tlasBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, tlasMemory);
        context_.resourceManager.removeBuffer(tlasBuffer);
        context_.resourceManager.removeMemory(tlasMemory);
        throw std::runtime_error("Failed to create TLAS");
    }
    tlasHandle_ = tlasHandleLocal;

    // Scratch for TLAS
    VkDeviceSize tlasScratchSizeAligned = (tlasSizeInfo.buildScratchSize + asProps.minAccelerationStructureScratchOffsetAlignment - 1) &
                                          ~(asProps.minAccelerationStructureScratchOffsetAlignment - 1);
    VkBuffer tlasScratchBuffer;
    VkDeviceMemory tlasScratchMemory;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, tlasScratchSizeAligned,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasScratchBuffer, tlasScratchMemory, nullptr, context_.resourceManager);

    // Build TLAS
    vkAllocateCommandBuffers(context_.device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &beginInfo);

    buildInfo.dstAccelerationStructure = tlasHandle_;
    buildInfo.scratchData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, tlasScratchBuffer);
    pRangeInfo = &tlasRange;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);

    vkEndCommandBuffer(cmd);

    // Submit TLAS
    VkFence tlasFence;
    vkCreateFence(context_.device, &fenceInfo, nullptr, &tlasFence);
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submit, tlasFence) != VK_SUCCESS) {
        vkDestroyFence(context_.device, tlasFence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        throw std::runtime_error("Failed to submit TLAS build");
    }
    if (vkWaitForFences(context_.device, 1, &tlasFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        vkDestroyFence(context_.device, tlasFence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        throw std::runtime_error("Failed to wait for TLAS build");
    }
    vkDestroyFence(context_.device, tlasFence, nullptr);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);

    // Clean TLAS scratch
    Dispose::destroySingleBuffer(context_.device, tlasScratchBuffer);
    Dispose::freeSingleDeviceMemory(context_.device, tlasScratchMemory);
    context_.resourceManager.removeBuffer(tlasScratchBuffer);
    context_.resourceManager.removeMemory(tlasScratchMemory);

    LOG_INFO("Acceleration structures built successfully. BLAS: {}, TLAS: {}", (void*)blasHandle_, (void*)tlasHandle_);
}

// -----------------------------------------------------------------------------
//  NEW: Update Descriptor Set with TLAS (Call per-frame if dynamic)
//  Pass the TLAS handle from createAccelerationStructures (store it!).
// -----------------------------------------------------------------------------
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet, VkAccelerationStructureKHR tlasHandle) {
    VkWriteDescriptorSetAccelerationStructureKHR accelWrite{};
    accelWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelWrite.accelerationStructureCount = 1;
    accelWrite.pAccelerationStructures = &tlasHandle;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS);  // 0
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    write.pNext = &accelWrite;

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
}

} // namespace VulkanRTX