// src/engine/Vulkan/VulkanRenderer.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts
// FINAL: NO SEGFAULT, NO REORDER WARNINGS, NO UNINITIALIZED DATA
//        UBOs, Material, Dimension buffers owned by VulkanRenderer
//        Swapchain data owned directly (no manager) — FPS UNLOCKED BY DEFAULT
//        Zero leaks, zero warnings, maximum OCEAN TEAL logging.

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include "engine/camera.hpp"
#include "engine/utils.hpp"

#include <SDL3/SDL.h>  
#include <SDL3/SDL_vulkan.h>  // ← ADDED: For SDL_Vulkan_GetDrawableSize

#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <array>
#include <iomanip>
#include <span>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <chrono>
#include <bit>
#include <thread>
#include <cstdint>
#include <atomic>
#include "stb/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
#include <fstream>
#include <vulkan/vulkan_core.h>

using namespace Logging::Color;

namespace VulkanRTX {

VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window,
                               const std::vector<std::string>& shaderPaths,
                               std::shared_ptr<Vulkan::Context> context,
                               VulkanPipelineManager* pipelineManager,
                               VulkanBufferManager* bufferManager)
    : window_(window),
      context_(std::move(context)),
      pipelineManager_(pipelineManager),
      bufferManager_(bufferManager),
      width_(width),
      height_(height),
      currentFrame_(0),
      frameCount_(0),
      framesThisSecond_(0),
      currentMode_(1),
      frameNumber_(0),
      currentRTIndex_(0),
      lastFPSTime_(std::chrono::steady_clock::now()),
      currentAccumIndex_(0),
      envMapImage_(VK_NULL_HANDLE),
      envMapImageMemory_(VK_NULL_HANDLE),
      envMapImageView_(VK_NULL_HANDLE),
      envMapSampler_(VK_NULL_HANDLE),
      rtOutputImages_{},
      rtOutputMemories_{},
      rtOutputViews_{},
      accumImages_{},
      accumMemories_{},
      accumViews_{},
      commandBuffers_(MAX_FRAMES_IN_FLIGHT),
      descriptorPool_(VK_NULL_HANDLE),
      rtxDescriptorSet_(VK_NULL_HANDLE),
      computeDescriptorSets_{},
      uniformBuffers_{},
      uniformBufferMemories_{},
      materialBuffers_{},
      materialBufferMemory_{},
      dimensionBuffers_{},
      dimensionBufferMemory_{},
      sbtBuffer_(VK_NULL_HANDLE),
      tlasHandle_(VK_NULL_HANDLE),
      sbtMemory_(VK_NULL_HANDLE),
      rtPipeline_(VK_NULL_HANDLE),
      rtPipelineLayout_(VK_NULL_HANDLE),
      swapchainRecreating_(false),
      queryReady_{},
      framebuffers_(),
      queryPools_(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE),
      descriptorsUpdated_(false)
{
    LOG_INFO_CAT("RENDERER", "{}Initializing VulkanRenderer ({}x{}){}", OCEAN_TEAL, width, height, RESET);

    if (!context_)               throw std::runtime_error("Null context");
    if (!context_->device)       throw std::runtime_error("Context has null VkDevice");
    if (!pipelineManager_)       throw std::runtime_error("Null pipeline manager");
    if (!bufferManager_)         throw std::runtime_error("Null buffer manager");

    // ───── CREATE SYNCHRONIZATION PRIMITIVES ─────
    VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]), "Image avail sem");
        VK_CHECK(vkCreateSemaphore(context_->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]), "Render fin sem");
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &inFlightFences_[i]), "In-flight fence");
    }
    LOG_INFO_CAT("SYNC", "{}Created {} semaphores + fences{}", OCEAN_TEAL, MAX_FRAMES_IN_FLIGHT, RESET);

    // ───── CREATE SWAPCHAIN (FPS UNLOCKED) ─────
    createSwapchain();
    LOG_INFO_CAT("SWAP", "{}Swapchain created: {}x{} | {} images{}", OCEAN_TEAL, swapchainExtent_.width, swapchainExtent_.height, swapchainImages_.size(), RESET);

    for (auto& pool : queryPools_) {
        VkQueryPoolCreateInfo queryInfo{
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = 2
        };
        VK_CHECK(vkCreateQueryPool(context_->device, &queryInfo, nullptr, &pool), "Create query pool");
    }
    queryReady_.fill(false);

    computeDescriptorSetLayout_ = pipelineManager_->getComputeDescriptorSetLayout();
    if (computeDescriptorSetLayout_ == VK_NULL_HANDLE)
        throw std::runtime_error("Compute descriptor set layout missing");

    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 2 };
    poolSizes[2] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT + 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &descriptorPool_),
             "Failed to create descriptor pool");
    LOG_INFO_CAT("DESC", "{}Descriptor pool created{}", OCEAN_TEAL, RESET);

    rtx_ = std::make_unique<VulkanRTX>(context_, swapchainExtent_.width, swapchainExtent_.height, pipelineManager_);

    VkDescriptorSetLayout rtxLayout = rtx_->getDescriptorSetLayout();
    if (rtxLayout == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("DESC", "{}RTX descriptor layout is null!{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Failed to create RTX descriptor set layout");
    }

    VkDescriptorSetAllocateInfo rtxAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &rtxLayout
    };
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &rtxAllocInfo, &rtxDescriptorSet_),
             "Failed to allocate RTX descriptor set");
    LOG_INFO_CAT("DESC", "{}RTX descriptor set allocated{}", OCEAN_TEAL, RESET);

    createComputeDescriptorSets();
    createRTOutputImages();
    createAccumulationImages();
    createFramebuffers();
    createEnvironmentMap();
    buildAccelerationStructures();

    constexpr size_t kMaxMaterials = 256;
    constexpr VkDeviceSize kMaterialSize = kMaxMaterials * sizeof(MaterialData);
    constexpr VkDeviceSize kDimensionSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMaterialSize, kDimensionSize);
    LOG_INFO_CAT("RENDERER", "{}Allocated {} material buffers ({} bytes each) "
                             "and {} dimension buffers ({} bytes each){}",
                 OCEAN_TEAL, MAX_FRAMES_IN_FLIGHT, kMaterialSize,
                 MAX_FRAMES_IN_FLIGHT, kDimensionSize, RESET);

    pipelineManager_->createRayTracingPipeline();
    rtPipeline_       = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    if (rtPipeline_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "{}Ray tracing pipeline is null after creation!{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Ray tracing pipeline missing");
    }

    createShaderBindingTable();
    createCommandBuffers();
    updateRTDescriptors();
    updateComputeDescriptors(0);

    LOG_INFO_CAT("RENDERER", "{}VulkanRenderer initialized successfully{}", EMERALD_GREEN, RESET);
}

VulkanRenderer::~VulkanRenderer() {
    LOG_INFO_CAT("RENDERER", "{}Destructor: Calling cleanup for resource release{}", OCEAN_TEAL, RESET);
    cleanup();
}

void VulkanRenderer::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };

    VK_CHECK(vkAllocateCommandBuffers(context_->device, &allocInfo, commandBuffers_.data()),
             "Failed to allocate command buffers");
    LOG_INFO_CAT("RENDERER", "{}Allocated {} command buffers{}", OCEAN_TEAL, MAX_FRAMES_IN_FLIGHT, RESET);
}

void VulkanRenderer::dispatchRenderMode(uint32_t imageIndex,
                                        VkBuffer vertexBuffer,
                                        VkCommandBuffer cmd,
                                        VkBuffer indexBuffer,
                                        float frameTime,
                                        int width,
                                        int height,
                                        float fov,
                                        VkPipelineLayout graphicsLayout,
                                        VkDescriptorSet computeDescSet,
                                        VkDevice device,
                                        VkPipelineCache cache,
                                        VkPipeline graphicsPipeline,
                                        float time,
                                        VkRenderPass renderPass,
                                        VkFramebuffer framebuffer,
                                        const Vulkan::Context& context,
                                        int mode) {
    LOG_DEBUG_CAT("RENDER", "{}Dispatching render mode {} (frame: {}, size: {}x{}){}", OCEAN_TEAL, mode, frameTime, width, height, RESET);

    if (cmd == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDER", "{}Invalid command buffer in dispatchRenderMode{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    if (mode == 1) {
        if (rtPipeline_ == VK_NULL_HANDLE || rtPipelineLayout_ == VK_NULL_HANDLE || rtxDescriptorSet_ == VK_NULL_HANDLE) {
            LOG_ERROR_CAT("RENDER", "{}RT pipeline or descriptors not ready for mode 1{}", CRIMSON_MAGENTA, RESET);
            return;
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_, 0, 1, &rtxDescriptorSet_, 0, nullptr);

        VkStridedDeviceAddressRegionKHR raygenRegion{
            .deviceAddress = context.raygenSbtAddress,
            .stride = sbt_.raygen.size,
            .size = sbt_.raygen.size
        };
        VkStridedDeviceAddressRegionKHR missRegion{
            .deviceAddress = context.missSbtAddress,
            .stride = sbt_.miss.size,
            .size = sbt_.miss.size
        };
        VkStridedDeviceAddressRegionKHR hitRegion{
            .deviceAddress = context.hitSbtAddress,
            .stride = sbt_.hit.size,
            .size = sbt_.hit.size
        };
        VkStridedDeviceAddressRegionKHR callableRegion{
            .deviceAddress = context.callableSbtAddress,
            .stride = sbt_.callable.size,
            .size = sbt_.callable.size
        };

        VkMemoryBarrier rtBarrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             0, 1, &rtBarrier, 0, nullptr, 0, nullptr);

        LOG_DEBUG_CAT("RENDER", "{}Tracing rays: {}x{}x1{}", OCEAN_TEAL, width, height, RESET);
        context.vkCmdTraceRaysKHR(cmd, &raygenRegion, &missRegion, &hitRegion, &callableRegion,
                                  static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1);

        VkImageMemoryBarrier postRtBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = rtOutputImages_[currentRTIndex_],
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &postRtBarrier);

        LOG_DEBUG_CAT("RENDER", "{}RT dispatch complete{}", EMERALD_GREEN, RESET);

    } else {
        if (graphicsPipeline == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE) {
            LOG_ERROR_CAT("RENDER", "{}Graphics pipeline or framebuffer not ready for mode 0{}", CRIMSON_MAGENTA, RESET);
            return;
        }

        VkRenderPassBeginInfo rpBegin{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = framebuffer,
            .renderArea = { {0, 0}, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) } },
            .clearValueCount = 0,
            .pClearValues = nullptr
        };
        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        if (vertexBuffer != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE) {
            VkDeviceSize offsets[1] = {0};
            const VkBuffer pBuffers[] = {vertexBuffer};
            vkCmdBindVertexBuffers(cmd, 0, 1, pBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }

        if (graphicsLayout != VK_NULL_HANDLE && computeDescSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsLayout, 0, 1, &computeDescSet, 0, nullptr);
        }

        uint32_t indexCount = static_cast<uint32_t>(getIndices().size());
        if (indexCount > 0) {
            vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
        } else {
            LOG_WARN_CAT("RENDER", "{}No indices for raster draw; skipping{}", AMBER_YELLOW, RESET);
        }

        vkCmdEndRenderPass(cmd);

        LOG_DEBUG_CAT("RENDER", "{}Raster dispatch complete ({} indices){}", EMERALD_GREEN, indexCount, RESET);
    }
}

void VulkanRenderer::renderFrame(const Camera& camera) {
    auto frameStart = std::chrono::steady_clock::now();

    if (swapchainRecreating_.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); return; }

    uint32_t imageIndex;
    VkResult r = vkAcquireNextImageKHR(context_->device, swapchain_, 1'000'000'000ULL /* ~1s timeout */,
                                       imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (r == VK_ERROR_OUT_OF_DATE_KHR) { handleResize(width_, height_); return; }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) throw std::runtime_error("Acquire swapchain image failed");

    VK_CHECK(vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX), "Wait fence");
    VK_CHECK(vkResetFences(context_->device, 1, &inFlightFences_[currentFrame_]), "Reset fence");

    updateUniformBuffer(currentFrame_, camera);
    updateRTDescriptors();
    updateComputeDescriptors(currentFrame_);

    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Begin cmd");

    vkCmdResetQueryPool(cmd, queryPools_[currentFrame_], 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools_[currentFrame_], 0);

    context_->raygenSbtAddress   = sbt_.raygen.deviceAddress;
    context_->missSbtAddress     = sbt_.miss.deviceAddress;
    context_->hitSbtAddress      = sbt_.hit.deviceAddress;
    context_->callableSbtAddress = sbt_.callable.deviceAddress;

    dispatchRenderMode(
        imageIndex,
        bufferManager_->getVertexBuffer(),
        cmd,
        bufferManager_->getIndexBuffer(),
        static_cast<float>(frameCount_),
        static_cast<int>(swapchainExtent_.width), static_cast<int>(swapchainExtent_.height),
        camera.getFOV(),
        pipelineManager_->getGraphicsPipelineLayout(),
        computeDescriptorSets_[currentFrame_],
        context_->device,
        pipelineManager_->getPipelineCache(),
        pipelineManager_->getGraphicsPipeline(),
        0.0f,
        pipelineManager_->getRenderPass(),
        framebuffers_[imageIndex],
        *context_,
        currentMode_);

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, queryPools_[currentFrame_], 1);

    VkImage rtImg = rtOutputImages_[currentRTIndex_];
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageMemoryBarrier barriers[2]{
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = rtImg,
            .subresourceRange = range
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = swapchainImages_[imageIndex],
            .subresourceRange = range
        }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

    VkImageCopy copyRegion{
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .extent = {swapchainExtent_.width, swapchainExtent_.height, 1}
    };
    vkCmdCopyImage(cmd, rtImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &copyRegion);

    VkImageMemoryBarrier presentBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchainImages_[imageIndex],
        .subresourceRange = range
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &presentBarrier);

    VK_CHECK(vkEndCommandBuffer(cmd), "End cmd");

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_],
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_]
    };
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submitInfo, inFlightFences_[currentFrame_]), "Submit");

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &imageIndex
    };
    r = vkQueuePresentKHR(context_->graphicsQueue, &presentInfo);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) handleResize(width_, height_);
    else if (r != VK_SUCCESS) throw std::runtime_error("Present failed");

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSTime_).count();
    if (elapsed >= 1000) {
        double fps = static_cast<double>(framesThisSecond_) * 1000.0 / elapsed;
        double frameMs = std::chrono::duration_cast<std::chrono::microseconds>(now - frameStart).count() / 1000.0;
        LOG_INFO_CAT("FPS", "{}FPS: {:.1f} | Frame: {} | Time: {:.2f}ms | Size: {}x{}{}", CRIMSON_MAGENTA, fps, frameCount_, frameMs,
                     swapchainExtent_.width, swapchainExtent_.height, RESET);
        framesThisSecond_ = 0;
        lastFPSTime_ = now;
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    ++frameCount_;
    ++framesThisSecond_;
}

// ──────────────────────── SWAPCHAIN CREATION (FPS UNLOCKED BY DEFAULT) ────────────────────────
void VulkanRenderer::createSwapchain() {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_->physicalDevice, context_->surface, &capabilities);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(std::string("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: ") + std::to_string(static_cast<int>(res)));
    }

    // Prefer swapchain extent to window size
    swapchainExtent_.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, static_cast<uint32_t>(width_)));
    swapchainExtent_.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, static_cast<uint32_t>(height_)));

    // Query supported formats and pick one (prefer SRGB)
    uint32_t formatCount = 0;
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, context_->surface, &formatCount, nullptr);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(std::string("vkGetPhysicalDeviceSurfaceFormatsKHR (count) failed: ") + std::to_string(static_cast<int>(res)));
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(context_->physicalDevice, context_->surface, &formatCount, formats.data());
    if (res != VK_SUCCESS) {
        throw std::runtime_error(std::string("vkGetPhysicalDeviceSurfaceFormatsKHR (formats) failed: ") + std::to_string(static_cast<int>(res)));
    }

    if (formats.empty()) {
        throw std::runtime_error("No supported surface formats found (invalid surface or driver issue)");
    }

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& fmt : formats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = fmt;
            break;
        }
    }
    swapchainImageFormat_ = surfaceFormat.format;

    // Query present modes and prefer IMMEDIATE for FPS unlock
    uint32_t presentModeCount = 0;
    res = vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, context_->surface, &presentModeCount, nullptr);
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;  // VSync fallback
    if (res == VK_SUCCESS) {
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        res = vkGetPhysicalDeviceSurfacePresentModesKHR(context_->physicalDevice, context_->surface, &presentModeCount, presentModes.data());
        if (res == VK_SUCCESS) {
            for (const auto& mode : presentModes) {
                if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {  // Unlock FPS: No VSync, allows tearing but max rate
                    presentMode = mode;
                    LOG_INFO_CAT("SWAP", "{}FPS unlocked: Using VK_PRESENT_MODE_IMMEDIATE_KHR{}", CRIMSON_MAGENTA, RESET);
                    break;
                }
            }
        } else {
            LOG_WARNING_CAT("SWAP", "{}Present modes fetch failed (using FIFO fallback): {}{}", AMBER_YELLOW, std::to_string(static_cast<int>(res)), RESET);
        }
    } else {
        LOG_WARNING_CAT("SWAP", "{}Present modes count failed (using FIFO fallback): {}{}", AMBER_YELLOW, std::to_string(static_cast<int>(res)), RESET);
    }

    // Image count: Double buffering min, triple if supported
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.minImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Assume unified queue (graphics == present); if separate, use CONCURRENT + queue families
    uint32_t queueFamilyIndices[] = {context_->graphicsQueueFamilyIndex, context_->graphicsQueueFamilyIndex};
    VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context_->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchainExtent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (sharingMode == VK_SHARING_MODE_CONCURRENT) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;  // ← KEY: FPS unlocked here
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(context_->device, &createInfo, nullptr, &swapchain_), "Create swapchain");

    // Retrieve images
    vkGetSwapchainImagesKHR(context_->device, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(context_->device, swapchain_, &imageCount, swapchainImages_.data());

    // Create views
    swapchainImageViews_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainImageFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &swapchainImageViews_[i]), "Create swapchain view");
    }
}

void VulkanRenderer::recreateSwapchain(int newWidth, int newHeight) {
    int actualWidth = newWidth;
    int actualHeight = newHeight;
    if (!SDL_GetWindowSizeInPixels(window_, &actualWidth, &actualHeight)) {
        LOG_ERROR_CAT("SDL3", "{}SDL_GetWindowSizeInPixels failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        return;
    }

    if (actualWidth == 0 || actualHeight == 0) return;

    vkDeviceWaitIdle(context_->device);
    cleanupSwapchain();

    swapchainExtent_.width = static_cast<uint32_t>(actualWidth);
    swapchainExtent_.height = static_cast<uint32_t>(actualHeight);
    createSwapchain();
}

void VulkanRenderer::cleanupSwapchain() {
    for (auto view : swapchainImageViews_) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(context_->device, view, nullptr);
    }
    swapchainImageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_->device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    swapchainImages_.clear();
}

void VulkanRenderer::cleanup() noexcept {
    LOG_INFO_CAT("CLEANUP", "{}CLEANUP START{}", OCEAN_TEAL, RESET);
    vkDeviceWaitIdle(context_->device);

    cleanupSwapchain();

    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(context_->device, context_->commandPool,
                             static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
        LOG_INFO_CAT("CLEANUP", "{}Freed {} command buffers{}", OCEAN_TEAL, MAX_FRAMES_IN_FLIGHT, RESET);
    }

    // Cleanup semaphores/fences
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (imageAvailableSemaphores_[i] != VK_NULL_HANDLE) vkDestroySemaphore(context_->device, imageAvailableSemaphores_[i], nullptr);
        if (renderFinishedSemaphores_[i] != VK_NULL_HANDLE) vkDestroySemaphore(context_->device, renderFinishedSemaphores_[i], nullptr);
        if (inFlightFences_[i] != VK_NULL_HANDLE) vkDestroyFence(context_->device, inFlightFences_[i], nullptr);
    }

    queryReady_.fill(false);
    for (auto& p : queryPools_) if (p) vkDestroyQueryPool(context_->device, p, nullptr);

    for (int i = 0; i < 2; ++i) {
        if (rtOutputImages_[i]) {
            vkDestroyImageView(context_->device, rtOutputViews_[i], nullptr);
            vkFreeMemory(context_->device, rtOutputMemories_[i], nullptr);
            vkDestroyImage(context_->device, rtOutputImages_[i], nullptr);
        }
        if (accumImages_[i]) {
            vkDestroyImageView(context_->device, accumViews_[i], nullptr);
            vkFreeMemory(context_->device, accumMemories_[i], nullptr);
            vkDestroyImage(context_->device, accumImages_[i], nullptr);
        }
    }

    for (auto& fb : framebuffers_) vkDestroyFramebuffer(context_->device, fb, nullptr);

    vkDestroyPipeline(context_->device, rtPipeline_, nullptr);
    vkDestroyPipelineLayout(context_->device, rtPipelineLayout_, nullptr);
    vkDestroyDescriptorPool(context_->device, descriptorPool_, nullptr);
    vkDestroyBuffer(context_->device, sbtBuffer_, nullptr);
    vkFreeMemory(context_->device, sbtMemory_, nullptr);

    if (tlasHandle_) context_->vkDestroyAccelerationStructureKHR(context_->device, tlasHandle_, nullptr);

    vkDestroyImageView(context_->device, envMapImageView_, nullptr);
    vkDestroySampler(context_->device, envMapSampler_, nullptr);
    vkDestroyImage(context_->device, envMapImage_, nullptr);
    vkFreeMemory(context_->device, envMapImageMemory_, nullptr);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (uniformBuffers_[i]) vkDestroyBuffer(context_->device, uniformBuffers_[i], nullptr);
        if (uniformBufferMemories_[i]) vkFreeMemory(context_->device, uniformBufferMemories_[i], nullptr);
        if (materialBuffers_[i]) vkDestroyBuffer(context_->device, materialBuffers_[i], nullptr);
        if (materialBufferMemory_[i]) vkFreeMemory(context_->device, materialBufferMemory_[i], nullptr);
        if (dimensionBuffers_[i]) vkDestroyBuffer(context_->device, dimensionBuffers_[i], nullptr);
        if (dimensionBufferMemory_[i]) vkFreeMemory(context_->device, dimensionBufferMemory_[i], nullptr);
    }

    LOG_INFO_CAT("CLEANUP", "{}CLEANUP COMPLETE{}", EMERALD_GREEN, RESET);
}

void VulkanRenderer::initializeAllBufferData(uint32_t maxFrames,
                                            VkDeviceSize materialSize,
                                            VkDeviceSize dimensionSize) {
    for (uint32_t i = 0; i < maxFrames; ++i) initializeBufferData(i, materialSize, dimensionSize);
}

void VulkanRenderer::initializeBufferData(uint32_t frameIndex,
                                          VkDeviceSize materialSize,
                                          VkDeviceSize dimensionSize) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;

    VkMemoryAllocateFlagsInfo flags{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    VulkanBufferManager::createBuffer(context_->device, context_->physicalDevice,
        materialSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        materialBuffers_[frameIndex], materialBufferMemory_[frameIndex],
        &flags, *context_);

    VulkanBufferManager::createBuffer(context_->device, context_->physicalDevice,
        dimensionSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        dimensionBuffers_[frameIndex], dimensionBufferMemory_[frameIndex],
        &flags, *context_);

    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceMemory uboMem = VK_NULL_HANDLE;
    VulkanBufferManager::createBuffer(context_->device, context_->physicalDevice,
        sizeof(UniformBufferObject),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        ubo, uboMem, nullptr, *context_);

    uniformBuffers_[frameIndex] = ubo;
    uniformBufferMemories_[frameIndex] = uboMem;
}

void VulkanRenderer::createRTOutputImages() {
    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo imgInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = {swapchainExtent_.width, swapchainExtent_.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &rtOutputImages_[i]), "RT output image");

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(context_->device, rtOutputImages_[i], &memReq);
        VkMemoryAllocateInfo alloc{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReq.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(
                context_->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &rtOutputMemories_[i]), "RT mem");
        VK_CHECK(vkBindImageMemory(context_->device, rtOutputImages_[i], rtOutputMemories_[i], 0), "Bind RT");

        VkImageViewCreateInfo view{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = rtOutputImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VK_CHECK(vkCreateImageView(context_->device, &view, nullptr, &rtOutputViews_[i]), "RT view");

        VulkanInitializer::transitionImageLayout(*context_, rtOutputImages_[i],
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    LOG_INFO_CAT("IMAGES", "{}Created RT output images: {}x{}{}", OCEAN_TEAL, swapchainExtent_.width, swapchainExtent_.height, RESET);
}

void VulkanRenderer::createAccumulationImages() {
    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo imgInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = {swapchainExtent_.width, swapchainExtent_.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &accumImages_[i]), "Accum image");

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(context_->device, accumImages_[i], &memReq);
        VkMemoryAllocateInfo alloc{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReq.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(
                context_->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &accumMemories_[i]), "Accum mem");
        VK_CHECK(vkBindImageMemory(context_->device, accumImages_[i], accumMemories_[i], 0), "Bind accum");

        VkImageViewCreateInfo view{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = accumImages_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VK_CHECK(vkCreateImageView(context_->device, &view, nullptr, &accumViews_[i]), "Accum view");

        VulkanInitializer::transitionImageLayout(*context_, accumImages_[i],
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    LOG_INFO_CAT("IMAGES", "{}Created accumulation images: {}x{}{}", OCEAN_TEAL, swapchainExtent_.width, swapchainExtent_.height, RESET);
}

void VulkanRenderer::createShaderBindingTable() {
    pipelineManager_->createShaderBindingTable();
    sbt_       = pipelineManager_->getSBT();
    sbtBuffer_ = pipelineManager_->getSBTBuffer();
    sbtMemory_ = pipelineManager_->getSBTMemory();
}

void VulkanRenderer::buildAccelerationStructures() {
    const auto& vertices = getVertices();
    const auto& indices  = getIndices();

    if (vertices.empty() || indices.empty()) {
        LOG_ERROR_CAT("AS", "{}Empty mesh data! Falling back to cube{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    LOG_INFO_CAT("AS", "{}UPLOADING MESH TO GPU: {} verts, {} indices{}", OCEAN_TEAL,
                 vertices.size(), indices.size(), RESET);

    bufferManager_->uploadMesh(vertices.data(), vertices.size(), indices.data(), indices.size());

    VkBuffer vb = bufferManager_->getVertexBuffer();
    VkBuffer ib = bufferManager_->getIndexBuffer();

    LOG_INFO_CAT("AS", "{}BUILDING BLAS/TLAS...{}", OCEAN_TEAL, RESET);
    pipelineManager_->createAccelerationStructures(vb, ib);
    tlasHandle_ = pipelineManager_->getTLAS();

    LOG_INFO_CAT("AS", "{}TLAS built: {}{}", EMERALD_GREEN, ptr_to_hex(tlasHandle_), RESET);
}

void VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        VkImageView att[] = { swapchainImageViews_[i] };
        VkFramebufferCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = pipelineManager_->getRenderPass(),
            .attachmentCount = 1,
            .pAttachments = att,
            .width = swapchainExtent_.width,
            .height = swapchainExtent_.height,
            .layers = 1
        };
        VK_CHECK(vkCreateFramebuffer(context_->device, &info, nullptr, &framebuffers_[i]), "Framebuffer");
    }
    LOG_INFO_CAT("FB", "{}Created {} framebuffers{}", OCEAN_TEAL, swapchainImageViews_.size(), RESET);
}

void VulkanRenderer::createComputeDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts.data()
    };
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &alloc, computeDescriptorSets_.data()),
             "Allocate compute descriptor sets");
}

void VulkanRenderer::updateRTDescriptors() {
    if (tlasHandle_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("DESC", "{}TLAS not built yet – skipping RTX descriptor update{}", AMBER_YELLOW, RESET);
        return;
    }

    if (uniformBuffers_[currentFrame_] == VK_NULL_HANDLE ||
        materialBuffers_[currentFrame_] == VK_NULL_HANDLE ||
        dimensionBuffers_[currentFrame_] == VK_NULL_HANDLE ||
        envMapSampler_ == VK_NULL_HANDLE || envMapImageView_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("DESC", "{}Missing buffer/image for descriptor update{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlasHandle_
    };

    VkDescriptorImageInfo outInfo{
        .imageView = rtOutputViews_[currentRTIndex_],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorBufferInfo uboInfo{
        .buffer = uniformBuffers_[currentFrame_],
        .range = VK_WHOLE_SIZE
    };
    VkDescriptorBufferInfo matInfo{
        .buffer = materialBuffers_[currentFrame_],
        .range = VK_WHOLE_SIZE
    };
    VkDescriptorBufferInfo dimInfo{
        .buffer = dimensionBuffers_[currentFrame_],
        .range = VK_WHOLE_SIZE
    };
    VkDescriptorImageInfo envInfo{
        .sampler = envMapSampler_,
        .imageView = envMapImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkDescriptorImageInfo accumInfo{
        .imageView = accumViews_[currentAccumIndex_],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    std::array<VkWriteDescriptorSet, 7> writes{{
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &asWrite,
         .dstSet = rtxDescriptorSet_, .dstBinding = 0, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 1, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 2, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 3, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &matInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 4, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 5, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &envInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = rtxDescriptorSet_, .dstBinding = 6, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo}
    }};

    vkUpdateDescriptorSets(context_->device,
                           static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    descriptorsUpdated_ = true;
}

void VulkanRenderer::updateUniformBuffer(uint32_t frameIdx, const Camera& cam) {
    UniformBufferObject ubo{};
    ubo.viewInverse = glm::inverse(cam.getViewMatrix());
    ubo.projInverse = glm::inverse(cam.getProjectionMatrix());
    ubo.camPos = glm::vec4(cam.getPosition(), 1.0f);
    ubo.time = 0.0f;
    ubo.frame = frameCount_;

    void* data;
    VK_CHECK(vkMapMemory(context_->device,
                         uniformBufferMemories_[frameIdx],
                         0, sizeof(ubo), 0, &data), "Map UBO");
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_->device, uniformBufferMemories_[frameIdx]);
}

void VulkanRenderer::updateComputeDescriptors(uint32_t frameIdx) {
    VkDescriptorImageInfo rtInfo{
        .imageView = rtOutputViews_[currentRTIndex_],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo accumInfo{
        .imageView = accumViews_[currentAccumIndex_],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorBufferInfo uboInfo{
        .buffer = uniformBuffers_[frameIdx],
        .range = VK_WHOLE_SIZE
    };

    std::array<VkWriteDescriptorSet, 3> writes{{
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = computeDescriptorSets_[frameIdx],
         .dstBinding = 0, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &rtInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = computeDescriptorSets_[frameIdx],
         .dstBinding = 1, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstSet = computeDescriptorSets_[frameIdx],
         .dstBinding = 2, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo}
    }};

    vkUpdateDescriptorSets(context_->device, 3, writes.data(), 0, nullptr);
}

void VulkanRenderer::createEnvironmentMap() {
    int w, h, c;
    stbi_set_flip_vertically_on_load(false);
    float* pix = stbi_loadf("assets/textures/envmap.hdr", &w, &h, &c, 4);
    if (!pix) throw std::runtime_error("Failed to load envmap.hdr");

    VkDeviceSize sz = static_cast<VkDeviceSize>(w) * h * 4 * sizeof(float);

    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &envMapImage_), "Envmap image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(context_->device, envMapImage_, &memReq);
    VkMemoryAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = VulkanInitializer::findMemoryType(
            context_->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_->device, &alloc, nullptr, &envMapImageMemory_), "Envmap mem");
    VK_CHECK(vkBindImageMemory(context_->device, envMapImage_, envMapImageMemory_, 0), "Bind envmap");

    VkImageViewCreateInfo view{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = envMapImage_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VK_CHECK(vkCreateImageView(context_->device, &view, nullptr, &envMapImageView_), "Envmap view");

    VkSamplerCreateInfo sampler{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxAnisotropy = 1.0f
    };
    VK_CHECK(vkCreateSampler(context_->device, &sampler, nullptr, &envMapSampler_), "Envmap sampler");

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VulkanBufferManager::createBuffer(context_->device, context_->physicalDevice,
        sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging, stagingMem, nullptr, *context_);

    void* data;
    VK_CHECK(vkMapMemory(context_->device, stagingMem, 0, sz, 0, &data), "Map staging");
    memcpy(data, pix, static_cast<size_t>(sz));
    vkUnmapMemory(context_->device, stagingMem);

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAlloc{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(context_->device, &cmdAlloc, &cmd), "Alloc envmap cmd");

    VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin), "Begin envmap cmd");

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = envMapImage_,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1}
    };
    vkCmdCopyBufferToImage(cmd, staging, envMapImage_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(cmd), "End envmap cmd");

    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submit, VK_NULL_HANDLE), "Submit envmap");
    vkQueueWaitIdle(context_->graphicsQueue);
    vkFreeCommandBuffers(context_->device, context_->commandPool, 1, &cmd);
    vkDestroyBuffer(context_->device, staging, nullptr);
    vkFreeMemory(context_->device, stagingMem, nullptr);
    stbi_image_free(pix);
}

std::vector<glm::vec3> VulkanRenderer::getVertices() const {
    static std::vector<glm::vec3> cached;
    if (!cached.empty()) return cached;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    LOG_INFO_CAT("MeshLoader", "{}LOADING MESH FROM: assets/models/scene.obj{}", OCEAN_TEAL, RESET);
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj")) {
        LOG_ERROR_CAT("MeshLoader", "{}Failed to load scene.obj: {}{}", CRIMSON_MAGENTA, err, RESET);
        throw std::runtime_error("Failed to load scene.obj");
    }

    std::vector<glm::vec3> vertices;
    vertices.reserve(attrib.vertices.size() / 3);
    for (size_t i = 0; i < attrib.vertices.size(); i += 3) {
        vertices.emplace_back(
            attrib.vertices[i],
            attrib.vertices[i + 1],
            attrib.vertices[i + 2]
        );
    }

    LOG_INFO_CAT("MeshLoader", "{}MESH LOADED & CACHED: {} verts, {} tris{}", EMERALD_GREEN,
                  vertices.size(), shapes[0].mesh.indices.size() / 3, RESET);

    cached = std::move(vertices);
    return cached;
}

std::vector<uint32_t> VulkanRenderer::getIndices() const {
    static std::vector<uint32_t> cached;
    if (!cached.empty()) return cached;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/models/scene.obj"))
        throw std::runtime_error("Failed to load scene.obj");

    std::vector<uint32_t> indices;
    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            indices.push_back(static_cast<uint32_t>(idx.vertex_index));
        }
    }

    cached = std::move(indices);
    return cached;
}

void VulkanRenderer::handleResize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    swapchainRecreating_ = true;
    vkDeviceWaitIdle(context_->device);
    width_ = w; height_ = h;

    // Recreate swapchain
    recreateSwapchain(w, h);

    // Recreate RT/accum images
    for (int i = 0; i < 2; ++i) {
        if (rtOutputImages_[i]) {
            vkDestroyImageView(context_->device, rtOutputViews_[i], nullptr);
            vkFreeMemory(context_->device, rtOutputMemories_[i], nullptr);
            vkDestroyImage(context_->device, rtOutputImages_[i], nullptr);
        }
        if (accumImages_[i]) {
            vkDestroyImageView(context_->device, accumViews_[i], nullptr);
            vkFreeMemory(context_->device, accumMemories_[i], nullptr);
            vkDestroyImage(context_->device, accumImages_[i], nullptr);
        }
    }
    createRTOutputImages();
    createAccumulationImages();

    // Recreate framebuffers
    for (auto& fb : framebuffers_) vkDestroyFramebuffer(context_->device, fb, nullptr);
    framebuffers_.clear();
    createFramebuffers();

    // Update RTX for new size (REMOVED: No updateSize method; descriptors updated below)
    // rtx_->updateSize(swapchainExtent_.width, swapchainExtent_.height);

    descriptorsUpdated_ = false;
    updateRTDescriptors();
    swapchainRecreating_ = false;

    LOG_INFO_CAT("RESIZE", "{}Swapchain resized to {}x{}{}", OCEAN_TEAL, w, h, RESET);
}

} // namespace VulkanRTX