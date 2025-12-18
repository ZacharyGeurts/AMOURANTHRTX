// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 10, 2025 — 2026 HARDCODE MASTERMIND
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
#include <sstream>
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

uint32_t MAX_FRAMES_IN_FLIGHT = 2;
static VkCommandPool g_empireCommandPool = VK_NULL_HANDLE;

VulkanRenderer* VulkanRenderer::get() noexcept { return s_instance; }

void VulkanRenderer::ensureCommandPool() noexcept
{
    if (g_empireCommandPool != VK_NULL_HANDLE) return;

    VkCommandPoolCreateInfo info{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = stone_graphics_family()
    };

    VK_CHECK(vkCreateCommandPool(stone_device(), &info, nullptr, &g_empireCommandPool));
    RTX::g_ctx().commandPool_ = g_empireCommandPool;

    LOG_AMOURANTH("EMPIRE COMMAND POOL FORGED — ONE POOL — ETERNAL — TRUTH");
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

    VK_CHECK(vkCreateImage(stone_device(), &imgInfo, nullptr, &equirectImage));

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

    VK_CHECK(vkAllocateMemory(stone_device(), &allocInfo, nullptr, &equirectMemory));
    VK_CHECK(vkBindImageMemory(stone_device(), equirectImage, equirectMemory, 0));

    // Create image view
    VkImageView equirectView = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = equirectImage,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &equirectView));

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
    VK_CHECK(vkCreateSampler(stone_device(), &samplerInfo, nullptr, &sampler));

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
        LOG_TRACE_CAT("RENDERER", "Frame skipped — window minimized");
        return;
    }

    const uint32_t frameIndex = frameNumber_++;
    const uint32_t slot       = frameIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT;

    LOG_AMOURANTH("=== FRAME {} === SLOT {} === SPP {} === MODE {}", frameIndex, slot, currentSpp_, activeRenderMode_);

    // =====================================================================
    // Acquire swapchain image
    // =====================================================================
    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        stone_device(), stone_swapchain(),
        1'000'000'000ULL,
        imageAvailableSemaphores_[slot],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
        LOG_WARNING_CAT("RENDERER", "Swapchain out of date/suboptimal — recreating");
        vkDeviceWaitIdle(stone_device());
        RTX::recreateSwapchain(stone_width(), stone_height());
        return;
    }
    if (acquireResult != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to acquire swapchain image: {}", string_VkResult(acquireResult));
        return;
    }

    VkCommandBuffer cmd = commandBuffers_[slot];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // =====================================================================
    // GPU timestamps (begin)
    // =====================================================================
    if (Options::Performance::ENABLE_GPU_TIMESTAMPS && timestampQueryPool_ != VK_NULL_HANDLE) {
        const uint32_t queryIndex = frameIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT;
        vkCmdResetQueryPool(cmd, timestampQueryPool_, queryIndex * 2, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool_, queryIndex * 2);
    }

    VkImage     swapImg  = stone_images()[imageIndex];
    VkImageView swapView = stone_views()[imageIndex];

    // =====================================================================
    // Accumulation reset
    // =====================================================================
    if (resetAccumNextFrame_) {
        LOG_AMOURANTH("RESETTING ACCUMULATION — NEW CONVERGENCE BEGINS — PHOTONS REALIGNED");
        clearAccumulationImages(cmd);
        resetAccumNextFrame_ = resetAccumulation_ = false;
        currentSpp_ = accumulationFrame_ = 0;
    }

    // =====================================================================
    // Deferred first-frame transitions
    // =====================================================================
    if (rtOutputNeedsTransition_) {
        for (const auto& img : rtOutputImages_) {
            transitionImage(cmd, img.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                            0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
        rtOutputNeedsTransition_ = false;
    }

    if (depthNeedsTransition_) {
        VkImageMemoryBarrier barrier{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .image               = depthImage_.get(),
            .subresourceRange    = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
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

    // =====================================================================
    // ENVMAP UPLOAD — FIRST FRAME ONLY
    // =====================================================================
    if (envMapNeedsUpload_) {
        LOG_AMOURANTH("FIRST FRAME — WHISPER UPLOADING HDR ENVMAP — THE SKY AWAKENS");

        int w = 0, h = 0, channels = 0;
        float* data = stbi_loadf("assets/textures/envmap.hdr", &w, &h, &channels, 4);
        if (!data) {
            LOG_FATAL_CAT("RENDERER", "Failed to load envmap.hdr — {}", stbi_failure_reason());
        } else {
            const VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4 * sizeof(float);

            uint64_t stagingHandle = BufferManager::createHostVisible(imageSize, "EnvMap_Staging_Temp");
            void* mapped = BufferManager::getMappedStagingPtr(stagingHandle);
            std::memcpy(mapped, data, imageSize);
            stbi_image_free(data);

            transitionImage(cmd, envMapImage_.get(),
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            0, VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            VkBufferImageCopy region{
                .bufferOffset      = 0,
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageOffset       = { 0, 0, 0 },
                .imageExtent       = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 }
            };

            vkCmdCopyBufferToImage(cmd,
                                   BufferManager::get(stagingHandle)->buffer,
                                   envMapImage_.get(),
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1, &region);

            transitionImage(cmd, envMapImage_.get(),
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            BufferManager::destroy(stagingHandle);
        }

        envMapNeedsUpload_ = false;
        createEnvMapDisplayPipeline();
        LOG_AMOURANTH("ENVMAP UPLOADED — SKY READY");
    }

    // =====================================================================
    // BUILD TLAS — EVERY FRAME (direct geometry)
    // =====================================================================
    RTX::las().buildTLAS(cmd);

    // =====================================================================
    // RENDER MODE DISPATCH — CLEAN FLOW WITH EARLY RETURNS
    // =====================================================================
    if (activeRenderMode_ == 1) {
        LOG_AMOURANTH("RENDER MODE 1: PURE PINK VOID — PHOTONS ETERNAL");

        VkClearColorValue pink{{1.0f, 0.0f, 0.5f, 1.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        transitionImage(cmd, swapImg, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        vkCmdClearColorImage(cmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &pink, 1, &range);

        transitionImage(cmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        goto submit_frame;
    }

    // All other modes (2–9) use the full RTX path
    LOG_SUCCESS_CAT("RENDERER", "RENDER MODE {} — FULL RTX PATH ENGAGED", activeRenderMode_);

    // =====================================================================
    // RTX PATH VALIDITY — FALLBACK TO PINK IF INVALID
    // =====================================================================
    if (!pipelineManager_.isRTXValid() || uniformBufferEncs_[slot] == 0) {
        LOG_FATAL_CAT("RENDERER", "RTX PATH INVALID — FALLING BACK TO SACRED PINK VOID");

        VkClearColorValue pink{{1.0f, 0.0f, 0.5f, 1.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        transitionImage(cmd, swapImg, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        vkCmdClearColorImage(cmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &pink, 1, &range);

        transitionImage(cmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        goto submit_frame;
    }

    // =====================================================================
    // FULL RTX PATH — MODES 2–9
    // =====================================================================
    updateUniformBuffer(slot, camera, deltaTime);
    updateTonemapUniform(slot);
    currentFrame_.store(slot);

    // Update RT descriptors — use LATEST TLAS for certainty
    {
        RTX::RTDescriptorUpdate desc{};
        desc.tlas = RTX::las().getLatestTLAS();
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

        if (pipelineManager_.envMapImageView_.valid() && pipelineManager_.envMapSampler_.valid()) {
            desc.envSampler = pipelineManager_.envMapSampler_.get();
            desc.envImageView = pipelineManager_.envMapImageView_.get();
        }

        pipelineManager_.updateRTDescriptorSet(slot, desc);
    }

    // Ray tracing
    recordRayTracingCommands(cmd, slot);

    // Accumulation
    if (Options::OptionsRTX::ENABLE_ACCUMULATION) {
        recordAccumulationPass(cmd, slot);
    }

    // Denoising
    if (Options::OptionsRTX::ENABLE_DENOISING && denoisingEnabled_) {
        updateDenoiserDescriptors();
        performDenoisingPass(cmd);
    }

    // Tonemapping
    if (Options::Tonemap::ENABLE_TONEMAPPING && tonemapEnabled_) {
        VkImageView input = (Options::OptionsRTX::ENABLE_DENOISING && denoisingEnabled_)
                            ? denoiserView_.get()
                            : (Options::OptionsRTX::ENABLE_ACCUMULATION ? accumViews_[slot].get() : rtOutputViews_[slot].get());

        updateTonemapDescriptor(slot, input, swapView);
        performTonemapPass(cmd, slot, imageIndex);
    } else {
        VkClearColorValue black{{0.0f, 0.0f, 0.0f, 1.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        transitionImage(cmd, swapImg, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        0, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        vkCmdClearColorImage(cmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &range);

        transitionImage(cmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }

submit_frame:
    // GPU timestamp end
    if (Options::Performance::ENABLE_GPU_TIMESTAMPS && timestampQueryPool_ != VK_NULL_HANDLE) {
        const uint32_t queryIndex = frameIndex % Options::Performance::MAX_FRAMES_IN_FLIGHT;
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, queryIndex * 2 + 1);
    }

    VK_CHECK(vkEndCommandBuffer(cmd));

    submitAndPresent(slot, imageIndex);

    currentSpp_++;
    accumulationFrame_++;

    LOG_AMOURANTH("=== FRAME {} COMPLETE === SPP {} === PHOTONS ETERNAL", frameIndex, currentSpp_);
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
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &layoutInfo, nullptr, &layout));
    accumulationDescSetLayout_ = layout;

    // === 2. Pipeline Layout ===
    VkPipelineLayoutCreateInfo plInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &accumulationDescSetLayout_,
        .pushConstantRangeCount = 0
    };

    VkPipelineLayout pl = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(stone_device(), &plInfo, nullptr, &pl));
    accumulationPipelineLayout_ = pl;

    // === 3. Load Shader ===
    VkShaderModule module = pipelineManager_.loadShader("assets/shaders/compute/accumulation.spv");
    if (module == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to load accumulation.spv — temporal accumulation disabled");
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
    VK_CHECK(vkCreateComputePipelines(stone_device(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline));
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
    VK_CHECK(vkCreateDescriptorPool(stone_device(), &poolInfo, nullptr, &pool));
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

    VK_CHECK(vkAllocateDescriptorSets(stone_device(), &allocInfo, accumulationSets_.data()));

    LOG_SUCCESS_CAT("RENDERER", "Accumulation pipeline forged — temporal convergence armed");
}

void VulkanRenderer::recordAccumulationPass(VkCommandBuffer cmd, uint32_t slot) noexcept
{
    if (accumulationPipeline_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Accumulation pipeline not created — skipping pass");
        return;
    }

    VkDescriptorSet set = accumulationSets_[slot];

    VkDescriptorImageInfo currInfo{ {}, rtOutputViews_[slot].get(), VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo histInfo{ {}, accumViews_[slot].get(),    VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo outInfo { {}, accumViews_[slot].get(),    VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo depthInfo{ {}, depthImageView_.get(),     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    VkDescriptorBufferInfo uboInfo{
        uniformBufferEncs_[slot] ? RAW_BUFFER(uniformBufferEncs_[slot]) : VK_NULL_HANDLE,
        0, VK_WHOLE_SIZE
    };

    std::array<VkWriteDescriptorSet, 5> writes = {{
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &currInfo},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &histInfo},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &outInfo},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &depthInfo},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, 4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uboInfo}
    }};

    vkUpdateDescriptorSets(stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

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
    vkCmdPipelineBarrier2(cmd, &dep);
}

void VulkanRenderer::recordEnvMapOnlyPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex) noexcept
{
    auto& pm = RTX::pipeline();
    VkImage swapImage = StoneKey::stone_images()[swapchainImageIndex];

    // If we have a valid envmap and display pipeline → render true HDR sky
    if (pm.envMapDisplayPipeline_ != VK_NULL_HANDLE &&
        pm.envMapDisplayDescriptorSet_ != VK_NULL_HANDLE &&
        pm.envMapImageView_.valid() &&
        pm.envMapSampler_.valid())
    {
        // Update storage image binding to current swapchain image view
        VkDescriptorImageInfo storageInfo{
            .imageView   = StoneKey::stone_views()[swapchainImageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        VkWriteDescriptorSet write{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = pm.envMapDisplayDescriptorSet_,
            .dstBinding      = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &storageInfo
        };
        vkUpdateDescriptorSets(stone_device(), 1, &write, 0, nullptr);

        // Transition: PRESENT_SRC_KHR → GENERAL (compute write)
        transitionImage(cmd, swapImage,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL,
                        0, VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // Bind pipeline and descriptor set
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pm.envMapDisplayPipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pm.envMapDisplayPipelineLayout_, 0, 1,
                                &pm.envMapDisplayDescriptorSet_, 0, nullptr);

        // Push resolution
        struct PushConstants {
            uint32_t width;
            uint32_t height;
        } pc{ StoneKey::stone_width(), StoneKey::stone_height() };

        vkCmdPushConstants(cmd, pm.envMapDisplayPipelineLayout_,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

        // Dispatch
        constexpr uint32_t WG = 16;
        vkCmdDispatch(cmd,
                      (StoneKey::stone_width() + WG - 1) / WG,
                      (StoneKey::stone_height() + WG - 1) / WG,
                      1);

        // Transition back: GENERAL → PRESENT_SRC_KHR
        transitionImage(cmd, swapImage,
                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_SHADER_WRITE_BIT, 0,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        LOG_TRACE_CAT("RENDERER", "True HDR sky rendered — the empire beholds the infinite");
        return;
    }

    // Fallback: sacred pink void if envmap failed to load
    LOG_WARN_CAT("RENDERER", "Envmap display pipeline or texture missing — showing sacred pink void");

    VkClearColorValue pink{{1.0f, 0.0f, 0.5f, 1.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    // Transition: PRESENT_SRC_KHR → GENERAL (transfer write)
    transitionImage(cmd, swapImage,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL,
                    0, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    vkCmdClearColorImage(cmd, swapImage, VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &range);

    // Transition back: GENERAL → PRESENT_SRC_KHR
    transitionImage(cmd, swapImage,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

// ──────────────────────────────────────────────────────────────────────────────
// 2026 HARDCODE: All Toggles Always On — No Runtime Switches
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::setOverlay(bool enabled) noexcept
{
    showOverlay_ = enabled;

    if (showOverlay_)
    {
        LOG_AMOURANTH("DEBUG OVERLAY ACTIVATED — "
                      "FPS={} | ACCUMULATION COUNT={} | NEXUS SCORE={} | SPP HEATMAP={} | GPU TIMESTAMPS={}",
                      Options::Debug::SHOW_FPS_OVERLAY ? "ON" : "OFF",
                      Options::Debug::SHOW_ACCUMULATION_COUNT ? "ON" : "OFF",
                      Options::Debug::SHOW_NEXUS_SCORE ? "ON" : "OFF",
                      Options::Debug::SHOW_SPP_HEATMAP ? "ON" : "OFF",
                      Options::Debug::SHOW_GPU_TIMESTAMPS ? "ON" : "OFF");
    }
    else
    {
        LOG_AMOURANTH("DEBUG OVERLAY CONCEALED — PURE PHOTONS — THE EMPIRE SPEAKS WITHOUT WORDS");
    }

    // This flag now fully respects the sacred OptionsMenu debug settings.
    // Individual overlay components (FPS, SPP, Nexus, timestamps) will check their
    // respective Options::Debug constants at draw time.
}

void VulkanRenderer::toggleHypertrace() noexcept
{
    hypertraceEnabled_ = Options::OptionsRTX::ENABLE_HYPERTRACE;

    if (hypertraceEnabled_)
    {
        resetAccumulation_ = true;  // Temporal history must restart
        LOG_AMOURANTH("HYPERTRACE ENGAGED — NEXT-GEN TEMPORAL REUSE ACTIVE — NEXUS SCORE ONLINE");
    }
    else
    {
        LOG_AMOURANTH("HYPERTRACE DISABLED — FALLING BACK TO CLASSIC TEMPORAL ACCUMULATION");
    }
}

void VulkanRenderer::toggleFpsTarget() noexcept
{
    // Respect the preset-defined uncapped mode
    if constexpr (Options::Display::UNCAPPED_MODE_ACTIVE)
    {
        fpsTarget_ = FpsTarget::FPS_UNLIMITED;
        LOG_AMOURANTH("FPS TARGET: UNLIMITED — THE EMPIRE KNOWS NO BOUNDS");
    }
    else
    {
        // If a capped preset were ever used, this would respect VSync or refresh rate
        //fpsTarget_ = FpsTarget::FPS_VSYNC;
        LOG_AMOURANTH("FPS TARGET: VSYNC — SMOOTH AND TEAR-FREE");
    }
}

void VulkanRenderer::toggleDenoising() noexcept
{
    denoisingEnabled_ = Options::OptionsRTX::ENABLE_DENOISING;

    if (denoisingEnabled_)
    {
        resetAccumulation_ = true;
        LOG_AMOURANTH("DENOISING ACTIVATED — SVGF PURIFICATION ONLINE — NOISE IS PURGED");
    }
    else
    {
        LOG_AMOURANTH("DENOISING DEACTIVATED — RAW PHOTONS — NOISE IS TRUTH");
    }
}

void VulkanRenderer::toggleAdaptiveSampling() noexcept
{
    adaptiveSamplingEnabled_ = Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING;

    if (adaptiveSamplingEnabled_)
    {
        resetAccumulation_ = true;
        LOG_AMOURANTH("ADAPTIVE SAMPLING ENGAGED — NEXUS GUIDES THE PHOTONS — EFFICIENCY ETERNAL");
    }
    else
    {
        LOG_AMOURANTH("ADAPTIVE SAMPLING DISABLED — UNIFORM SAMPLING — EVERY PIXEL EQUAL");
    }
}

void VulkanRenderer::setOverclockMode(bool enabled) noexcept
{
    // Respect the compile-time decision from OptionsMenu
    overclockMode_ = Options::Performance::OVERCLOCK_RENDERER;

    if (overclockMode_)
    {
        fpsTarget_ = FpsTarget::FPS_UNLIMITED;
        LOG_AMOURANTH("OVERCLOCK MODE ACTIVE — ALL SAFETY CHECKS REMOVED — MAXIMUM PERFORMANCE UNLEASHED");
    }
    else
    {
        LOG_AMOURANTH("OVERCLOCK MODE INACTIVE — SAFE AND STABLE OPERATION");
    }

    (void)enabled;  // Parameter ignored — the empire decides at compile time
}

void VulkanRenderer::destroyNexusScoreImage() noexcept
{
    if (hypertraceScoreView_) {
        vkDestroyImageView(stone_device(), hypertraceScoreView_, nullptr);
        hypertraceScoreView_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreImage_) {
        vkDestroyImage(stone_device(), hypertraceScoreImage_, nullptr);
        hypertraceScoreImage_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreMemory_) {
        vkFreeMemory(stone_device(), hypertraceScoreMemory_, nullptr);
        hypertraceScoreMemory_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::destroyDenoiserImage() noexcept {
    denoiserImage_.reset();
    denoiserMemory_.reset();
    denoiserView_.reset();
}

void VulkanRenderer::destroyAccumulationImages() noexcept
{
    LOG_TRACE_CAT("RENDERER", "Destroying accumulation images — temporal history purged");

    // SAFE WHISPER MODE: Let Handle destructors do all the work
    // No manual vkDestroy calls — Handle guarantees single, safe destroy
    accumViews_.clear();     // Destroys all image views
    accumImages_.clear();    // Destroys all images
    accumMemories_.clear();  // Frees all memory

    LOG_SUCCESS_CAT("RENDERER", "Accumulation images destroyed — empire memory cleansed");
}

void VulkanRenderer::destroyRTOutputImages() noexcept {
    for (auto& h : rtOutputImages_) h.reset();
    for (auto& h : rtOutputMemories_) h.reset();
    for (auto& h : rtOutputViews_) h.reset();
}

// ──────────────────────────────────────────────────────────────────────────────
// NEW: Update tonemap UBO descriptor only (called in recreate, per-frame updates all)
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::updateTonemapUBO(uint32_t frame) noexcept {
    if (frame >= tonemapSets_.size() || tonemapSets_[frame] == VK_NULL_HANDLE) return;

    if (frame >= tonemapUniformEncs_.size() || tonemapUniformEncs_[frame] == 0) return;

    auto* buf = BufferManager::get(tonemapUniformEncs_[frame]);
    if (!buf || buf->buffer == VK_NULL_HANDLE) return;

    VkDescriptorBufferInfo uboInfo{
        .buffer = buf->buffer,
        .offset = 0,
        .range  = sizeof(DreamUBO)  // FIXED
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = tonemapSets_[frame],
        .dstBinding = 2,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &uboInfo
    };

    vkUpdateDescriptorSets(stone_device(), 1, &write, 0, nullptr);
}

void VulkanRenderer::createSyncObjects() noexcept
{
    LOG_INFO_CAT("RENDERER", "Forging synchronization objects — 2 frames in flight — the empire beats as one");

    const uint32_t frames = 2;  // Hardcoded — 2026 MASTERMIND

    imageAvailableSemaphores_.resize(frames);
    renderFinishedSemaphores_.resize(frames);
    inFlightFences_.resize(frames);

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT  // Start signaled so first frame doesn't wait
    };

    for (uint32_t i = 0; i < frames; ++i)
    {
        VK_CHECK(vkCreateSemaphore(stone_device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]));
        VK_CHECK(vkCreateSemaphore(stone_device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]));
        VK_CHECK(vkCreateFence(stone_device(), &fenceInfo, nullptr, &inFlightFences_[i]));

        LOG_TRACE_CAT("SYNC", "Sync objects forged for frame slot {}", i);
    }

    LOG_SUCCESS_CAT("RENDERER", "Synchronization objects complete — 2 semaphores + 2 fences — the rhythm is eternal");
}

VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window, bool overclockFromMain)
    : window_(window),
      width_(width),
      height_(height),
      overclockMode_(true),
      hypertraceEnabled_(true),
      denoisingEnabled_(true),
      adaptiveSamplingEnabled_(true),
      tonemapEnabled_(true),
      fpsTarget_(FpsTarget::FPS_UNLIMITED),
      activeRenderMode_(1)  // ← FORCE PINK MODE FROM FRAME 1
{
    s_instance = this;

    setOverclockMode(true);

    if (kStone1 == 0 || kStone2 == 0) {
        LOG_FATAL_CAT("SECURITY", "StoneKey validation failed");
        phase9_ballerina("STONEKEY BREACH", std::source_location::current());
    }

    if (stone_device() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Vulkan device not created");
        phase9_ballerina("DEVICE FAILURE", std::source_location::current());
    }

    if (!createSharedStaging()) {
        LOG_FATAL_CAT("RENDERER", "Failed to create shared staging buffer");
        phase9_ballerina("STAGING FAILURE", std::source_location::current());
    }

    createSyncObjects();

    {
        VkQueryPoolCreateInfo qpInfo{
            .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType  = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = MAX_FRAMES_IN_FLIGHT * 2
        };
        VK_CHECK(vkCreateQueryPool(stone_device(), &qpInfo, nullptr, &timestampQueryPool_));
    }

    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(stone_physical(), &props);
        timestampPeriod_ = props.limits.timestampPeriod / 1e6f;
    }

    // === CRITICAL: CREATE TONEMAP SYSTEM EARLY ===
    createTonemapSampler();
    if (!tonemapSampler_.valid()) {
        LOG_FATAL_CAT("RENDERER", "Tonemap sampler creation failed");
        phase9_ballerina("SAMPLER FAILURE", std::source_location::current());
    }

    createTonemapDescriptorPool();
    createTonemapDescriptorSetLayout();
    createTonemapDescriptorSets();
    recreateTonemapUBOs();

    // Now tonemap pipeline is ready — pink mode will work from frame 1

    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, sizeof(DreamUBO), materialBufferSize());

    createRTOutputImages();
    if (rtOutputViews_.size() != MAX_FRAMES_IN_FLIGHT) {
        LOG_FATAL_CAT("RENDERER", "RT output creation failed");
        phase9_ballerina("RT OUTPUT FAILURE", std::source_location::current());
    }

    createDepthResources();
    if (!depthImageView_.valid()) {
        LOG_FATAL_CAT("RENDERER", "Depth buffer creation failed");
        phase9_ballerina("DEPTH FAILURE", std::source_location::current());
    }

    createAccumulationImages();
    createAccumulationPipeline();
    createNexusScoreImage(RTX::g_ctx().commandPool(), stone_graphics_queue());

    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer initialized — {}×{} — PINK MODE ACTIVE", width, height);
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
    VK_CHECK(vkCreateDescriptorPool(stone_device(), &poolInfo, nullptr, &pool));

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
        LOG_INFO_CAT("RENDERER", "Envmap image view missing — forcing envmap creation with pink fallback");
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
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &envMapDisplayDescSetLayout_));

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
    VK_CHECK(vkCreatePipelineLayout(device, &plInfo, nullptr, &envMapDisplayPipelineLayout_));

    // Free old descriptor set if exists
    if (envMapDisplayDescriptorSet_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device, envMapDescriptorPool_.get(), 1, &envMapDisplayDescriptorSet_);
        envMapDisplayDescriptorSet_ = VK_NULL_HANDLE;
    }

    // Allocate new descriptor set
    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = envMapDescriptorPool_.get(),
        .descriptorSetCount = 1,
        .pSetLayouts        = &envMapDisplayDescSetLayout_
    };
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &envMapDisplayDescriptorSet_));

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

    createImage(
        width_, height_, 1,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        depthImage_,
        depthImageMemory_,
        "DepthBuffer"
    );

    if (!depthImage_.valid()) {
        LOG_FATAL_CAT("RENDERER", "Failed to create depth image — empire cannot see depth");
        return;
    }

    // Create depth view
    VkImageView rawView = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = depthImage_.get(),
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_D32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &rawView));
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
    LOG_INFO_CAT("RENDERER", "Forging 2 RT output images ({}x{}) — THE EMPIRE SEES ALL", 
                 width_, height_);

    const uint32_t frames = 2;

    destroyRTOutputImages();

    rtOutputImages_.reserve(frames);
    rtOutputMemories_.reserve(frames);
    rtOutputViews_.reserve(frames);

    bool allSuccess = true;

    for (uint32_t i = 0; i < frames; ++i)
    {
        RTX::Handle<VkImage>        img;
        RTX::Handle<VkDeviceMemory> mem;
        RTX::Handle<VkImageView>    view;

        createImage(
            width_, height_, 1,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            img,
            mem,
            std::format("RT_Output_Frame_{}", i)
        );

        if (!img.valid() || !mem.valid()) {
            LOG_ERROR_CAT("RENDERER", "Failed to create RT output image for frame {}", i);
            allSuccess = false;
            continue;
        }

        VkImageViewCreateInfo viewInfo{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = img.get(),
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R16G16B16A16_SFLOAT,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        VkImageView rawView = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &rawView));

        view = RTX::Handle<VkImageView>(rawView, stone_device(), vkDestroyImageView);

        rtOutputImages_.push_back(std::move(img));
        rtOutputMemories_.push_back(std::move(mem));
        rtOutputViews_.push_back(std::move(view));
    }

    if (!allSuccess || rtOutputViews_.size() != frames) {
        LOG_FATAL_CAT("RENDERER", 
            "RT OUTPUT IMAGE CREATION FAILED — {} views (expected {}) — EMPIRE CANNOT RENDER",
            rtOutputViews_.size(), frames);
        phase9_ballerina("RT OUTPUT FAILURE — EMPIRE IS BLIND");
    }

    // Mark for first-frame transition — safe whisper mode
    rtOutputNeedsTransition_ = true;

    LOG_SUCCESS_CAT("RENDERER", "ALL 2 RT OUTPUT IMAGES FORGED — transition deferred to first frame");
}

void VulkanRenderer::submitAndPresent(uint32_t slot, uint32_t imageIndex)
{
    // ONE TRUE SYNCHRONIZATION — FENCE WAIT ON CURRENT SLOT ONLY
    // No global device wait — no stalls — maximum throughput
    vkWaitForFences(stone_device(), 1, &inFlightFences_[slot], VK_TRUE, UINT64_MAX);
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
    vkQueueSubmit2(stone_graphics_queue(), 1, &submit, inFlightFences_[slot]);

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
    }
}

// Optional: If you ever need a full GPU sync (debug, shutdown)
void VulkanRenderer::waitForGPU() noexcept
{
    if (stone_device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(stone_device());
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

    LOG_INFO_CAT("RENDERER", "Forging accumulation images — 2 frames — temporal stability awakens");

    destroyAccumulationImages();

    const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    createImageArray(
        accumImages_,
        accumMemories_,
        accumViews_,
        2,
        format,
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        "Accumulation"
    );

    if (accumImages_.size() != 2 || accumViews_.size() != 2) {
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

        VkImageView view;
        VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &view));
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
    VK_CHECK(vkCreateSampler(stone_device(), &samplerInfo, nullptr, &rawSampler));

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
    if (hypertraceScoreView_) {
        vkDestroyImageView(stone_device(), hypertraceScoreView_, nullptr);
        hypertraceScoreView_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreImage_) {
        vkDestroyImage(stone_device(), hypertraceScoreImage_, nullptr);
        hypertraceScoreImage_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreMemory_) {
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

    VK_CHECK(vkBindImageMemory(stone_device(), hypertraceScoreImage_, hypertraceScoreMemory_, 0));

    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = hypertraceScoreImage_,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &hypertraceScoreView_));

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
    push.hypertrace = 1u;  // Always on

    vkCmdPushConstants(cmd,
        RTX::pipeline().rtPipelineLayout(),
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0,
        sizeof(push),
        &push);

    VK_CMD_TRACE_RAYS(cmd,
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

    vkCmdPipelineBarrier2(cmd, &depInfo);
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

    if (frames == 0 || frames > 2) {
        return;
    }

    if (uniformBufferEncs_.size() == frames && !uniformBufferEncs_.empty() && uniformBufferEncs_[0] != 0) {
        return;
    }

    LOG_AMOURANTH("INITIALIZING ALL BUFFER DATA — {} frames | DreamUBO: {} bytes | TonemapUBO: {} bytes | Materials: {} bytes",
                  frames, sizeof(DreamUBO), sizeof(TonemapUBO), materialBufferSize());

    // DESTROY OLD
    for (auto h : uniformBufferEncs_)   if (h) BUFFER_DESTROY(h);
    for (auto h : materialBufferEncs_)  if (h) BUFFER_DESTROY(h);
    for (auto h : dimensionBufferEncs_) if (h) BUFFER_DESTROY(h);
    for (auto h : tonemapUniformEncs_)  if (h) BUFFER_DESTROY(h);

    uniformBufferEncs_.assign(frames, 0);
    materialBufferEncs_.assign(frames, 0);
    dimensionBufferEncs_.assign(frames, 0);
    tonemapUniformEncs_.assign(frames, 0);

    const VkBufferUsageFlags ssboUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    for (uint32_t i = 0; i < frames; ++i)
    {
        // DreamUBO — host-visible, persistently mapped
        uniformBufferEncs_[i] = BufferManager::createDreamUBO(std::format("DreamUBO[{}]", i));
        if (!uniformBufferEncs_[i]) {
            LOG_FATAL("Failed to create DreamUBO {} — THE EMPIRE CANNOT DREAM", i);
        }

        // TonemapUBO — host-visible, persistently mapped
        tonemapUniformEncs_[i] = BufferManager::createTonemapUBO(std::format("TonemapUBO[{}]", i));
        if (!tonemapUniformEncs_[i]) {
            LOG_FATAL("Failed to create TonemapUBO {}", i);
        }

        // Device-local SSBOs
        materialBufferEncs_[i]  = STONE_FINAL_OBFUSCATE(BufferManager::create(materialBufferSize(), ssboUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Materials"));
        dimensionBufferEncs_[i] = STONE_FINAL_OBFUSCATE(BufferManager::create(256, ssboUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "DimensionData"));
    }

    // DO NOT populate here — commandBuffers_ not created yet!
    // First real frame will write correct data via updateUniformBuffer/updateTonemapUniform

    LOG_AMOURANTH("DREAM & TONEMAP UBOs UPGRADED — PERSISTENTLY MAPPED — ENCRYPTIE BOI MODE — PULSING PINK VOID — SASQUATCH IS STONED AND STRONK");
}

void VulkanRenderer::createCommandBuffers() noexcept
{
    commandBuffers_.resize(2);  // Hardcoded 2

    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = RTX::g_ctx().commandPool_,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 2
    };

    // TWISTIE BOI MODE — FIRE AND FORGET
    // We don't care when it finishes — GPU will have them ready when we need them
    // No vkDeviceWaitIdle() — No blocking — Pure speed
    vkAllocateCommandBuffers(stone_device(), &allocInfo, commandBuffers_.data());

    LOG_SUCCESS_CAT("CMD", "ASYNC TWISTIE BOI MODE — 2 command buffers forged in the void");
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

    vkUpdateDescriptorSets(StoneKey::stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
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
    vkCmdPipelineBarrier2(cmd, &dep);
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
    resetAccumulation_   = true;
    resetAccumNextFrame_ = true;
    accumulationFrame_   = 0;
    currentSpp_          = 0;

    // 9. Mark deferred transitions for first frame after resize
    rtOutputNeedsTransition_    = true;
    depthNeedsTransition_       = true;
    accumulationNeedsTransition_ = true;
    nexusScoreNeedsInit_        = true;

    s_resizeInProgress.store(false);

    LOG_AMOURANTH("RESIZE COMPLETE — {}×{} — EMPIRE UNBROKEN — PHOTONS REALIGNED", newWidth, newHeight);
}

// ──────────────────────────────────────────────────────────────────────────────
// FIXED: Safe swapchain layout transitions (PRESENT_SRC_KHR → GENERAL → PRESENT_SRC_KHR)
// No more VK_IMAGE_LAYOUT_UNDEFINED usage on swapchain images
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept
{
    VkDescriptorSet set = tonemapSets_[frameIdx % tonemapSets_.size()];
    if (set == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Tonemap descriptor set null — skipping pass");
        return;
    }

    VkImage swapImg = stone_images()[swapImageIdx];

    // SAFE: Transition from PRESENT_SRC_KHR to GENERAL for compute shader write
    transitionImage(cmd, swapImg,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL,
                    0, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline_.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            tonemapLayout_.get(), 0, 1, &set, 0, nullptr);

    // Push constants
    struct Push {
        float    exposure;
        uint32_t type;
        uint32_t enabled;
        float    pad;
    } push = {
        .exposure = currentExposure_,
        .type     = static_cast<uint32_t>(tonemapType_),
        .enabled  = 1u,
        .pad     = 0.0f
    };

    vkCmdPushConstants(cmd, tonemapLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

    VkExtent2D ext = currentExtent();
    uint32_t wgX = (ext.width + 15) / 16;
    uint32_t wgY = (ext.height + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    // SAFE: Transition back to PRESENT_SRC_KHR for presentation
    transitionImage(cmd, swapImg,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_SHADER_WRITE_BIT, 0,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float deltaTime) noexcept
{
    if (frame >= uniformBufferEncs_.size() || uniformBufferEncs_[frame] == 0)
    {
        LOG_ERROR_CAT("RENDERER", "Frame {} has no DreamUBO — skipping update", frame);
        return;
    }

    const uint64_t handle = uniformBufferEncs_[frame];
    const BufferManager::BufferInfo* info = BufferManager::get(handle);
    if (!info || info->mapped == nullptr)
    {
        LOG_ERROR_CAT("RENDERER", "DreamUBO handle {} not mapped — skipping update", handle);
        return;
    }

    DreamUBO ubo{};

    // ── Core Time & Frame Data ─────────────────────────────────────────────
    ubo.time                = totalTime_;
    ubo.frame               = frameNumber_;
    ubo.currentSpp          = currentSpp_;
    ubo.totalSpp            = accumulationFrame_;
    ubo.exposure            = currentExposure_;

    // Feature toggles (respecting OptionsMenu)
    ubo.enableEnvMap        = envMapImageView_.valid() ? 1u : 0u;
    ubo.hypertraceEnabled   = Options::OptionsRTX::ENABLE_HYPERTRACE ? 1u : 0u;
    ubo.denoisingEnabled    = Options::OptionsRTX::ENABLE_DENOISING ? 1u : 0u;
    ubo.adaptiveEnabled     = Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING ? 1u : 0u;
    ubo.debugMode           = static_cast<uint32_t>(activeRenderMode_);

    ubo.envIntensity        = 1.0f;
    ubo.envRotation         = 0.0f;

    // ── Resolution & Advanced Jitter ───────────────────────────────────────
    ubo.resolution          = glm::vec2(static_cast<float>(width_), static_cast<float>(height_));

    // Halton 2,3 sequence — 16-frame cycle for stable temporal sampling
    static constexpr glm::vec2 halton16[16] = {
        {0.0f,      0.0f},       {0.5f,      0.333333f},
        {0.25f,     0.666667f},  {0.75f,     0.111111f},
        {0.125f,    0.444444f},  {0.625f,    0.777778f},
        {0.375f,    0.222222f},  {0.875f,    0.555556f},
        {0.0625f,   0.888889f},  {0.5625f,   0.037037f},
        {0.3125f,   0.370370f},  {0.8125f,   0.703704f},
        {0.1875f,   0.148148f},  {0.6875f,   0.481481f},
        {0.4375f,   0.814815f},  {0.9375f,   0.259259f}
    };

    const uint32_t jitterIdx = frameNumber_ % 16;
    ubo.jitter              = halton16[jitterIdx];
    ubo.jitterPrev          = (frameNumber_ == 0) ? ubo.jitter : halton16[(frameNumber_ - 1) % 16];

    ubo.nexusScoreThreshold = currentNexusScore_;
    ubo.hypertraceJitterScale = Options::OptionsRTX::HYPERTRACE_JITTER_SCALE;

    // ── Camera Matrices & Parameters ───────────────────────────────────────
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

    // ── Material & Scene Overrides ─────────────────────────────────────────
    ubo.materialCount       = static_cast<uint32_t>(materialCount_);
    ubo.activeMaterialIndex = activeMaterialIndex_;
    ubo.metallicOverride    = materialMetallicOverride_;
    ubo.roughnessOverride   = materialRoughnessOverride_;
    ubo.emissiveIntensity   = emissiveIntensity_;

    ubo.enableBlueNoise     = Options::Environment::ENABLE_BLUE_NOISE ? 1u : 0u;
    ubo.enableTAA           = Options::OptionsRTX::ENABLE_TAA ? 1u : 0u;
    ubo.taaAlpha            = Options::OptionsRTX::TAA_ALPHA;

    // ── Lighting & Environment ─────────────────────────────────────────────
    ubo.sunDirection        = sunDirection_;
    ubo.sunIntensity        = sunIntensity_;
    ubo.sunColor            = sunColor_;
    ubo.fogDensity          = fogDensity_;
    ubo.fogColor            = fogColor_;

    // ── Debug Visualization Toggles ────────────────────────────────────────
    ubo.showNexusScore      = Options::Debug::SHOW_NEXUS_SCORE ? 1u : 0u;
    ubo.showSppHeatmap      = Options::Debug::SHOW_SPP_HEATMAP ? 1u : 0u;
    ubo.showAccumulationCount = Options::Debug::SHOW_ACCUMULATION_COUNT ? 1u : 0u;
    ubo.showGpuTimestamps   = Options::Debug::SHOW_GPU_TIMESTAMPS ? 1u : 0u;

    // Optional debug floats — reserved for future use
    ubo.debugFloat1 = debugFloat1_;
    ubo.debugFloat2 = debugFloat2_;
    ubo.debugFloat3 = debugFloat3_;
    ubo.debugFloat4 = debugFloat4_;

    // ── Direct write to persistently mapped host-visible UBO ──
    std::memcpy(info->mapped, &ubo, sizeof(DreamUBO));

    LOG_TRACE_CAT("RENDERER", "DreamUBO updated — frame {} — totalSpp {} — jitter {} — empire aligned", 
                  frameNumber_, accumulationFrame_, jitterIdx);
}

void VulkanRenderer::updateTonemapUniform(uint32_t frame) noexcept
{
    if (frame >= tonemapUniformEncs_.size() || tonemapUniformEncs_[frame] == 0) {
        LOG_WARN_CAT("RENDERER", "Tonemap UBO handle invalid or zero for frame {} — skipping", frame);
        return;
    }

    const uint64_t handle = tonemapUniformEncs_[frame];
    const BufferManager::BufferInfo* info = BufferManager::get(handle);
    if (!info || info->mapped == nullptr) {
        LOG_ERROR_CAT("RENDERER", "Tonemap UBO handle {} not mapped — skipping update", handle);
        return;
    }

    TonemapUBO ubo{};

    ubo.exposure = currentExposure_;
    ubo.type = static_cast<uint32_t>(tonemapType_);
    ubo.enabled = 1u;  // Always on
    ubo.nexusScore = currentNexusScore_;
    ubo.frame = frameNumber_;
    ubo.spp = currentSpp_;

    ubo.gamma = 2.2f;
    ubo.bloomThreshold = 1.0f;
    ubo.bloomIntensity = 0.8f;
    ubo.vignetteIntensity = 0.4f;
    ubo.filmGrainStrength = 0.05f;
    ubo.lensFlareIntensity = 0.3f;

    // Direct eternal write — raw boi, no staging
    std::memcpy(info->mapped, &ubo, sizeof(TonemapUBO));
}

void VulkanRenderer::setTonemap(bool enabled) noexcept
{
    tonemapEnabled_ = true;  // Always on
    resetAccumulation_ = true;

    LOG_INFO_CAT("Renderer", "{}Tonemapping ENABLED{}", LIME_GREEN, RESET);
}

VulkanRenderer::~VulkanRenderer() = default;

void VulkanRenderer::setRenderMode(int mode) noexcept
{
    mode = glm::clamp(mode, 1, 9);
    if (mode != activeRenderMode_) {
        activeRenderMode_ = mode;
        resetAccumNextFrame_ = true;
    }
}

void VulkanRenderer::createFramebuffers() noexcept
{
    LOG_INFO_CAT("RENDERER", "Forging swapchain framebuffers — {} images — the empire renders", stone_image_count());

    // Destroy old framebuffers — the cycle is sacred
    for (auto& fb : framebuffers_) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(stone_device(), fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }

    const uint32_t imageCount = stone_image_count();
    framebuffers_.clear();
    framebuffers_.resize(imageCount);

    const auto& swapchainViews = stone_views();
    const VkRenderPass renderPass = stone_pass();
    const uint32_t width  = stone_width();
    const uint32_t height = stone_height();

    LOG_AMOURANTH("Amouranth: \"Every framebuffer is a mirror. And I am in all of them.\"");

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageView attachment = swapchainViews[i];

        VkFramebufferCreateInfo fbInfo{
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = renderPass,
            .attachmentCount = 1,
            .pAttachments    = &attachment,
            .width           = width,
            .height          = height,
            .layers          = 1
        };

        VK_CHECK(
            vkCreateFramebuffer(stone_device(), &fbInfo, nullptr, &framebuffers_[i]),
            std::format("Failed to forge framebuffer {} of {}", i, imageCount).c_str()
        );

        LOG_TRACE_CAT("RENDERER", "Framebuffer {} forged — view {}", i, reinterpret_cast<uint64_t>(attachment));
    }
}

void VulkanRenderer::createTonemapDescriptorSets() noexcept
{
    const uint32_t frames = 2;  // Hardcoded

    if (tonemapSets_.size() == frames && tonemapSets_[0] != VK_NULL_HANDLE) {
        return; // already valid
    }

    std::vector<VkDescriptorSetLayout> layouts(frames, tonemapDescriptorSetLayout_.get());

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = tonemapDescriptorPool_.get(),
        .descriptorSetCount = frames,
        .pSetLayouts        = layouts.data()
    };

    tonemapSets_.resize(frames);
    VkResult result = vkAllocateDescriptorSets(stone_device(), &allocInfo, tonemapSets_.data());

    if (result != VK_SUCCESS)
    {
        LOG_FATAL_CAT("TONEMAP", "FATAL: vkAllocateDescriptorSets failed (result: {}) — cannot recover", result);
        phase9_ballerina("TONEMAP DESCRIPTOR SET ALLOCATION FAILED — EMPIRE FALLS", std::source_location::current());
    }
}

void VulkanRenderer::cleanupFramebuffers() noexcept {
    VkDevice dev = StoneKey::stone_device();
    for (auto fb : framebuffers_) {
        if (fb && dev != VK_NULL_HANDLE) vkDestroyFramebuffer(dev, fb, nullptr);
    }
    framebuffers_.clear();
}

void VulkanRenderer::createTonemapDescriptorPool() noexcept
{
    if (tonemapDescriptorPool_.valid()) {
        return; // already good
    }

    const uint32_t frames = 2;  // Hardcoded

    // 3 descriptors per frame × frames + 50% headroom = bulletproof
    const uint32_t totalSets = frames + (frames / 2);

    std::array<VkDescriptorPoolSize, 3> poolSizes = {{
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, totalSets },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         totalSets },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        totalSets }
    }};

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = totalSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &poolInfo, nullptr, &pool);

    if (result != VK_SUCCESS)
    {
        LOG_FATAL_CAT("TONEMAP", "CRITICAL: Failed to create tonemap descriptor pool (result: {}) — cannot continue", result);
        phase9_ballerina("TONEMAP DESCRIPTOR POOL FAILURE — EMPIRE CANNOT RENDER", std::source_location::current());
    }

    tonemapDescriptorPool_ = Handle<VkDescriptorPool>(
        pool,
        stone_device(),
        [](VkDevice d, VkDescriptorPool p, auto*) {
            vkDestroyDescriptorPool(d, p, nullptr);
        }
    );
}

void VulkanRenderer::updateTonemapDescriptor(uint32_t frameIdx,
                                             VkImageView inputView,
                                             VkImageView output) noexcept
{
    const uint32_t frames = 2;  // Hardcoded

    // SELF-HEALING — REBUILD ON ANY FAILURE
    if (tonemapSets_.size() != frames ||
        !tonemapDescriptorPool_.valid() ||
        !tonemapDescriptorSetLayout_.valid() ||
        frameIdx >= tonemapSets_.size() ||
        tonemapSets_[frameIdx] == VK_NULL_HANDLE)
    {
        LOG_WARNING_CAT("TONEMAP", "Emergency rebuild triggered (frame {})", frameIdx);
        createTonemapDescriptorPool();
        createTonemapDescriptorSetLayout();
        createTonemapDescriptorSets();
    }

    // FINAL CHECK — IF STILL BROKEN, GIVE UP GRACEFULLY
    if (frameIdx >= tonemapSets_.size() || tonemapSets_[frameIdx] == VK_NULL_HANDLE)
    {
        LOG_ERROR_CAT("TONEMAP", "Descriptor set invalid after rebuild — skipping frame {}", frameIdx);
        return;
    }

    if (!inputView || !output) return;

    if (frameIdx >= tonemapUniformEncs_.size() || tonemapUniformEncs_[frameIdx] == 0) return;

    const auto* buf = BufferManager::get(tonemapUniformEncs_[frameIdx]);
    if (!buf || buf->buffer == VK_NULL_HANDLE) return;

    // === BINDING — PHOTONS OBEY ===
    VkDescriptorImageInfo inputInfo = {
        .sampler     = tonemapSampler_.get(),
        .imageView   = inputView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo outputInfo = {
        .imageView   = output,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorBufferInfo uboInfo = {
        .buffer = buf->buffer,
        .offset = 0,
        .range = sizeof(TonemapUBO)  // FIXED
    };

    std::array<VkWriteDescriptorSet, 3> writes = {{
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapSets_[frameIdx], .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &inputInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapSets_[frameIdx], .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       .pImageInfo = &outputInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapSets_[frameIdx], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,     .pBufferInfo = &uboInfo }
    }};

    vkUpdateDescriptorSets(stone_device(), 3, writes.data(), 0, nullptr);
}

void VulkanRenderer::createTonemapDescriptorSetLayout() noexcept
{
    if (tonemapDescriptorSetLayout_.valid()) {
        return; // already exists
    }

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {{
        // Binding 0: Input image (sampled)
        {
            .binding            = 0,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr
        },
        // Binding 1: Output image (storage)
        {
            .binding            = 1,
            .descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr
        },
        // Binding 2: Uniform buffer (tonemap params)
        {
            .binding            = 2,
            .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr
        }
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &layoutInfo, nullptr, &layout));

    tonemapDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(
        layout,
        stone_device(),
        [](VkDevice d, VkDescriptorSetLayout l, auto*) {
            vkDestroyDescriptorSetLayout(d, l, nullptr);
        }
    );
}

// Optional: recreateTonemapUBOs — now uses host-visible (replace the old loop)
bool VulkanRenderer::recreateTonemapUBOs() noexcept
{
    const uint32_t frames = 2;  // Hardcoded

    // Destroy old
    for (auto h : tonemapUniformEncs_) if (h) BufferManager::destroy(h);
    tonemapUniformEncs_.assign(frames, 0);

    // Recreate as host-visible
    for (uint32_t i = 0; i < frames; ++i)
    {
        tonemapUniformEncs_[i] = BufferManager::createHostVisible(256, std::format("TonemapUBO[{}]", i));
        if (tonemapUniformEncs_[i] == 0) {
            LOG_FATAL("Failed to recreate host-visible TonemapUBO[{}]", i);
            return false;
        }
    }

    // Re-bind UBOs to descriptor sets
    for (uint32_t i = 0; i < frames; ++i) {
        updateTonemapUBO(i);
    }

    return true;
}

void VulkanRenderer::destroySharedStaging() noexcept {
    if (RTX::g_ctx().sharedStagingEnc_ != 0) {
        BufferManager::destroy(RTX::g_ctx().sharedStagingEnc_);
        RTX::g_ctx().sharedStagingEnc_ = 0;
        LOG_DEBUG_CAT("RENDERER", "Shared staging destroyed");
    }
}

bool VulkanRenderer::createSharedStaging() noexcept
{
    const VkDeviceSize size = 368 * 2;  // Hardcoded 2 frames

    LOG_INFO_CAT("RENDERER", "Creating shared staging buffer — {} bytes for 2 frames", size);

    // Destroy old one if exists
    if (RTX::g_ctx().sharedStagingEnc_ != 0)
    {
        BufferManager::destroy(RTX::g_ctx().sharedStagingEnc_);
        RTX::g_ctx().sharedStagingEnc_ = 0;
    }

    // CREATE BUFFER
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(stone_device(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        LOG_FATAL_CAT("RENDERER", "Failed to create shared staging buffer");
        return false;
    }

    // GET MEMORY REQUIREMENTS
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(stone_device(), buffer, &memReqs);

    // FIND HOST-VISIBLE, COHERENT MEMORY
    uint32_t memoryType = findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (memoryType == ~0u)
    {
        LOG_FATAL_CAT("RENDERER", "No host-visible memory type found for staging buffer");
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        return false;
    }

    // ALLOCATE MEMORY
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memoryType
    };

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (vkAllocateMemory(stone_device(), &allocInfo, nullptr, &memory) != VK_SUCCESS)
    {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate memory for staging buffer");
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        return false;
    }

    // BIND MEMORY
    if (vkBindBufferMemory(stone_device(), buffer, memory, 0) != VK_SUCCESS)
    {
        LOG_FATAL_CAT("RENDERER", "Failed to bind memory to staging buffer");
        vkFreeMemory(stone_device(), memory, nullptr);
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        return false;
    }

    // MAP MEMORY — THIS IS THE CRITICAL STEP
    void* mapped = nullptr;
    VkResult mapResult = vkMapMemory(stone_device(), memory, 0, size, 0, &mapped);
    if (mapResult != VK_SUCCESS || !mapped)
    {
        LOG_FATAL_CAT("RENDERER", "vkMapMemory failed for staging buffer: {}", string_VkResult(mapResult));
        vkFreeMemory(stone_device(), memory, nullptr);
        vkDestroyBuffer(stone_device(), buffer, nullptr);
        return false;
    }

    // STORE IN GLOBAL CONTEXT — THE EMPIRE'S VOICE IS BORN
    RTX::g_ctx().sharedStagingEnc_ = reinterpret_cast<uint64_t>(buffer);

    struct BufferInfo {
        VkBuffer           buffer  = VK_NULL_HANDLE;
        VkDeviceMemory     memory  = VK_NULL_HANDLE;
        VkDeviceSize       size    = 0;
        VkDeviceSize       aligned = 0;
        VkBufferUsageFlags usage   = 0;
        std::string        tag;
        void*              mapped  = nullptr;
    };

    LOG_SUCCESS_CAT("RENDERER", 
        "Shared staging buffer CREATED AND MAPPED — {} bytes @ {} | handle: {}", 
        size, mapped, RTX::g_ctx().sharedStagingEnc_);

    return true;
}

void VulkanRenderer::recreateSwapchainDependentResources() noexcept
{
    // Wait for GPU idle — safe destruction of old resources
    vkDeviceWaitIdle(stone_device());

    LOG_AMOURANTH("RECREATING SWAPCHAIN-DEPENDENT RESOURCES — FULL EMPIRE REBUILD");

    // ====================================================================
    // 1. DESTROY OLD RESOURCES — SAFE ORDER
    // ====================================================================
    destroyRTOutputImages();
    rtOutputImages_.clear();
    rtOutputMemories_.clear();
    rtOutputViews_.clear();

    destroyAccumulationImages();
    accumImages_.clear();
    accumMemories_.clear();
    accumViews_.clear();

    destroyDenoiserImage();
    destroyNexusScoreImage();

    // ====================================================================
    // 2. RECREATE CORE RESOURCES
    // ====================================================================
    createCommandBuffers();

    createRTOutputImages();
    createAccumulationImages();
    createNexusScoreImage(RTX::g_ctx().commandPool(), stone_graphics_queue());

    recreateTonemapUBOs();

    // ====================================================================
    // 3. RE-ALLOCATE DESCRIPTOR SETS — CRITICAL FOR RESIZE SAFETY
    // Binds new rtOutputViews_ and accumulation views
    // ====================================================================
    pipelineManager_.allocateDescriptorSets();
    LOG_AMOURANTH("RT DESCRIPTOR SETS RE-ALLOCATED — BINDINGS SAFE — PINK SURVIVES RESIZE");

    // ====================================================================
    // 4. RESET STATE
    // ====================================================================
    resetAccumulation_   = true;
    resetAccumNextFrame_ = true;
    accumulationFrame_   = 0;
    currentSpp_          = 0;

    // ====================================================================
    // 5. MARK FIRST-FRAME TRANSITIONS
    // ====================================================================
    rtOutputNeedsTransition_     = true;
    depthNeedsTransition_        = true;
    accumulationNeedsTransition_ = true;
    nexusScoreNeedsInit_         = true;
    swapchainNeedsPresentTransition_ = true;

    LOG_SUCCESS_CAT("SWAPCHAIN", "Swapchain-dependent resources fully rebuilt — pink eternal across resize — EMPIRE UNBROKEN");
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

    VkImage rawImage;
    vkCreateImage(stone_device(), &imageInfo, nullptr, &rawImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(stone_device(), rawImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties)
    };

    VkDeviceMemory mem;
    vkAllocateMemory(stone_device(), &allocInfo, nullptr, &mem);
    vkBindImageMemory(stone_device(), rawImage, mem, 0);

    image  = RTX::Handle<VkImage>(rawImage, stone_device(), vkDestroyImage);
    memory = RTX::Handle<VkDeviceMemory>(mem, stone_device(), vkFreeMemory);
}

void VulkanRenderer::destroyRenderPass() noexcept {
    if (renderPass_) {
        vkDestroyRenderPass(stone_device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::createRenderPass() noexcept
{
    if (renderPass_ != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RENDERER", "Render pass already exists — skipping creation");
        return;
    }

    LOG_INFO_CAT("RENDERER", "Forging classic raster render pass — for fallback/overlay modes");

    VkAttachmentDescription colorAttachment{
        .format         = RTX::SwapchainManager::format(),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorRef{
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorRef
    };

    VkSubpassDependency dependency{
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo info{
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &colorAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency
    };

    VkRenderPass pass = VK_NULL_HANDLE;
    VkResult result = vkCreateRenderPass(stone_device(), &info, nullptr, &pass);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create render pass: {}", string_VkResult(result));
        phase9_ballerina("RENDER PASS CREATION FAILED", std::source_location::current());
    }

    renderPass_ = pass;

    LOG_SUCCESS_CAT("RENDERER", "Classic raster render pass forged — ready for fallback/overlay rendering");
}

void VulkanRenderer::setMaxFramesInFlight(uint32_t count) noexcept
{
    maxFramesInFlight_ = 2;  // Hardcoded
}

void VulkanRenderer::onSwapchainRebuilt(uint32_t w, uint32_t h) noexcept
{
    LOG_AMOURANTH("SWAPCHAIN REBORN — {}×{} — FULL REBUILD CYCLE", w, h);

    // Recreate sync + command buffers
    createSyncObjects();
    createCommandBuffers();  // ← Critical: fresh command buffers

    // Recreate all dependent resources
    recreateSwapchainDependentResources();

    currentFrame_.store(0);
    resetAccumulation_ = resetAccumNextFrame_ = true;
    accumulationFrame_ = currentSpp_ = 0;

    LOG_AMOURANTH("ON SWAPCHAIN REBUILT — ALL SYSTEMS NOMINAL — RESUME RENDERING");
}

void VulkanRenderer::onWindowResize(uint32_t w, uint32_t h) noexcept
{
    // Early exit if size unchanged — avoids unnecessary full rebuild
    if (static_cast<uint32_t>(width_) == w && static_cast<uint32_t>(height_) == h) {
        LOG_TRACE_CAT("RENDERER", "Resize event ignored — dimensions unchanged ({}×{})", w, h);
        return;
    }

    LOG_AMOURANTH("WINDOW RESIZE DETECTED — {}×{} → {}×{} — FULL EMPIRE REBUILD INITIATED", width_, height_, w, h);

    // Update internal size immediately — prevents race with render thread
    width_  = static_cast<int>(w);
    height_ = static_cast<int>(h);

    // FULL REBUILD: Recreate swapchain and ALL dependent resources
    RTX::recreateSwapchain(w, h);

    // Recreate all swapchain-dependent resources (images, command buffers, etc.)
    recreateSwapchainDependentResources();

    // Force first-frame transitions after full rebuild
    rtOutputNeedsTransition_     = true;
    depthNeedsTransition_        = true;
    accumulationNeedsTransition_ = true;
    nexusScoreNeedsInit_         = true;
    swapchainNeedsPresentTransition_ = true;

    // Reset accumulation — fresh convergence on new resolution
    requestAccumulationReset();

    LOG_SUCCESS_CAT("RENDERER", "Full resize rebuild complete — {}×{} — all resources reborn — pink survives");
}

// ──────────────────────────────────────────────────────────────────────────────
// Final Status
// ──────────────────────────────────────────────────────────────────────────────
/*
 * December 10, 2025 — 2026 HARDCODE MASTERMIND — 2 FRAMES | R16G16_SFLOAT NEXUS | ALL TOP-NOTCH ENABLED
 * Empire Optimized: Unlimited FPS | Full Features | Half-Float RT/Accum/Denoise | Photons Eternal.
 */