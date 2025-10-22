// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan pipeline management implementation.
// Dependencies: Vulkan 1.3+, GLM, VulkanCore.hpp, Vulkan_init.hpp, VulkanRTX_Setup.hpp, logging.hpp, Dispose.hpp.
// Supported platforms: Linux, Windows, Consoles (PS5, Xbox Series X).
// Optimized for high-end GPUs with 8 GB VRAM (e.g., NVIDIA RTX 3070, AMD RX 6800).
// Zachary Geurts 2025

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/logging.hpp"
#include "engine/Dispose.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <array>
#include <fstream>
#include <source_location>
#include <cstring>
#include <vector>
#include <glm/glm.hpp>
#include <format>
#include <unordered_map>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

VkShaderModule VulkanPipelineManager::loadShader(VkDevice device, const std::string& shaderType) {
    auto it = shaderPaths_.find(shaderType);
    if (it == shaderPaths_.end()) {
        LOG_ERROR_CAT("PipelineManager", "Shader type {} not found in registry", shaderType);
        throw std::runtime_error("Shader type not found: " + shaderType);
    }
    const std::string& filename = it->second;
    LOG_DEBUG_CAT("PipelineManager", "Loading shader: {} ({})", shaderType, filename);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("PipelineManager", "Failed to open shader file: {}", filename);
        throw std::runtime_error("Failed to open shader file: " + filename);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize % 4 != 0) {
        LOG_ERROR_CAT("PipelineManager", "Invalid SPIR-V file size (not multiple of 4): {}", filename);
        throw std::runtime_error("Invalid SPIR-V file size: " + filename);
    }
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

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
        LOG_ERROR_CAT("PipelineManager", "Failed to create shader module for {}: VkResult={}", filename, static_cast<int>(result));
        throw std::runtime_error("Failed to create shader module: " + filename);
    }
    LOG_DEBUG_CAT("PipelineManager", "Loaded shader module: {:p} from {}", static_cast<void*>(shaderModule), filename);
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
      shaderPaths_({
          {"vertex", "assets/shaders/rasterization/vertex.spv"},
          {"fragment", "assets/shaders/rasterization/fragment.spv"},
          {"raygen", "assets/shaders/raytracing/raygen.spv"},
          {"miss", "assets/shaders/raytracing/miss.spv"},
          {"closesthit", "assets/shaders/raytracing/closesthit.spv"},
          {"shadowmiss", "assets/shaders/raytracing/shadowmiss.spv"},
          {"compute", "assets/shaders/compute/xorshift.spv"}
      }),
      sbt_(),
      platformConfig_({.graphicsQueueFamily = context.graphicsQueueFamilyIndex,
                      .computeQueueFamily = context.computeQueueFamilyIndex,
                      .preferDeviceLocalMemory = true}) {
    LOG_INFO_CAT("PipelineManager", "Initializing VulkanPipelineManager with resolution {}x{}", width, height);

#ifdef ENABLE_VULKAN_DEBUG
    debugMessenger_ = VK_NULL_HANDLE;
    setupDebugCallback();
#endif

    if (!context_.device || !context_.physicalDevice || !context_.graphicsQueue) {
        LOG_ERROR_CAT("PipelineManager", "Invalid Vulkan context: device={:p}, physicalDevice={:p}, graphicsQueue={:p}",
                      static_cast<void*>(context_.device), static_cast<void*>(context_.physicalDevice), static_cast<void*>(context_.graphicsQueue));
        throw std::runtime_error("Invalid Vulkan context");
    }

    // Ensure command buffers are allocated
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
            LOG_ERROR_CAT("PipelineManager", "Failed to allocate command buffer");
            throw std::runtime_error("Failed to allocate command buffer");
        }
        context_.commandBuffers.push_back(commandBuffer);
        LOG_INFO_CAT("PipelineManager", "Allocated command buffer: {:p}", static_cast<void*>(commandBuffer));
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
        LOG_ERROR_CAT("PipelineManager", "Failed to create render pass");
        throw std::runtime_error("Failed to create render pass");
    }
    context_.resourceManager.addRenderPass(renderPass_);
    LOG_INFO_CAT("PipelineManager", "Created render pass: {:p}", static_cast<void*>(renderPass_));

    createGraphicsPipeline(width, height);
    createRayTracingPipeline();
    createComputePipeline();
    createShaderBindingTable();
    LOG_INFO_CAT("PipelineManager", "VulkanPipelineManager initialized successfully");
}

VulkanPipelineManager::~VulkanPipelineManager() {
    if (pipelineCache_ != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(context_.device, pipelineCache_, nullptr);
        LOG_DEBUG_CAT("PipelineManager", "Destroyed pipeline cache: {:p}", static_cast<void*>(pipelineCache_));
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        context_.resourceManager.removeRenderPass(renderPass_);
        vkDestroyRenderPass(context_.device, renderPass_, nullptr);
        LOG_DEBUG_CAT("PipelineManager", "Destroyed render pass: {:p}", static_cast<void*>(renderPass_));
        renderPass_ = VK_NULL_HANDLE;
    }
#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (vkDestroyDebugUtilsMessengerEXT) {
            vkDestroyDebugUtilsMessengerEXT(context_.instance, debugMessenger_, nullptr);
            LOG_DEBUG_CAT("PipelineManager", "Destroyed debug messenger: {:p}", static_cast<void*>(debugMessenger_));
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
                LOG_ERROR_CAT("VulkanValidation", "Validation error: {}", pCallbackData->pMessage);
            } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                LOG_WARNING_CAT("VulkanValidation", "Validation warning: {}", pCallbackData->pMessage);
            }
            return VK_FALSE;
        },
        .pUserData = nullptr
    };

    auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(context_.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (vkCreateDebugUtilsMessengerEXT && vkCreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
        LOG_WARNING_CAT("PipelineManager", "Failed to create debug messenger");
    } else {
        LOG_INFO_CAT("PipelineManager", "Created debug messenger: {:p}", static_cast<void*>(debugMessenger_));
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
        LOG_ERROR_CAT("PipelineManager", "Failed to create pipeline cache");
        throw std::runtime_error("Failed to create pipeline cache");
    }
    LOG_INFO_CAT("PipelineManager", "Created pipeline cache: {:p}", static_cast<void*>(pipelineCache_));
}

void VulkanPipelineManager::createRayTracingDescriptorSetLayout() {
    LOG_DEBUG_CAT("PipelineManager", "Creating ray-tracing descriptor set layout");
    std::array<VkDescriptorSetLayoutBinding, 7> bindings = {};

    bindings[0].binding = static_cast<uint32_t>(DescriptorBindings::TLAS);
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = static_cast<uint32_t>(DescriptorBindings::StorageImage);
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[1].pImmutableSamplers = nullptr;

    bindings[2].binding = static_cast<uint32_t>(DescriptorBindings::CameraUBO);
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[2].pImmutableSamplers = nullptr;

    bindings[3].binding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO);
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    bindings[3].pImmutableSamplers = nullptr;

    bindings[4].binding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO);
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    bindings[4].pImmutableSamplers = nullptr;

    bindings[5].binding = static_cast<uint32_t>(DescriptorBindings::DenoiseImage);
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    bindings[5].pImmutableSamplers = nullptr;

    bindings[6].binding = static_cast<uint32_t>(DescriptorBindings::EnvMap);
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    bindings[6].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    if (vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &context_.rayTracingDescriptorSetLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create ray-tracing descriptor set layout");
        throw std::runtime_error("Failed to create ray-tracing descriptor set layout");
    }
    context_.resourceManager.addDescriptorSetLayout(context_.rayTracingDescriptorSetLayout);
    LOG_INFO_CAT("PipelineManager", "Created ray-tracing descriptor set layout with {} bindings: {:p}",
                 bindings.size(), static_cast<void*>(context_.rayTracingDescriptorSetLayout));
}

void VulkanPipelineManager::createGraphicsDescriptorSetLayout() {
    LOG_DEBUG_CAT("PipelineManager", "Creating graphics descriptor set layout");
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};

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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    if (vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &context_.graphicsDescriptorSetLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create graphics descriptor set layout");
        throw std::runtime_error("Failed to create graphics descriptor set layout");
    }
    context_.resourceManager.addDescriptorSetLayout(context_.graphicsDescriptorSetLayout);
    LOG_INFO_CAT("PipelineManager", "Created graphics descriptor set layout: {:p}",
                 static_cast<void*>(context_.graphicsDescriptorSetLayout));
}

void VulkanPipelineManager::createGraphicsPipeline(int width, int height) {
    LOG_DEBUG_CAT("PipelineManager", "Creating graphics pipeline for {}x{}", width, height);
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

    VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .stride = sizeof(glm::vec3) + sizeof(glm::vec2),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        },
        VkVertexInputAttributeDescription{
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(glm::vec3)
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
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
        LOG_ERROR_CAT("PipelineManager", "Failed to create graphics pipeline layout");
        vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
        vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline layout");
    }
    graphicsPipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>>(
        context_.device, pipelineLayout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(pipelineLayout);
    LOG_INFO_CAT("PipelineManager", "Created graphics pipeline layout: {:p}", static_cast<void*>(pipelineLayout));

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
        .pDynamicState = nullptr,
        .layout = pipelineLayout,
        .renderPass = renderPass_,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create graphics pipeline");
        vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
        vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
        vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    graphicsPipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
        context_.device, pipeline, vkDestroyPipeline);
    context_.resourceManager.addPipeline(pipeline);
    LOG_INFO_CAT("PipelineManager", "Created graphics pipeline: {:p}", static_cast<void*>(pipeline));

    vkDestroyShaderModule(context_.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(context_.device, fragShaderModule, nullptr);
}

void VulkanPipelineManager::createRayTracingPipeline() {
    LOG_DEBUG_CAT("PipelineManager", "Creating ray-tracing pipeline");
    std::vector<std::string> shaderTypes = {"raygen", "miss", "closesthit", "shadowmiss"};
    std::vector<VkShaderModule> shaderModules;
    for (const auto& type : shaderTypes) {
        shaderModules.push_back(loadShader(context_.device, type));
    }

    std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages = {
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = shaderModules[0],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[1],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = shaderModules[2],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[3],
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 4> shaderGroups = {
        VkRayTracingShaderGroupCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        VkRayTracingShaderGroupCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        VkRayTracingShaderGroupCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        VkRayTracingShaderGroupCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 3,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        }
    };

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
        .offset = 0,
        .size = sizeof(MaterialData::PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.rayTracingDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create ray-tracing pipeline layout");
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        throw std::runtime_error("Failed to create ray-tracing pipeline layout");
    }
    rayTracingPipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>>(
        context_.device, pipelineLayout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(pipelineLayout);
    LOG_INFO_CAT("PipelineManager", "Created ray-tracing pipeline layout: {:p}", static_cast<void*>(pipelineLayout));

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    rtProperties.pNext = nullptr;
    VkPhysicalDeviceProperties2 properties2 = {};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties2);

    uint32_t recursionDepth = (rtProperties.maxRayRecursionDepth > 0) ? std::min(2u, rtProperties.maxRayRecursionDepth) : 1;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .groupCount = static_cast<uint32_t>(shaderGroups.size()),
        .pGroups = shaderGroups.data(),
        .maxPipelineRayRecursionDepth = recursionDepth,
        .pLibraryInfo = nullptr,
        .pLibraryInterface = nullptr,
        .pDynamicState = nullptr,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    auto vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCreateRayTracingPipelinesKHR"));
    if (!vkCreateRayTracingPipelinesKHR) {
        LOG_ERROR_CAT("PipelineManager", "Failed to get vkCreateRayTracingPipelinesKHR function pointer");
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to get vkCreateRayTracingPipelinesKHR function pointer");
    }

    VkPipeline pipeline;
    VkResult result = vkCreateRayTracingPipelinesKHR(context_.device, VK_NULL_HANDLE, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create ray-tracing pipeline: VkResult={}", static_cast<int>(result));
        for (auto module : shaderModules) {
            vkDestroyShaderModule(context_.device, module, nullptr);
        }
        vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to create ray-tracing pipeline");
    }
    rayTracingPipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
        context_.device, pipeline, vkDestroyPipeline);
    context_.resourceManager.addPipeline(pipeline);
    LOG_INFO_CAT("PipelineManager", "Created ray-tracing pipeline: {:p}", static_cast<void*>(pipeline));

    for (auto module : shaderModules) {
        vkDestroyShaderModule(context_.device, module, nullptr);
        LOG_DEBUG_CAT("PipelineManager", "Destroyed shader module: {:p}", static_cast<void*>(module));
    }
}

void VulkanPipelineManager::createComputePipeline() {
    LOG_DEBUG_CAT("PipelineManager", "Creating compute pipeline");
    auto computeShaderModule = loadShader(context_.device, "compute");

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(MaterialData::PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &context_.rayTracingDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create compute pipeline layout");
        vkDestroyShaderModule(context_.device, computeShaderModule, nullptr);
        throw std::runtime_error("Failed to create compute pipeline layout");
    }
    computePipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>>(
        context_.device, pipelineLayout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(pipelineLayout);
    LOG_INFO_CAT("PipelineManager", "Created compute pipeline layout: {:p}", static_cast<void*>(pipelineLayout));

    VkPipelineShaderStageCreateInfo shaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };

    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shaderStage,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline pipeline;
    if (vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create compute pipeline");
        vkDestroyShaderModule(context_.device, computeShaderModule, nullptr);
        vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
        throw std::runtime_error("Failed to create compute pipeline");
    }
    computePipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
        context_.device, pipeline, vkDestroyPipeline);
    context_.resourceManager.addPipeline(pipeline);
    LOG_INFO_CAT("PipelineManager", "Created compute pipeline: {:p}", static_cast<void*>(pipeline));

    vkDestroyShaderModule(context_.device, computeShaderModule, nullptr);
}

void VulkanPipelineManager::createShaderBindingTable() {
    LOG_DEBUG_CAT("PipelineManager", "Creating shader binding table");
    if (!rayTracingPipeline_) {
        LOG_ERROR_CAT("PipelineManager", "Ray-tracing pipeline not initialized");
        throw std::runtime_error("Ray-tracing pipeline not initialized");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    rtProperties.pNext = nullptr;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties);

    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
    const uint32_t raygenGroupCount = 1;
    const uint32_t missGroupCount = 2; // Primary and shadow miss
    const uint32_t hitGroupCount = 1;
    const uint32_t callableGroupCount = 0;
    const uint32_t handleSizeAligned = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const VkDeviceSize sbtSize = (raygenGroupCount + missGroupCount + hitGroupCount + callableGroupCount) * static_cast<VkDeviceSize>(handleSizeAligned);

    // Create device-local SBT buffer
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sbtSize,
        .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &sbtBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create SBT buffer");
        throw std::runtime_error("Failed to create SBT buffer");
    }
    LOG_DEBUG_CAT("PipelineManager", "Created SBT buffer: {:p}, size: {}", static_cast<void*>(sbtBuffer), sbtSize);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, sbtBuffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &allocFlagsInfo,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = VulkanInitializer::findMemoryType(context_.physicalDevice, memRequirements.memoryTypeBits,
                                                            platformConfig_.preferDeviceLocalMemory ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT :
                                                                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(context_.device, &allocInfo, nullptr, &sbtMemory) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to allocate SBT memory");
        vkDestroyBuffer(context_.device, sbtBuffer, nullptr);
        throw std::runtime_error("Failed to allocate SBT memory");
    }
    if (vkBindBufferMemory(context_.device, sbtBuffer, sbtMemory, 0) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to bind SBT memory");
        vkDestroyBuffer(context_.device, sbtBuffer, nullptr);
        vkFreeMemory(context_.device, sbtMemory, nullptr);
        throw std::runtime_error("Failed to bind SBT memory");
    }
    context_.resourceManager.addBuffer(sbtBuffer);
    context_.resourceManager.addMemory(sbtMemory);
    LOG_DEBUG_CAT("PipelineManager", "Allocated and bound SBT memory: {:p}", static_cast<void*>(sbtMemory));

    // Create staging buffer for SBT data
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (platformConfig_.preferDeviceLocalMemory) {
        VkBufferCreateInfo stagingInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sbtSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };
        if (vkCreateBuffer(context_.device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            LOG_ERROR_CAT("PipelineManager", "Failed to create SBT staging buffer");
            vkDestroyBuffer(context_.device, sbtBuffer, nullptr);
            vkFreeMemory(context_.device, sbtMemory, nullptr);
            throw std::runtime_error("Failed to create SBT staging buffer");
        }

        VkMemoryRequirements stagingMemReq;
        vkGetBufferMemoryRequirements(context_.device, stagingBuffer, &stagingMemReq);
        VkMemoryAllocateInfo stagingAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = stagingMemReq.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(context_.physicalDevice, stagingMemReq.memoryTypeBits,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        if (vkAllocateMemory(context_.device, &stagingAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
            LOG_ERROR_CAT("PipelineManager", "Failed to allocate SBT staging memory");
            vkDestroyBuffer(context_.device, sbtBuffer, nullptr);
            vkFreeMemory(context_.device, sbtMemory, nullptr);
            vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
            throw std::runtime_error("Failed to allocate SBT staging memory");
        }
        if (vkBindBufferMemory(context_.device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
            LOG_ERROR_CAT("PipelineManager", "Failed to bind SBT staging memory");
            vkDestroyBuffer(context_.device, sbtBuffer, nullptr);
            vkFreeMemory(context_.device, sbtMemory, nullptr);
            vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
            vkFreeMemory(context_.device, stagingMemory, nullptr);
            throw std::runtime_error("Failed to bind SBT staging memory");
        }
    }

    auto vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetRayTracingShaderGroupHandlesKHR"));
    if (!vkGetRayTracingShaderGroupHandlesKHR) {
        LOG_ERROR_CAT("PipelineManager", "Failed to get vkGetRayTracingShaderGroupHandlesKHR function pointer");
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        throw std::runtime_error("Failed to get vkGetRayTracingShaderGroupHandlesKHR function pointer");
    }

    sbt_ = ShaderBindingTable(context_.device, sbtBuffer, sbtMemory, vkDestroyBuffer, vkFreeMemory);

    std::vector<uint8_t> shaderGroupHandles((raygenGroupCount + missGroupCount + hitGroupCount + callableGroupCount) * handleSize);
    if (vkGetRayTracingShaderGroupHandlesKHR(context_.device, rayTracingPipeline_->get(), 0, raygenGroupCount + missGroupCount + hitGroupCount + callableGroupCount, shaderGroupHandles.size(), shaderGroupHandles.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to get shader group handles");
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        throw std::runtime_error("Failed to get shader group handles");
    }

    void* mappedData = nullptr;
    VkDeviceMemory targetMemory = platformConfig_.preferDeviceLocalMemory ? stagingMemory : sbtMemory;
    if (vkMapMemory(context_.device, targetMemory, 0, sbtSize, 0, &mappedData) != VK_SUCCESS || !mappedData) {
        LOG_ERROR_CAT("PipelineManager", "Failed to map SBT memory or mapped memory is null");
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
        throw std::runtime_error("Failed to map SBT memory or mapped memory is null");
    }

    uint64_t currentOffset = 0;
    memcpy(static_cast<uint8_t*>(mappedData) + currentOffset, shaderGroupHandles.data() + 0 * handleSize, handleSize);
    currentOffset += handleSizeAligned;
    memcpy(static_cast<uint8_t*>(mappedData) + currentOffset, shaderGroupHandles.data() + 1 * handleSize, handleSize);
    currentOffset += handleSizeAligned;
    memcpy(static_cast<uint8_t*>(mappedData) + currentOffset, shaderGroupHandles.data() + 3 * handleSize, handleSize);
    currentOffset += handleSizeAligned;
    memcpy(static_cast<uint8_t*>(mappedData) + currentOffset, shaderGroupHandles.data() + 2 * handleSize, handleSize);
    currentOffset += handleSizeAligned;
    vkUnmapMemory(context_.device, targetMemory);

    if (platformConfig_.preferDeviceLocalMemory) {
        if (context_.commandBuffers.empty()) {
            LOG_ERROR_CAT("PipelineManager", "No command buffers available for SBT transfer");
            Dispose::destroySingleBuffer(context_.device, sbtBuffer);
            Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
            Dispose::destroySingleBuffer(context_.device, stagingBuffer);
            Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
            throw std::runtime_error("No command buffers available for SBT transfer");
        }

        VkCommandBuffer commandBuffer = context_.commandBuffers[0];
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            LOG_ERROR_CAT("PipelineManager", "Failed to begin SBT transfer command buffer");
            Dispose::destroySingleBuffer(context_.device, sbtBuffer);
            Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
            Dispose::destroySingleBuffer(context_.device, stagingBuffer);
            Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
            throw std::runtime_error("Failed to begin SBT transfer command buffer");
        }

        VkBufferCopy copyRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = sbtSize
        };
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, sbtBuffer, 1, &copyRegion);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            LOG_ERROR_CAT("PipelineManager", "Failed to end SBT transfer command buffer");
            Dispose::destroySingleBuffer(context_.device, sbtBuffer);
            Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
            Dispose::destroySingleBuffer(context_.device, stagingBuffer);
            Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
            throw std::runtime_error("Failed to end SBT transfer command buffer");
        }

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };
        if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            LOG_ERROR_CAT("PipelineManager", "Failed to submit SBT transfer command buffer");
            Dispose::destroySingleBuffer(context_.device, sbtBuffer);
            Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
            Dispose::destroySingleBuffer(context_.device, stagingBuffer);
            Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
            throw std::runtime_error("Failed to submit SBT transfer command buffer");
        }
        vkQueueWaitIdle(context_.graphicsQueue);
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);
    }

    VkBufferDeviceAddressInfo bufferAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = sbtBuffer
    };
    auto vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
        vkGetDeviceProcAddr(context_.device, "vkGetBufferDeviceAddress"));
    if (!vkGetBufferDeviceAddress) {
        LOG_ERROR_CAT("PipelineManager", "Failed to get vkGetBufferDeviceAddress function pointer");
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        throw std::runtime_error("Failed to get vkGetBufferDeviceAddress function pointer");
    }
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(context_.device, &bufferAddressInfo);

    sbt_.raygen = {sbtAddress, handleSizeAligned, handleSizeAligned};
    sbt_.miss = {sbtAddress + 1ULL * handleSizeAligned, 2ULL * handleSizeAligned, handleSizeAligned};
    sbt_.hit = {sbtAddress + 3ULL * handleSizeAligned, handleSizeAligned, handleSizeAligned};
    sbt_.callable = {0, 0, 0};

    LOG_INFO_CAT("PipelineManager", "Created shader binding table with size: {}, raygen={:x}, miss={:x} (size {}), hit={:x}, callable={:x}",
                 sbtSize, sbt_.raygen.deviceAddress, sbt_.miss.deviceAddress, sbt_.miss.size, sbt_.hit.deviceAddress, sbt_.callable.deviceAddress);
}

void VulkanPipelineManager::recordGraphicsCommands(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, VkImage denoiseImage) {
    if (commandBuffer == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE || descriptorSet == VK_NULL_HANDLE || denoiseImage == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PipelineManager", "Invalid handle in recordGraphicsCommands: commandBuffer={:p}, framebuffer={:p}, descriptorSet={:p}, denoiseImage={:p}",
                      static_cast<void*>(commandBuffer), static_cast<void*>(framebuffer), static_cast<void*>(descriptorSet), static_cast<void*>(denoiseImage));
        throw std::runtime_error("Invalid handle in recordGraphicsCommands");
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to begin command buffer recording");
        throw std::runtime_error("Failed to begin command buffer recording");
    }

    VkImageMemoryBarrier samplerBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = platformConfig_.computeQueueFamily,
        .dstQueueFamilyIndex = platformConfig_.graphicsQueueFamily,
        .image = denoiseImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    LOG_DEBUG_CAT("PipelineManager", "Barrier for denoiseImage {:p}: oldLayout=GENERAL, newLayout=SHADER_READ_ONLY_OPTIMAL",
                  static_cast<void*>(denoiseImage));
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &samplerBarrier);

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass_,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = {width, height}
        },
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };

    struct GraphicsPushConstants {
        glm::vec4 clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        float lightIntensity = 1.0f;
        float padding[3] = {0.0f, 0.0f, 0.0f};
    } pushConstants;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_->get());
    LOG_DEBUG_CAT("PipelineManager", "Binding graphics pipeline: {:p}, descriptorSet: {:p}",
                  static_cast<void*>(graphicsPipeline_->get()), static_cast<void*>(descriptorSet));
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout_->get(), 0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, graphicsPipelineLayout_->get(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(GraphicsPushConstants), &pushConstants);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to end command buffer recording");
        throw std::runtime_error("Failed to end command buffer recording");
    }
    LOG_INFO_CAT("PipelineManager", "Recorded graphics commands for framebuffer {:p}", static_cast<void*>(framebuffer));
}

void VulkanPipelineManager::recordComputeCommands(VkCommandBuffer commandBuffer, VkImage outputImage, VkDescriptorSet descriptorSet, uint32_t width, uint32_t height) {
    if (commandBuffer == VK_NULL_HANDLE || outputImage == VK_NULL_HANDLE || descriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PipelineManager", "Invalid handle in recordComputeCommands: commandBuffer={:p}, outputImage={:p}, descriptorSet={:p}",
                      static_cast<void*>(commandBuffer), static_cast<void*>(outputImage), static_cast<void*>(descriptorSet));
        throw std::runtime_error("Invalid handle in recordComputeCommands");
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to begin compute command buffer");
        throw std::runtime_error("Failed to begin compute command buffer");
    }

    VkImageMemoryBarrier imageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = platformConfig_.graphicsQueueFamily,
        .dstQueueFamilyIndex = platformConfig_.computeQueueFamily,
        .image = outputImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    LOG_DEBUG_CAT("PipelineManager", "Barrier for outputImage {:p}: oldLayout=GENERAL, newLayout=GENERAL",
                  static_cast<void*>(outputImage));
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_->get());
    LOG_DEBUG_CAT("PipelineManager", "Binding compute pipeline: {:p}, descriptorSet: {:p}",
                  static_cast<void*>(computePipeline_->get()), static_cast<void*>(descriptorSet));
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout_->get(), 0, 1, &descriptorSet, 0, nullptr);

    MaterialData::PushConstants pushConstants = {
        .clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        .cameraPosition = glm::vec3(0.0f, 0.0f, 0.0f),
        .lightDirection = glm::vec3(0.0f, 0.0f, -1.0f),
        .lightIntensity = 1.0f,
        .samplesPerPixel = 1u,
        .maxDepth = 2u,
        .maxBounces = 2u,
        .russianRoulette = 0.8f
    };
    vkCmdPushConstants(commandBuffer, computePipelineLayout_->get(),
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    vkCmdDispatch(commandBuffer, (width + 15) / 16, (height + 15) / 16, 1);

    imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.srcQueueFamilyIndex = platformConfig_.computeQueueFamily;
    imageBarrier.dstQueueFamilyIndex = platformConfig_.graphicsQueueFamily;
    LOG_DEBUG_CAT("PipelineManager", "Barrier for outputImage {:p}: oldLayout=GENERAL, newLayout=SHADER_READ_ONLY_OPTIMAL",
                  static_cast<void*>(outputImage));
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to end compute command buffer");
        throw std::runtime_error("Failed to end compute command buffer");
    }
    LOG_INFO_CAT("PipelineManager", "Recorded compute commands for outputImage {:p}", static_cast<void*>(outputImage));
}

} // namespace VulkanRTX