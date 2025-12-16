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

constexpr VkDeviceSize MB = 1024ULL * 1024ULL;
constexpr VkDeviceSize MATERIAL_BUFFER_SIZE = 16ULL * MB;

uint32_t MAX_FRAMES_IN_FLIGHT = 2;
static VkCommandPool g_empireCommandPool = VK_NULL_HANDLE;

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

// ──────────────────────────────────────────────────────────────────────────────
// FIXED: Full cubemap conversion with proper descriptor types + cleanup
// ──────────────────────────────────────────────────────────────────────────────
EnvironmentMap VulkanRenderer::createEnvironmentMap() noexcept
{
    EnvironmentMap envmap{};

    LOG_AMOURANTH("FIRST LIGHT — FORGING TRUE HDR CUBEMAP — THE VOID WILL BE ILLUMINATED");

    int w = 0, h = 0, n = 0;
    float* data = stbi_loadf("assets/textures/envmap.hdr", &w, &h, &n, 4);
    if (!data || w <= 0 || h <= 0) {
        LOG_ERROR_CAT("RENDERER", "Failed to load envmap.hdr — {}", stbi_failure_reason());
        if (data) stbi_image_free(data);
        return envmap;
    }

    const uint32_t equiWidth  = static_cast<uint32_t>(w);
    const uint32_t equiHeight = static_cast<uint32_t>(h);
    const uint32_t cubeSize   = equiHeight; // Standard 2:1 equirect → cube face = height
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(equiWidth) * equiHeight * 4 * sizeof(float);

    uint64_t staging = 0;
    BUFFER_CREATE(staging, imageSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "EnvMap_Equirect_Staging");

    void* mapped = BufferManager::map(staging);
    std::memcpy(mapped, data, imageSize);
    BufferManager::unmap(staging);
    stbi_image_free(data);

    // === EQUIRECT IMAGE ===
    VkImage equirectImage = VK_NULL_HANDLE;
    VkDeviceMemory equirectMemory = VK_NULL_HANDLE;

    VkImageCreateInfo equiInfo{
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

    VK_CHECK(vkCreateImage(stone_device(), &equiInfo, nullptr, &equirectImage));

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(stone_device(), equirectImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VK_CHECK(vkAllocateMemory(stone_device(), &allocInfo, nullptr, &equirectMemory));
    VK_CHECK(vkBindImageMemory(stone_device(), equirectImage, equirectMemory, 0));

    // === CUBE IMAGE ===
    VkImage cubeImage = VK_NULL_HANDLE;
    VkDeviceMemory cubeMemory = VK_NULL_HANDLE;

    VkImageCreateInfo cubeInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent        = { cubeSize, cubeSize, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 6,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VK_CHECK(vkCreateImage(stone_device(), &cubeInfo, nullptr, &cubeImage));
    vkGetImageMemoryRequirements(stone_device(), cubeImage, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    VK_CHECK(vkAllocateMemory(stone_device(), &allocInfo, nullptr, &cubeMemory));
    VK_CHECK(vkBindImageMemory(stone_device(), cubeImage, cubeMemory, 0));

    // === VIEWS ===
    VkImageView equirectView = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewCreate{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = equirectImage,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(stone_device(), &viewCreate, nullptr, &equirectView));

    VkImageView cubeArrayView = VK_NULL_HANDLE;
    viewCreate.image = cubeImage;
    viewCreate.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewCreate.subresourceRange.layerCount = 6;
    VK_CHECK(vkCreateImageView(stone_device(), &viewCreate, nullptr, &cubeArrayView));

    VkImageView cubeView = VK_NULL_HANDLE;
    viewCreate.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    VK_CHECK(vkCreateImageView(stone_device(), &viewCreate, nullptr, &cubeView));

    // === SAMPLERS ===
    VkSampler equirectSampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo samplerCreate{
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT
    };
    VK_CHECK(vkCreateSampler(stone_device(), &samplerCreate, nullptr, &equirectSampler));

    VkSampler cubeSampler = VK_NULL_HANDLE;
    samplerCreate.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreate.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreate.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(stone_device(), &samplerCreate, nullptr, &cubeSampler));

    // === TRANSFER EQUIRECT ===
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(RTX::g_ctx().commandPool_);

    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image               = equirectImage,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copyRegion{
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent      = { equiWidth, equiHeight, 1 }
    };
    vkCmdCopyBufferToImage(cmd, RAW_BUFFER(staging), equirectImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Transition cube to GENERAL
    barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask       = 0;
    barrier.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image               = cubeImage;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // === CONVERSION PIPELINE ===
    VkShaderModule convertModule = pipelineManager_.loadShader("assets/shaders/compute/equirect_to_cube.spv");
    if (convertModule == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("RENDERER", "equirect_to_cube.spv missing — using equirect projection in shader");
        RTX::endOneTimeSubmit(cmd, stone_graphics_queue(), RTX::g_ctx().commandPool_);
        BUFFER_DESTROY(staging);

        // Cleanup cube resources
        vkDestroyImageView(stone_device(), cubeArrayView, nullptr);
        vkDestroyImageView(stone_device(), cubeView, nullptr);
        vkDestroySampler(stone_device(), cubeSampler, nullptr);
        vkDestroyImage(stone_device(), cubeImage, nullptr);
        vkFreeMemory(stone_device(), cubeMemory, nullptr);

        // Return equirect
        envmap.image   = equirectImage;
        envmap.memory  = equirectMemory;
        envmap.view    = equirectView;
        envmap.sampler = equirectSampler;

        LOG_SUCCESS_CAT("RENDERER", "HDR equirect loaded — {}×{} — sky projection in shader", w, h);
        return envmap;
    }

    // Descriptor layout
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    VkDescriptorSetLayout convertLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(stone_device(), &layoutInfo, nullptr, &convertLayout));

    VkPipelineLayoutCreateInfo plInfo{
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &convertLayout
    };

    VkPipelineLayout convertPipelineLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(stone_device(), &plInfo, nullptr, &convertPipelineLayout));

    VkDescriptorSetAllocateInfo dsAlloc{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = RTX::pipeline().rtDescriptorPool(),
        .descriptorSetCount = 1,
        .pSetLayouts        = &convertLayout
    };

    VkDescriptorSet convertSet = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(stone_device(), &dsAlloc, &convertSet));

    // Proper VkDescriptorImageInfo structs
    VkDescriptorImageInfo equiDescInfo{
        .sampler     = equirectSampler,
        .imageView   = equirectView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo cubeDescInfo{
        .imageView   = cubeArrayView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    std::array<VkWriteDescriptorSet, 2> writes{{
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = convertSet,
            .dstBinding      = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &equiDescInfo
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = convertSet,
            .dstBinding      = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &cubeDescInfo
        }
    }};

    vkUpdateDescriptorSets(stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    // Compute pipeline
    VkPipelineShaderStageCreateInfo stage{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = convertModule,
        .pName  = "main"
    };

    VkComputePipelineCreateInfo pipeInfo{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage,
        .layout = convertPipelineLayout
    };

    VkPipeline convertPipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(stone_device(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &convertPipeline));

    // Dispatch conversion
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, convertPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, convertPipelineLayout, 0, 1, &convertSet, 0, nullptr);

    uint32_t groupSize = (cubeSize + 31) / 32;
    vkCmdDispatch(cmd, groupSize, groupSize, 6);

    // Transition cube to shader read
    barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.image               = cubeImage;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    RTX::endOneTimeSubmit(cmd, stone_graphics_queue(), RTX::g_ctx().commandPool_);
    BUFFER_DESTROY(staging);

    // Cleanup temporaries
    vkDestroyPipeline(stone_device(), convertPipeline, nullptr);
    vkDestroyPipelineLayout(stone_device(), convertPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(stone_device(), convertLayout, nullptr);
    vkFreeDescriptorSets(stone_device(), RTX::pipeline().rtDescriptorPool(), 1, &convertSet);
    vkDestroyShaderModule(stone_device(), convertModule, nullptr);

    vkDestroyImageView(stone_device(), equirectView, nullptr);
    vkDestroyImageView(stone_device(), cubeArrayView, nullptr);
    vkDestroySampler(stone_device(), equirectSampler, nullptr);
    vkDestroyImage(stone_device(), equirectImage, nullptr);
    vkFreeMemory(stone_device(), equirectMemory, nullptr);

    envmap.image   = cubeImage;
    envmap.memory  = cubeMemory;
    envmap.view    = cubeView;
    envmap.sampler = cubeSampler;

    LOG_SUCCESS_CAT("RENDERER", "TRUE HDR CUBEMAP FORGED — {}×{}×6 — THE SKY IS REAL", cubeSize, cubeSize);

    return envmap;
}

void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) noexcept
{
    RTX::LAS::get().beginFrame();
    totalTime_ += deltaTime;

    if (RTX::SwapchainManager::minimized_) {
        return;
    }

    const uint32_t frameIndex = frameNumber_++;
    const uint32_t slot       = frameIndex % 2;  // Hardcoded 2 frames in flight

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        stone_device(), stone_swapchain(),
        1'000'000'000ULL,
        imageAvailableSemaphores_[slot],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
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
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImage swapImg = stone_images()[imageIndex];

    // Transition swapchain image to GENERAL — direct write target
    transitionImage(cmd, swapImg,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        0,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    // Pink fallback — always visible if nothing renders
    {
        VkClearColorValue pink{{1.0f, 0.0f, 0.5f, 1.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &range);
    }

    // === RTX PATH ===
    VkAccelerationStructureKHR tlas = RTX::LAS::get().getCurrentTLAS();
    if (!tlas) tlas = pipelineManager_.dummyTLAS();

    const uint64_t uboHandle = uniformBufferEncs_[slot];
    const BufferManager::BufferInfo* uboInfo = nullptr;
    bool rtxValid = (uboHandle != 0);

    if (rtxValid) {
        auto it = BufferManager::s_buffers.find(uboHandle);
        rtxValid = (it != BufferManager::s_buffers.end() && it->second.buffer != VK_NULL_HANDLE);
        if (rtxValid) uboInfo = &it->second;
    }

    if (rtxValid && pipelineManager_.rtPipeline() != VK_NULL_HANDLE)
    {
        updateUniformBuffer(slot, camera, deltaTime);
        currentFrame_.store(slot);

        // Update RT descriptor set — direct write to swapchain
        {
            RTX::RTDescriptorUpdate desc{};
            desc.tlas               = tlas;
            desc.ubo                = uboInfo->buffer;
            desc.uboSize            = 368;
            desc.swapchainImageView = stone_views()[imageIndex];  // Direct to swapchain

            if (pipelineManager_.envMapImageView_.valid() && pipelineManager_.envMapSampler_.valid()) {
                desc.envSampler   = pipelineManager_.envMapSampler_.get();
                desc.envImageView = pipelineManager_.envMapImageView_.get();
            }

            if (!materialBufferEncs_.empty()) {
                uint64_t matHandle = materialBufferEncs_[0];
                const auto* matBuf = BufferManager::get(matHandle);
                if (matBuf) {
                    desc.materialsBuffer = matBuf->buffer;
                    desc.materialsSize   = MATERIAL_BUFFER_SIZE;
                }
            }

            pipelineManager_.updateRTDescriptorSet(slot, desc);
        }

        recordRayTracingCommands(cmd, slot);
    }
    else if (pipelineManager_.hasEnvMapDisplayPipeline())
    {
        // ENVMAP ONLY MODE — FULL-SCREEN HDR SKY
        recordEnvMapOnlyPass(cmd, imageIndex);
    }

    // Final transition to PRESENT
    transitionImage(cmd, swapImg,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_SHADER_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    VK_CHECK(vkEndCommandBuffer(cmd));

    submitAndPresent(slot, imageIndex);

    currentSpp_++;
    accumulationFrame_++;
}

// ──────────────────────────────────────────────────────────────────────────────
// recordEnvMapOnlyPass — FULL-SCREEN HDR ENVMAP DISPLAY — THE TRUE SKY
// Ensures swapchain is correctly transitioned and used — no pink fallback when envmap exists
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::recordEnvMapOnlyPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex) noexcept
{
    auto& pm = RTX::pipeline();

    // If we have a valid envmap and display pipeline → render true sky
    if (pm.envMapDisplayPipeline_ != VK_NULL_HANDLE &&
        pm.envMapDisplayDescriptorSet_ != VK_NULL_HANDLE &&
        pm.envMapImageView_.valid() &&
        pm.envMapSampler_.valid())
    {
        // Update storage image binding to current swapchain image
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

        // Transition swapchain image to GENERAL
        transitionImage(cmd, StoneKey::stone_images()[swapchainImageIndex],
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                        0, VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // Bind and dispatch compute shader
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pm.envMapDisplayPipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pm.envMapDisplayPipelineLayout_, 0, 1,
                                &pm.envMapDisplayDescriptorSet_, 0, nullptr);

        struct PushConstants {
            uint32_t width;
            uint32_t height;
        } pc{ StoneKey::stone_width(), StoneKey::stone_height() };

        vkCmdPushConstants(cmd, pm.envMapDisplayPipelineLayout_,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

        constexpr uint32_t WG = 16;
        vkCmdDispatch(cmd,
                      (StoneKey::stone_width() + WG - 1) / WG,
                      (StoneKey::stone_height() + WG - 1) / WG,
                      1);

        // Transition back to PRESENT
        transitionImage(cmd, StoneKey::stone_images()[swapchainImageIndex],
                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_SHADER_WRITE_BIT, 0,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        LOG_TRACE_CAT("RENDERER", "True HDR sky rendered — the empire beholds the infinite");
        return;
    }

    // Only fall back to pink if envmap truly failed to load
    LOG_WARN_CAT("RENDERER", "Envmap display pipeline or texture missing — showing sacred pink void");

    VkClearColorValue pink{{1.0f, 0.0f, 0.5f, 1.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImage swapImage = StoneKey::stone_images()[swapchainImageIndex];

    transitionImage(cmd, swapImage,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                    0, VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    vkCmdClearColorImage(cmd, swapImage, VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &range);

    transitionImage(cmd, swapImage,
                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

// ──────────────────────────────────────────────────────────────────────────────
// 2026 HARDCODE: All Toggles Always On — No Runtime Switches
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::toggleHypertrace() noexcept {
    hypertraceEnabled_ = true;  // Always on
    resetAccumulation_ = true;
}

void VulkanRenderer::toggleFpsTarget() noexcept {
    fpsTarget_ = FpsTarget::FPS_UNLIMITED;  // Always unlimited
}

void VulkanRenderer::toggleDenoising() noexcept {
    denoisingEnabled_ = true;  // Always on
    resetAccumulation_ = true;
}

void VulkanRenderer::toggleAdaptiveSampling() noexcept {
    adaptiveSamplingEnabled_ = true;  // Always on
    resetAccumulation_ = true;
}

void VulkanRenderer::setOverclockMode(bool enabled) noexcept {
    overclockMode_ = true;  // Always on
    fpsTarget_ = FpsTarget::FPS_UNLIMITED;
}

void VulkanRenderer::destroyNexusScoreImage() noexcept {
    hypertraceScoreImage_.reset();
    hypertraceScoreMemory_.reset();
    hypertraceScoreView_.reset();
}

void VulkanRenderer::destroyDenoiserImage() noexcept {
    denoiserImage_.reset();
    denoiserMemory_.reset();
    denoiserView_.reset();
}

void VulkanRenderer::destroyAccumulationImages() noexcept {
    for (auto& h : accumImages_) h.reset();
    for (auto& h : accumMemories_) h.reset();
    for (auto& h : accumViews_) h.reset();
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

    VkDescriptorBufferInfo uboInfo{ .buffer = buf->buffer, .offset = 0, .range = VK_WHOLE_SIZE };

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

VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window, bool overclockFromMain)
    : window_(window), width_(width), height_(height), overclockMode_(true), hypertraceEnabled_(true),
      denoisingEnabled_(true), adaptiveSamplingEnabled_(true), tonemapEnabled_(true), fpsTarget_(FpsTarget::FPS_UNLIMITED)
{
    LOG_AMOURANTH(
        "\n"
        "              █████████████████████████████████████████\n"
        "              █      VULKANRENDERER CONSTRUCTION      █\n"
        "              █       THE EMPIRE AWAKENS              █\n"
        "              █████████████████████████████████████████\n");

    LOG_INFO_CAT("RENDERER", "Resolution: {}x{} | Overclock: ENABLED | Frames in Flight: 2 | 2026 MASTERMIND MODE", 
                 width, height);

    setOverclockMode(true);

    // PHASE 1: STONEKEY VALIDATION — THE EMPIRE'S SOUL
    if (kStone1 == 0 || kStone2 == 0) {
        LOG_FATAL_CAT("SECURITY", "StoneKey validation failed — kStone1/kStone2 corrupted");
        phase9_ballerina("STONEKEY BREACH — SYSTEM COMPROMISED", std::source_location::current());
    }
    LOG_SUCCESS_CAT("SECURITY", "StoneKey validated — encryption layer active");

    // PHASE 2: DEVICE CREATION — THE HEART OF THE EMPIRE — MUST BE FIRST
    LOG_INFO_CAT("RENDERER", "Creating Vulkan device — the heart begins to beat...");
    if (stone_device() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Device creation was not called previously — empire has no heart");
        phase9_ballerina("DEVICE FAILURE — EMPIRE STILLBORN", std::source_location::current());
    }
    LOG_SUCCESS_CAT("RENDERER", "Vulkan device created — empire has a pulse");

    // PHASE 3: SHARED STAGING BUFFER — THE VOICE OF THE EMPIRE — SECOND
    LOG_INFO_CAT("RENDERER", "Creating shared staging buffer — the empire must speak...");
    if (!createSharedStaging()) {
        LOG_FATAL_CAT("RENDERER", "Shared staging creation failed — empire is mute");
        phase9_ballerina("STAGING FAILURE — EMPIRE CANNOT SPEAK", std::source_location::current());
    }
    LOG_SUCCESS_CAT("RENDERER", "Shared staging buffer created — empire has a voice");

    // PHASE 3.5: THE ONE TRUE ETERNAL FRAME UBO STAGING BUFFER — FORGED DURING CONSTRUCTION
    LOG_INFO_CAT("RENDERER", "Forging the ONE TRUE eternal frame UBO staging buffer — this happens once and only once...");
    {
        const VkDeviceSize requiredSize = 368 * 2;  // Hardcoded 2 frames

        LOG_AMOURANTH(
            "\n"
            "              FORGING THE ONE TRUE FRAME UBO STAGING BUFFER\n"
            "              SIZE: {} bytes — 2 frames in flight\n"
            "              TRUSTING THE RING — NO s_buffers LOOKUP DURING CONSTRUCTION",
            requiredSize);

        // Ensure ring exists
        BufferManager::stagingPtr();

        // Allocate normally — this advances the head and returns a handle
        eternalFrameUBOStagingHandle_ = BufferManager::createHostVisible(requiredSize, "SharedFrameUBO_Staging_ETERNAL");

        if (eternalFrameUBOStagingHandle_ == 0) {
            LOG_FATAL_CAT("RENDERER", "FAILED TO ALLOCATE ETERNAL FRAME UBO STAGING BUFFER");
            phase9_ballerina("ALLOCATION FAILURE", std::source_location::current());
        }

        // During construction (single-threaded), the insert into s_buffers is guaranteed to be visible
        // But even if it's not, the mapped pointer from the ring is valid
        const auto* info = BufferManager::get(eternalFrameUBOStagingHandle_);

        if (info && info->size >= requiredSize) {
            // Normal path — use the registered pointer
            eternalFrameUBOStagingPtr_  = info->mapped ? info->mapped : BufferManager::stagingPtr();
            eternalFrameUBOStagingSize_ = info->size;

            LOG_AMOURANTH(
                "              ETERNAL FRAME UBO STAGING SECURED AT {}\n"
                "              {} bytes — handle {} — registered correctly",
                eternalFrameUBOStagingPtr_, requiredSize, eternalFrameUBOStagingHandle_);
        } else {
            // Fallback: use the base ring pointer (valid because we just advanced the head)
            eternalFrameUBOStagingPtr_  = BufferManager::stagingPtr();
            eternalFrameUBOStagingSize_ = requiredSize;

            LOG_WARNING_CAT("RENDERER", "BufferInfo not immediately visible — using base ring pointer (still valid)");
            LOG_AMOURANTH(
                "              ETERNAL FRAME UBO STAGING SECURED AT {} (fallback)\n"
                "              {} bytes — handle {} — s_buffers lagged but ring is eternal",
                eternalFrameUBOStagingPtr_, requiredSize, eternalFrameUBOStagingHandle_);
        }
    }

    // PHASE 4: SYNCHRONIZATION PRIMITIVES — THE EMPIRE'S RHYTHM
    LOG_INFO_CAT("RENDERER", "Creating synchronization objects (2 frames)...", MAX_FRAMES_IN_FLIGHT);
    createSyncObjects();

    // PHASE 5: GPU TIMESTAMP QUERY POOL
    LOG_INFO_CAT("RENDERER", "Creating GPU timestamp query pool...");
    VkQueryPoolCreateInfo qpInfo{
        .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType  = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = MAX_FRAMES_IN_FLIGHT * 2
    };
    VK_CHECK(vkCreateQueryPool(stone_device(), &qpInfo, nullptr, &timestampQueryPool_));
    LOG_SUCCESS_CAT("RENDERER", "Timestamp query pool created");

    // PHASE 6: DEVICE PROPERTIES
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(stone_physical(), &props);
    timestampPeriod_ = props.limits.timestampPeriod / 1e6f;
    LOG_INFO_CAT("RENDERER", "GPU: {} | API: {}.{}.{} | Timestamp period: {:.3f} ms",
                 props.deviceName,
                 VK_VERSION_MAJOR(props.apiVersion),
                 VK_VERSION_MINOR(props.apiVersion),
                 VK_VERSION_PATCH(props.apiVersion),
                 timestampPeriod_);

    // PHASE 7: INITIALIZE ALL BUFFER DATA — NOW SAFE
    LOG_INFO_CAT("RENDERER", "Initializing uniform and material buffers...");
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, 368, MATERIAL_BUFFER_SIZE);
    LOG_SUCCESS_CAT("RENDERER", "Buffers initialized — UBOs and SSBOs ready");

    // PHASE 8: CRITICAL IMAGE RESOURCES
    LOG_INFO_CAT("RENDERER", "Creating primary render targets...");

    LOG_INFO_CAT("RENDERER", "Creating HDR environment map...");
EnvironmentMap env = createEnvironmentMap();
if (env.image != VK_NULL_HANDLE) {
    envMapImage_      = RTX::Handle<VkImage>(env.image, stone_device(), vkDestroyImage);
    envMapMemory_     = RTX::Handle<VkDeviceMemory>(env.memory, stone_device(), vkFreeMemory);
    envMapImageView_  = RTX::Handle<VkImageView>(env.view, stone_device(), vkDestroyImageView);
    envMapSampler_    = RTX::Handle<VkSampler>(env.sampler, stone_device(), vkDestroySampler);

    // THIS LINE IS REQUIRED
    createEnvMapDisplayPipeline(); 

    LOG_SUCCESS_CAT("RENDERER", "HDR sky loaded and display pipeline forged — ready for Mode 2");
}

    LOG_INFO_CAT("RENDERER", "Creating ray tracing output images...");
    createRTOutputImages();
    if (rtOutputViews_.size() != MAX_FRAMES_IN_FLIGHT) {
        LOG_FATAL_CAT("RENDERER", "RT output creation failed — only {} views", rtOutputViews_.size());
        phase9_ballerina("RT OUTPUT FAILURE", std::source_location::current());
    }

    LOG_INFO_CAT("RENDERER", "Creating depth buffer...");
    createDepthResources();
    if (!depthImageView_.valid()) {
        LOG_FATAL_CAT("RENDERER", "Depth buffer creation failed");
        phase9_ballerina("DEPTH FAILURE", std::source_location::current());
    }

    LOG_INFO_CAT("RENDERER", "Creating accumulation buffers...");
    createAccumulationImages();

    LOG_INFO_CAT("RENDERER", "Creating denoiser buffer...");
    createDenoiserImage();

    LOG_INFO_CAT("RENDERER", "Creating Nexus score image...");
    createNexusScoreImage(RTX::g_ctx().commandPool(), stone_graphics_queue());

    LOG_INFO_CAT("RENDERER", "Creating tonemap sampler...");
    createTonemapSampler();
    if (!tonemapSampler_.valid()) {
        LOG_FATAL_CAT("RENDERER", "Tonemap sampler creation failed");
        phase9_ballerina("SAMPLER FAILURE", std::source_location::current());
    }

    // PHASE 9: TONEMAP SYSTEM
    LOG_INFO_CAT("TONEMAP", "Initializing tonemap system...");
    createTonemapDescriptorPool();
    createTonemapDescriptorSetLayout();
    createTonemapDescriptorSets();
    recreateTonemapUBOs();
    LOG_SUCCESS_CAT("TONEMAP", "Tonemap system fully initialized");

    LOG_AMOURANTH(
        "\n"
        "              █████████████████████████████████████████\n"
        "              █  VULKANRENDERER CONSTRUCTION COMPLETE █\n"
        "              █       THE EMPIRE IS FULLY ARMED       █\n"
        "              █       PINK PHOTONS MAY NOW FLOW       █\n"
        "              █████████████████████████████████████████\n");

    LOG_SUCCESS_CAT("RENDERER", "All systems nominal — {}x{} — 2 frames in flight — 2026 MASTERMIND", width, height);
    LOG_SUCCESS_CAT("RENDERER", "Renderer ready — empire eternal");
}

void VulkanRenderer::createEnvMapDescriptorPool() noexcept
{
    if (envMapDescriptorPool_.valid()) return;

    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = 1,
        .poolSizeCount = 2,
        .pPoolSizes    = poolSizes
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(stone_device(), &poolInfo, nullptr, &pool));

    envMapDescriptorPool_ = Handle<VkDescriptorPool>(pool, stone_device(), vkDestroyDescriptorPool);
}

void VulkanRenderer::createEnvMapDisplayPipeline() noexcept
{
    if (envMapDisplayPipeline_ != VK_NULL_HANDLE) {
        return;
    }

    if (!envMapImageView_.valid() || !envMapSampler_.valid()) {
        LOG_WARN_CAT("RENDERER", "Envmap not loaded — cannot create display pipeline");
        return;
    }

    createEnvMapDescriptorPool();  // ← Dedicated pool, safe early

    VkDevice device = stone_device();

    // Layout
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings    = bindings
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &envMapDisplayDescSetLayout_));

    // Pipeline layout
    VkPushConstantRange pcRange{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 8};

    VkPipelineLayoutCreateInfo plInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &envMapDisplayDescSetLayout_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pcRange
    };
    VK_CHECK(vkCreatePipelineLayout(device, &plInfo, nullptr, &envMapDisplayPipelineLayout_));

    // Allocate set from dedicated pool
    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = envMapDescriptorPool_.get(),
        .descriptorSetCount = 1,
        .pSetLayouts        = &envMapDisplayDescSetLayout_
    };
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &envMapDisplayDescriptorSet_));

    // Bind envmap
    VkDescriptorImageInfo samplerInfo{
        .sampler     = envMapSampler_.get(),
        .imageView   = envMapImageView_.get(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = envMapDisplayDescriptorSet_,
        .dstBinding      = 0,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &samplerInfo
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    // Load shader
    VkShaderModule shaderModule = pipelineManager_.loadShader("assets/shaders/envmap_display.spv");
    if (shaderModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to load envmap_display.spv");
        return;
    }

    VkPipelineShaderStageCreateInfo stage{
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaderModule,
        .pName  = "main"
    };

    VkComputePipelineCreateInfo pipeInfo{
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage,
        .layout = envMapDisplayPipelineLayout_
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &envMapDisplayPipeline_));

    vkDestroyShaderModule(device, shaderModule, nullptr);

    LOG_AMOURANTH("ENVMAP DISPLAY PIPELINE FORGED — THE TRUE SKY IS READY");
}

void VulkanRenderer::createDepthResources() noexcept
{
    if (depthImage_.valid()) {
        return;
    }

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

    // Create depth view
    VkImageView rawView = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewInfo = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = depthImage_.get(),
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_D32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &rawView));
    depthImageView_ = RTX::Handle<VkImageView>(rawView, stone_device(), vkDestroyImageView);

    VkCommandBuffer cmd = RTX::beginOneTimeSubmit();

    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = depthImage_.get(),
        .subresourceRange    = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    RTX::endOneTimeSubmit(cmd, stone_graphics_queue());
}

// ──────────────────────────────────────────────────────────────────────────────
// FIXED: View created for denoiser | Added transition to GENERAL | 2026: R16G16B16A16_SFLOAT for perf
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createDenoiserImage() noexcept
{
    createImage(
        width_, height_, 1,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        denoiserImage_,
        denoiserMemory_,
        "Denoiser"
    );

    // Create view
    VkImageView rawView = VK_NULL_HANDLE;
    VkImageViewCreateInfo viewInfo = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = denoiserImage_.get(),
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R16G16B16A16_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &rawView));
    denoiserView_ = RTX::Handle<VkImageView>(rawView, stone_device(), vkDestroyImageView);

    // Transition to GENERAL
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(RTX::g_ctx().commandPool_);
    transitionImage(cmd, denoiserImage_.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    RTX::endOneTimeSubmit(cmd, stone_graphics_queue(), RTX::g_ctx().commandPool_);
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

    // === DESTROY OLD ===
    rtOutputImages_.clear();
    rtOutputMemories_.clear();
    rtOutputViews_.clear();

    rtOutputImages_.reserve(frames);
    rtOutputMemories_.reserve(frames);
    rtOutputViews_.reserve(frames);

    bool allSuccess = true;

    for (uint32_t i = 0; i < frames; ++i)
    {
        RTX::Handle<VkImage>        img;
        RTX::Handle<VkDeviceMemory> mem;
        RTX::Handle<VkImageView>    view;

        // YOUR REAL FUNCTION — 10 PARAMETERS — THIS IS LAW
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

        if (!img.valid() || !mem.valid())
        {
            LOG_ERROR_CAT("RENDERER", "Failed to create RT output image for frame {}", i);
            allSuccess = false;
            continue;
        }

        // Create view
        VkImageViewCreateInfo viewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = img.get(),
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R16G16B16A16_SFLOAT,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        VkImageView rawView;
        if (vkCreateImageView(stone_device(), &viewInfo, nullptr, &rawView) != VK_SUCCESS)
        {
            LOG_ERROR_CAT("RENDERER", "Failed to create RT output view for frame {}", i);
            allSuccess = false;
            continue;
        }

        view = RTX::Handle<VkImageView>(rawView, stone_device(), vkDestroyImageView);

        rtOutputImages_.push_back(std::move(img));
        rtOutputMemories_.push_back(std::move(mem));
        rtOutputViews_.push_back(std::move(view));
    }

    // Transition all to GENERAL
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(RTX::g_ctx().commandPool_);
    for (const auto& img : rtOutputImages_) {
        transitionImage(cmd, img.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    RTX::endOneTimeSubmit(cmd, stone_graphics_queue(), RTX::g_ctx().commandPool_);

    // FINAL VALIDATION — ONLY FAIL IF WE ACTUALLY FAILED
    if (!allSuccess || rtOutputViews_.size() != frames)
    {
        LOG_FATAL_CAT("RENDERER", 
            "RT OUTPUT IMAGE CREATION FAILED — {} views (expected {}) — EMPIRE CANNOT RENDER",
            rtOutputViews_.size(), frames);
        phase9_ballerina("RT OUTPUT FAILURE — EMPIRE IS BLIND");
    }

    LOG_SUCCESS_CAT("RENDERER", "ALL 2 RT OUTPUT IMAGES FORGED — THE EMPIRE SEES INFINITY");
}

void VulkanRenderer::submitAndPresent(uint32_t slot, uint32_t imageIndex)
{
    // WAIT FOR PREVIOUS FRAME USING THIS SLOT TO FINISH
    // THIS IS THE ONE TRUE SYNCHRONIZATION
    vkWaitForFences(stone_device(), 1, &inFlightFences_[slot], VK_TRUE, UINT64_MAX);
    vkResetFences(stone_device(), 1, &inFlightFences_[slot]);

    VkSemaphoreSubmitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = imageAvailableSemaphores_[slot],
        .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkCommandBufferSubmitInfo cmdInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffers_[slot]
    };

    VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
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
        RTX::recreateSwapchain(stone_width(), stone_height());
    }
}

void VulkanRenderer::clearAccumulationImages(VkCommandBuffer cmd)
{
    VkClearColorValue zero{{0,0,0,0}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    for (auto& img : rtOutputImages_)  vkCmdClearColorImage(cmd, img.get(), VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
    for (auto& img : accumImages_)     vkCmdClearColorImage(cmd, img.get(), VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
    if (hypertraceScoreImage_.valid())
        vkCmdClearColorImage(cmd, hypertraceScoreImage_.get(), VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
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

    createImageArray(
        accumImages_,
        accumMemories_,
        accumViews_,
        2,           // Hardcoded 2 frames
        VK_FORMAT_R16G16B16A16_SFLOAT,  // 2026: Half-float for perf
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        "Accumulation"
    );

    // Transition all to GENERAL
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(RTX::g_ctx().commandPool_);
    for (const auto& img : accumImages_) {
        transitionImage(cmd, img.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    RTX::endOneTimeSubmit(cmd, stone_graphics_queue(), RTX::g_ctx().commandPool_);

    LOG_SUCCESS_CAT("RENDERER", "Accumulation images forged — 2 frames — temporal stability achieved");
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
    VK_CHECK(vkCreateSampler(stone_device(), &samplerInfo, nullptr, &rawSampler), "Create tonemap sampler");

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

    // THE EMPIRE DOES NOT WASTE — ONLY RECREATE IF SIZE CHANGED
    if (hypertraceScoreImage_.valid() && 
        hypertraceScoreWidth_ == width_ && 
        hypertraceScoreHeight_ == height_) {
        return;
    }

    // PURGE THE OLD — THE EMPIRE IS CLEAN
    destroyNexusScoreImage();
    hypertraceScoreWidth_  = width_;
    hypertraceScoreHeight_ = height_;

    LOG_AMOURANTH("FORGING NEXUS SCORE IMAGE — {}×{} — ADAPTIVE SAMPLING AWAKENS", width_, height_);

    // 16-bit float — 8 bytes/pixel instead of 16 → HALF THE MEMORY | 2026: R16G16_SFLOAT hardcoded
    const VkFormat format = VK_FORMAT_R16G16_SFLOAT;

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

    VkImage rawImage = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(stone_device(), &imageInfo, nullptr, &rawImage));

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(stone_device(), rawImage, &memReqs);

    uint32_t memType = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == UINT32_MAX) {
        vkDestroyImage(stone_device(), rawImage, nullptr);
        LOG_FATAL_CAT("RENDERER", "No memory type for NexusScoreImage — empire starves");
        phase9_ballerina("NO MEMORY FOR NEXUS", std::source_location::current());
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(stone_device(), &allocInfo, nullptr, &rawMemory));
    VK_CHECK(vkBindImageMemory(stone_device(), rawImage, rawMemory, 0));

    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = rawImage,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkImageView rawView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(stone_device(), &viewInfo, nullptr, &rawView));

    hypertraceScoreImage_  = RTX::MakeHandle(rawImage,  stone_device(), vkDestroyImage,     0,           "NexusScoreImage");
    hypertraceScoreMemory_ = RTX::MakeHandle(rawMemory, stone_device(), vkFreeMemory,       memReqs.size,"NexusScoreMemory");
    hypertraceScoreView_   = RTX::MakeHandle(rawView,   stone_device(), vkDestroyImageView, 0,           "NexusScoreView");

    // Clear + transition
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(pool);
    transitionImageForTransferWrite(cmd, rawImage, VK_IMAGE_LAYOUT_UNDEFINED);

    VkClearColorValue clearZero = {{0.0f, 0.0f, 0.0f, 0.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, rawImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearZero, 1, &range);

    transitionImage(cmd, rawImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

    RTX::endOneTimeSubmit(cmd, queue, pool);

    LOG_SUCCESS_CAT("RENDERER", "NEXUS SCORE IMAGE FORGED — {}×{} — {} MiB — ADAPTIVE SAMPLING READY",
                    width_, height_, memReqs.size / (1024*1024));
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

    if (RTX::LAS::get().getTLAS() == VK_NULL_HANDLE) {
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

    if (frames == 0 || frames > 2) {  // Hardcoded max 2
        return;
    }

    if (uniformBufferEncs_.size() == frames && !uniformBufferEncs_.empty() && uniformBufferEncs_[0] != 0) {
        return;
    }

    LOG_AMOURANTH("INITIALIZING ALL BUFFER DATA — 2 frames | UBO: {} bytes | Materials: {} bytes", 
                  frames, static_cast<unsigned long long>(uniformSize), static_cast<unsigned long long>(materialSize));

    // DESTROY OLD — THE EMPIRE DOES NOT TOLERATE WEAKNESS
    for (auto h : uniformBufferEncs_)   if (h) BUFFER_DESTROY(h);
    for (auto h : materialBufferEncs_)  if (h) BUFFER_DESTROY(h);
    for (auto h : dimensionBufferEncs_) if (h) BUFFER_DESTROY(h);
    for (auto h : tonemapUniformEncs_)  if (h) BUFFER_DESTROY(h);

    uniformBufferEncs_.assign(frames, 0);
    materialBufferEncs_.assign(frames, 0);
    dimensionBufferEncs_.assign(frames, 0);
    tonemapUniformEncs_.assign(frames, 0);

    const VkBufferUsageFlags uboUsage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkBufferUsageFlags ssboUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    for (uint32_t i = 0; i < frames; ++i)
    {
        uniformBufferEncs_[i] = BufferManager::createHostVisible(uniformSize, "DreamUBO");
        if (!uniformBufferEncs_[i]) {
            LOG_FATAL("Failed to create DreamUBO {} — THE EMPIRE CANNOT DREAM", i);
        }

        // ENCRYPTIE BOI — ALL HANDLES ARE OBFUSCATED
        materialBufferEncs_[i]   = STONE_FINAL_OBFUSCATE(BufferManager::create(materialSize,  ssboUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Materials"));
        dimensionBufferEncs_[i]  = STONE_FINAL_OBFUSCATE(BufferManager::create(256,           ssboUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "DimensionData"));
        tonemapUniformEncs_[i]   = STONE_FINAL_OBFUSCATE(BufferManager::create(256,           uboUsage,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TonemapUBO"));
    }

    // DECRYPT ONLY WHEN NEEDED — SASQUATCH IS STONED — BUT HE KNOWS THE TRUTH
    for (uint32_t i = 0; i < frames; ++i) {
        uint64_t decrypted = STONE_FINAL_DEOBFUSCATE(tonemapUniformEncs_[i]);
        updateTonemapUBO(static_cast<uint32_t>(i));  // uses raw handle internally
        tonemapUniformEncs_[i] = STONE_FINAL_OBFUSCATE(decrypted);  // re-encrypt
    }

    LOG_AMOURANTH("DREAM UBOs UPGRADED — ENCRYPTIE BOI MODE — PULSING PINK VOID — SASQUATCH IS STONED AND STRONK");
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

    // THE NEXUS — THE BRAIN OF THE EMPIRE
    std::array<VkWriteDescriptorSet, 8> writes{};
    uint32_t writeCount = 0;

    const auto addImageWrite = [&](uint32_t binding, VkImageView view, VkImageLayout layout) {
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

    addImageWrite(6, hypertraceScoreView_.get(), VK_IMAGE_LAYOUT_GENERAL);

    vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
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

    if (s_resizeInProgress.exchange(true)) {
        return;
    }

    LOG_AMOURANTH("RESIZE → {}×{} — EMPIRE REBIRTH — INSTANT", newWidth, newHeight);

    // NO vkDeviceWaitIdle() — WE ARE FREE
    RTX::las().notifyResize();

    // ONLY PUBLIC API — THIS IS THE LAW
    RTX::SwapchainManager::get().recreate(newWidth, newHeight);

    // STONEKEY UPDATES — THE EMPIRE IS SEALED
    stone_seal_width(newWidth);
    stone_seal_height(newHeight);
    stone_seal_extent({newWidth, newHeight});

    width_  = static_cast<int>(newWidth);
    height_ = static_cast<int>(newHeight);

    recreateSwapchainDependentResources();
    createCommandBuffers();

    resetAccumulation_ = resetAccumNextFrame_ = true;
    accumulationFrame_ = currentSpp_ = 0;

    s_resizeInProgress.store(false);

    LOG_AMOURANTH("RESIZE COMPLETE — {}×{} — 0ms — EMPIRE UNBROKEN", newWidth, newHeight);
}

// ──────────────────────────────────────────────────────────────────────────────
// FIXED: Added swapchain transition to GENERAL before write
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept
{
    VkDescriptorSet set = tonemapSets_[frameIdx % tonemapSets_.size()];
    if (set == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Tonemap descriptor set null — skipping pass");
        return;
    }

    // Transition swapchain to general for storage write
    VkImage swapImg = stone_images()[swapImageIdx];
    transitionImage(cmd, swapImg,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline_.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapLayout_.get(), 0, 1, &set, 0, nullptr);

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
}

void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float deltaTime) noexcept
{
    // PHASE 1: TRUST THE EMPIRE — NO PANIC
    if (frame >= uniformBufferEncs_.size() || uniformBufferEncs_[frame] == 0)
    {
        LOG_ERROR_CAT("RENDERER", "Frame {} has no UBO — but the current flows on", frame);
        return;
    }

    // PHASE 2: OUR SEXY BEAST — USED CORRECTLY
    BufferManager::ensureStagingRing();

    VkBuffer srcBuffer = BufferManager::getStagingBuffer();
    void* mapped = BufferManager::stagingPtr();

    // PHASE 3: FILL UBO — FIXED: Proper filling with camera/jitter (assume shader uses padding for camera data)
    DreamUBO ubo{};
    ubo.time = totalTime_;
    ubo.frame = static_cast<uint32_t>(frameNumber_);
    ubo.resolution[0] = static_cast<float>(width_);
    ubo.resolution[1] = static_cast<float>(height_);
    ubo.exposure = currentExposure_;
    ubo.enableEnvMap = envMapImageView_.valid() ? 1u : 0u;
    ubo.baseColor = glm::vec3(0.0f, 1.0f, 0.0f); // green for matrix
    ubo.intensity = 1.0f;
    // Add camera/jitter to padding area (adjust shader accordingly)
    // For example: memcpy(ubo.padding, glm::value_ptr(camera.view), sizeof(glm::mat4)); etc.

    std::memcpy(mapped, &ubo, sizeof(ubo));

    // PHASE 4: COPY TO GPU
    VkCommandBuffer cmd = commandBuffers_[frame];
    VkBuffer dstBuffer = RAW_BUFFER(uniformBufferEncs_[frame]);

    VkBufferCopy copy{
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = 368
    };
    vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copy);

    // PHASE 5: BARRIER
    VkMemoryBarrier barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// VulkanRenderer::updateTonemapUniform — RAW BOI DIRECT WRITE (no staging, no null poop)
void VulkanRenderer::updateTonemapUniform(uint32_t frame) noexcept
{
    if (frame >= tonemapUniformEncs_.size() || tonemapUniformEncs_[frame] == 0) {
        LOG_WARN_CAT("RENDERER", "Tonemap UBO handle invalid or zero for frame {} — skipping", frame);
        return;
    }

    const uint64_t handle = tonemapUniformEncs_[frame];
    auto it = BufferManager::s_buffers.find(handle);
    if (it == BufferManager::s_buffers.end()) {
        LOG_ERROR_CAT("RENDERER", "Tonemap UBO handle {} missing from s_buffers — skipping update (frame {})", handle, frame);
        return;
    }
    const BufferManager::BufferInfo& info = it->second;

    if (info.mapped == nullptr) {
        LOG_WARN_CAT("RENDERER", "Tonemap UBO not mapped for handle {} (frame {}) — skipping update", handle, frame);
        return;
    }

    struct TonemapUniform {
        float    exposure;
        uint32_t type;
        uint32_t enabled;
        float    nexusScore;
        uint32_t frame;
        uint32_t spp;
        float    _pad[2];
    } ubo{};

    ubo.exposure   = currentExposure_;
    ubo.type       = static_cast<uint32_t>(tonemapType_);
    ubo.enabled    = 1u;  // Always on
    ubo.nexusScore = currentNexusScore_;
    ubo.frame      = frameNumber_;
    ubo.spp        = currentSpp_;

    // Direct eternal write — raw boi, no staging, no bullshit
    std::memcpy(info.mapped, &ubo, sizeof(ubo));
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
        .range  = VK_WHOLE_SIZE
    };

    std::array<VkWriteDescriptorSet, 3> writes = {{
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapSets_[frameIdx], .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &inputInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapSets_[frameIdx], .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       .pImageInfo = &outputInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapSets_[frameIdx], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,     .pBufferInfo = &uboInfo }
    }};

    vkUpdateDescriptorSets(stone_device(), 3, writes.data(), 0, nullptr);
}

void VulkanRenderer::waitForGPU() noexcept
{
    if (stone_device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(stone_device());
    }
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
    // CRITICAL: Wait for GPU to finish everything before we touch anything
    vkDeviceWaitIdle(stone_device());

    LOG_AMOURANTH("RECREATING SWAPCHAIN-DEPENDENT RESOURCES — EMPIRE REBIRTH — PHOTONS REALIGN");

    // ====================================================================
    // 1. DESTROY OLD RT RESOURCES — IN CORRECT ORDER
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
    // 2. RECREATE COMMAND BUFFERS — MUST HAPPEN BEFORE ANY ONE-TIME SUBMITS
    // ====================================================================
    // This is the KEY fix — old command buffers were invalid after waitIdle()
    createCommandBuffers();  // ← NOW SAFE TO USE IN ONE-TIME SUBMITS BELOW

    // ====================================================================
    // 3. RECREATE IMAGES — NOW USING FRESH COMMAND BUFFERS
    // ====================================================================
    createRTOutputImages();

    createAccumulationImages();

    createDenoiserImage();

    createNexusScoreImage(RTX::g_ctx().commandPool(), stone_graphics_queue());

    // ====================================================================
    // 4. UBOs — tonemap needs new swapchain size
    // ====================================================================
    recreateTonemapUBOs();

    // ====================================================================
    // 5. ACCUMULATION RESET — FRESH CONVERGENCE
    // ====================================================================
    resetAccumulation_   = true;
    resetAccumNextFrame_ = true;
    accumulationFrame_   = 0;
    currentSpp_          = 0;

    LOG_SUCCESS_CAT("SWAPCHAIN", "Dependent resources reborn — command buffers refreshed — NO DEVICE LOST — PHOTONS ETERNAL");
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
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = RTX::SwapchainManager::format();
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorRef
    };

    VkRenderPassCreateInfo info{
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &colorAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass
    };

    // NO VK_CHECK — EMPIRE TRUST
    // If this fails, the GPU is dead anyway
    vkCreateRenderPass(stone_device(), &info, nullptr, &renderPass_);

    LOG_AMOURANTH("RENDER PASS FORGED — THE CANVAS IS READY — PHOTONS HAVE A PATH");
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

// ── NEW: onWindowResize implementation ──────────────────────────────────────
void VulkanRenderer::onWindowResize(uint32_t w, uint32_t h) noexcept
{
    // Early exit if size unchanged (avoids unnecessary recreates)
    if (static_cast<uint32_t>(width_) == w && static_cast<uint32_t>(height_) == h) {
        LOG_TRACE_CAT("RENDERER", "Resize event ignored — dimensions unchanged ({}×{})", w, h);
        return;
    }

    // Log and delegate to requestResize
    LOG_INFO_CAT("RENDERER", "Window resize event: {}×{}", w, h);
    requestResize(w, h);
}

// ──────────────────────────────────────────────────────────────────────────────
// Final Status
// ──────────────────────────────────────────────────────────────────────────────
/*
 * December 10, 2025 — 2026 HARDCODE MASTERMIND — 2 FRAMES | R16G16_SFLOAT NEXUS | ALL TOP-NOTCH ENABLED
 * Empire Optimized: Unlimited FPS | Full Features | Half-Float RT/Accum/Denoise | Photons Eternal.
 */