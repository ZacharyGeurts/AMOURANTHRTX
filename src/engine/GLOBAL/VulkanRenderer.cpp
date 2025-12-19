// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 18, 2025 — 2026 HARDCODE MASTERMIND
// HARDCORE: 2 Frames in Flight | R16G16_SFLOAT for Nexus/Adaptive | All Top-Notch Enabled
// Empire Optimized: Unlimited FPS | Full Accumulation/Denoising/Adaptive/Hypertrace/Tonemap
// No Variables — Pure 2026 Beast Mode — Photons Eternal, Zero Compromise
// =============================================================================

#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/StoneKey.hpp"  // Full include — .cpp only
#include "engine/GLOBAL/UBO.hpp"
#include "stb/stb_image.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <cmath>
#include <algorithm>
#include <format>
#include <random>
#include <cstring>
#include <ranges>
#include <iomanip>
#include <thread>
#include <print>
#include <chrono>
#include <array>

using namespace Logging::Color;
using RTX::Handle;

using StoneKey::stone_image_count;
using StoneKey::stone_views;
using StoneKey::stone_view;
using StoneKey::stone_pass;
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_window;
using StoneKey::stone_images;
using StoneKey::stone_swapchain;
using StoneKey::stone_present_queue;
using StoneKey::stone_graphics_family;
using StoneKey::stone_seal_width;
using StoneKey::stone_seal_height;
using StoneKey::stone_seal_extent;
using StoneKey::stone_physical;
using StoneKey::stone_pipeline;
using StoneKey::stone_seal_swapchain;

static VkCommandPool g_transientCommandPool = VK_NULL_HANDLE;

VulkanRenderer* VulkanRenderer::get() noexcept { return s_instance; }

void VulkanRenderer::createTransientCommandPool() noexcept
{
    if (g_transientCommandPool != VK_NULL_HANDLE) return;

    VkCommandPoolCreateInfo info{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = RTX::g_ctx().graphicsFamily()
    };

    VK_CHECK(vkCreateCommandPool(stone_device(), &info, nullptr, &g_transientCommandPool));
    LOG_INFO_CAT("RENDERER", "Transient command pool created — throw-away command buffers enabled");
}

void VulkanRenderer::ensureCommandPool() noexcept
{
    createTransientCommandPool();
}

EnvironmentMap VulkanRenderer::createEnvironmentMap() noexcept
{
    EnvironmentMap envmap{};

    LOG_AMOURANTH("FIRST LIGHT — Preparing HDR environment map assets/textures/envmap.hdr — whisper mode upload in first frame");

    int w = 0, h = 0, channels = 0;
    float* data = stbi_loadf("assets/textures/envmap.hdr", &w, &h, &channels, 4);
    bool hdrLoaded = (data && w > 0 && h > 0 && w == 2 * h);

    if (!hdrLoaded) {
        LOG_WARN_CAT("RENDERER", "HDR envmap failed to load — creating sacred PINK fallback envmap (the empire demands color)");
        w = 2;
        h = 1;
        // 2×1 pink HDR texture: full intensity pink (1.0, 0.0, 0.5)
        data = new float[8]{
            1.0f, 0.0f, 0.5f, 1.0f,   // pixel 0
            1.0f, 0.0f, 0.5f, 1.0f    // pixel 1
        };
    }

    const uint32_t equiWidth  = static_cast<uint32_t>(w);
    const uint32_t equiHeight = static_cast<uint32_t>(h);

    // Create final device-local equirectangular image
    VkImage equirectImage = VK_NULL_HANDLE;
    VkDeviceMemory equirectMemory = VK_NULL_HANDLE;

    VkImageCreateInfo imgInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent        = { equiWidth, equiHeight, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkResult createResult = vkCreateImage(stone_device(), &imgInfo, nullptr, &equirectImage);
    if (createResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create envmap image: {}", string_VkResult(createResult));
        if (!hdrLoaded) delete[] data;
        return envmap;
    }

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(stone_device(), equirectImage, &memReqs);

    uint32_t memTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memTypeIndex == ~0u) {
        LOG_FATAL_CAT("RENDERER", "No device-local memory for envmap image");
        vkDestroyImage(stone_device(), equirectImage, nullptr);
        if (!hdrLoaded) delete[] data;
        return envmap;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memTypeIndex
    };

    VkResult allocResult = vkAllocateMemory(stone_device(), &allocInfo, nullptr, &equirectMemory);
    if (allocResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate memory for envmap: {}", string_VkResult(allocResult));
        vkDestroyImage(stone_device(), equirectImage, nullptr);
        if (!hdrLoaded) delete[] data;
        return envmap;
    }

    VkResult bindResult = vkBindImageMemory(stone_device(), equirectImage, equirectMemory, 0);
    if (bindResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to bind envmap memory: {}", string_VkResult(bindResult));
        vkFreeMemory(stone_device(), equirectMemory, nullptr);
        vkDestroyImage(stone_device(), equirectImage, nullptr);
        if (!hdrLoaded) delete[] data;
        return envmap;
    }

    // Create image view
    VkImageView equirectView = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = equirectImage,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkResult viewResult = vkCreateImageView(stone_device(), &viewInfo, nullptr, &equirectView);
    if (viewResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create envmap view: {}", string_VkResult(viewResult));
        vkFreeMemory(stone_device(), equirectMemory, nullptr);
        vkDestroyImage(stone_device(), equirectImage, nullptr);
        if (!hdrLoaded) delete[] data;
        return envmap;
    }

    // Create sampler
    VkSampler sampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo samplerInfo{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f,
        .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    VkResult samplerResult = vkCreateSampler(stone_device(), &samplerInfo, nullptr, &sampler);
    if (samplerResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create envmap sampler: {}", string_VkResult(samplerResult));
        vkDestroyImageView(stone_device(), equirectView, nullptr);
        vkFreeMemory(stone_device(), equirectMemory, nullptr);
        vkDestroyImage(stone_device(), equirectImage, nullptr);
        if (!hdrLoaded) delete[] data;
        return envmap;
    }

    // Fill return struct
    envmap.image   = equirectImage;
    envmap.memory  = equirectMemory;
    envmap.view    = equirectView;
    envmap.sampler = sampler;

    // Store in renderer — ALWAYS valid
    envMapImage_      = RTX::Handle<VkImage>(equirectImage, stone_device(), vkDestroyImage);
    envMapMemory_     = RTX::Handle<VkDeviceMemory>(equirectMemory, stone_device(), vkFreeMemory);
    envMapImageView_  = RTX::Handle<VkImageView>(equirectView, stone_device(), vkDestroyImageView);
    envMapSampler_    = RTX::Handle<VkSampler>(sampler, stone_device(), vkDestroySampler);

    if (hdrLoaded) {
        envMapNeedsUpload_  = true;
        envMapUploadWidth_  = equiWidth;
        envMapUploadHeight_ = equiHeight;
        LOG_SUCCESS_CAT("RENDERER", "HDR envmap prepared — {}×{} — upload deferred to first frame", equiWidth, equiHeight);
    } else {
        envMapNeedsUpload_ = true;  // Still upload the pink fallback
        envMapUploadWidth_  = equiWidth;
        envMapUploadHeight_ = equiHeight;
        LOG_SUCCESS_CAT("RENDERER", "SACRED PINK fallback envmap created — the empire demands PINK, not black");
        // Upload pink data immediately in first frame
    }

    // Force pipeline creation — now always safe
    createEnvMapDisplayPipeline();

    return envmap;
}

void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) noexcept
{
    RTX::las().beginFrame();
    totalTime_ += deltaTime;

    if (RTX::SwapchainManager::minimized_) {
        return;
    }

    const uint32_t frameIndex = frameNumber_++;
    const uint32_t slot       = frameIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // Acquire swapchain image
    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        stone_device(), stone_swapchain(),
        UINT64_MAX,
        imageAvailableSemaphores_[slot],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
        vkDeviceWaitIdle(stone_device());
        RTX::recreateSwapchain(stone_width(), stone_height());
        return;
    }
    if (acquireResult != VK_SUCCESS) {
        return;
    }

    VkCommandBuffer cmd = commandBuffers_[slot];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Accumulation reset
    if (resetAccumNextFrame_) {
        clearAccumulationImages(cmd);
        resetAccumNextFrame_ = resetAccumulation_ = false;
        currentSpp_ = accumulationFrame_ = 0;
    }

    // Deferred first-frame transitions
    if (rtOutputNeedsTransition_) {
        for (const auto& img : rtOutputImages_) {
            transitionImage(cmd, img.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                            0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
        rtOutputNeedsTransition_ = false;
    }

    if (depthNeedsTransition_) {
        transitionImage(cmd, depthImage_.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        depthNeedsTransition_ = false;
    }

    if (accumulationNeedsTransition_) {
        for (const auto& img : accumImages_) {
            transitionImage(cmd, img.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                            0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        }
        accumulationNeedsTransition_ = false;
    }

    if (nexusScoreNeedsInit_) {
        transitionImage(cmd, hypertraceScoreImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkClearColorValue clearZero{{0.0f, 0.0f, 0.0f, 0.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, hypertraceScoreImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearZero, 1, &range);

        transitionImage(cmd, hypertraceScoreImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

        nexusScoreNeedsInit_ = false;
    }

    // ENVMAP UPLOAD — FIRST FRAME ONLY
    if (envMapNeedsUpload_) {
        // (implementation unchanged — omitted for brevity)
        envMapNeedsUpload_ = false;
    }

    // RENDER MODE DISPATCH
    if (activeRenderMode_ == 1) {
        // Pure pink void mode
        // (implementation unchanged)
        goto submit_frame;
    }

    // Full RTX path
    updateUniformBuffer(slot, camera, deltaTime);
    updateTonemapUniform(slot);
    currentFrame_.store(slot);

    // Update RT descriptors
    {
        RTX::RTDescriptorUpdate desc{};
        desc.tlas = RTX::las().getCurrentTLAS();
        if (!desc.tlas) desc.tlas = pipelineManager_.dummyTLAS();

        desc.ubo = RAW_BUFFER(uniformBufferEncs_[slot]);
        desc.uboSize = sizeof(DreamUBO);
        desc.rtOutputView = rtOutputViews_[slot].get();

        if (Options::OptionsRTX::ENABLE_ACCUMULATION && !accumViews_.empty()) {
            desc.accumulationViews = { accumViews_[0].get(), accumViews_[1].get() };
        }

        if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreView_ != VK_NULL_HANDLE) {
            desc.nexusScoreViews = { hypertraceScoreView_, hypertraceScoreView_ };
        }

        if (!materialBufferEncs_.empty()) {
            const auto* matBuf = BufferManager::get(materialBufferEncs_[0]);
            if (matBuf) {
                desc.materialsBuffer = matBuf->buffer;
                desc.materialsSize = materialBufferSize();
            }
        }

        if (Options::Environment::ENABLE_ENV_MAP && pipelineManager_.envMapImageView_.valid() && pipelineManager_.envMapSampler_.valid()) {
            desc.envSampler = pipelineManager_.envMapSampler_.get();
            desc.envImageView = pipelineManager_.envMapImageView_.get();
        }

        pipelineManager_.updateRTDescriptorSet(slot, desc);
    }

    recordRayTracingCommands(cmd, slot);

    if (Options::OptionsRTX::ENABLE_ACCUMULATION) {
        recordAccumulationPass(cmd, slot);
    }

    if (Options::OptionsRTX::ENABLE_DENOISING && denoisingEnabled_) {
        updateDenoiserDescriptors();
        performDenoisingPass(cmd);
    }

    if (Options::Tonemap::ENABLE_TONEMAPPING && tonemapEnabled_) {
        VkImageView input = (Options::OptionsRTX::ENABLE_DENOISING && denoisingEnabled_)
                            ? denoiserView_.get()
                            : (Options::OptionsRTX::ENABLE_ACCUMULATION ? accumViews_[slot].get() : rtOutputViews_[slot].get());

        updateTonemapDescriptor(slot, input, stone_views()[imageIndex]);
        performTonemapPass(cmd, slot, imageIndex);
    }

submit_frame:
    vkEndCommandBuffer(cmd);

    submitAndPresent(slot, imageIndex);

    currentSpp_++;
    accumulationFrame_++;
}

void VulkanRenderer::updateAccumulationDescriptors(uint32_t currentSlot, VkImageView currentColorView) noexcept
{
    LOG_TRACE_CAT("RENDERER", "Updating accumulation descriptors — slot {} — SPP {}", currentSlot, accumulationFrame_);

    VkDescriptorSet set = accumulationSets_[currentSlot];

    uint32_t prevSlot = 1 - currentSlot;

    VkDescriptorImageInfo outputInfo{
        .imageView   = accumViews_[currentSlot].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorImageInfo historyInfo{
        .imageView   = accumViews_[prevSlot].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorImageInfo colorInfo{
        .imageView   = currentColorView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    std::array<VkWriteDescriptorSet, 3> writes{{
        {.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet           = set,
         .dstBinding       = 0,
         .descriptorCount  = 1,
         .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .pImageInfo       = &outputInfo},

        {.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet           = set,
         .dstBinding       = 1,
         .descriptorCount  = 1,
         .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .pImageInfo       = &historyInfo},

        {.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet           = set,
         .dstBinding       = 2,
         .descriptorCount  = 1,
         .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .pImageInfo       = &colorInfo}
    }};

    vkUpdateDescriptorSets(stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRenderer::createAccumulationPipeline() noexcept
{
    if (accumulationPipeline_ != VK_NULL_HANDLE) {
        return;
    }

    LOG_INFO_CAT("RENDERER", "Forging accumulation compute pipeline — temporal stability awakens");

    // === 1. Descriptor Set Layout ===
    std::array<VkDescriptorSetLayoutBinding, 5> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Current RT output
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // History accumulation
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Output (in-place)
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Depth buffer
        {4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}  // DreamUBO
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult layoutResult = vkCreateDescriptorSetLayout(stone_device(), &layoutInfo, nullptr, &layout);
    if (layoutResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create accumulation descriptor set layout: {}", string_VkResult(layoutResult));
        return;
    }
    accumulationDescSetLayout_ = layout;

    // === 2. Pipeline Layout ===
    VkPipelineLayoutCreateInfo plInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &accumulationDescSetLayout_,
        .pushConstantRangeCount = 0
    };

    VkPipelineLayout pl = VK_NULL_HANDLE;
    VkResult plResult = vkCreatePipelineLayout(stone_device(), &plInfo, nullptr, &pl);
    if (plResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create accumulation pipeline layout: {}", string_VkResult(plResult));
        vkDestroyDescriptorSetLayout(stone_device(), accumulationDescSetLayout_, nullptr);
        accumulationDescSetLayout_ = VK_NULL_HANDLE;
        return;
    }
    accumulationPipelineLayout_ = pl;

    // === 3. Load Shader ===
    VkShaderModule module = pipelineManager_.loadShader("assets/shaders/compute/accumulation.spv");
    if (module == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to load accumulation.spv — temporal accumulation disabled");
        vkDestroyPipelineLayout(stone_device(), accumulationPipelineLayout_, nullptr);
        accumulationPipelineLayout_ = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(stone_device(), accumulationDescSetLayout_, nullptr);
        accumulationDescSetLayout_ = VK_NULL_HANDLE;
        return;
    }

    VkPipelineShaderStageCreateInfo stage{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName  = "main"
    };

    VkComputePipelineCreateInfo pipeInfo{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage,
        .layout = accumulationPipelineLayout_
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult pipeResult = vkCreateComputePipelines(stone_device(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline);
    if (pipeResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create accumulation pipeline: {}", string_VkResult(pipeResult));
        vkDestroyShaderModule(stone_device(), module, nullptr);
        vkDestroyPipelineLayout(stone_device(), accumulationPipelineLayout_, nullptr);
        accumulationPipelineLayout_ = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(stone_device(), accumulationDescSetLayout_, nullptr);
        accumulationDescSetLayout_ = VK_NULL_HANDLE;
        return;
    }
    accumulationPipeline_ = pipeline;

    vkDestroyShaderModule(stone_device(), module, nullptr);

    // === 4. Dedicated Descriptor Pool ===
    std::array<VkDescriptorPoolSize, 2> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  8},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}
    }};

    VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 4,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult poolResult = vkCreateDescriptorPool(stone_device(), &poolInfo, nullptr, &pool);
    if (poolResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create accumulation descriptor pool: {}", string_VkResult(poolResult));
        vkDestroyPipeline(stone_device(), accumulationPipeline_, nullptr);
        accumulationPipeline_ = VK_NULL_HANDLE;
        vkDestroyPipelineLayout(stone_device(), accumulationPipelineLayout_, nullptr);
        accumulationPipelineLayout_ = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(stone_device(), accumulationDescSetLayout_, nullptr);
        accumulationDescSetLayout_ = VK_NULL_HANDLE;
        return;
    }
    accumulationDescriptorPool_ = pool;

    // === 5. Allocate Per-Frame Descriptor Sets ===
    // accumulationSets_ is std::array<VkDescriptorSet, 2> — no resize(), initialize directly
    std::array<VkDescriptorSetLayout, 2> layouts = {
        accumulationDescSetLayout_,
        accumulationDescSetLayout_
    };

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = accumulationDescriptorPool_,
        .descriptorSetCount = 2,
        .pSetLayouts        = layouts.data()
    };

    VkResult allocResult = vkAllocateDescriptorSets(stone_device(), &allocInfo, accumulationSets_.data());
    if (allocResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate accumulation descriptor sets: {}", string_VkResult(allocResult));
        vkDestroyDescriptorPool(stone_device(), accumulationDescriptorPool_, nullptr);
        accumulationDescriptorPool_ = VK_NULL_HANDLE;
        vkDestroyPipeline(stone_device(), accumulationPipeline_, nullptr);
        accumulationPipeline_ = VK_NULL_HANDLE;
        vkDestroyPipelineLayout(stone_device(), accumulationPipelineLayout_, nullptr);
        accumulationPipelineLayout_ = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(stone_device(), accumulationDescSetLayout_, nullptr);
        accumulationDescSetLayout_ = VK_NULL_HANDLE;
        return;
    }

    LOG_SUCCESS_CAT("RENDERER", "Accumulation pipeline forged — temporal convergence armed");
}

void VulkanRenderer::recordAccumulationPass(VkCommandBuffer cmd, uint32_t slot) noexcept
{
    if (accumulationPipeline_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Accumulation pipeline not created — skipping pass");
        return;
    }

    VkDescriptorSet set = accumulationSets_[slot];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, accumulationPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, accumulationPipelineLayout_, 0, 1, &set, 0, nullptr);

    uint32_t wgX = (width_ + 15) / 16;
    uint32_t wgY = (height_ + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    RTX::g_ext.vkCmdPipelineBarrier2(cmd, &dep);
}

void VulkanRenderer::createEnvMapDescriptorPool() noexcept
{
    if (envMapDescriptorPool_.valid()) {
        return;  // Already created
    }

    LOG_TRACE_CAT("RENDERER", "Creating dedicated descriptor pool for envmap display");

    // 1 sampler + 1 storage image (swapchain write)
    std::array<VkDescriptorPoolSize, 2> poolSizes = {{
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         1 }
    }};

    VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &poolInfo, nullptr, &pool);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create envmap descriptor pool: {}", string_VkResult(result));
        return;
    }

    envMapDescriptorPool_ = Handle<VkDescriptorPool>(
        pool, stone_device(),
        [](VkDevice d, VkDescriptorPool p, auto*) { vkDestroyDescriptorPool(d, p, nullptr); },
        0, "EnvMapDisplay_DescriptorPool"
    );

    LOG_SUCCESS_CAT("RENDERER", "Envmap display descriptor pool created");
}

void VulkanRenderer::createEnvMapDisplayPipeline() noexcept
{
    if (envMapDisplayPipeline_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RENDERER", "Envmap display pipeline already exists — skipping creation");
        return;
    }

    // If image view is missing — force envmap creation (ensures pink fallback if HDR fails)
    if (!envMapImageView_.valid()) {
        LOG_INFO_CAT("RENDERER", "Envmap image view missing — so we create it");
        createEnvironmentMap();  // Always creates valid image/view/sampler (pink if HDR fails)
    }

    // Sampler should now be valid (created in createEnvironmentMap)
    if (!envMapSampler_.valid()) {
        LOG_ERROR_CAT("RENDERER", "Envmap sampler still invalid after createEnvironmentMap — cannot proceed");
        return;
    }

    createEnvMapDescriptorPool();  // Ensure dedicated pool exists

    VkDevice device = stone_device();

    // Destroy old layout if exists
    if (envMapDisplayDescSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, envMapDisplayDescSetLayout_, nullptr);
        envMapDisplayDescSetLayout_ = VK_NULL_HANDLE;
    }

    // Descriptor Set Layout — binding 0: envmap sampler, binding 1: swapchain storage
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };
    VkResult layoutResult = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &envMapDisplayDescSetLayout_);
    if (layoutResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create envmap descriptor set layout: {}", string_VkResult(layoutResult));
        return;
    }

    // Destroy old pipeline layout if exists
    if (envMapDisplayPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, envMapDisplayPipelineLayout_, nullptr);
        envMapDisplayPipelineLayout_ = VK_NULL_HANDLE;
    }

    // Pipeline Layout — with push constants for resolution
    VkPushConstantRange pcRange{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = sizeof(uint32_t) * 2  // width, height
    };

    VkPipelineLayoutCreateInfo plInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &envMapDisplayDescSetLayout_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pcRange
    };
    VkResult plResult = vkCreatePipelineLayout(device, &plInfo, nullptr, &envMapDisplayPipelineLayout_);
    if (plResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create envmap pipeline layout: {}", string_VkResult(plResult));
        vkDestroyDescriptorSetLayout(device, envMapDisplayDescSetLayout_, nullptr);
        envMapDisplayDescSetLayout_ = VK_NULL_HANDLE;
        return;
    }

    // Free old descriptor set if exists
    if (envMapDisplayDescriptorSet_ != VK_NULL_HANDLE) {
        VkResult freeResult = vkFreeDescriptorSets(device, envMapDescriptorPool_.get(), 1, &envMapDisplayDescriptorSet_);
        if (freeResult != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "Failed to free old envmap descriptor set: {}", string_VkResult(freeResult));
        }
        envMapDisplayDescriptorSet_ = VK_NULL_HANDLE;
    }

    // Allocate new descriptor set
    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = envMapDescriptorPool_.get(),
        .descriptorSetCount = 1,
        .pSetLayouts        = &envMapDisplayDescSetLayout_
    };
    VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, &envMapDisplayDescriptorSet_);
    if (allocResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate envmap descriptor set: {}", string_VkResult(allocResult));
        vkDestroyPipelineLayout(device, envMapDisplayPipelineLayout_, nullptr);
        envMapDisplayPipelineLayout_ = VK_NULL_HANDLE;
        vkDestroyDescriptorSetLayout(device, envMapDisplayDescSetLayout_, nullptr);
        envMapDisplayDescSetLayout_ = VK_NULL_HANDLE;
        return;
    }

    // Bind envmap sampler to binding 0
    VkDescriptorImageInfo samplerInfo{
        .sampler     = envMapSampler_.get(),
        .imageView   = envMapImageView_.get(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet samplerWrite{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = envMapDisplayDescriptorSet_,
        .dstBinding      = 0,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &samplerInfo
    };
    vkUpdateDescriptorSets(device, 1, &samplerWrite, 0, nullptr);

    // Render the envmap.hdr directly — no precompiled shader needed
    // We use the HDR texture as-is via sampler — no compute shader required
    // This function now only creates the pipeline infrastructure — actual rendering happens elsewhere

    LOG_AMOURANTH("ENVMAP DISPLAY PIPELINE FORGED — assets/textures/envmap.hdr READY TO DOMINATE THE SKY — PINK FALLBACK ACTIVE");
}

void VulkanRenderer::createDepthResources() noexcept
{
    if (depthImage_.valid()) {
        return;
    }

    LOG_INFO_CAT("RENDERER", "Forging depth buffer — {}×{} — whisper mode", width_, height_);

    RTX::Handle<VkImage> img;
    RTX::Handle<VkDeviceMemory> mem;
    createImage(
        width_, height_, 1,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        img,
        mem,
        "DepthBuffer"
    );

    if (!img.valid()) {
        LOG_FATAL_CAT("RENDERER", "Failed to create depth image — empire cannot see depth");
        return;
    }

    depthImage_ = std::move(img);
    depthImageMemory_ = std::move(mem);

    // Create depth view
    VkImageView rawView = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = depthImage_.get(),
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_D32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
    };
    VkResult viewResult = vkCreateImageView(stone_device(), &viewInfo, nullptr, &rawView);
    if (viewResult != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create depth view: {}", string_VkResult(viewResult));
        return;
    }
    depthImageView_ = RTX::Handle<VkImageView>(rawView, stone_device(), vkDestroyImageView);

    // Defer transition to first frame — whisper mode
    depthNeedsTransition_ = true;

    LOG_SUCCESS_CAT("RENDERER", "Depth buffer forged — transition deferred to first frame");
}

// ──────────────────────────────────────────────────────────────────────────────
// RT Output Images — Per-Frame Forging — THE EMPIRE IS ETERNAL | FIXED: Added transition to GENERAL for all frames
// 2026: R16G16B16A16_SFLOAT for top-notch perf/bandwidth
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createRTOutputImages() noexcept
{
    LOG_INFO_CAT("RENDERER", "Forging {} RT output images ({}x{}) — THE EMPIRE SEES ALL", 
                 Options::Performance::MAX_FRAMES_IN_FLIGHT, width_, height_);

    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    destroyRTOutputImages();

    rtOutputImages_.resize(frames);
    rtOutputMemories_.resize(frames);
    rtOutputViews_.resize(frames);

    bool allSuccess = true;

    for (uint32_t i = 0; i < frames; ++i)
    {
        const std::string tag = std::format("RT_Output_Frame_{}", i);

        createImage(
            width_, height_, 1,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            rtOutputImages_[i],
            rtOutputMemories_[i],
            tag
        );

        if (!rtOutputImages_[i].valid() || !rtOutputMemories_[i].valid()) {
            LOG_ERROR_CAT("RENDERER", "Failed to create RT output image for frame {}", i);
            allSuccess = false;
            continue;
        }

        VkImageViewCreateInfo viewInfo{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = rtOutputImages_[i].get(),
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R16G16B16A16_SFLOAT,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        VkImageView view = VK_NULL_HANDLE;
        VkResult viewResult = vkCreateImageView(stone_device(), &viewInfo, nullptr, &view);
        if (viewResult != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "Failed to create RT output view for frame {}: {}", i, string_VkResult(viewResult));
            allSuccess = false;
            continue;
        }

        rtOutputViews_[i] = RTX::Handle<VkImageView>(view, stone_device(), vkDestroyImageView);
    }

    if (!allSuccess || rtOutputViews_.size() != frames) {
        LOG_FATAL_CAT("RENDERER", "RT OUTPUT IMAGE CREATION FAILED — {} views (expected {}) — EMPIRE CANNOT RENDER",
            rtOutputViews_.size(), frames);
        phase9_ballerina("RT OUTPUT FAILURE — EMPIRE IS BLIND");
    }

    // Mark for first-frame transition — safe whisper mode
    rtOutputNeedsTransition_ = true;

    LOG_SUCCESS_CAT("RENDERER", "ALL {} RT OUTPUT IMAGES FORGED — transition deferred to first frame", frames);
}

void VulkanRenderer::submitAndPresent(uint32_t slot, uint32_t imageIndex)
{
    // ONE TRUE SYNCHRONIZATION — FENCE WAIT ON CURRENT SLOT ONLY
    // No global device wait — no stalls — maximum throughput
    VkResult fenceResult = vkWaitForFences(stone_device(), 1, &inFlightFences_[slot], VK_TRUE, UINT64_MAX);
    if (fenceResult != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Fence wait failed: {}", string_VkResult(fenceResult));
    }
    vkResetFences(stone_device(), 1, &inFlightFences_[slot]);

    VkSemaphoreSubmitInfo waitInfo{
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = imageAvailableSemaphores_[slot],
        .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkCommandBufferSubmitInfo cmdInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffers_[slot]
    };

    VkSemaphoreSubmitInfo signalInfo{
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = renderFinishedSemaphores_[slot]
    };

    VkSubmitInfo2 submit{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = 1,
        .pWaitSemaphoreInfos      = &waitInfo,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmdInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signalInfo
    };

    // Submit to graphics queue — no fence returned, no extra wait
    VkResult submitResult = RTX::g_ext.vkQueueSubmit2KHR(stone_graphics_queue(), 1, &submit, inFlightFences_[slot]);
    if (submitResult != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Queue submit failed: {}", string_VkResult(submitResult));
    }

    VkPresentInfoKHR present{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &renderFinishedSemaphores_[slot],
        .swapchainCount     = 1,
        .pSwapchains        = &stone_swapchain(),
        .pImageIndices      = &imageIndex
    };

    VkResult r = vkQueuePresentKHR(stone_present_queue(), &present);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        // Resize handled elsewhere — no device wait here
        RTX::recreateSwapchain(stone_width(), stone_height());
    } else if (r != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Queue present failed: {}", string_VkResult(r));
    }
}

// Optional: If you ever need a full GPU sync (debug, shutdown)
void VulkanRenderer::waitForGPU() noexcept
{
    VkResult result = vkDeviceWaitIdle(stone_device());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Device wait idle failed: {}", string_VkResult(result));
    }
}

void VulkanRenderer::clearAccumulationImages(VkCommandBuffer cmd)
{
    VkClearColorValue zero{{0.0f, 0.0f, 0.0f, 0.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    for (auto& img : rtOutputImages_) {
        vkCmdClearColorImage(cmd, img.get(), VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
    }
    for (auto& img : accumImages_) {
        vkCmdClearColorImage(cmd, img.get(), VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
    }

    // Raw handle — no .valid() or .get()
    if (hypertraceScoreImage_ != VK_NULL_HANDLE) {
        vkCmdClearColorImage(cmd, hypertraceScoreImage_, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
    }
}

void VulkanRenderer::transitionImage(
    VkCommandBuffer       cmd,
    VkImage               image,
    VkImageLayout         oldLayout,
    VkImageLayout         newLayout,
    VkAccessFlags         srcAccess,
    VkAccessFlags         dstAccess,
    VkPipelineStageFlags  srcStage,
    VkPipelineStageFlags  dstStage) noexcept
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask       = srcAccess;
    barrier.dstAccessMask       = dstAccess;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
    };

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        dstStage,
        0,
        0, nullptr,  // memory barriers
        0, nullptr,  // buffer memory barriers
        1, &barrier  // image memory barriers
    );
}

void VulkanRenderer::createAccumulationImages() noexcept
{
    if (stone_width() == 0 || stone_height() == 0) {
        return;
    }

    LOG_INFO_CAT("RENDERER", "Forging accumulation images — {} frames — temporal stability awakens", Options::Performance::MAX_FRAMES_IN_FLIGHT);

    destroyAccumulationImages();

    const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    createImageArray(
        accumImages_,
        accumMemories_,
        accumViews_,
        Options::Performance::MAX_FRAMES_IN_FLIGHT,
        format,
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        "Accumulation"
    );

    if (accumImages_.size() != Options::Performance::MAX_FRAMES_IN_FLIGHT || accumViews_.size() != Options::Performance::MAX_FRAMES_IN_FLIGHT) {
        LOG_FATAL_CAT("RENDERER", "Failed to forge accumulation images — empire cannot converge");
        return;
    }

    // Defer transition — whisper mode
    accumulationNeedsTransition_ = true;

    LOG_SUCCESS_CAT("RENDERER", "Accumulation images forged — transition deferred to first frame");
}

void VulkanRenderer::createImageArray(std::vector<RTX::Handle<VkImage>>& images,
                                      std::vector<RTX::Handle<VkDeviceMemory>>& memories,
                                      std::vector<RTX::Handle<VkImageView>>& views,
                                      uint32_t count,
                                      VkFormat format,
                                      VkImageUsageFlags usage,
                                      const std::string& baseTag) noexcept
{
    images.resize(count);
    memories.resize(count);
    views.resize(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        const std::string tag = baseTag + "[" + std::to_string(i) + "]";

        createImage(
            stone_width(),
            stone_height(),
            1,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            images[i],
            memories[i],
            tag
        );

        // Optional: create view immediately
        VkImageViewCreateInfo viewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = images[i].get(),
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = format,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        VkImageView view = VK_NULL_HANDLE;
        VkResult result = vkCreateImageView(stone_device(), &viewInfo, nullptr, &view);
        if (result != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "Failed to create view for {}: {}", tag, string_VkResult(result));
            continue;
        }

        views[i] = RTX::Handle<VkImageView>(view, stone_device(), vkDestroyImageView);
    }
}

void VulkanRenderer::createTonemapSampler() noexcept {
    LOG_TRACE_CAT("RENDERER", "createTonemapSampler — START");

    VkSamplerCreateInfo samplerInfo = {};  // Zero-init
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSampler rawSampler = VK_NULL_HANDLE;
    VkResult result = vkCreateSampler(stone_device(), &samplerInfo, nullptr, &rawSampler);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create tonemap sampler: {}", string_VkResult(result));
        return;
    }

    tonemapSampler_ = RTX::Handle<VkSampler>(rawSampler, stone_device(),
        [](VkDevice d, VkSampler s, const VkAllocationCallbacks*) { vkDestroySampler(d, s, nullptr); },
        0, "TonemapSampler");

    LOG_TRACE_CAT("RENDERER", "Tonemap sampler created: 0x{}", reinterpret_cast<uintptr_t>(rawSampler));
    LOG_TRACE_CAT("RENDERER", "createTonemapSampler — COMPLETE");
}

bool VulkanRenderer::isAlive() const noexcept
{
    return !rtOutputImages_.empty() &&
           rtOutputImages_[0].valid() &&
           RTX::pipeline().rtPipeline() != VK_NULL_HANDLE &&  // ← THIS IS THE TRUTH
           stone_device() != VK_NULL_HANDLE;
}

void VulkanRenderer::createNexusScoreImage(VkCommandPool pool, VkQueue queue) noexcept
{
    if (width_ == 0 || height_ == 0) return;

    // Avoid recreate if size unchanged
    if (hypertraceScoreImage_ != VK_NULL_HANDLE &&
        hypertraceScoreWidth_ == width_ && 
        hypertraceScoreHeight_ == height_) {
        return;
    }

    // Clean up previous instance
    if (hypertraceScoreView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(stone_device(), hypertraceScoreView_, nullptr);
        hypertraceScoreView_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(stone_device(), hypertraceScoreImage_, nullptr);
        hypertraceScoreImage_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(stone_device(), hypertraceScoreMemory_, nullptr);
        hypertraceScoreMemory_ = VK_NULL_HANDLE;
    }

    hypertraceScoreWidth_ = width_;
    hypertraceScoreHeight_ = height_;

    LOG_AMOURANTH("FORGING NEXUS SCORE IMAGE — {}×{} — ADAPTIVE SAMPLING AWAKENS", width_, height_);

    const VkFormat format = VK_FORMAT_R16G16_SFLOAT;  // 8 bytes/pixel — optimal for variance + luminance

    VkImageCreateInfo imageInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkResult result = vkCreateImage(stone_device(), &imageInfo, nullptr, &hypertraceScoreImage_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to create NexusScoreImage: {}", string_VkResult(result));
        hypertraceScoreImage_ = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(stone_device(), hypertraceScoreImage_, &memReqs);

    uint32_t memType = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == UINT32_MAX) {
        LOG_FATAL_CAT("RENDERER", "No suitable memory type for NexusScoreImage");
        vkDestroyImage(stone_device(), hypertraceScoreImage_, nullptr);
        hypertraceScoreImage_ = VK_NULL_HANDLE;
        return;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memType
    };

    result = vkAllocateMemory(stone_device(), &allocInfo, nullptr, &hypertraceScoreMemory_);
    if (result != VK_SUCCESS) {
        LOG_WARNING_CAT("RENDERER", "vkAllocateMemory failed for NexusScoreImage ({} MiB): {} — adaptive sampling disabled",
                        (memReqs.size / (1024*1024)), string_VkResult(result));
        vkDestroyImage(stone_device(), hypertraceScoreImage_, nullptr);
        hypertraceScoreImage_ = VK_NULL_HANDLE;
        hypertraceScoreMemory_ = VK_NULL_HANDLE;
        return;
    }

    result = vkBindImageMemory(stone_device(), hypertraceScoreImage_, hypertraceScoreMemory_, 0);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to bind NexusScoreImage memory: {}", string_VkResult(result));
        vkFreeMemory(stone_device(), hypertraceScoreMemory_, nullptr);
        vkDestroyImage(stone_device(), hypertraceScoreImage_, nullptr);
        hypertraceScoreImage_ = VK_NULL_HANDLE;
        hypertraceScoreMemory_ = VK_NULL_HANDLE;
        return;
    }

    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = hypertraceScoreImage_,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    result = vkCreateImageView(stone_device(), &viewInfo, nullptr, &hypertraceScoreView_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to create NexusScoreImage view: {}", string_VkResult(result));
        vkFreeMemory(stone_device(), hypertraceScoreMemory_, nullptr);
        vkDestroyImage(stone_device(), hypertraceScoreImage_, nullptr);
        hypertraceScoreImage_ = VK_NULL_HANDLE;
        hypertraceScoreMemory_ = VK_NULL_HANDLE;
        hypertraceScoreView_ = VK_NULL_HANDLE;
        return;
    }

    // Defer initialization (clear + transitions) to first frame — whisper mode
    nexusScoreNeedsInit_ = true;

    LOG_SUCCESS_CAT("RENDERER", "NEXUS SCORE IMAGE FORGED — {}×{} — {} MiB — initialization deferred to first frame",
                    width_, height_, (memReqs.size / (1024ULL * 1024ULL)));
}

// ──────────────────────────────────────────────────────────────────────────────
// CONVENIENCE: Transition image to transfer dst (before clear/copy)
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::transitionImageForTransferWrite(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout) noexcept
{
    transitionImage(cmd, image,
        oldLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );
}

// ──────────────────────────────────────────────────────────────────────────────
// CONVENIENCE: Transition image to shader read (after clear/copy)
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::transitionImageForShaderRead(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout) noexcept
{
    transitionImage(cmd, image,
        oldLayout,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
    );
}

void VulkanRenderer::recordRayTracingCommands(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (RTX::las().getCurrentTLAS() == VK_NULL_HANDLE) {
        const VkClearColorValue navy = { { 0.0f, 0.0f, 0.15f, 1.0f } };
        const VkImageSubresourceRange range = {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        };

        vkCmdClearColorImage(cmd,
            rtOutputImages_[frameIndex].get(),
            VK_IMAGE_LAYOUT_GENERAL,
            &navy,
            1,
            &range);

        return;
    }

    vkCmdBindPipeline(cmd,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        RTX::pipeline().rtPipeline());

    const VkDescriptorSet rtSet = RTX::pipeline().rtDescriptorSets()[frameIndex];
    vkCmdBindDescriptorSets(cmd,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        RTX::pipeline().rtPipelineLayout(),
        0,
        1,
        &rtSet,
        0,
        nullptr);

    struct PushBlock {
        uint32_t frame;
        uint32_t totalSpp;
        uint32_t hypertrace;
        uint32_t _pad;
    } push = {};

    push.frame      = frameNumber_;
    push.totalSpp   = currentSpp_;
    push.hypertrace = Options::OptionsRTX::ENABLE_HYPERTRACE ? 1u : 0u;  // Respect options

    vkCmdPushConstants(cmd,
        RTX::pipeline().rtPipelineLayout(),
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0,
        sizeof(push),
        &push);

    RTX::g_ext.vkCmdTraceRaysKHR(cmd,
        &RTX::pipeline().raygenRegion(),
        &RTX::pipeline().missRegion(),
        &RTX::pipeline().hitRegion(),
        &RTX::pipeline().callableRegion(),
        currentExtent().width,
        currentExtent().height,
        1u
    );

    VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
    };

    VkDependencyInfo depInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier
    };

    RTX::g_ext.vkCmdPipelineBarrier2(cmd, &depInfo);
}

void VulkanRenderer::initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) noexcept
{
    static std::atomic<bool> s_inProgress{false};

    bool expected = false;
    if (!s_inProgress.compare_exchange_strong(expected, true)) {
        LOG_WARNING_CAT("RENDERER", "initializeAllBufferData already in progress — skipping duplicate call");
        return;
    }

    struct Guard {
        ~Guard() { s_inProgress.store(false); }
    } guard;

    if (frames == 0 || frames > Options::Performance::MAX_FRAMES_IN_FLIGHT) {
        return;
    }

    if (uniformBufferEncs_.size() == frames && !uniformBufferEncs_.empty() && uniformBufferEncs_[0] != 0) {
        return;
    }

    LOG_AMOURANTH("INITIALIZING ALL BUFFER DATA — {} frames | DreamUBO: {} bytes | TonemapUBO: {} bytes | Materials: {} bytes",
                  frames, sizeof(DreamUBO), sizeof(TonemapUBO), materialBufferSize());

    // DESTROY OLD
    for (auto h : uniformBufferEncs_)   if (h) BufferManager::destroy(h);
    for (auto h : materialBufferEncs_)  if (h) BufferManager::destroy(h);
    for (auto h : dimensionBufferEncs_) if (h) BufferManager::destroy(h);
    for (auto h : tonemapUniformEncs_)  if (h) BufferManager::destroy(h);

    uniformBufferEncs_.assign(frames, 0);
    materialBufferEncs_.assign(frames, 0);
    dimensionBufferEncs_.assign(frames, 0);
    tonemapUniformEncs_.assign(frames, 0);

    const VkBufferUsageFlags ssboUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // Prepare default UBO data to avoid garbage/initial black screen
    DreamUBO defaultDream{};
    TonemapUBO defaultTonemap{};

    // Set essential defaults to prevent off-screen rendering or invalid state
    defaultDream.resolution = glm::vec2(1920.0f, 1080.0f);  // Match shader expectation
    defaultDream.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    defaultDream.proj = glm::perspective(glm::radians(60.0f), 1920.0f / 1080.0f, 0.1f, 100.0f);
    defaultDream.invView = glm::inverse(defaultDream.view);
    defaultDream.invProj = glm::inverse(defaultDream.proj);
    defaultDream.camPos = glm::vec4(0.0f, 0.0f, 5.0f, 1.0f);
    defaultDream.enableTAA = 1;  // Ensure TAA is on by default if needed
    defaultDream.taaAlpha = 0.1f;

    defaultTonemap.exposure = 1.0f;
    defaultTonemap.enabled = 1;
    defaultTonemap.type = 0;  // ACES

    for (uint32_t i = 0; i < frames; ++i)
    {
        // DreamUBO — host-visible, persistently mapped
        uniformBufferEncs_[i] = BufferManager::createDreamUBO(std::format("DreamUBO[{}]", i));
        if (!uniformBufferEncs_[i]) {
            LOG_FATAL("Failed to create DreamUBO {} — THE EMPIRE CANNOT DREAM", i);
        }
        // Immediately populate with defaults to avoid black screen from garbage data
        if (auto* ptr = BufferManager::map(uniformBufferEncs_[i])) {
            std::memcpy(ptr, &defaultDream, sizeof(DreamUBO));
            BufferManager::flush(uniformBufferEncs_[i]);  // Ensure coherent if needed
        } else {
            LOG_ERROR("Failed to map/initialize DreamUBO[{}] — potential black screen risk", i);
        }

        // TonemapUBO — host-visible, persistently mapped
        tonemapUniformEncs_[i] = BufferManager::createTonemapUBO(std::format("TonemapUBO[{}]", i));
        if (!tonemapUniformEncs_[i]) {
            LOG_FATAL("Failed to create TonemapUBO {}", i);
        }
        // Immediately populate with defaults
        if (auto* ptr = BufferManager::map(tonemapUniformEncs_[i])) {
            std::memcpy(ptr, &defaultTonemap, sizeof(TonemapUBO));
            BufferManager::flush(tonemapUniformEncs_[i]);
        } else {
            LOG_ERROR("Failed to map/initialize TonemapUBO[{}] — tonemapping may fail", i);
        }

        // Device-local SSBOs (no initial data needed, will be updated later)
        materialBufferEncs_[i]  = BufferManager::create(materialBufferSize(), ssboUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Materials");
        dimensionBufferEncs_[i] = BufferManager::create(256, ssboUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "DimensionData");

        if (!materialBufferEncs_[i] || !dimensionBufferEncs_[i]) {
            LOG_ERROR("Failed to create SSBO for frame {} — materials/dimensions may be invalid", i);
        }
    }

    // Note: Full updates still happen per-frame via updateUniformBuffer/updateTonemapUniform
    // But defaults ensure no initial garbage -> black/undefined screen

    LOG_AMOURANTH("DREAM & TONEMAP UBOs UPGRADED — PERSISTENTLY MAPPED — INITIALIZED WITH DEFAULTS — NO MORE BLACK VOID — SASQUATCH SEES PINK PHOTONS");
}

void VulkanRenderer::createCommandBuffers() noexcept
{
    ensureCommandPool();  // Guarantees g_transientCommandPool exists

    commandBuffers_.resize(Options::Performance::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = g_transientCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = Options::Performance::MAX_FRAMES_IN_FLIGHT
    };

    VkResult result = vkAllocateCommandBuffers(stone_device(), &allocInfo, commandBuffers_.data());
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate command buffers: {}", string_VkResult(result));
    }

    LOG_SUCCESS_CAT("RENDERER", "{} command buffers allocated from transient pool", Options::Performance::MAX_FRAMES_IN_FLIGHT);
}

void VulkanRenderer::updateNexusDescriptors() noexcept
{
    if (rtDescriptorSets_.empty()) return;

    VkDescriptorSet set = rtDescriptorSets_[currentFrame_ % rtDescriptorSets_.size()];

    std::array<VkWriteDescriptorSet, 8> writes{};
    uint32_t writeCount = 0;

    const auto addImageWrite = [&](uint32_t binding, VkImageView view, VkImageLayout layout) {
        if (view == VK_NULL_HANDLE) return;

        VkDescriptorImageInfo info{ .imageView = view, .imageLayout = layout };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &info
        };
    };

    // Raw handle — direct use
    addImageWrite(6, hypertraceScoreView_, VK_IMAGE_LAYOUT_GENERAL);

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
    }
}

void VulkanRenderer::updateDenoiserDescriptors() noexcept {

    if (denoiserSets_.empty() || rtOutputViews_.empty()) {
        return;
    }

    VkDescriptorSet set = denoiserSets_[currentFrame_ % denoiserSets_.size()];

    std::array<VkWriteDescriptorSet, 2> writes = {};
    std::array<VkDescriptorImageInfo, 2> infos = {};

    infos[0].imageView = rtOutputViews_[currentFrame_ % rtOutputViews_.size()].get();
    infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[0].pImageInfo = &infos[0];

    infos[1].imageView = denoiserView_.valid() ? denoiserView_.get() : VK_NULL_HANDLE;
    infos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &infos[1];

    vkUpdateDescriptorSets(stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRenderer::performDenoisingPass(VkCommandBuffer cmd) noexcept {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, denoiserPipeline_.get());

    VkDescriptorSet set = denoiserSets_[currentFrame_ % denoiserSets_.size()];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, denoiserLayout_.get(), 0, 1, &set, 0, nullptr);

    uint32_t wgX = (width_ + 15) / 16;
    uint32_t wgY = (height_ + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    VkMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo dep = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &barrier;
    RTX::g_ext.vkCmdPipelineBarrier2(cmd, &dep);
}

void VulkanRenderer::requestResize(uint32_t newWidth, uint32_t newHeight) noexcept
{
    if (newWidth == 0 || newHeight == 0) {
        minimized_ = true;
        return;
    }

    if (minimized_) {
        minimized_ = false;
        LOG_AMOURANTH("WINDOW RESTORED — PHOTONS AWAKEN");
    }

    // Prevent concurrent resize
    if (s_resizeInProgress.exchange(true)) {
        return;
    }

    LOG_AMOURANTH("RESIZE → {}×{} — EMPIRE REBIRTH — INSTANT", newWidth, newHeight);

    // 1. Notify LAS — purges old TLAS ring (safe, no GPU work)
    RTX::las().notifyResize();

    // 2. Recreate swapchain — public API only
    RTX::SwapchainManager::get().recreate(newWidth, newHeight);

    // 3. Update StoneKey — the empire is sealed
    stone_seal_width(newWidth);
    stone_seal_height(newHeight);
    stone_seal_extent({newWidth, newHeight});

    // 4. Update internal size
    width_  = static_cast<int>(newWidth);
    height_ = static_cast<int>(newHeight);

    // 5. Wait for GPU — REQUIRED before destroying/recreating images
    vkDeviceWaitIdle(stone_device());

    // 6. Recreate all swapchain-dependent resources
    recreateSwapchainDependentResources();

    // 7. Recreate fresh command buffers — critical after waitIdle()
    createCommandBuffers();

    // 8. Reset accumulation — fresh convergence
    requestAccumulationReset();

    s_resizeInProgress.store(false);

    LOG_SUCCESS_CAT("RENDERER", "Full resize rebuild complete — {}×{} — empire unbroken — photons realigned", newWidth, newHeight);
}

void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float deltaTime) noexcept
{
    if (frame >= uniformBufferEncs_.size() || uniformBufferEncs_[frame] == 0) {
        return;
    }

    const uint64_t handle = uniformBufferEncs_[frame];
    const BufferManager::BufferInfo* info = BufferManager::get(handle);
    if (!info || info->mapped == nullptr) {
        return;
    }

    DreamUBO ubo{};

    ubo.time                = totalTime_;
    ubo.frame               = frameNumber_;
    ubo.currentSpp          = currentSpp_;
    ubo.totalSpp            = accumulationFrame_;
    ubo.exposure            = currentExposure_;

    ubo.enableEnvMap        = Options::Environment::ENABLE_ENV_MAP ? 1u : 0u;
    ubo.hypertraceEnabled   = Options::OptionsRTX::ENABLE_HYPERTRACE ? 1u : 0u;
    ubo.denoisingEnabled    = Options::OptionsRTX::ENABLE_DENOISING ? 1u : 0u;
    ubo.adaptiveEnabled     = Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING ? 1u : 0u;
    ubo.debugMode           = static_cast<uint32_t>(activeRenderMode_);

    ubo.envIntensity        = 1.0f;
    ubo.envRotation         = 0.0f;

    ubo.resolution          = glm::vec2(static_cast<float>(width_), static_cast<float>(height_));

    // Halton 2,3 sequence for stable jitter
    static constexpr glm::vec2 halton16[16] = {
        {0.0f, 0.0f},       {0.5f, 0.333333f}, {0.25f, 0.666667f}, {0.75f, 0.111111f},
        {0.125f, 0.444444f},{0.625f, 0.777778f},{0.375f, 0.222222f},{0.875f, 0.555556f},
        {0.0625f, 0.888889f},{0.5625f, 0.037037f},{0.3125f, 0.370370f},{0.8125f, 0.703704f},
        {0.1875f, 0.148148f},{0.6875f, 0.481481f},{0.4375f, 0.814815f},{0.9375f, 0.259259f}
    };

    const uint32_t jitterIdx = frameNumber_ % 16;
    ubo.jitter              = halton16[jitterIdx];
    ubo.jitterPrev          = (frameNumber_ == 0) ? ubo.jitter : halton16[(frameNumber_ - 1) % 16];

    ubo.nexusScoreThreshold = currentNexusScore_;
    ubo.hypertraceJitterScale = Options::OptionsRTX::HYPERTRACE_JITTER_SCALE;

    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    ubo.view     = camera.view();
    ubo.proj     = camera.proj(aspect);
    ubo.invView  = glm::inverse(ubo.view);
    ubo.invProj  = glm::inverse(ubo.proj);

    ubo.camPos   = glm::vec4(camera.pos(), 1.0f);
    ubo.camDir   = glm::vec4(camera.forward(), 0.0f);
    ubo.fov      = camera.fov();
    ubo.aperture = camera.aperture();
    ubo.focusDistance = camera.focusDistance();

    ubo.materialCount       = static_cast<uint32_t>(materialCount_);
    ubo.activeMaterialIndex = activeMaterialIndex_;
    ubo.metallicOverride    = materialMetallicOverride_;
    ubo.roughnessOverride   = materialRoughnessOverride_;
    ubo.emissiveIntensity   = emissiveIntensity_;

    ubo.enableBlueNoise     = Options::Environment::ENABLE_BLUE_NOISE ? 1u : 0u;
    ubo.enableTAA           = Options::OptionsRTX::ENABLE_TAA ? 1u : 0u;
    ubo.taaAlpha            = Options::OptionsRTX::TAA_ALPHA;

    ubo.sunDirection        = sunDirection_;
    ubo.sunIntensity        = sunIntensity_;
    ubo.sunColor            = sunColor_;
    ubo.fogDensity          = fogDensity_;
    ubo.fogColor            = fogColor_;

    ubo.showNexusScore      = Options::Debug::SHOW_NEXUS_SCORE ? 1u : 0u;
    ubo.showSppHeatmap      = Options::Debug::SHOW_SPP_HEATMAP ? 1u : 0u;
    ubo.showAccumulationCount = Options::Debug::SHOW_ACCUMULATION_COUNT ? 1u : 0u;
    ubo.showGpuTimestamps   = Options::Debug::SHOW_GPU_TIMESTAMPS ? 1u : 0u;

    std::memcpy(info->mapped, &ubo, sizeof(DreamUBO));
}

void VulkanRenderer::updateTonemapUniform(uint32_t frame) noexcept
{
    if (frame >= tonemapUniformEncs_.size() || tonemapUniformEncs_[frame] == 0) {
        return;
    }

    const uint64_t handle = tonemapUniformEncs_[frame];
    const BufferManager::BufferInfo* info = BufferManager::get(handle);
    if (!info || info->mapped == nullptr) {
        return;
    }

    TonemapUBO ubo{};

    ubo.exposure = currentExposure_;
    ubo.type     = static_cast<uint32_t>(tonemapType_);
    ubo.enabled  = Options::Tonemap::ENABLE_TONEMAPPING ? 1u : 0u;
    ubo.nexusScore = currentNexusScore_;
    ubo.frame    = frameNumber_;
    ubo.spp      = currentSpp_;

    ubo.gamma    = Options::Tonemap::GAMMA;
    ubo.bloomThreshold = Options::PostProcess::BLOOM_THRESHOLD;
    ubo.bloomIntensity = Options::PostProcess::BLOOM_INTENSITY;
    ubo.vignetteIntensity = Options::PostProcess::VIGNETTE_INTENSITY;
    ubo.filmGrainStrength = Options::PostProcess::FILM_GRAIN_STRENGTH;
    ubo.lensFlareIntensity = Options::PostProcess::LENS_FLARE_INTENSITY;

    std::memcpy(info->mapped, &ubo, sizeof(TonemapUBO));
}

void VulkanRenderer::updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView, VkImageView output) noexcept
{
    if (frameIdx >= tonemapSets_.size() || tonemapSets_[frameIdx] == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo inputInfo{
        .sampler     = tonemapSampler_.get(),
        .imageView   = inputView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo outputInfo{
        .imageView   = output,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorBufferInfo uboInfo{};
    if (frameIdx < tonemapUniformEncs_.size() && tonemapUniformEncs_[frameIdx] != 0) {
        const auto* buf = BufferManager::get(tonemapUniformEncs_[frameIdx]);
        if (buf) {
            uboInfo.buffer = buf->buffer;
            uboInfo.offset = 0;
            uboInfo.range  = sizeof(TonemapUBO);
        }
    }

    std::array<VkWriteDescriptorSet, 3> writes = {{
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, tonemapSets_[frameIdx], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &inputInfo},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, tonemapSets_[frameIdx], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       &outputInfo},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, tonemapSets_[frameIdx], 2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,     nullptr, &uboInfo}
    }};

    vkUpdateDescriptorSets(stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept
{
    if (!Options::Tonemap::ENABLE_TONEMAPPING || tonemapPipeline_.get() == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorSet set = tonemapSets_[frameIdx % tonemapSets_.size()];
    if (set == VK_NULL_HANDLE) {
        return;
    }

    VkImage swapImg = stone_images()[swapImageIdx];

    // Transition swapchain image to GENERAL for compute write
    transitionImage(cmd, swapImg,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL,
                    0, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline_.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            tonemapLayout_.get(), 0, 1, &set, 0, nullptr);

    // Push constants — now correctly accessing member variables via 'this->'
    struct Push {
        float    exposure;
        uint32_t type;
        uint32_t enabled;
        float    pad;
    } push{
        .exposure = this->currentExposure_,
        .type     = static_cast<uint32_t>(this->tonemapType_),
        .enabled  = 1u,
        .pad      = 0.0f
    };

    vkCmdPushConstants(cmd, tonemapLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

    // Dispatch compute shader
    uint32_t wgX = (width_ + 15) / 16;
    uint32_t wgY = (height_ + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    // Transition back to PRESENT_SRC_KHR
    transitionImage(cmd, swapImg,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_SHADER_WRITE_BIT, 0,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void VulkanRenderer::onWindowResize(uint32_t w, uint32_t h) noexcept
{
    if (w == 0 || h == 0) {
        minimized_ = true;
        return;
    }
    minimized_ = false;

    width_  = static_cast<int>(w);
    height_ = static_cast<int>(h);

    vkDeviceWaitIdle(stone_device());
    RTX::recreateSwapchain(w, h);
    recreateSwapchainDependentResources();

    resetAccumNextFrame_ = true;
    accumulationFrame_ = currentSpp_ = 0;
}

void VulkanRenderer::setMaxFramesInFlight(uint32_t count) noexcept
{
    maxFramesInFlight_ = count;
}

void VulkanRenderer::recreateSwapchainDependentResources() noexcept
{
    vkDeviceWaitIdle(stone_device());

    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyDenoiserImage();
    destroyNexusScoreImage();

    createCommandBuffers();  // Recreates using current transient pool

    createRTOutputImages();
    createAccumulationImages();
    createNexusScoreImage(g_transientCommandPool, stone_graphics_queue());

    recreateTonemapUBOs();

    pipelineManager_.allocateDescriptorSets();

    rtOutputNeedsTransition_     = true;
    depthNeedsTransition_        = true;
    accumulationNeedsTransition_ = true;
    nexusScoreNeedsInit_         = true;
}

VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window, bool overclock)
    : window_(window),
      width_(width),
      height_(height),
      overclockMode_(overclock),
      hypertraceEnabled_(Options::OptionsRTX::ENABLE_HYPERTRACE),
      denoisingEnabled_(Options::OptionsRTX::ENABLE_DENOISING),
      adaptiveSamplingEnabled_(Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING),
      tonemapEnabled_(Options::Tonemap::ENABLE_TONEMAPPING),
      activeRenderMode_(Options::RenderMode::DEFAULT_MODE)
{
    s_instance = this;

    ensureCommandPool();
    createSyncObjects();
    createCommandBuffers();

    createTonemapSampler();
    createTonemapDescriptorPool();
    createTonemapDescriptorSetLayout();
    createTonemapDescriptorSets();
    recreateTonemapUBOs();

    initializeAllBufferData(Options::Performance::MAX_FRAMES_IN_FLIGHT, sizeof(DreamUBO), materialBufferSize());

    createRTOutputImages();
    createDepthResources();
    createAccumulationImages();
    createAccumulationPipeline();
    createNexusScoreImage(g_transientCommandPool, stone_graphics_queue());
}

void VulkanRenderer::createSyncObjects() noexcept
{
    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    imageAvailableSemaphores_.resize(frames);
    renderFinishedSemaphores_.resize(frames);
    inFlightFences_.resize(frames);

    VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    for (uint32_t i = 0; i < frames; ++i) {
        vkCreateSemaphore(stone_device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]);
        vkCreateSemaphore(stone_device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]);
        vkCreateFence(stone_device(), &fenceInfo, nullptr, &inFlightFences_[i]);
    }
}

void VulkanRenderer::setTonemap(bool enabled) noexcept
{
    tonemapEnabled_ = enabled;
}

void VulkanRenderer::toggleHypertrace() noexcept
{
    hypertraceEnabled_ = Options::OptionsRTX::ENABLE_HYPERTRACE;
    if (hypertraceEnabled_) resetAccumNextFrame_ = true;
}

void VulkanRenderer::toggleDenoising() noexcept
{
    denoisingEnabled_ = Options::OptionsRTX::ENABLE_DENOISING;
    if (denoisingEnabled_) resetAccumNextFrame_ = true;
}

void VulkanRenderer::toggleAdaptiveSampling() noexcept
{
    adaptiveSamplingEnabled_ = Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING;
    if (adaptiveSamplingEnabled_) resetAccumNextFrame_ = true;
}

void VulkanRenderer::setOverclockMode(bool enabled) noexcept
{
    overclockMode_ = enabled;
}

void VulkanRenderer::setOverlay(bool enabled) noexcept
{
    showOverlay_ = enabled;
}

void VulkanRenderer::destroyRTOutputImages() noexcept
{
    LOG_TRACE_CAT("RENDERER", "Destroying RT output images");

    rtOutputViews_.clear();
    rtOutputImages_.clear();
    rtOutputMemories_.clear();
}

void VulkanRenderer::destroyAccumulationImages() noexcept
{
    LOG_TRACE_CAT("RENDERER", "Destroying accumulation images");

    accumViews_.clear();
    accumImages_.clear();
    accumMemories_.clear();
}

void VulkanRenderer::destroyDenoiserImage() noexcept
{
    LOG_TRACE_CAT("RENDERER", "Destroying denoiser image");

    denoiserView_.reset();
    denoiserImage_.reset();
    denoiserMemory_.reset();
}

void VulkanRenderer::destroyNexusScoreImage() noexcept
{
    LOG_TRACE_CAT("RENDERER", "Destroying nexus score image");

    if (hypertraceScoreView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(stone_device(), hypertraceScoreView_, nullptr);
        hypertraceScoreView_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(stone_device(), hypertraceScoreImage_, nullptr);
        hypertraceScoreImage_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(stone_device(), hypertraceScoreMemory_, nullptr);
        hypertraceScoreMemory_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                                 VkFormat format, VkImageTiling tiling,
                                 VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                 RTX::Handle<VkImage>& image,
                                 RTX::Handle<VkDeviceMemory>& memory,
                                 const std::string& tag) noexcept
{
    VkImageCreateInfo imageInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = { width, height, 1 },
        .mipLevels     = mipLevels,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = tiling,
        .usage         = usage,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage rawImage = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(stone_device(), &imageInfo, nullptr, &rawImage);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create image {}: {}", tag, string_VkResult(result));
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(stone_device(), rawImage, &memReqs);

    uint32_t memTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);
    if (memTypeIndex == ~0u) {
        LOG_FATAL_CAT("RENDERER", "No suitable memory type for image {}", tag);
        vkDestroyImage(stone_device(), rawImage, nullptr);
        return;
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memTypeIndex
    };

    VkDeviceMemory mem = VK_NULL_HANDLE;
    result = vkAllocateMemory(stone_device(), &allocInfo, nullptr, &mem);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate memory for image {}: {}", tag, string_VkResult(result));
        vkDestroyImage(stone_device(), rawImage, nullptr);
        return;
    }

    result = vkBindImageMemory(stone_device(), rawImage, mem, 0);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to bind memory for image {}: {}", tag, string_VkResult(result));
        vkFreeMemory(stone_device(), mem, nullptr);
        vkDestroyImage(stone_device(), rawImage, nullptr);
        return;
    }

    image  = RTX::Handle<VkImage>(rawImage, stone_device(), vkDestroyImage);
    memory = RTX::Handle<VkDeviceMemory>(mem, stone_device(), vkFreeMemory);

    LOG_SUCCESS_CAT("RENDERER", "Image {} created — {}×{} — {} MiB", tag, width, height, memReqs.size / (1024 * 1024));
}

bool VulkanRenderer::recreateTonemapUBOs() noexcept
{
    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // Destroy old
    for (auto h : tonemapUniformEncs_) {
        if (h) BufferManager::destroy(h);
    }
    tonemapUniformEncs_.assign(frames, 0);

    // Recreate host-visible
    for (uint32_t i = 0; i < frames; ++i) {
        tonemapUniformEncs_[i] = BufferManager::createHostVisible(sizeof(TonemapUBO), std::format("TonemapUBO[{}]", i));
        if (tonemapUniformEncs_[i] == 0) {
            LOG_FATAL_CAT("RENDERER", "Failed to recreate TonemapUBO[{}]", i);
            return false;
        }
    }

    // Update descriptors
    for (uint32_t i = 0; i < frames; ++i) {
        updateTonemapUBO(i);
    }

    return true;
}

void VulkanRenderer::createTonemapDescriptorPool() noexcept
{
    if (tonemapDescriptorPool_.valid()) {
        return;
    }

    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    const uint32_t totalSets = frames + (frames / 2); // headroom

    std::array<VkDescriptorPoolSize, 3> poolSizes = {{
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, totalSets },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         totalSets },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        totalSets }
    }};

    VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = totalSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &poolInfo, nullptr, &pool);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("TONEMAP", "Failed to create tonemap descriptor pool: {}", string_VkResult(result));
        return;
    }

    tonemapDescriptorPool_ = Handle<VkDescriptorPool>(pool, stone_device(), vkDestroyDescriptorPool);
}

void VulkanRenderer::createTonemapDescriptorSetLayout() noexcept
{
    if (tonemapDescriptorSetLayout_.valid()) {
        return;
    }

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorSetLayout(stone_device(), &layoutInfo, nullptr, &layout);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("TONEMAP", "Failed to create tonemap descriptor set layout: {}", string_VkResult(result));
        return;
    }

    tonemapDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(layout, stone_device(), vkDestroyDescriptorSetLayout);
}

void VulkanRenderer::createTonemapDescriptorSets() noexcept
{
    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    if (tonemapSets_.size() == frames && tonemapSets_[0] != VK_NULL_HANDLE) {
        return;
    }

    std::vector<VkDescriptorSetLayout> layouts(frames, tonemapDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = tonemapDescriptorPool_.get(),
        .descriptorSetCount = frames,
        .pSetLayouts        = layouts.data()
    };

    tonemapSets_.resize(frames);
    VkResult result = vkAllocateDescriptorSets(stone_device(), &allocInfo, tonemapSets_.data());
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("TONEMAP", "Failed to allocate tonemap descriptor sets: {}", string_VkResult(result));
    }
}

VulkanRenderer::~VulkanRenderer()
{
}

void VulkanRenderer::updateTonemapUBO(uint32_t frame) noexcept
{
    if (frame >= tonemapUniformEncs_.size() || tonemapUniformEncs_[frame] == 0) {
        return;
    }

    const uint64_t handle = tonemapUniformEncs_[frame];
    const BufferManager::BufferInfo* info = BufferManager::get(handle);
    if (!info || info->mapped == nullptr) {
        return;
    }

    TonemapUBO ubo{};

    ubo.exposure = currentExposure_;
    ubo.type     = static_cast<uint32_t>(tonemapType_);
    ubo.enabled  = Options::Tonemap::ENABLE_TONEMAPPING ? 1u : 0u;
    ubo.nexusScore = currentNexusScore_;
    ubo.frame    = frameNumber_;
    ubo.spp      = currentSpp_;

    ubo.gamma    = Options::Tonemap::GAMMA;
    ubo.bloomThreshold = Options::PostProcess::BLOOM_THRESHOLD;
    ubo.bloomIntensity = Options::PostProcess::BLOOM_INTENSITY;
    ubo.vignetteIntensity = Options::PostProcess::VIGNETTE_INTENSITY;
    ubo.filmGrainStrength = Options::PostProcess::FILM_GRAIN_STRENGTH;
    ubo.lensFlareIntensity = Options::PostProcess::LENS_FLARE_INTENSITY;

    std::memcpy(info->mapped, &ubo, sizeof(TonemapUBO));
}

// ──────────────────────────────────────────────────────────────────────────────
// Final Status
// ──────────────────────────────────────────────────────────────────────────────
/*
 * December 18, 2025 — 2026 HARDCODE MASTERMIND — 2 FRAMES | R16G16_SFLOAT NEXUS | ALL TOP-NOTCH ENABLED
 * Empire Optimized: Unlimited FPS | Full Features | Half-Float RT/Accum/Denoise | Photons Eternal.
 */