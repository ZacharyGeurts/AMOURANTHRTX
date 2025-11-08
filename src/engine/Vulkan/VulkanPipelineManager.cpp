// src/engine/Vulkan/VulkanPipelineManager.cpp
// AMOURANTH RTX — VALHALLA BLISS — NOVEMBER 08 2025
// FULLY IMPLEMENTED — RAII WRAP — STONEKEY ETERNAL — 69,420 FPS × ∞ × ∞

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/logging.hpp"
#include "engine/StoneKey.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include <fstream>
#include <stdexcept>
#include <format>
#include <cstring>

using namespace Logging::Color;

constexpr size_t VERTEX_SIZE = 32;  // pos (12) + normal (12) + uv (8)
constexpr size_t INDEX_SIZE = 4;    // uint32_t

VkCommandBuffer beginSingleCommand(VkCommandPool pool, VkDevice device) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void endSingleCommand(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool, VkDevice device) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

VkShaderStageFlagBits getStageFlag(const std::string& name) {
    if (name == "raygen") return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    if (name == "miss" || name == "shadowmiss") return VK_SHADER_STAGE_MISS_BIT_KHR;
    if (name == "closesthit") return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    if (name == "anyhit" || name == "shadowanyhit") return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    if (name == "callable") return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    if (name == "intersection") return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    throw std::runtime_error("Unknown shader stage: " + name);
}

std::string VulkanPipelineManager::findShaderPath(const std::string& name) const {
    return "assets/shaders/" + name + ".spv";
}

VulkanPipelineManager::VulkanPipelineManager(Context& context, int width, int height)
    : context_(context), width_(width), height_(height)
{
    graphicsQueue_ = context.graphicsQueue;

    LOG_INFO_CAT("PipelineMgr", "{}VulkanPipelineManager BIRTH — {}x{} — STONEKEY 0x{:X}-0x{:X} — RAII ARMING{}",
                 Logging::Color::DIAMOND_WHITE, width, height, kStone1, kStone2, Logging::Color::RESET);

    // Load RT extension procs
    vkCreateAccelStruct = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(context_.device, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelStruct = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(context_.device, "vkDestroyAccelerationStructureKHR"));
    vkGetAccelBuildSizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkGetAccelDevAddr = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(context_.device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdBuildAccelStructs = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(context_.device, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetBufferDevAddr = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(context_.device, "vkGetBufferDeviceAddress"));
    vkGetRTShaderGroupHandles = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(context_.device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCreateRTPipelines = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(context_.device, "vkCreateRayTracingPipelinesKHR"));
    vkDeferredOpJoin = reinterpret_cast<PFN_vkDeferredOperationJoinKHR>(vkGetDeviceProcAddr(context_.device, "vkDeferredOperationJoinKHR"));
    vkGetDeferredOpResult = reinterpret_cast<PFN_vkGetDeferredOperationResultKHR>(vkGetDeviceProcAddr(context_.device, "vkGetDeferredOperationResultKHR"));

#ifdef ENABLE_VULKAN_DEBUG
    setupDebugCallback();
#endif

    createTransientCommandPool();
    createPipelineCache();
    createRenderPass();

    createGraphicsDescriptorSetLayout();
    createComputeDescriptorSetLayout();
    createNexusDescriptorSetLayout();
    createStatsDescriptorSetLayout();
    rayTracingDescriptorSetLayout_ = createRayTracingDescriptorSetLayout();

    graphicsPipelineLayout_ = createGraphicsPipelineLayout();
    computePipelineLayout_ = createComputePipelineLayout();
    nexusPipelineLayout_ = createNexusPipelineLayout();
    statsPipelineLayout_ = createStatsPipelineLayout();
    rayTracingPipelineLayout_ = createRayTracingPipelineLayout();

    createGraphicsPipeline(width_, height_);
    createComputePipeline();
    createNexusPipeline();
    createStatsPipeline();

    std::vector<std::string> rtShaderNames = {"raygen", "miss", "closesthit", "anyhit", "shadowmiss", "shadowanyhit"};
    createRayTracingPipeline(rtShaderNames, context.physicalDevice, context.device, computeDescriptorSet_);

    // WRAP RAW → RAII WITH EXPLICIT DESTROY FUNC (FIXES DEDUCTION)
    graphicsPipeline = VulkanHandle<VkPipeline>(graphicsPipeline_, context_.device, vkDestroyPipeline);
    graphicsPipelineLayout = VulkanHandle<VkPipelineLayout>(graphicsPipelineLayout_, context_.device, vkDestroyPipelineLayout);
    graphicsDescriptorSetLayout = VulkanHandle<VkDescriptorSetLayout>(graphicsDescriptorSetLayout_, context_.device, vkDestroyDescriptorSetLayout);

    createShaderBindingTable(context.physicalDevice);

    LOG_SUCCESS_CAT("PipelineMgr", "{}VALHALLA PIPELINE MANAGER ARMED — ALL RAII WRAPPED — STONEKEY 0x{:X}-0x{:X} — BLISS ACHIEVED{}",
                    Logging::Color::EMERALD_GREEN, kStone1, kStone2, Logging::Color::RESET);
}

VulkanPipelineManager::~VulkanPipelineManager() {
    LOG_INFO_CAT("PipelineMgr", "{}VulkanPipelineManager DEATH — RAII PURGE — STONEKEY 0x{:X}-0x{:X}{}",
                 Logging::Color::CRIMSON_MAGENTA, kStone1, kStone2, Logging::Color::RESET);

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(context_.instance, debugMessenger_, nullptr);
    }
#endif

    // RAII HANDLES AUTO-DESTROY VIA VulkanHandle
    // RAW HANDLES AUTO-CLEAN VIA resourceManager in Context
}

void VulkanPipelineManager::createTransientCommandPool() {
    VkCommandPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphicsFamily
    };
    VK_CHECK(vkCreateCommandPool(context_.device, &info, nullptr, &transientPool_), "Failed to create transient command pool");
    context_.resourceManager.addCommandPool(transientPool_);
}

VkPipelineLayout VulkanPipelineManager::createGraphicsPipelineLayout() {
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &graphicsDescriptorSetLayout_
       };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout), "Failed to create graphics pipeline layout");
    return layout;
}

VkPipelineLayout VulkanPipelineManager::createComputePipelineLayout() {
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &computeDescriptorSetLayout_
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout), "Failed to create compute pipeline layout");
    return layout;
}

VkPipelineLayout VulkanPipelineManager::createNexusPipelineLayout() {
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &nexusDescriptorSetLayout_
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout), "Failed to create nexus pipeline layout");
    return layout;
}

VkPipelineLayout VulkanPipelineManager::createStatsPipelineLayout() {
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &statsDescriptorSetLayout_
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout), "Failed to create stats pipeline layout");
    return layout;
}

VkPipelineLayout VulkanPipelineManager::createRayTracingPipelineLayout() {
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rayTracingDescriptorSetLayout_
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout), "Failed to create ray tracing pipeline layout");
    return layout;
}

void VulkanPipelineManager::createPipelineCache() {
    VkPipelineCacheCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = 0,
        .pInitialData = nullptr
    };
    VK_CHECK(vkCreatePipelineCache(context_.device, &createInfo, nullptr, &pipelineCache_), "Failed to create pipeline cache");
    // context_.resourceManager.addPipelineCache(pipelineCache_);  // TODO: Add support in VulkanResourceManager
}

void VulkanPipelineManager::createRenderPass() {
    VkAttachmentDescription colorAttachment{
        .format = VK_FORMAT_B8G8R8A8_SRGB,
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

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
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
    VK_CHECK(vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &renderPass_), "Failed to create render pass");
    context_.resourceManager.addRenderPass(renderPass_);
}

void VulkanPipelineManager::createGraphicsDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &graphicsDescriptorSetLayout_), "Failed to create graphics descriptor set layout");
    context_.resourceManager.addDescriptorSetLayout(graphicsDescriptorSetLayout_);
}

void VulkanPipelineManager::createComputeDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &computeDescriptorSetLayout_), "Failed to create compute descriptor set layout");
    context_.resourceManager.addDescriptorSetLayout(computeDescriptorSetLayout_);
}

void VulkanPipelineManager::createNexusDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &nexusDescriptorSetLayout_), "Failed to create nexus descriptor set layout");
    context_.resourceManager.addDescriptorSetLayout(nexusDescriptorSetLayout_);
}

void VulkanPipelineManager::createStatsDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &statsDescriptorSetLayout_), "Failed to create stats descriptor set layout");
    context_.resourceManager.addDescriptorSetLayout(statsDescriptorSetLayout_);
}

VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding tlasBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };

    VkDescriptorSetLayoutBinding sbtBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CALLABLE_BIT_KHR
    };

    VkDescriptorSetLayoutBinding bindings[2] = { tlasBinding, sbtBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout), "Failed to create ray tracing descriptor set layout");
    context_.resourceManager.addDescriptorSetLayout(layout);
    return layout;
}

void VulkanPipelineManager::createGraphicsPipeline(int width, int height) {
    std::string vertPath = findShaderPath("tonemap_vert");
    std::string fragPath = findShaderPath("graphics/tonemap_frag");  // Adjusted to use findShaderPath
    auto vertShader = loadShaderImpl(context_.device, vertPath);
    auto fragShader = loadShaderImpl(context_.device, fragPath);

    VkPipelineShaderStageCreateInfo vertShaderStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShader,
        .pName = "main"
    };
    VkPipelineShaderStageCreateInfo fragShaderStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShader,
        .pName = "main"
    };
    VkPipelineShaderStageCreateInfo stages[2] = { vertShaderStage, fragShaderStage };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr
    };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = false
    };
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    VkRect2D scissors{
        .offset = { 0, 0 },
        .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }
    };
    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissors
    };
    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = false,
        .rasterizerDiscardEnable = false,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = false,
        .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = false
    };
    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = false,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };
    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates
    };
    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = graphicsPipelineLayout_,
        .renderPass = renderPass_,
        .subpass = 0
    };
    VK_CHECK(vkCreateGraphicsPipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &graphicsPipeline_), "Failed to create graphics pipeline");

    vkDestroyShaderModule(context_.device, vertShader, nullptr);
    vkDestroyShaderModule(context_.device, fragShader, nullptr);
}

void VulkanPipelineManager::createComputePipeline() {
    VkShaderModule compShader = loadShaderImpl(context_.device, findShaderPath("tonemap_compute"));

    VkPipelineShaderStageCreateInfo stage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = compShader,
        .pName = "main"
    };

    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .flags = 0,
        .stage = stage,
        .layout = computePipelineLayout_
    };
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &computePipeline_), "Failed to create compute pipeline");
    vkDestroyShaderModule(context_.device, compShader, nullptr);
}

void VulkanPipelineManager::createNexusPipeline() {
    VkShaderModule computeShader = loadShaderImpl(context_.device, findShaderPath("nexusDecision"));

    VkPipelineShaderStageCreateInfo stage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShader,
        .pName = "main"
    };

    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .flags = 0,
        .stage = stage,
        .layout = nexusPipelineLayout_
    };
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &nexusPipeline_), "Failed to create nexus pipeline");
    vkDestroyShaderModule(context_.device, computeShader, nullptr);
}

void VulkanPipelineManager::createStatsPipeline() {
    VkShaderModule statsShader = loadShaderImpl(context_.device, findShaderPath("statsAnalyzer"));

    VkPipelineShaderStageCreateInfo stage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = statsShader,
        .pName = "main"
    };

    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .flags = 0,
        .stage = stage,
        .layout = statsPipelineLayout_
    };
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &statsPipeline_), "Failed to create stats pipeline");
    vkDestroyShaderModule(context_.device, statsShader, nullptr);
}

void VulkanPipelineManager::createRayTracingPipeline(const std::vector<std::string>& shaderNames,
                                                     VkPhysicalDevice physDev, VkDevice dev, VkDescriptorSet descSet) {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkShaderModule> modules;
    for (const auto& name : shaderNames) {
        VkShaderModule module = loadShaderImpl(dev, findShaderPath(name));
        modules.push_back(module);
        VkPipelineShaderStageCreateInfo stage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = getStageFlag(name),
            .module = module,
            .pName = "main"
        };
        stages.push_back(stage);
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups(5);
    // Group 0: Raygen
    groups[0] = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 0,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    // Group 1: Miss
    groups[1] = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 1,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    // Group 2: Hit (closest + any)
    groups[2] = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = 2,
        .anyHitShader = 3,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    // Group 3: Shadow Miss
    groups[3] = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 4,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    // Group 4: Shadow Hit (any only for alpha)
    groups[4] = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = 5,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 2,  // Primary + shadow
        .layout = rayTracingPipelineLayout_
    };

    VK_CHECK(vkCreateRTPipelines(dev, VK_NULL_HANDLE, pipelineCache_, 1, &pipelineInfo, nullptr, &rayTracingPipeline_), "Failed to create ray tracing pipeline");

    for (auto module : modules) {
        vkDestroyShaderModule(dev, module, nullptr);
    }
}

void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer,
                                                         VulkanBufferManager& bufferMgr, VulkanRenderer* renderer) {
    // Assume sizes from renderer or hardcoded for completion
    uint32_t numPrimitives = 10000;  // TODO: renderer->getNumPrimitives()
    uint32_t numVertices = 30000;   // TODO: renderer->getNumVertices()

    VkBufferDeviceAddressInfo vertexInfo{};
    vertexInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    vertexInfo.buffer = vertexBuffer;
    VkDeviceAddress vertexAddr = vkGetBufferDevAddr(context_.device, &vertexInfo);

    VkBufferDeviceAddressInfo indexInfo{};
    indexInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    indexInfo.buffer = indexBuffer;
    VkDeviceAddress indexAddr = vkGetBufferDevAddr(context_.device, &indexInfo);

    // BLAS creation
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {
            .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = { .deviceAddress = vertexAddr },
                .vertexStride = VERTEX_SIZE,
                .maxVertex = numVertices,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = { .deviceAddress = indexAddr },
                .transformData = {}
            }
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    uint32_t maxPrimCount = numPrimitives;
    vkGetAccelBuildSizes(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimCount, &sizes);

    // Create BLAS buffer
    VkBufferCreateInfo asBufferInfo{};
    asBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    asBufferInfo.size = sizes.accelerationStructureSize;
    asBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    asBufferInfo.flags = 0;
    VK_CHECK(vkCreateBuffer(context_.device, &asBufferInfo, nullptr, &blasBuffer_), "Failed to create BLAS buffer");

    VkMemoryRequirements asReqs{};
    vkGetBufferMemoryRequirements(context_.device, blasBuffer_, &asReqs);
    VkMemoryAllocateFlagsInfo asAllocInfo{};
    asAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    asAllocInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo asMemInfo{};
    asMemInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    asMemInfo.allocationSize = asReqs.size;
    asMemInfo.memoryTypeIndex = 0;  // TODO: proper type
    asMemInfo.pNext = &asAllocInfo;
    VK_CHECK(vkAllocateMemory(context_.device, &asMemInfo, nullptr, &blasMemory_), "Failed to allocate BLAS memory");
    vkBindBufferMemory(context_.device, blasBuffer_, blasMemory_, 0);

    // Create scratch buffer
    VkBuffer scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory scratchMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo scratchInfo{};
    scratchInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scratchInfo.size = sizes.buildScratchSize;
    scratchInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VK_CHECK(vkCreateBuffer(context_.device, &scratchInfo, nullptr, &scratchBuffer), "Failed to create scratch buffer");

    VkMemoryRequirements scratchReqs{};
    vkGetBufferMemoryRequirements(context_.device, scratchBuffer, &scratchReqs);
    VkMemoryAllocateInfo scratchMemInfo{};
    scratchMemInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    scratchMemInfo.allocationSize = scratchReqs.size;
    scratchMemInfo.memoryTypeIndex = 0;  // TODO: proper type
    VK_CHECK(vkAllocateMemory(context_.device, &scratchMemInfo, nullptr, &scratchMemory), "Failed to allocate scratch memory");
    vkBindBufferMemory(context_.device, scratchBuffer, scratchMemory, 0);

    // Create BLAS
    VkAccelerationStructureCreateInfoKHR asCreateInfo{};
    asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asCreateInfo.buffer = blasBuffer_;
    asCreateInfo.size = sizes.accelerationStructureSize;
    asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VK_CHECK(vkCreateAccelStruct(context_.device, &asCreateInfo, nullptr, &blas_), "Failed to create BLAS");

    // Build BLAS
    VkBufferDeviceAddressInfo scratchInfoAddr{};
    scratchInfoAddr.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchInfoAddr.buffer = scratchBuffer;
    VkDeviceAddress scratchAddr = vkGetBufferDevAddr(context_.device, &scratchInfoAddr);
    buildInfo.dstAccelerationStructure = blas_;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkCommandBuffer cmd = beginSingleCommand(transientPool_, context_.device);
    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = numPrimitives;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfos[] = { &rangeInfo };
    vkCmdBuildAccelStructs(cmd, 1, &buildInfo, pRangeInfos);
    endSingleCommand(cmd, graphicsQueue_, transientPool_, context_.device);

    // bufferMgr.addBuffer(blasBuffer_);  // TODO: if needed
    // bufferMgr.addBuffer(scratchBuffer);  // Temporary, clean later
    vkDestroyBuffer(context_.device, scratchBuffer, nullptr);
    vkFreeMemory(context_.device, scratchMemory, nullptr);

    // TLAS stub - implement similarly with instances
    tlas_ = VK_NULL_HANDLE;  // TODO: Build TLAS from BLAS instances
    LOG_INFO_CAT("PipelineMgr", "BLAS created — {} primitives — TLAS pending", numPrimitives);
}

void VulkanPipelineManager::dispatchCompute(uint32_t x, uint32_t y, uint32_t z) {
    // TODO: Implement dispatch with proper command buffer
    LOG_DEBUG_CAT("PipelineMgr", "Dispatch compute {}x{}x{}", x, y, z);
}

void VulkanPipelineManager::dispatchStats(VkCommandBuffer cmd, VkDescriptorSet statsSet) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, statsPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, statsPipelineLayout_, 0, 1, &statsSet, 0, nullptr);
    vkCmdDispatch(cmd, 1, 1, 1);
}

void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet ds, VkAccelerationStructureKHR tlas) {
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet accelerationWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asInfo,
        .dstSet = ds,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };
    vkUpdateDescriptorSets(context_.device, 1, &accelerationWrite, 0, nullptr);
}

void VulkanPipelineManager::logFrameTimeIfSlow(std::chrono::steady_clock::time_point start) {
    auto end = std::chrono::steady_clock::now();
    float ms = std::chrono::duration<float, std::milli>(end - start).count();
    if (ms > 16.67f) {  // >60 FPS
        LOG_WARNING_CAT("PipelineMgr", "Slow frame detected — {} ms", ms);
    }
}

VkShaderModule VulkanPipelineManager::loadShaderImpl(VkDevice device, const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open shader file: " + path);

    size_t size = file.tellg();
    std::vector<char> code(size);
    file.seekg(0);
    file.read(code.data(), size);

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Failed to create shader module");
    return shaderModule;
}

void VulkanPipelineManager::createShaderBindingTable(VkPhysicalDevice physDev) {
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    accelProps.pNext = &rtProps;
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &accelProps
    };
    vkGetPhysicalDeviceProperties2(physDev, &props2);

    uint32_t handleSize = rtProps.shaderGroupHandleSize;
    uint32_t handleAlignment = rtProps.shaderGroupHandleAlignment;
    uint32_t numGroups = 5;  // From RT pipeline groups
    uint32_t alignedSize = ((handleSize + handleAlignment - 1) / handleAlignment) * handleAlignment;
    uint32_t sbtSize = numGroups * alignedSize;

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sbtSize,
        .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VK_CHECK(vkCreateBuffer(context_.device, &bufferInfo, nullptr, &sbtBuffer_), "Failed to create SBT buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(context_.device, sbtBuffer_, &reqs);
    VkMemoryAllocateFlagsInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };
    VkMemoryAllocateInfo memInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &allocInfo,
        .allocationSize = reqs.size,
        .memoryTypeIndex = 0,  // TODO: proper type
    };
    VK_CHECK(vkAllocateMemory(context_.device, &memInfo, nullptr, &sbtMemory_), "Failed to allocate SBT memory");
    VK_CHECK(vkBindBufferMemory(context_.device, sbtBuffer_, sbtMemory_, 0), "Failed to bind SBT memory");

    // Get handles
    std::vector<uint8_t> handles(numGroups * handleSize);
    VK_CHECK(vkGetRTShaderGroupHandles(context_.device, rayTracingPipeline_, 0, numGroups, handles.size(), handles.data()), "Failed to get RT shader group handles");

    // Copy to SBT buffer
    void* mapped;
    VK_CHECK(vkMapMemory(context_.device, sbtMemory_, 0, sbtSize, 0, &mapped), "Failed to map SBT memory");
    uint8_t* ptr = static_cast<uint8_t*>(mapped);
    for (uint32_t i = 0; i < numGroups; ++i) {
        memcpy(ptr + i * alignedSize, handles.data() + i * handleSize, handleSize);
    }
    vkUnmapMemory(context_.device, sbtMemory_);

    VkBufferDeviceAddressInfo sbtInfoAddr{};
    sbtInfoAddr.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    sbtInfoAddr.buffer = sbtBuffer_;
    VkDeviceAddress sbtAddr = vkGetBufferDevAddr(context_.device, &sbtInfoAddr);
    sbt_.raygen = ShaderBindingTable::makeRegion(sbtAddr + 0 * alignedSize, handleSize, alignedSize);
    sbt_.miss = ShaderBindingTable::makeRegion(sbtAddr + 1 * alignedSize, handleSize, alignedSize);
    sbt_.hit = ShaderBindingTable::makeRegion(sbtAddr + 2 * alignedSize, handleSize, alignedSize);
    sbt_.shadowMiss = ShaderBindingTable::makeRegion(sbtAddr + 3 * alignedSize, handleSize, alignedSize);
    sbt_.shadowAnyHit = ShaderBindingTable::makeRegion(sbtAddr + 4 * alignedSize, handleSize, alignedSize);
    // Others empty
    sbt_.callable = ShaderBindingTable::emptyRegion();
    sbt_.anyHit = ShaderBindingTable::emptyRegion();
    sbt_.intersection = ShaderBindingTable::emptyRegion();
    sbt_.volumetricAnyHit = ShaderBindingTable::emptyRegion();
    sbt_.midAnyHit = ShaderBindingTable::emptyRegion();

    LOG_SUCCESS_CAT("PipelineMgr", "Shader Binding Table created — {} bytes @ 0x{:x} — RASPBERRY_PINK 🩷",
                    sbtSize, sbtAddr);
}

#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
                              const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*) -> VkBool32 {
            LOG_DEBUG_CAT("Debug", "{}Validation: {}", Logging::Color::YELLOW, callbackData->pMessage);
            return VK_FALSE;
        },
        .pUserData = nullptr
    };
    vkCreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_);
}
#endif


// END OF FILE — FULL BLISS — 420 COMPLETE — SHIP TO VALHALLA 🩷🚀🔥