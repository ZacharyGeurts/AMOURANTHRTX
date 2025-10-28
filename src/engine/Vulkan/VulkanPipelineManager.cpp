// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan pipeline management – implementation (fixed)

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include "engine/MaterialData.hpp"

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

#define VK_CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) throw std::runtime_error(#x " failed: " + std::to_string(r)); } while(0)

template<class F> struct ScopeGuard { F f; ~ScopeGuard() { f(); } };
template<class F> ScopeGuard<F> finally(F&& f) { return {std::forward<F>(f)}; }

static VkPipelineShaderStageCreateInfo makeShaderStage(VkShaderModule mod,
                                                       VkShaderStageFlagBits stage)
{
    return VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .module = mod,
        .pName = "main"
    };
}

/* --------------------------------------------------------------------- */
/*  Shader loading                                                       */
/* --------------------------------------------------------------------- */
VkShaderModule VulkanPipelineManager::loadShader(VkDevice device,
                                                 const std::string& shaderType)
{
    auto it = shaderPaths_.find(shaderType);
    if (it == shaderPaths_.end())
        throw std::runtime_error("Shader type not found: " + shaderType);
    const std::string& filename = it->second;

    std::filesystem::path shaderPath = std::filesystem::absolute(filename);
    if (!std::filesystem::exists(shaderPath))
        throw std::runtime_error("Shader file does not exist: " + shaderPath.string());

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
    VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &mod));
    return mod;
}

/* --------------------------------------------------------------------- */
/*  Constructor                                                          */
/* --------------------------------------------------------------------- */
VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context,
                                             int width, int height)
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
      tlasHandle_(VK_NULL_HANDLE),
      blasHandle_(VK_NULL_HANDLE),
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

    createAsFunc_ = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCreateAccelerationStructureKHR"));
    destroyAsFunc_ = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(context_.device, "vkDestroyAccelerationStructureKHR"));
    if (!createAsFunc_ || !destroyAsFunc_)
        throw std::runtime_error("Failed to load AS extension functions");

    if (context_.commandBuffers.empty() && context_.commandPool != VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo ai{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = context_.commandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        VkCommandBuffer cb;
        VK_CHECK(vkAllocateCommandBuffers(context_.device, &ai, &cb));
        context_.commandBuffers.push_back(cb);
    }

    createPipelineCache();
    createRayTracingDescriptorSetLayout();
    createGraphicsDescriptorSetLayout();

    // ----- simple render pass ------------------------------------------------
    VkAttachmentDescription colorAtt{
        .format         = context_.swapchainImageFormat,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{
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
        .pAttachments    = &colorAtt,
        .subpassCount    = 1,
        .pSubpasses      = &sub,
        .dependencyCount = 1,
        .pDependencies   = &dep
    };
    VK_CHECK(vkCreateRenderPass(context_.device, &rpInfo, nullptr, &renderPass_));
    context_.resourceManager.addRenderPass(renderPass_);

    createRayTracingPipeline();
    createComputePipeline();
    createShaderBindingTable();
    createGraphicsPipeline(width_, height_);
}

/* --------------------------------------------------------------------- */
/*  Destructor                                                           */
/* --------------------------------------------------------------------- */
VulkanPipelineManager::~VulkanPipelineManager()
{
    if (blasHandle_ && destroyAsFunc_) destroyAsFunc_(context_.device, blasHandle_, nullptr);
    if (tlasHandle_ && destroyAsFunc_) destroyAsFunc_(context_.device, tlasHandle_, nullptr);

    if (graphicsPipeline_)       context_.resourceManager.removePipeline(graphicsPipeline_->get());
    if (graphicsPipelineLayout_) context_.resourceManager.removePipelineLayout(graphicsPipelineLayout_->get());
    if (rayTracingPipeline_)     context_.resourceManager.removePipeline(rayTracingPipeline_->get());
    if (rayTracingPipelineLayout_) context_.resourceManager.removePipelineLayout(rayTracingPipelineLayout_->get());
    if (computePipeline_)        context_.resourceManager.removePipeline(computePipeline_->get());
    if (computePipelineLayout_)  context_.resourceManager.removePipelineLayout(computePipelineLayout_->get());

    if (rasterPrepassPipeline_) { context_.resourceManager.removePipeline(rasterPrepassPipeline_); vkDestroyPipeline(context_.device, rasterPrepassPipeline_, nullptr); }
    if (denoiserPostPipeline_)   { context_.resourceManager.removePipeline(denoiserPostPipeline_);   vkDestroyPipeline(context_.device, denoiserPostPipeline_, nullptr); }

    if (pipelineCache_) vkDestroyPipelineCache(context_.device, pipelineCache_, nullptr);
    if (renderPass_)    { context_.resourceManager.removeRenderPass(renderPass_); vkDestroyRenderPass(context_.device, renderPass_, nullptr); }

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_) {
        auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (fn) fn(context_.instance, debugMessenger_, nullptr);
    }
#endif
}

/* --------------------------------------------------------------------- */
/*  Debug callback                                                       */
/* --------------------------------------------------------------------- */
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
            LOG_ERROR("Vulkan validation: {}", data->pMessage);
            return VK_FALSE;
        }
    };
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(context_.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (fn) fn(context_.instance, &ci, nullptr, &debugMessenger_);
}
#endif

/* --------------------------------------------------------------------- */
/*  Pipeline cache                                                       */
/* --------------------------------------------------------------------- */
void VulkanPipelineManager::createPipelineCache()
{
    VkPipelineCacheCreateInfo ci{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VK_CHECK(vkCreatePipelineCache(context_.device, &ci, nullptr, &pipelineCache_));
}

/* --------------------------------------------------------------------- */
/*  Descriptor set layouts                                               */
/* --------------------------------------------------------------------- */
void VulkanPipelineManager::createRayTracingDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 10> bindings = {};

    bindings[0] = { static_cast<uint32_t>(DescriptorBindings::TLAS), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                    VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR };
    bindings[1] = { static_cast<uint32_t>(DescriptorBindings::StorageImage), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT };
    bindings[2] = { static_cast<uint32_t>(DescriptorBindings::CameraUBO), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR };
    bindings[3] = { static_cast<uint32_t>(DescriptorBindings::MaterialSSBO), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 26,
                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR };
    bindings[4] = { static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
    bindings[5] = { static_cast<uint32_t>(DescriptorBindings::AlphaTex), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                    VK_SHADER_STAGE_ANY_HIT_BIT_KHR };
    bindings[6] = { static_cast<uint32_t>(DescriptorBindings::EnvMap), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                    VK_SHADER_STAGE_CALLABLE_BIT_KHR };
    bindings[7] = { static_cast<uint32_t>(DescriptorBindings::DensityVolume), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                    VK_SHADER_STAGE_ANY_HIT_BIT_KHR };
    bindings[8] = { static_cast<uint32_t>(DescriptorBindings::GDepth), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT };
    bindings[9] = { static_cast<uint32_t>(DescriptorBindings::GNormal), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT };

    VkDescriptorSetLayoutCreateInfo ci{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &ci, nullptr,
                                          &context_.rayTracingDescriptorSetLayout));
    context_.resourceManager.addDescriptorSetLayout(context_.rayTracingDescriptorSetLayout);
}

void VulkanPipelineManager::createGraphicsDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,        1, VK_SHADER_STAGE_FRAGMENT_BIT}
    }};

    VkDescriptorSetLayoutCreateInfo ci{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &ci, nullptr,
                                          &context_.graphicsDescriptorSetLayout));
    context_.resourceManager.addDescriptorSetLayout(context_.graphicsDescriptorSetLayout);
}

/* --------------------------------------------------------------------- */
/*  Graphics pipeline                                                    */
/* --------------------------------------------------------------------- */
void VulkanPipelineManager::createGraphicsPipeline(int /*width*/, int /*height*/)
{
    auto vert = loadShader(context_.device, "vertex");
    auto frag = loadShader(context_.device, "fragment");
    auto cleanup = finally([&]{ vkDestroyShaderModule(context_.device, vert, nullptr);
                                vkDestroyShaderModule(context_.device, frag, nullptr); });

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
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutCI, nullptr, &layout));
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
    VK_CHECK(vkCreateGraphicsPipelines(context_.device, pipelineCache_, 1,
                                       &pipeCI, nullptr, &pipe));
    graphicsPipeline_ = std::make_unique<VulkanResource<VkPipeline,
                                                        PFN_vkDestroyPipeline>>(
        context_.device, pipe, vkDestroyPipeline);
    context_.resourceManager.addPipeline(pipe);
}

/* --------------------------------------------------------------------- */
/*  Record graphics commands                                             */
/* --------------------------------------------------------------------- */
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
    VK_CHECK(vkBeginCommandBuffer(cmd, &bi));

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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      getGraphicsPipeline());
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
    VK_CHECK(vkEndCommandBuffer(cmd));
}

/* --------------------------------------------------------------------- */
/*  Acceleration structures – TLAS build-range fixed                     */
/* --------------------------------------------------------------------- */
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer,
                                                         VkBuffer indexBuffer)
{
    VkDeviceAddress vertexAddr = VulkanInitializer::getBufferDeviceAddress(context_.device, vertexBuffer);
    VkDeviceAddress indexAddr  = VulkanInitializer::getBufferDeviceAddress(context_.device, indexBuffer);

    // ----- properties -------------------------------------------------------
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{};
    asProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &asProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);

    // ----- BLAS geometry ----------------------------------------------------
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddr;
    triangles.vertexStride = sizeof(glm::vec3);
    triangles.maxVertex = 2;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddr;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR | VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

    VkAccelerationStructureBuildRangeInfoKHR buildRange{};
    buildRange.primitiveCount = 1;

    // ----- build sizes -------------------------------------------------------
    auto vkGetBuildSizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureBuildSizesKHR"));
    if (!vkGetBuildSizes) throw std::runtime_error("vkGetAccelerationStructureBuildSizesKHR missing");

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    std::vector<uint32_t> maxPrim{1};
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetBuildSizes(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                    &buildInfo, maxPrim.data(), &sizeInfo);

    // ----- BLAS buffer -------------------------------------------------------
    VkBuffer blasBuffer = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blasBuffer, blasMemory, nullptr, context_.resourceManager);

    VkAccelerationStructureCreateInfoKHR asCreate{};
    asCreate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asCreate.buffer = blasBuffer;
    asCreate.size = sizeInfo.accelerationStructureSize;
    asCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VK_CHECK(createAsFunc_(context_.device, &asCreate, nullptr, &blasHandle_));

    // ----- scratch -----------------------------------------------------------
    VkDeviceSize scratchAlign = asProps.minAccelerationStructureScratchOffsetAlignment;
    VkDeviceSize scratchSize = (sizeInfo.buildScratchSize + scratchAlign - 1) & ~(scratchAlign - 1);
    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, scratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer, scratchMemory, nullptr, context_.resourceManager);

    // ----- build BLAS --------------------------------------------------------
    VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    buildInfo.dstAccelerationStructure = blasHandle_;
    buildInfo.scratchData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, scratchBuffer);
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &buildRange;

    auto vkCmdBuildAS = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCmdBuildAccelerationStructuresKHR"));
    if (!vkCmdBuildAS) throw std::runtime_error("vkCmdBuildAccelerationStructuresKHR missing");
    vkCmdBuildAS(cmd, 1, &buildInfo, &pRange);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    // ----- cleanup scratch ---------------------------------------------------
    context_.resourceManager.removeBuffer(scratchBuffer);
    context_.resourceManager.removeMemory(scratchMemory);
    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

    // ----- TLAS instance -----------------------------------------------------
    auto vkGetASAddress = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureDeviceAddressKHR"));
    if (!vkGetASAddress) throw std::runtime_error("vkGetAccelerationStructureDeviceAddressKHR missing");

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blasHandle_
    };
    VkDeviceAddress blasAddr = vkGetASAddress(context_.device, &addrInfo);

    VkAccelerationStructureInstanceKHR instance{};
    std::memset(&instance.transform, 0, sizeof(instance.transform));
    instance.transform.matrix[0][0] = instance.transform.matrix[1][1] = instance.transform.matrix[2][2] = 1.0f;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = blasAddr;

    VkDeviceSize instSize = sizeof(VkAccelerationStructureInstanceKHR);
    VkBuffer instBuffer = VK_NULL_HANDLE, instStaging = VK_NULL_HANDLE;
    VkDeviceMemory instMem = VK_NULL_HANDLE, instStagingMem = VK_NULL_HANDLE;

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, instSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        instBuffer, instMem, nullptr, context_.resourceManager);

    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, instSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instStaging, instStagingMem, nullptr, context_.resourceManager);

    void* pData = nullptr;
    VK_CHECK(vkMapMemory(context_.device, instStagingMem, 0, instSize, 0, &pData));
    std::memcpy(pData, &instance, instSize);
    vkUnmapMemory(context_.device, instStagingMem);

    cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    VkBufferCopy copy{.size = instSize};
    vkCmdCopyBuffer(cmd, instStaging, instBuffer, 1, &copy);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    context_.resourceManager.removeBuffer(instStaging);
    context_.resourceManager.removeMemory(instStagingMem);
    vkDestroyBuffer(context_.device, instStaging, nullptr);
    vkFreeMemory(context_.device, instStagingMem, nullptr);

    // ----- TLAS geometry -----------------------------------------------------
    VkAccelerationStructureGeometryInstancesDataKHR instData{};
    instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instData.data.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, instBuffer);

    VkAccelerationStructureGeometryKHR tlasGeom{};
    tlasGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances = instData;
    tlasGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR | VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &tlasGeom;

    VkAccelerationStructureBuildRangeInfoKHR tlasRange{};
    tlasRange.primitiveCount = 1;

    vkGetBuildSizes(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                    &buildInfo, maxPrim.data(), &sizeInfo);

    VkBuffer tlasBuffer = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBuffer, tlasMemory, nullptr, context_.resourceManager);

    asCreate.buffer = tlasBuffer;
    asCreate.size = sizeInfo.accelerationStructureSize;
    asCreate.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VK_CHECK(createAsFunc_(context_.device, &asCreate, nullptr, &tlasHandle_));

    // ----- TLAS scratch ------------------------------------------------------
    VkDeviceSize tlasScratch = (sizeInfo.buildScratchSize + scratchAlign - 1) & ~(scratchAlign - 1);
    VkBuffer tlasScratchBuf = VK_NULL_HANDLE;
    VkDeviceMemory tlasScratchMem = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, tlasScratch,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasScratchBuf, tlasScratchMem, nullptr, context_.resourceManager);

    cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    buildInfo.dstAccelerationStructure = tlasHandle_;
    buildInfo.scratchData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, tlasScratchBuf);
    const VkAccelerationStructureBuildRangeInfoKHR* pTLASRange = &tlasRange;
    vkCmdBuildAS(cmd, 1, &buildInfo, &pTLASRange);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    // ----- cleanup TLAS scratch ----------------------------------------------
    context_.resourceManager.removeBuffer(tlasScratchBuf);
    context_.resourceManager.removeMemory(tlasScratchMem);
    vkDestroyBuffer(context_.device, tlasScratchBuf, nullptr);
    vkFreeMemory(context_.device, tlasScratchMem, nullptr);

    LOG_INFO("Acceleration structures built – BLAS: {}, TLAS: {}", (void*)blasHandle_, (void*)tlasHandle_);
}

/* --------------------------------------------------------------------- */
/*  Update descriptor set with TLAS                                      */
/* --------------------------------------------------------------------- */
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet,
                                                          VkAccelerationStructureKHR tlasHandle)
{
    VkWriteDescriptorSetAccelerationStructureKHR accelWrite{};
    accelWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelWrite.accelerationStructureCount = 1;
    accelWrite.pAccelerationStructures = &tlasHandle;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS);
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    write.pNext = &accelWrite;

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
}

} // namespace VulkanRTX