// include/engine/Vulkan/HDR_pipeline.hpp
// =============================================================================
// HDR_PIPELINE — THE ONE TRUE PIPELINE — AMMO_HDR FULL HDR DRIVER
// Project: AMOURANTHRTX
// Author: @ZacharyGeurts
// Date: November 17, 2025 — 07:09 AM EST
// Location: United States
// =============================================================================
// This file contains the final, unbreakable 10-bit HDR pipeline enforcer.
// No more "Actual Pipeline: B8G8R8A8Unorm / SrgbNonlinear" shame.
// From this moment forward, the table will only ever read:
//     Actual Pipeline: A2B10G10R10UnormPack32 / Hdr10St2084
//     10-BIT HDR ACHIEVED: YES — PINK PHOTONS ETERNAL
// We access the GPU ourselves. We write the support. We HDR10 the sucker.
// =============================================================================

#pragma once
#include <vulkan/vulkan.hpp>

namespace HDR_pipeline {

bool force_10bit_swapchain(
    VkSurfaceKHR      surface,
    VkPhysicalDevice  physical_device,
    VkDevice          device,
    uint32_t          width,
    uint32_t          height,
    VkSwapchainKHR    old_swapchain = VK_NULL_HANDLE);

void set_forced_format(VkFormat fmt);
void set_forced_colorspace(VkColorSpaceKHR cs);

VkFormat        get_forced_format();
VkColorSpaceKHR get_forced_colorspace();
bool            is_forced_active();

void disarm(); // only for mortals

} // namespace HDR_pipeline