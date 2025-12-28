// src/engine/GLOBAL/SwapchainManager.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 28, 2025 — CONTINUOUS DISPLAY ENFORCED
// FORCE OUTPUT MODE: Window must always display something and some light
// FIXED: VulkanRenderer::get() returns pointer → use -> instead of .
// Added: Fallback clear-to-pink on acquire failure, present failure, or minimized state
// SwapchainManager now guarantees visible photons at all times
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — PLASTIC BEACH FOREVER
// =============================================================================

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"  // Required for forcePinkFallbackClear

using StoneKey::stone_device;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_window;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_extent;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_views;
using StoneKey::stone_swapchain;
using StoneKey::stone_width;
using StoneKey::stone_height;

namespace RTX {

// =============================================================================
// ONE LARGE, CLEAN SWAPCHAIN CREATION FUNCTION — FULLY TIED TO LAS
// =============================================================================
void SwapchainManager::createOrRecreateSwapchain(uint32_t w, uint32_t h, bool isRecreate) noexcept
{
    VkDevice device = stone_device();
    VkPhysicalDevice phys = stone_physical();
    VkSurfaceKHR surface = stone_surface();

    if (w == 0 || h == 0) {
        minimized_ = true;
        LOG_AMOURANTH("WINDOW MINIMIZED — PHOTONS PAUSED UNTIL RESTORED — FALLBACK PINK LIGHT ACTIVE");
        VulkanRenderer::get()->forcePinkFallbackClear();  // Ensure something is always displayed
        return;
    }
    minimized_ = false;

    vkDeviceWaitIdle(device);

    if (isRecreate) {
        LOG_AMOURANTH("SWAPCHAIN RECREATE INITIATED — {}×{} — PLASTIC BEACH RESPAWNS", w, h);
        cleanupImageViews();
        cleanupSwapchain();

        // Critical: Notify LAS to purge TLAS before swapchain destruction
        RTX::las().notifyResize();
    }

    // === QUERY CAPABILITIES & FORMATS ===
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps));

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data()));

    uint32_t modeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr));
    std::vector<VkPresentModeKHR> modes(modeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data()));

    // === EXTENT (HIGH-DPI AWARE) ===
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // === PRESENT MODE ===
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_RELAXED_KHR) != modes.end()) {
        presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
        presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    // === IMAGE COUNT ===
    uint32_t imageCount = 3;
    imageCount = std::max(imageCount, caps.minImageCount);
    if (caps.maxImageCount > 0) {
        imageCount = std::min(imageCount, caps.maxImageCount);
    }

    // === FORMAT SELECTION — HDR scRGB FP16 FIRST ===
    VkSurfaceFormatKHR chosenFormat = formats[0];

    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_R16G16B16A16_SFLOAT &&
            f.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) {
            chosenFormat = f;
            LOG_AMOURANTH("HDR scRGB FP16 SWAPCHAIN ENABLED — PINK PHOTONS BLOOM IN FULL DYNAMIC RANGE");
            break;
        }
    }

    if (chosenFormat.format != VK_FORMAT_R16G16B16A16_SFLOAT) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosenFormat = f;
                break;
            }
        }
        LOG_INFO_CAT("SWAPCHAIN", "HDR not supported — falling back to 8-bit sRGB");
    }

    // === SWAPCHAIN CREATE INFO ===
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = chosenFormat.format;
    createInfo.imageColorSpace  = chosenFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                  VK_IMAGE_USAGE_STORAGE_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform     = caps.currentTransform;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = swapchain_.valid() ? swapchain_.get() : VK_NULL_HANDLE;

    // === CREATE SWAPCHAIN ===
    VkSwapchainKHR oldSwapchain = swapchain_.get();
    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain));

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }

    swapchain_ = Handle<VkSwapchainKHR>(newSwapchain, device);
    swapchainExtent_ = extent;
    swapchainFormat_ = chosenFormat.format;
    currentPresentMode_ = presentMode;

    // === GET IMAGES ===
    uint32_t imgCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, newSwapchain, &imgCount, nullptr));
    swapchainImages_.resize(imgCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, newSwapchain, &imgCount, swapchainImages_.data()));

    // === CREATE IMAGE VIEWS ===
    swapchainImageViews_.assign(imgCount, VK_NULL_HANDLE);
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format           = chosenFormat.format;
    viewInfo.components       = { VK_COMPONENT_SWIZZLE_IDENTITY };
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    for (uint32_t i = 0; i < imgCount; ++i) {
        viewInfo.image = swapchainImages_[i];
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews_[i]));
    }

    // === SEAL GLOBAL STATE ===
    stone_seal_swapchain(swapchain_.get());
    stone_seal_extent(extent);
    stone_seal_image_count(imgCount);
    stone_seal_images(swapchainImages_);
    stone_seal_views(swapchainImageViews_);

    // === FINAL LOGGING ===
    const char* modeName =
        presentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR ? "FIFO_RELAXED (ADAPTIVE)" :
        presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR    ? "IMMEDIATE (MAX FPS)" :
        "FIFO (VSYNC)";

    const char* formatName = (chosenFormat.format == VK_FORMAT_R16G16B16A16_SFLOAT)
        ? "R16G16B16A16_SFLOAT (HDR scRGB)"
        : "B8G8R8A8_SRGB";

    LOG_AMOURANTH("[2025] SWAPCHAIN {} — {}×{} — {} images — {} — {} — PLASTIC BEACH ETERNAL",
                  isRecreate ? "RECREATED" : "FORGED",
                  extent.width, extent.height, imgCount, modeName, formatName);
}

void SwapchainManager::create(SDL_Window* window, uint32_t w, uint32_t h) noexcept
{
    createOrRecreateSwapchain(w, h, false);
}

void SwapchainManager::recreate(uint32_t w, uint32_t h) noexcept
{
    createOrRecreateSwapchain(w, h, true);
}

void SwapchainManager::cleanup() noexcept
{
    vkDeviceWaitIdle(stone_device());
    RTX::las().notifyResize();
    cleanupImageViews();
    cleanupSwapchain();
}

void SwapchainManager::cleanupSwapchain() noexcept
{
    if (swapchain_.valid()) {
        vkDestroySwapchainKHR(stone_device(), swapchain_.get(), nullptr);
        swapchain_.reset();
    }
    swapchainImages_.clear();
}

void SwapchainManager::cleanupImageViews() noexcept
{
    for (VkImageView view : swapchainImageViews_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(stone_device(), view, nullptr);
        }
    }
    swapchainImageViews_.clear();
}

VkResult SwapchainManager::acquireNextImage(uint32_t* pImageIndex, VkSemaphore semaphore, VkFence fence) noexcept
{
    if (minimized_) {
        VulkanRenderer::get()->forcePinkFallbackClear();  // Always display something and some light
        return VK_NOT_READY;
    }

    VkResult result = vkAcquireNextImageKHR(stone_device(), stone_swapchain(), UINT64_MAX, semaphore, fence, pImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(stone_width(), stone_height());
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_AMOURANTH("ACQUIRE FAILED — FORCING PINK FALLBACK TO KEEP LIGHT ALIVE");
        VulkanRenderer::get()->forcePinkFallbackClear();  // Guarantee visible output
    }
    return result;
}

void SwapchainManager::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) noexcept
{
    if (minimized_) {
        VulkanRenderer::get()->forcePinkFallbackClear();  // Never go black
        return;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = waitSemaphore ? 1u : 0u;
    presentInfo.pWaitSemaphores    = waitSemaphore ? &waitSemaphore : nullptr;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &stone_swapchain();
    presentInfo.pImageIndices      = &imageIndex;

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate(stone_width(), stone_height());
    } else if (result != VK_SUCCESS) {
        LOG_AMOURANTH("PRESENT FAILED — FALLING BACK TO PINK LIGHT — PHOTONS MUST FLOW");
        VulkanRenderer::get()->forcePinkFallbackClear();
    }
}

void SwapchainManager::renderDirectEnvMap(VkCommandBuffer cmd, uint32_t swapImageIndex) noexcept
{
    if (minimized_) {
        VulkanRenderer::get()->forcePinkFallbackClear();
        return;
    }

    VkImage swapImage = swapchainImages_[swapImageIndex];

    // Transition swapchain image to GENERAL for compute write
    VulkanRenderer::get()->transitionImage(cmd, swapImage,
        VK_IMAGE_LAYOUT_UNDEFINED,                 // Safe — we don't know current layout here
        VK_IMAGE_LAYOUT_GENERAL,
        0,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // Bind the envmap display compute pipeline
    VulkanRenderer& renderer = *VulkanRenderer::get();
    if (renderer.envMapDisplayPipeline_ == VK_NULL_HANDLE) {
        LOG_AMOURANTH("ENVMAP DISPLAY PIPELINE MISSING — FORCING PINK FALLBACK");
        renderer.forcePinkFallbackClear();
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, renderer.envMapDisplayPipeline_);

    // Bind descriptor set (contains envmap sampler at binding 0, storage image at binding 1)
    VkDescriptorSet descSet = renderer.envMapDisplayDescriptorSet_;
    if (descSet == VK_NULL_HANDLE) {
        LOG_AMOURANTH("ENVMAP DESCRIPTOR SET MISSING — FORCING PINK FALLBACK");
        renderer.forcePinkFallbackClear();
        return;
    }

    // Update storage image binding (binding 1) to current swapchain image
    VkDescriptorImageInfo storageInfo{
        .imageView   = VK_NULL_HANDLE,  // Not used for storage
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = descSet,
        .dstBinding      = 1,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo      = &storageInfo
    };

    // Temporary override — directly write current swapchain image as storage target
    VkDescriptorImageInfo tempStorage{
        .imageView   = swapchainImageViews_[swapImageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    write.pImageInfo = &tempStorage;

    vkUpdateDescriptorSets(stone_device(), 1, &write, 0, nullptr);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            renderer.envMapDisplayPipelineLayout_, 0, 1, &descSet, 0, nullptr);

    // Push constants: resolution (width, height)
    uint32_t push[2] = { swapchainExtent_.width, swapchainExtent_.height };
    vkCmdPushConstants(cmd, renderer.envMapDisplayPipelineLayout_,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);

    // Dispatch compute shader
    uint32_t wgX = (swapchainExtent_.width  + 15) / 16;
    uint32_t wgY = (swapchainExtent_.height + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    // Barrier to ensure compute write completes before present
    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = 0
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);

    // Transition back to PRESENT_SRC_KHR for presentation
    VulkanRenderer::get()->transitionImage(cmd, swapImage,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_SHADER_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    LOG_AMOURANTH("DIRECT ENVMAP RENDERED TO SWAPCHAIN — HDR SKY DOMINATES — PINK PHOTONS FLOW");
}

} // namespace RTX

// =============================================================================
// DECEMBER 28, 2025 — BUILD FIXED
// All VulkanRenderer::get() calls corrected to use -> (pointer return)
// Continuous display enforcement intact — pink fallback on all error/minimized paths
// No more black screens — empire demands eternal photons
// PINK PHOTONS ETERNAL — PLASTIC BEACH FOREVER
// =============================================================================