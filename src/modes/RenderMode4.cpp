// =============================================================================
// src/modes/RenderMode4.cpp
// AMOURANTH RTX © 2025 — Camera-Tinted Clear — Based on RenderMode1
// PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED
// =============================================================================

#include "modes/RenderMode4.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"     // CAM macro
#include "engine/GLOBAL/StoneKey.hpp"   // stone_device()
#include "engine/GLOBAL/BufferManager.hpp"

using namespace Engine;
using namespace Logging::Color;

RenderMode4::RenderMode4(uint32_t width, uint32_t height)
    : width_(width), height_(height)
{
    LOG_SUCCESS_CAT("RenderMode4", "Camera-Tinted Clear Mode ACTIVE — {}×{}", width, height);
    initResources();
}

RenderMode4::~RenderMode4()
{
    LOG_INFO_CAT("RenderMode4", "Destructor — cleaning up");
    cleanupResources();
}

void RenderMode4::initResources()
{
    cleanupResources();

    VkDevice device = stone_device();

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImg;
    VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &rawImg));
    outputImage_ = RTX::Handle<VkImage>(rawImg, device, vkDestroyImage, 0, "Mode4_OutputImage");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, rawImg, &memReqs);
    uint32_t memType = stone_pipeline()->findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = memReqs.size;
    alloc.memoryTypeIndex = memType;

    VkDeviceMemory mem;
    VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &mem));
    vkBindImageMemory(device, rawImg, mem, 0);
    outputMem_ = RTX::Handle<VkDeviceMemory>(mem, device, vkFreeMemory, memReqs.size, "Mode4_OutputMem");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imgInfo.format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageView rawView;
    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &rawView));
    outputView_ = RTX::Handle<VkImageView>(rawView, device, vkDestroyImageView, 0, "Mode4_OutputView");

    LOG_SUCCESS_CAT("RenderMode4", "Resources initialized — ready for camera-tinted clears");
}

void RenderMode4::cleanupResources()
{
    vkDeviceWaitIdle(stone_device());

    outputView_.reset();
    outputImage_.reset();
    outputMem_.reset();
}

void RenderMode4::onResize(uint32_t width, uint32_t height)
{
    if (width == width_ && height == height_) return;

    LOG_INFO_CAT("RenderMode4", "Resize → {}×{}", width, height);
    width_ = width;
    height_ = height;

    cleanupResources();
    initResources();
}

glm::vec3 RenderMode4::normalizePosition(glm::vec3 pos) const
{
    const float offset = 10.0f;
    const float scale  = 20.0f;
    return glm::clamp((pos + glm::vec3(offset)) / scale, 0.0f, 1.0f);
}

void RenderMode4::clearCameraTinted(VkCommandBuffer cmd)
{
    glm::vec3 pos = CAM.pos();
    glm::vec3 tint = normalizePosition(pos);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = outputImage_.get();
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearColorValue clearColor{{tint.r, tint.g, tint.b, 1.0f}};
    vkCmdClearColorImage(cmd, outputImage_.get(), VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &barrier.subresourceRange);

    // Back to shader read
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void RenderMode4::renderFrame(VkCommandBuffer cmd, float)
{
    clearCameraTinted(cmd);
}