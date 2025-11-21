// src/modes/RenderMode4.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "modes/RenderMode4.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

using namespace Engine;
using namespace Logging::Color;

RenderMode4::RenderMode4(VulkanRTX& rtx, uint32_t width, uint32_t height)
    : rtx_(rtx), width_(width), height_(height) {
    LOG_INFO_CAT("RenderMode4", "VALHALLA MODE 4 INIT — {}×{} — CAMERA TINT ENGAGED", PLASMA_FUCHSIA, width, height, RESET);
    initResources();
    LOG_SUCCESS_CAT("RenderMode4", "Mode 4 Initialized — {}×{} — Position-Based Tint", ELECTRIC_BLUE, width, height, RESET);
}

RenderMode4::~RenderMode4() {
    LOG_INFO_CAT("RenderMode4", "Destructor invoked — Safe cleanup");

    vkDeviceWaitIdle(RTX::g_ctx().device());

    rtx_.updateRTXDescriptors(0,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    LOG_SUCCESS_CAT("RenderMode4", "Mode 4 destroyed — CAMERA PHOTONS ETERNAL");
}

void RenderMode4::initResources() {
    LOG_INFO_CAT("RenderMode4", "initResources() — Creating output image only");
    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.device();

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImg;
    LOG_INFO_CAT("RenderMode4", "Creating output image: {}×{} | Format: R8G8B8A8_UNORM", width_, height_);
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg), "Output image creation");
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "OutputImage");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView rawView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView), "Output view creation");
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "OutputView");

    LOG_INFO_CAT("RenderMode4", "Updating RTX descriptors for frame 0 (output only)");
    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, *outputView_, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
    LOG_SUCCESS_CAT("RenderMode4", "initResources complete — Ready for camera-tinted clears");
}

void RenderMode4::renderFrame(VkCommandBuffer cmd, float deltaTime) {
    LOG_DEBUG_CAT("RenderMode4", "renderFrame() — Delta: {:.3f}ms", deltaTime * 1000.0f);
    clearCameraTinted(cmd);
}

glm::vec3 RenderMode4::normalizePosition(const glm::vec3& pos) {
    const float offset = 10.0f;
    const float scale  = 20.0f;
    return glm::clamp((pos + glm::vec3(offset)) / scale, 0.0f, 1.0f);
}

void RenderMode4::clearCameraTinted(VkCommandBuffer cmd) {
    glm::vec3 camPos = g_lazyCam.pos();
    glm::vec3 tint   = normalizePosition(camPos);

    LOG_DEBUG_CAT("RenderMode4", "Clear color: R={:.3f} G={:.3f} B={:.3f} | CamPos: ({:.1f}, {:.1f}, {:.1f})",
                  tint.x, tint.y, tint.z, camPos.x, camPos.y, camPos.z);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = *outputImage_;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearColorValue color{{tint.x, tint.y, tint.z, 1.0f}};
    vkCmdClearColorImage(cmd, *outputImage_, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &barrier.subresourceRange);
}

void RenderMode4::onResize(uint32_t width, uint32_t height) {
    LOG_INFO_CAT("RenderMode4", "onResize() — New: {}×{} (old: {}×{})", width, height, width_, height_);

    auto& ctx = RTX::g_ctx();
    VkDevice device = ctx.device();
    VK_CHECK(vkDeviceWaitIdle(device), "vkDeviceWaitIdle failed in onResize");

    rtx_.updateRTXDescriptors(0, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);

    outputImage_.reset();
    outputView_.reset();

    width_  = width;
    height_ = height;

    initResources();
    LOG_SUCCESS_CAT("RenderMode4", "Resize complete — Camera tint will adapt");
}