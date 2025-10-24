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
        LOG_ERROR_CAT("PipelineManager", "Shader type '{}' not found in registry", shaderType);
        throw std::runtime_error("Shader type not found: " + shaderType);
    }
    const std::string& filename = it->second;
    LOG_DEBUG_CAT("PipelineManager", "Loading shader: {} ({})", shaderType, filename);

    // Log current working directory for path resolution debugging
    std::filesystem::path cwd = std::filesystem::current_path();
    LOG_DEBUG_CAT("PipelineManager", "Current working directory: {}", cwd.string());

    // Convert relative path to absolute path for better diagnostics
    std::filesystem::path shaderPath = std::filesystem::absolute(filename);
    LOG_DEBUG_CAT("PipelineManager", "Resolved absolute shader path: {}", shaderPath.string());

    // Check if file exists
    if (!std::filesystem::exists(shaderPath)) {
        LOG_ERROR_CAT("PipelineManager", "Shader file does not exist: {}", shaderPath.string());
        throw std::runtime_error("Shader file does not exist: " + shaderPath.string());
    }

    // Check if path points to a regular file
    if (!std::filesystem::is_regular_file(shaderPath)) {
        LOG_ERROR_CAT("PipelineManager", "Shader path '{}' is not a regular file (e.g., is a directory)", shaderPath.string());
        throw std::runtime_error("Shader path is not a regular file: " + shaderPath.string());
    }

    // Check file permissions
    std::filesystem::file_status status = std::filesystem::status(shaderPath);
    auto perms = status.permissions();
    if ((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
        LOG_ERROR_CAT("PipelineManager", "Shader file '{}' lacks read permissions", shaderPath.string());
        throw std::runtime_error("Shader file lacks read permissions: " + shaderPath.string());
    }

    // Attempt to open the file
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::string errorMsg = std::strerror(errno);
        LOG_ERROR_CAT("PipelineManager", "Failed to open shader file '{}': {}", shaderPath.string(), errorMsg);
        throw std::runtime_error("Failed to open shader file: " + shaderPath.string() + ", reason: " + errorMsg);
    }

    // Get file size
    size_t fileSize = static_cast<size_t>(file.tellg());
    LOG_DEBUG_CAT("PipelineManager", "Shader file size: {} bytes", fileSize);

    // Check if file is empty
    if (fileSize == 0) {
        file.close();
        LOG_ERROR_CAT("PipelineManager", "Shader file '{}' is empty", shaderPath.string());
        throw std::runtime_error("Shader file is empty: " + shaderPath.string());
    }

    // Check if file size is a multiple of 4 (SPIR-V requirement)
    if (fileSize % 4 != 0) {
        file.close();
        LOG_ERROR_CAT("PipelineManager", "Invalid SPIR-V file size (not multiple of 4): {} bytes for '{}'", fileSize, shaderPath.string());
        throw std::runtime_error("Invalid SPIR-V file size: " + shaderPath.string());
    }

    // Basic SPIR-V validation: Check magic number
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    if (file.fail()) {
        std::string errorMsg = std::strerror(errno);
        file.close();
        LOG_ERROR_CAT("PipelineManager", "Failed to read shader file '{}': {}", shaderPath.string(), errorMsg);
        throw std::runtime_error("Failed to read shader file: " + shaderPath.string() + ", reason: " + errorMsg);
    }
    file.close();

    // Check SPIR-V magic number (0x07230203)
    if (fileSize >= 4) {
        uint32_t magic = *reinterpret_cast<const uint32_t*>(buffer.data());
        if (magic != 0x07230203) {
            LOG_ERROR_CAT("PipelineManager", "Invalid SPIR-V magic number (expected 0x07230203, got 0x{:08x}) for '{}'", magic, shaderPath.string());
            throw std::runtime_error("Invalid SPIR-V magic number for: " + shaderPath.string());
        }
    }

    // Create shader module
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
        LOG_ERROR_CAT("PipelineManager", "Failed to create shader module for '{}': VkResult={}", shaderPath.string(), static_cast<int>(result));
        throw std::runtime_error("Failed to create shader module for: " + shaderPath.string() + ", VkResult=" + std::to_string(static_cast<int>(result)));
    }

    LOG_DEBUG_CAT("PipelineManager", "Successfully loaded shader module: {:p} from '{}'", static_cast<void*>(shaderModule), shaderPath.string());
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

    // Skipped createGraphicsPipeline as we are using ray tracing and do not require rasterization fallback
    // createGraphicsPipeline(width, height);
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
        LOG_ERROR_CAT("PipelineManager", "Failed to create ray-tracing descriptor set layout");
        throw std::runtime_error("Failed to create ray-tracing descriptor set layout");
    }
    context_.resourceManager.addDescriptorSetLayout(context_.rayTracingDescriptorSetLayout);
    LOG_INFO_CAT("PipelineManager", "Created ray-tracing descriptor set layout with {} bindings: {:p}",
                 bindings.size(), static_cast<void*>(context_.rayTracingDescriptorSetLayout));
}

void VulkanPipelineManager::createGraphicsDescriptorSetLayout() {
    LOG_DEBUG_CAT("PipelineManager", "Creating graphics descriptor set layout");
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
    std::vector<std::string> shaderTypes = {"raygen", "miss", "closesthit", "shadowmiss", "anyhit", "intersection", "callable", "shadow_anyhit", "mid_anyhit", "volumetric_anyhit", "compute"};
    std::vector<VkShaderModule> shaderModules;
    for (const auto& type : shaderTypes) {
        shaderModules.push_back(loadShader(context_.device, type));
    }

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = shaderModules[0],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[1],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = shaderModules[2],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[3],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[4],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
            .module = shaderModules[5],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
            .module = shaderModules[6],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[7],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[8],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[9],
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups = {
        // Raygen (group 0)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Primary miss (group 1)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Primary hit: triangles (group 2)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 4,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Shadow miss (group 3)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 3,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Shadow hit: triangles (group 4)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = 7,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Mid-layer hit: triangles (group 5)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 8,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Volumetric hit: procedural (group 6)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = 9,
            .intersectionShader = 5,
            .pShaderGroupCaptureReplayHandle = nullptr
        },
        // Callable (group 7)
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 6,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        }
    };

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
                      VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | 
                      VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
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
    VkPhysicalDeviceProperties2 properties2 = {};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties2);

    uint32_t recursionDepth = (rtProperties.maxRayRecursionDepth > 0) ? std::min(4u, rtProperties.maxRayRecursionDepth) : 1;

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
    std::vector<std::string> computeShaders = {"compute", "raster_prepass", "denoiser_post"};
    std::vector<VkPipeline> computePipelines;

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
        throw std::runtime_error("Failed to create compute pipeline layout");
    }
    computePipelineLayout_ = std::make_unique<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>>(
        context_.device, pipelineLayout, vkDestroyPipelineLayout);
    context_.resourceManager.addPipelineLayout(pipelineLayout);
    LOG_INFO_CAT("PipelineManager", "Created compute pipeline layout: {:p}", static_cast<void*>(pipelineLayout));

    for (const auto& shaderType : computeShaders) {
        auto computeShaderModule = loadShader(context_.device, shaderType);
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
            LOG_ERROR_CAT("PipelineManager", "Failed to create compute pipeline for {}", shaderType);
            vkDestroyShaderModule(context_.device, computeShaderModule, nullptr);
            vkDestroyPipelineLayout(context_.device, pipelineLayout, nullptr);
            throw std::runtime_error("Failed to create compute pipeline for " + shaderType);
        }
        computePipelines.push_back(pipeline);
        context_.resourceManager.addPipeline(pipeline);
        if (shaderType == "raster_prepass") {
            rasterPrepassPipeline_ = pipeline;
        } else if (shaderType == "denoiser_post") {
            denoiserPostPipeline_ = pipeline;
        }
        LOG_INFO_CAT("PipelineManager", "Created compute pipeline for {}: {:p}", shaderType, static_cast<void*>(pipeline));
        vkDestroyShaderModule(context_.device, computeShaderModule, nullptr);
    }

    computePipeline_ = std::make_unique<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>>(
        context_.device, computePipelines[0], vkDestroyPipeline); // Primary compute pipeline (xorshift)
}

void VulkanPipelineManager::createShaderBindingTable() {
    LOG_DEBUG_CAT("PipelineManager", "Creating shader binding table");
    if (!rayTracingPipeline_) {
        LOG_ERROR_CAT("PipelineManager", "Ray-tracing pipeline not initialized");
        throw std::runtime_error("Ray-tracing pipeline not initialized");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties);

    const uint32_t groupCount = 8; // From createRayTracingPipeline: raygen, miss, closesthit, shadowmiss, shadow_anyhit, mid_anyhit, volumetric_anyhit, callable
    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
    const uint32_t alignedHandleSize = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const uint32_t sbtSize = groupCount * alignedHandleSize;

    std::vector<uint8_t> shaderGroupHandles(sbtSize);
    auto vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetRayTracingShaderGroupHandlesKHR"));
    if (!vkGetRayTracingShaderGroupHandlesKHR) {
        LOG_ERROR_CAT("PipelineManager", "Failed to get vkGetRayTracingShaderGroupHandlesKHR function pointer");
        throw std::runtime_error("Failed to get vkGetRayTracingShaderGroupHandlesKHR function pointer");
    }

    VkResult result = vkGetRayTracingShaderGroupHandlesKHR(context_.device, rayTracingPipeline_->get(), 0, groupCount, sbtSize, shaderGroupHandles.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to get shader group handles: VkResult={}", static_cast<int>(result));
        throw std::runtime_error("Failed to get shader group handles");
    }

    VkBufferUsageFlags sbtUsage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags sbtMemoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sbtSize, sbtUsage, sbtMemoryProps, sbtBuffer, sbtMemory, nullptr, context_.resourceManager);

    // Create staging buffer for SBT initialization
    VkBufferCreateInfo stagingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sbtSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(context_.device, &stagingCreateInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create staging buffer for SBT");
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to create staging buffer for SBT");
    }
    context_.resourceManager.addBuffer(stagingBuffer);  // Add to resource manager for cleanup

    VkMemoryRequirements stagingMemReq{};
    vkGetBufferMemoryRequirements(context_.device, stagingBuffer, &stagingMemReq);
    uint32_t stagingMemType = VulkanInitializer::findMemoryType(context_.physicalDevice, stagingMemReq.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo stagingAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = stagingMemReq.size,
        .memoryTypeIndex = stagingMemType
    };
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (vkAllocateMemory(context_.device, &stagingAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to allocate staging memory for SBT");
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to allocate staging memory for SBT");
    }
    context_.resourceManager.addMemory(stagingMemory);  // Add to resource manager for cleanup
    if (vkBindBufferMemory(context_.device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to bind staging memory for SBT");
        // Cleanup
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        vkFreeMemory(context_.device, stagingMemory, nullptr);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to bind staging memory for SBT");
    }

    // Map staging and copy data with proper alignment
    void* mappedData = nullptr;
    if (vkMapMemory(context_.device, stagingMemory, 0, sbtSize, 0, &mappedData) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to map staging memory for SBT");
        // Cleanup
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        vkFreeMemory(context_.device, stagingMemory, nullptr);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to map staging memory for SBT");
    }
    uint8_t* data = static_cast<uint8_t*>(mappedData);
    memset(data, 0, sbtSize);

    // Raygen (group 0) at offset 0
    memcpy(data + 0 * alignedHandleSize, shaderGroupHandles.data() + 0 * handleSize, handleSize);

    // Miss: primary (group 1) at 1a, shadow (group 3) at 2a
    memcpy(data + 1 * alignedHandleSize, shaderGroupHandles.data() + 1 * handleSize, handleSize);
    memcpy(data + 2 * alignedHandleSize, shaderGroupHandles.data() + 3 * handleSize, handleSize);

    // Hit: primary (group 2) at 3a + 0a, shadow (group 4) at 3a + 1a, mid (group 5) at 3a + 2a, vol (group 6) at 3a + 3a
    memcpy(data + 3 * alignedHandleSize + 0 * alignedHandleSize, shaderGroupHandles.data() + 2 * handleSize, handleSize);
    memcpy(data + 3 * alignedHandleSize + 1 * alignedHandleSize, shaderGroupHandles.data() + 4 * handleSize, handleSize);
    memcpy(data + 3 * alignedHandleSize + 2 * alignedHandleSize, shaderGroupHandles.data() + 5 * handleSize, handleSize);
    memcpy(data + 3 * alignedHandleSize + 3 * alignedHandleSize, shaderGroupHandles.data() + 6 * handleSize, handleSize);

    // Callable (group 7) at 7a
    memcpy(data + 7 * alignedHandleSize, shaderGroupHandles.data() + 7 * handleSize, handleSize);

    vkUnmapMemory(context_.device, stagingMemory);

    // Copy from staging to SBT buffer using command buffer
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, &cmd) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to allocate command buffer for SBT copy");
        // Cleanup
        vkUnmapMemory(context_.device, stagingMemory);  // Already unmapped
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        vkFreeMemory(context_.device, stagingMemory, nullptr);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to allocate command buffer for SBT copy");
    }

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to begin command buffer for SBT copy");
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup staging and sbt...
        Dispose::destroySingleBuffer(context_.device, stagingBuffer);
        vkFreeMemory(context_.device, stagingMemory, nullptr);
        Dispose::destroySingleBuffer(context_.device, sbtBuffer);
        Dispose::freeSingleDeviceMemory(context_.device, sbtMemory);
        context_.resourceManager.removeBuffer(stagingBuffer);
        context_.resourceManager.removeMemory(stagingMemory);
        context_.resourceManager.removeBuffer(sbtBuffer);
        context_.resourceManager.removeMemory(sbtMemory);
        throw std::runtime_error("Failed to begin command buffer for SBT copy");
    }

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = sbtSize
    };
    vkCmdCopyBuffer(cmd, stagingBuffer, sbtBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to end command buffer for SBT copy");
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup...
        throw std::runtime_error("Failed to end command buffer for SBT copy");
    }

    // Submit
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(context_.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to create fence for SBT copy");
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup...
        throw std::runtime_error("Failed to create fence for SBT copy");
    }

    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to submit command buffer for SBT copy");
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup...
        throw std::runtime_error("Failed to submit command buffer for SBT copy");
    }

    if (vkWaitForFences(context_.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to wait for fence in SBT copy");
        vkDestroyFence(context_.device, fence, nullptr);
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);
        // Cleanup...
        throw std::runtime_error("Failed to wait for fence in SBT copy");
    }

    vkDestroyFence(context_.device, fence, nullptr);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &cmd);

    // Cleanup staging
    context_.resourceManager.removeBuffer(stagingBuffer);
    context_.resourceManager.removeMemory(stagingMemory);
    Dispose::destroySingleBuffer(context_.device, stagingBuffer);
    Dispose::freeSingleDeviceMemory(context_.device, stagingMemory);

    VkDeviceAddress sbtAddress = VulkanInitializer::getBufferDeviceAddress(context_.device, sbtBuffer);

    sbt_ = ShaderBindingTable(context_.device, sbtBuffer, sbtMemory, vkDestroyBuffer, vkFreeMemory);
    sbt_.raygen = {sbtAddress + 0 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
    sbt_.miss = {sbtAddress + 1 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
    sbt_.hit = {sbtAddress + 3 * alignedHandleSize, alignedHandleSize, alignedHandleSize};
    sbt_.callable = {sbtAddress + 7 * alignedHandleSize, alignedHandleSize, alignedHandleSize};

    LOG_INFO_CAT("PipelineManager", "Created shader binding table: buffer={:p}, memory={:p}, size={}", 
                 static_cast<void*>(sbtBuffer), static_cast<void*>(sbtMemory), sbtSize);
}

void VulkanPipelineManager::recordGraphicsCommands(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, 
                                                  VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, 
                                                  VkImage denoiseImage) {
    (void)denoiseImage;
    LOG_DEBUG_CAT("PipelineManager", "Recording graphics commands for framebuffer {:p}", static_cast<void*>(framebuffer));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to begin command buffer {:p}", static_cast<void*>(commandBuffer));
        throw std::runtime_error("Failed to begin command buffer");
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
        LOG_ERROR_CAT("PipelineManager", "Failed to end command buffer {:p}", static_cast<void*>(commandBuffer));
        throw std::runtime_error("Failed to end command buffer");
    }
    LOG_DEBUG_CAT("PipelineManager", "Graphics commands recorded for command buffer {:p}", static_cast<void*>(commandBuffer));
}

void VulkanPipelineManager::recordComputeCommands(VkCommandBuffer commandBuffer, VkImage outputImage, 
                                                 VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, 
                                                 VkImage gDepth, VkImage gNormal, VkImage historyImage) {
    (void)historyImage;
    LOG_DEBUG_CAT("PipelineManager", "Recording compute commands");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to begin compute command buffer {:p}", static_cast<void*>(commandBuffer));
        throw std::runtime_error("Failed to begin compute command buffer");
    }

    VkImageMemoryBarrier imageBarriers[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = outputImage,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = gDepth,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = gNormal,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                         0, 0, nullptr, 0, nullptr, 3, imageBarriers);

    if (rasterPrepassPipeline_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PipelineManager", "Raster prepass pipeline not found");
        throw std::runtime_error("Raster prepass pipeline not found");
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, rasterPrepassPipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, getComputePipelineLayout(), 
                            0, 1, &descriptorSet, 0, nullptr);

    uint32_t groupCountX = (width + 15) / 16;
    uint32_t groupCountY = (height + 15) / 16;
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    VkImageMemoryBarrier postDispatchBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &postDispatchBarrier);

    if (denoiserPostPipeline_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("PipelineManager", "Denoiser post pipeline not found");
        throw std::runtime_error("Denoiser post pipeline not found");
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiserPostPipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, getComputePipelineLayout(), 
                            0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

    VkImageMemoryBarrier finalBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to end compute command buffer {:p}", static_cast<void*>(commandBuffer));
        throw std::runtime_error("Failed to end compute command buffer");
    }
    LOG_DEBUG_CAT("PipelineManager", "Compute commands recorded for command buffer {:p}", static_cast<void*>(commandBuffer));
}

void VulkanPipelineManager::recordRayTracingCommands(VkCommandBuffer commandBuffer, VkImage outputImage, 
                                                    VkDescriptorSet descriptorSet, uint32_t width, uint32_t height, 
                                                    VkImage gDepth, VkImage gNormal) {
    LOG_DEBUG_CAT("PipelineManager", "Recording ray-tracing commands");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to begin ray-tracing command buffer {:p}", static_cast<void*>(commandBuffer));
        throw std::runtime_error("Failed to begin ray-tracing command buffer");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &properties);

    VkImageMemoryBarrier imageBarriers[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = outputImage,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = gDepth,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = gNormal,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
                         0, 0, nullptr, 0, nullptr, 3, imageBarriers);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, getRayTracingPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, getRayTracingPipelineLayout(), 
                            0, 1, &descriptorSet, 0, nullptr);

    MaterialData::PushConstants pushConstants{};
    pushConstants.resolution = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    vkCmdPushConstants(commandBuffer, getRayTracingPipelineLayout(), 
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
                       VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | 
                       VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR, 
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCmdTraceRaysKHR"));
    if (!vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("PipelineManager", "Failed to get vkCmdTraceRaysKHR function pointer");
        throw std::runtime_error("Failed to get vkCmdTraceRaysKHR function pointer");
    }

    VkStridedDeviceAddressRegionKHR raygenSbt = sbt_.raygen;
    VkStridedDeviceAddressRegionKHR missSbt = sbt_.miss;
    VkStridedDeviceAddressRegionKHR hitSbt = sbt_.hit;
    VkStridedDeviceAddressRegionKHR callableSbt = sbt_.callable;

    vkCmdTraceRaysKHR(commandBuffer, &raygenSbt, &missSbt, &hitSbt, &callableSbt, width, height, 1);

    VkImageMemoryBarrier finalBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                         0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("PipelineManager", "Failed to end ray-tracing command buffer {:p}", static_cast<void*>(commandBuffer));
        throw std::runtime_error("Failed to end ray-tracing command buffer");
    }
    LOG_DEBUG_CAT("PipelineManager", "Ray-tracing commands recorded for command buffer {:p}", static_cast<void*>(commandBuffer));
}

} // namespace VulkanRTX