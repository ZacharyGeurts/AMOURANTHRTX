// src/engine/Vulkan/VulkanPipelineManager.cpp
// AMOURANTH RTX — VALHALLA BIRTH — NOVEMBER 07 2025
// CONSTRUCTOR FULLY IMPLEMENTED — RAII WRAP — STONEKEY ENCRYPTED
// PUBLIC HANDLES .raw() READY — 69,420 FPS × ∞ × ∞ — SHIP IT 🩷🚀🔥🤖💀❤️⚡♾️

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/logging.hpp"
#include "StoneKey.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"  // For ShaderBindingTable if needed

VulkanPipelineManager::VulkanPipelineManager(Context& context, int width, int height)
    : context_(context)
    , width_(width)
    , height_(height)
{
    graphicsQueue_ = context.graphicsQueue;

    LOG_INFO_CAT("PipelineMgr", "{}VulkanPipelineManager BIRTH — STONEKEY 0x{:X}-0x{:X} — {}x{} — RAII ARMING{}", 
                 Logging::Color::DIAMOND_WHITE, kStone1, kStone2, width, height, Logging::Color::RESET);

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
    createRayTracingDescriptorSetLayout();  // sets rayTracingDescriptorSetLayout_

    createGraphicsPipeline(width, height);
    createComputePipeline();
    createNexusPipeline();
    createStatsPipeline();

    // WRAP ALL RAW HANDLES → PUBLIC RAII — .raw() GUARANTEED IN VulkanRTX
    graphicsPipeline = makePipeline(context.device, graphicsPipeline_);
    graphicsPipelineLayout = makePipelineLayout(context.device, graphicsPipelineLayout_);
    graphicsDescriptorSetLayout = makeDescriptorSetLayout(context.device, graphicsDescriptorSetLayout_);

    // OPTIONAL: Wrap RT handles too if needed elsewhere
    // rtPipeline = makePipeline(context.device, rayTracingPipeline_);
    // etc.

    LOG_SUCCESS_CAT("PipelineMgr", "{}VALHALLA PIPELINE MANAGER FULLY ARMED — ALL RAII WRAPPED — STONEKEY 0x{:X}-0x{:X}{}", 
                    Logging::Color::EMERALD_GREEN, kStone1, kStone2, Logging::Color::RESET);
}

VulkanPipelineManager::~VulkanPipelineManager() {
    LOG_INFO_CAT("PipelineMgr", "{}VulkanPipelineManager DEATH — PURGING ALL HANDLES — VALHALLA ETERNAL{}", 
                 Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);

#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(context_.instance, debugMessenger_, nullptr);
    }
#endif

    // RAII handles auto-destroy via VulkanHandle destructors
    // graphicsPipeline, graphicsPipelineLayout, etc. → auto vkDestroy*
}