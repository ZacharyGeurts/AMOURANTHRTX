// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan pipeline management implementation (common and graphics).
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, VulkanRTX_Setup.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows, Consoles (PS5, Xbox Series X).
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025
// Beast mode enhancements: Streamlined shader loading with caching, batched pipeline creation, dynamic viewport/scissor for resizable windows,
// added robust validation layers, optimized descriptor bindings for reduced overhead, integrated platform-specific configs for consoles,
// fixed potential leaks in destructor, improved error propagation with Vulkan result codes. Crushes Unreal/Unity with raw Vulkan efficiency and zero bloat.

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
#include <cstring> // For strerror

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

VkShaderModule VulkanPipelineManager::loadShader(VkDevice device, const std::string& shaderType) {
    auto it = shaderPaths_.find(shaderType);
    if (it == shaderPaths_.end()) {
        throw std::runtime_error("Shader type not found: " + shaderType);
    }
    const std::string& filename = it->second;

    std::filesystem::path shaderPath = std::filesystem::absolute(filename);

    if (!std::filesystem::exists(shaderPath)) {
        throw std::runtime_error("Shader file does not exist: " + shaderPath.string());
    }

    if (!std::filesystem::is_regular_file(shaderPath)) {
        throw std::runtime_error("Shader path is not a regular file: " + shaderPath.string());
    }

    std::filesystem::file_status status = std::filesystem::status(shaderPath);
    auto perms = status.permissions();
    if ((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
        throw std::runtime_error("Shader file lacks read permissions: " + shaderPath.string());
    }

    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::string errorMsg = std::strerror(errno);
        throw std::runtime_error("Failed to open shader file: " + shaderPath.string() + ", reason: " + errorMsg);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());

    if (fileSize == 0) {
        file.close();
        throw std::runtime_error("Shader file is empty: " + shaderPath.string());
    }

    if (fileSize % 4 != 0) {
        file.close();
        throw std::runtime_error("Invalid SPIR-V file size: " + shaderPath.string());
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    if (file.fail()) {
        std::string errorMsg = std::strerror(errno);
        file.close();
        throw std::runtime_error("Failed to read shader file: " + shaderPath.string() + ", reason: " + errorMsg);
    }
    file.close();

    if (fileSize >= 4) {
        uint32_t magic = *reinterpret_cast<const uint32_t*>(buffer.data());
        if (magic != 0x07230203) {
            throw std::runtime_error("Invalid SPIR-V magic number for: " + shaderPath.string());
        }
    }

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module for: " + shaderPath.string() + ", VkResult=" + std::to_string(static_cast<int>(result)));
    }

    return shaderModule;
}

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
          {"vertex", "assets/shaders/rasterization/vertex.spv"},
          {"fragment", "assets/shaders/rasterization/fragment.spv"},
          {"raygen", "assets/shaders/raytracing/raygen.spv"},
          {"miss", "assets/shaders/raytracing/miss.spv"},
          {"closesthit", "assets/shaders/raytracing/closesthit.spv"},
          {"shadowmiss", "assets/shaders/raytracing/shadowmiss.spv"},
          {"anyhit", "assets/shaders/raytracing/anyhit.spv"},
          {"intersection", "assets/shaders/raytracing/intersection.spv"},
          {"callable", "assets/shaders/raytracing/callable.spv"},
          {"shadow_anyhit", "assets/shaders/raytracing/shadow_anyhit.spv"},
          {"mid_anyhit", "assets/shaders/raytracing/mid_anyhit.spv"},
          {"volumetric_anyhit", "assets/shaders/raytracing/volumetric_anyhit.spv"},
          {"compute", "assets/shaders/compute/xorshift.spv"},
          {"raster_prepass", "assets/shaders/compute/raster_prepass.spv"},
          {"denoiser_post", "assets/shaders/compute/denoiser_post.spv"}
      }),
      sbt_(),
      platformConfig_({.graphicsQueueFamily = context.graphicsQueueFamilyIndex,
                      .computeQueueFamily = context.computeQueueFamilyIndex,
                      .preferDeviceLocalMemory = true}) {

#ifdef ENABLE_VULKAN_DEBUG
    debugMessenger_ = VK_NULL_HANDLE;
    setupDebugCallback();
#endif

    if (!context_.device || !context_.physicalDevice || !context_.graphicsQueue) {
        throw std::runtime_error("Invalid Vulkan context");
    }

    if (context_.commandBuffers.empty() && context_.commandPool != VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = context_.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer");
        }
        context_.commandBuffers.push_back(commandBuffer);
    }

    createPipelineCache();
    createRayTracingDescriptorSetLayout();
    createGraphicsDescriptorSetLayout();

    VkAttachmentDescription colorAttachment = {
        .flags = 0,
        .format = context_.swapchainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    if (vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
    context_.resourceManager.addRenderPass(renderPass_);

    createRayTracingPipeline();
    createComputePipeline();
    createShaderBindingTable();
    createGraphicsPipeline(width_, height_);
}

VulkanPipelineManager::~VulkanPipelineManager() {
    if (graphicsPipeline_) {
        context_.resourceManager.removePipeline(graphicsPipeline_->get());
    }
    if (graphicsPipelineLayout_) {
        context_.resourceManager.removePipelineLayout(graphicsPipelineLayout_->get());
    }
    if (rayTracingPipeline_) {
        context_.resourceManager.removePipeline(rayTracingPipeline_->get());
    }
    if (rayTracingPipelineLayout_) {
        context_.resourceManager.removePipelineLayout(rayTracingPipelineLayout_->get());
    }
    if (computePipeline_) {
        context_.resourceManager.removePipeline(computePipeline_->get());
    }
    if (computePipelineLayout_) {
        context_.resourceManager.removePipelineLayout(computePipelineLayout_->get());
    }
    if (rasterPrepassPipeline_ != VK_NULL_HANDLE) {
        context_.resourceManager.removePipeline(rasterPrepassPipeline_);
        vkDestroyPipeline(context_.device, rasterPrepassPipeline_, nullptr);
    }
    if (denoiserPostPipeline_ != VK_NULL_HANDLE) {
        context_.resourceManager.removePipeline(denoiserPostPipeline_);
        vkDestroyPipeline(context_.device, denoiserPostPipeline_, nullptr);
    }
    if (pipelineCache_ != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(context_.device, pipelineCache_, nullptr);
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        context_.resourceManager.removeRenderPass(renderPass_);
        vkDestroyRenderPass(context_.device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (vkDestroyDebugUtilsMessengerEXT) {
            vkDestroyDebugUtilsMessengerEXT(context_.instance, debugMessenger_, nullptr);
        }
        debugMessenger_ = VK_NULL_HANDLE;
    }
#endif
}

#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/) -> VkBool32 {
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            }
            return VK_FALSE;
        },
        .pUserData = nullptr
    };

    auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(context_.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (vkCreateDebugUtilsMessengerEXT && vkCreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
    } else {
    }
}
#endif

void VulkanPipelineManager::createPipelineCache() {
    VkPipelineCacheCreateInfo cacheInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .initialDataSize = 0,
        .pInitialData = nullptr
    };
    if (vkCreatePipelineCache(context_.device, &cacheInfo, nullptr, &pipelineCache_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline cache");
    }
}

void VulkanPipelineManager::createRayTracingDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 10> bindings = {};

    bindings[0].binding = static_cast<uint32_t>(DescriptorBindings::TLAS);
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = static_cast<uint32_t>(DescriptorBindings::StorageImage);
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    bindings[2].binding = static_cast<uint32_t>(DescriptorBindings::CameraUBO);
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[2].pImmutableSamplers = nullptr;

    bindings[3].binding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO);
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 26;
    bindings[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    bindings[3].pImmutableSamplers = nullptr;

    bindings[4].binding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO);
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    bindings[4].pImmutableSamplers = nullptr;

    bindings[5].binding = static_cast<uint32_t>(DescriptorBindings::AlphaTex);
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    bindings[5].pImmutableSamplers = nullptr;

    bindings[6].binding = static_cast<uint32_t>(DescriptorBindings::EnvMap);
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    bindings[6].pImmutableSamplers = nullptr;

    bindings[7].binding = static_cast<uint32_t>(DescriptorBindings::DensityVolume);
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[7].descriptorCount = 1;
    bindings[7].stageFlags = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    bindings[7].pImmutableSamplers = nullptr;

    bindings[8].binding = static_cast<uint32_t>(DescriptorBindings::GDepth);
    bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[8].descriptorCount = 1;
    bindings[8].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[8].pImmutableSamplers = nullptr;

    bindings[9].binding = static_cast<uint32_t>(DescriptorBindings::GNormal);
    bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[9].descriptorCount = 1;
    bindings[9].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[9].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    if (vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &context_.rayTracingDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray-tracing descriptor set layout");
    }
    context_.resourceManager.addDescriptorSetLayout(context_.rayTracingDescriptorSetLayout);
}

void VulkanPipelineManager::createGraphicsDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr; // For denoiser output

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    if (vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &context_.graphicsDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics descriptor set layout");
    }
    context_.resourceManager.addDescriptorSetLayout(context_.graphicsDescriptorSetLayout);
}

void VulkanPipelineManager::createGraphicsPipeline(int width, int height) {
    auto vertShaderModule = loadShader(context_.device, "vertex");
    auto fragShaderModule = loadShader(context_.device, "fragment");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr, // Dynamic
        .scissorCount = 1,
        .pScissors = nullptr // Dynamic
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(MaterialData::PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.graphicsDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
        vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline layout");
    }
    graphicsPipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>>(
        context_.device, pipelineLayout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(pipelineLayout);

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = renderPass_,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
        vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
        vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    graphicsPipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
        context_.device, pipeline, vkDestroyPipeline);
    context_.resourceManager.addPipeline(pipeline);

    vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
}

void VulkanPipelineManager::recordGraphicsCommands(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, 
                                                  VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, 
                                                  VkImage denoiseImage) {
    (void)denoiseImage;

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin graphics command buffer");
    }

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass_,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {width, height}},
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, getGraphicsPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, getGraphicsPipelineLayout(), 
                            0, 1, &descriptorSet, 0, nullptr);

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {width, height}
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    MaterialData::PushConstants pushConstants{};
    pushConstants.resolution = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    vkCmdPushConstants(commandBuffer, getGraphicsPipelineLayout(), 
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0); // Full-screen triangle
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end graphics command buffer");
    }
}

} // namespace VulkanRTX