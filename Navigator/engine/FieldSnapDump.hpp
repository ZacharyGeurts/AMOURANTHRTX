#pragma once

// GPU HDR framebuffer readback → PPM (for AmouranthOS visual QA / OCR).

#include "AMOURANTHRTX.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace FieldSnapDump {

inline bool dumpHdrImagePpm(VkImage image, std::uint32_t w, std::uint32_t h,
                            const char* path) noexcept {
    if (!image || !path || w == 0u || h == 0u) return false;

    const VkDeviceSize bufSize = static_cast<VkDeviceSize>(w) * h * 16u;
    VkBufferCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sci.size = bufSize;
    sci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer staging = VK_NULL_HANDLE;
    if (vkCreateBuffer(rtx().device, &sci, nullptr, &staging) != VK_SUCCESS)
        return false;

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(rtx().device, staging, &req);
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = Memory::findMemoryType(
        req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(rtx().device, &mai, nullptr, &mem) != VK_SUCCESS) {
        vkDestroyBuffer(rtx().device, staging, nullptr);
        return false;
    }
    vkBindBufferMemory(rtx().device, staging, mem, 0);

    VkCommandBuffer cmd = beginTransientCommandBuffer();
    if (!cmd) {
        vkDestroyBuffer(rtx().device, staging, nullptr);
        vkFreeMemory(rtx().device, mem, nullptr);
        return false;
    }

    VkImageMemoryBarrier toSrc{};
    toSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSrc.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSrc.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSrc.image = image;
    toSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSrc);

    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {w, h, 1};
    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &copy);

    VkImageMemoryBarrier back{};
    back.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    back.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    back.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    back.image = image;
    back.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &back);

    endSubmitAndWait(cmd);

    void* mapped = nullptr;
    vkMapMemory(rtx().device, mem, 0, bufSize, 0, &mapped);
    const auto* px = static_cast<const float*>(mapped);

    std::FILE* fp = std::fopen(path, "wb");
    if (!fp) {
        vkUnmapMemory(rtx().device, mem);
        vkDestroyBuffer(rtx().device, staging, nullptr);
        vkFreeMemory(rtx().device, mem, nullptr);
        return false;
    }
    std::fprintf(fp, "P6\n%u %u\n255\n", w, h);
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::uint32_t row = h - 1u - y;
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::size_t i = (static_cast<std::size_t>(row) * w + x) * 4u;
            auto tonemap = [](float c) -> std::uint8_t {
                c = c / (1.f + c);
                c = std::clamp(c, 0.f, 1.f);
                return static_cast<std::uint8_t>(c * 255.f + 0.5f);
            };
            const std::uint8_t rgb[3] = {
                tonemap(px[i]), tonemap(px[i + 1u]), tonemap(px[i + 2u])};
            std::fwrite(rgb, 1, 3, fp);
        }
    }
    std::fclose(fp);
    vkUnmapMemory(rtx().device, mem);
    vkDestroyBuffer(rtx().device, staging, nullptr);
    vkFreeMemory(rtx().device, mem, nullptr);
    std::fprintf(stderr, "[SNAP] wrote %s (%ux%u)\n", path, w, h);
    return true;
}

} // namespace FieldSnapDump