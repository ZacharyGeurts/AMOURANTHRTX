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
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * w + x) * 4u;
            // Shader finishHudColor already applies displayMap; clamp only.
            auto tonemap = [](float c) -> std::uint8_t {
                return static_cast<std::uint8_t>(std::clamp(c, 0.f, 1.f) * 255.f + 0.5f);
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

inline bool dumpGuestVgaPpm(const std::uint8_t* ram, std::uint32_t fbBase,
        int fbW, int fbH, const char* path) noexcept {
    if (!ram || !path || fbW <= 0 || fbH <= 0) return false;
    std::FILE* fp = std::fopen(path, "wb");
    if (!fp) return false;
    std::fprintf(fp, "P6\n%d %d\n255\n", fbW, fbH);
    static const std::uint8_t kVgaRgb[256][3] = {
        {0,0,0},{0,0,42},{0,42,0},{0,42,42},{42,0,0},{42,0,42},{42,21,0},{42,42,42},
        {21,21,21},{21,21,63},{21,63,21},{21,63,63},{63,21,21},{63,21,63},{63,63,21},{63,63,63},
        {0,0,0},{5,5,5},{8,8,8},{11,11,11},{14,14,14},{17,17,17},{20,20,20},{24,24,24},
        {28,28,28},{32,32,32},{36,36,36},{40,40,40},{45,45,45},{50,50,50},{55,55,55},{60,60,60},
        {65,65,65},{71,71,71},{77,77,77},{83,83,83},{89,89,89},{96,96,96},{103,103,103},{110,110,110},
        {117,117,117},{125,125,125},{133,133,133},{141,141,141},{149,149,149},{158,158,158},{167,167,167},{176,176,176},
        {185,185,185},{194,194,194},{203,203,203},{213,213,213},{223,223,223},{233,233,233},{243,243,243},{255,255,255},
        {0,0,63},{0,16,63},{0,32,63},{0,48,63},{0,64,63},{0,80,63},{0,96,63},{0,112,63},
        {16,0,63},{32,0,63},{48,0,63},{64,0,63},{80,0,63},{96,0,63},{112,0,63},{128,0,63},
        {63,0,0},{63,16,0},{63,32,0},{63,48,0},{63,64,0},{63,80,0},{63,96,0},{63,112,0},
        {63,0,16},{63,0,32},{63,0,48},{63,0,64},{63,0,80},{63,0,96},{63,0,112},{63,0,128},
        {0,63,0},{16,63,0},{32,63,0},{48,63,0},{64,63,0},{80,63,0},{96,63,0},{112,63,0},
        {0,63,16},{0,63,32},{0,63,48},{0,63,64},{0,63,80},{0,63,96},{0,63,112},{0,63,128},
        {63,63,0},{80,63,0},{96,63,0},{112,63,0},{128,63,0},{144,63,0},{160,63,0},{176,63,0},
        {63,80,0},{63,96,0},{63,112,0},{63,128,0},{63,144,0},{63,160,0},{63,176,0},{63,192,0},
        {0,63,63},{16,63,63},{32,63,63},{48,63,63},{64,63,63},{80,63,63},{96,63,63},{112,63,63},
        {0,80,63},{0,96,63},{0,112,63},{0,128,63},{0,144,63},{0,160,63},{0,176,63},{0,192,63},
        {63,0,63},{80,0,63},{96,0,63},{112,0,63},{128,0,63},{144,0,63},{160,0,63},{176,0,63},
        {63,16,63},{63,32,63},{63,48,63},{63,64,63},{63,80,63},{63,96,63},{63,112,63},{63,128,63},
        {95,95,95},{111,111,111},{127,127,127},{143,143,143},{159,159,159},{175,175,175},{191,191,191},{207,207,207},
        {223,223,223},{239,239,239},{255,0,0},{255,64,0},{255,128,0},{255,192,0},{255,255,0},{192,255,0},
        {128,255,0},{64,255,0},{0,255,0},{0,255,64},{0,255,128},{0,255,192},{0,255,255},{0,192,255},
        {0,128,255},{0,64,255},{0,0,255},{64,0,255},{128,0,255},{192,0,255},{255,0,255},{255,0,192},
        {255,0,128},{255,0,64},{255,255,255},{255,220,220},{255,185,185},{255,150,150},{255,115,115},{255,80,80},
        {255,45,45},{255,10,10},{240,0,0},{205,0,0},{170,0,0},{135,0,0},{100,0,0},{65,0,0},
        {255,255,220},{255,255,185},{255,255,150},{255,255,115},{255,255,80},{255,255,45},{255,255,10},{240,240,0},
        {205,205,0},{170,170,0},{135,135,0},{100,100,0},{65,65,0},{220,255,220},{185,255,185},{150,255,150},
        {115,255,115},{80,255,80},{45,255,45},{10,255,10},{0,240,0},{0,205,0},{0,170,0},{0,135,0},
        {220,220,255},{185,185,255},{150,150,255},{115,115,255},{80,80,255},{45,45,255},{10,10,255},{0,0,240},
        {255,220,255},{255,185,255},{255,150,255},{255,115,255},{255,80,255},{255,45,255},{255,10,255},{240,0,240},
    };
    for (int y = 0; y < fbH; ++y) {
        for (int x = 0; x < fbW; ++x) {
            const std::uint32_t off = fbBase + static_cast<std::uint32_t>(y * fbW + x);
            const std::uint8_t idx = ram[off];
            const std::uint8_t* rgb = kVgaRgb[idx & 0xFFu];
            std::fwrite(rgb, 1, 3, fp);
        }
    }
    std::fclose(fp);
    std::fprintf(stderr, "[SNAP] guest VGA %s (%dx%d)\n", path, fbW, fbH);
    return true;
}

} // namespace FieldSnapDump