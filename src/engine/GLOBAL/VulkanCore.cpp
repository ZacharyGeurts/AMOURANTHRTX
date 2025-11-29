// src/engine/GLOBAL/VulkanCore.cpp
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VULKAN CORE v∞ — FINAL, CLEAN, GREEN — NOVEMBER 28, 2025
// NO DEAD CODE — NO CUBES — ONLY LAS — ONLY TRUTH — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace RTX;
using namespace Logging::Color;

// =============================================================================
// Persistent 1GB Staging Pool — ONE DEFINITION — ETERNAL
// =============================================================================
namespace VulkanRTXDetail {
    alignas(64) std::mutex g_stagingMutex;
    alignas(64) uint64_t   g_stagingPool = 0;
    alignas(64) VkDeviceMemory g_stagingMem = VK_NULL_HANDLE;
    alignas(64) VkBuffer   g_stagingBuffer = VK_NULL_HANDLE;
    alignas(64) void*      g_mappedBase = nullptr;
    alignas(64) std::atomic<VkDeviceSize> g_mappedOffset{0};
    constexpr VkDeviceSize STAGING_POOL_SIZE = 1ULL << 30; // 1 GB
}

// Bring them into scope — correct way
using VulkanRTXDetail::g_stagingMutex;
using VulkanRTXDetail::g_stagingPool;
using VulkanRTXDetail::g_stagingMem;
using VulkanRTXDetail::g_stagingBuffer;
using VulkanRTXDetail::g_mappedBase;
using VulkanRTXDetail::g_mappedOffset;
using VulkanRTXDetail::STAGING_POOL_SIZE;

// =============================================================================
// VulkanRTX — Minimal, Perfect, LAS-Only
// =============================================================================
VulkanRTX::VulkanRTX(int w, int h, PipelineManager*) noexcept
    : extent_{static_cast<uint32_t>(w), static_cast<uint32_t>(h)}
    , device_(g_ctx().device_)
{
    if (!device_ || w <= 0 || h <= 0) {
        LOG_WARN_CAT("RTX", "VulkanRTX dummy mode — invalid device or size");
        return;
    }

    LOG_SUCCESS_CAT("RTX", "VulkanRTX forged — {}×{} — device 0x{:016X}", w, h, reinterpret_cast<uintptr_t>(device_));
    initBlackFallbackImage();

    LOG_SUCCESS_CAT("RTX", "VULKANRTX ASCENDED — LAS WILL BUILD THE UNIVERSE — FIRST LIGHT ETERNAL");
}

VulkanRTX::~VulkanRTX() noexcept
{
    LOG_TRACE_CAT("RTX", "VulkanRTX destructor — cleansing the void");

    if (g_mappedBase) {
        vkUnmapMemory(device_, g_stagingMem);
        g_mappedBase = nullptr;
        g_mappedOffset.store(0);
    }
    if (g_stagingPool) {
        BUFFER_DESTROY(g_stagingPool);
        g_stagingPool = 0;
        g_stagingMem = VK_NULL_HANDLE;
        g_stagingBuffer = VK_NULL_HANDLE;
    }

    blackFallbackView_.reset();
    blackFallbackMemory_.reset();
    blackFallbackImage_.reset();

    LOG_SUCCESS_CAT("RTX", "VulkanRTX destroyed — pure and clean");
}

// =============================================================================
// Black Fallback Image — The Void's Safety Net
// =============================================================================
void VulkanRTX::initBlackFallbackImage()
{
    LOG_INFO_CAT("RTX", "Forging 1x1 black fallback — the abyss is contained");

    constexpr uint32_t blackPixel = 0xFF000000u;

    uint64_t staging = BufferManager::create(4,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "BlackPixel_Staging");

    void* mapped = BufferManager::map(staging);
    std::memcpy(mapped, &blackPixel, 4);
    BufferManager::unmap(staging);

    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {1, 1, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage img = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(device_, &imgInfo, nullptr, &img));
    // CORRECT ORDER: handle, device, deleter, size, tag
    blackFallbackImage_ = MakeHandle(img, device_, vkDestroyImage, 0, "BlackFallbackImage");

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device_, img, &memReqs);
    uint32_t memType = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory mem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &mem));
    VK_CHECK(vkBindImageMemory(device_, img, mem, 0));
    blackFallbackMemory_ = MakeHandle(mem, device_, vkFreeMemory, memReqs.size, "BlackFallbackMemory");

    VkCommandBuffer cmd = beginOneTimeSubmit(g_ctx().commandPool_);

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = img,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copy{
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {1, 1, 1}
    };
    vkCmdCopyBufferToImage(cmd, RAW_BUFFER(staging), img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endOneTimeSubmit(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool_);
    BUFFER_DESTROY(staging);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &view));
    blackFallbackView_ = MakeHandle(view, device_, vkDestroyImageView, 0, "BlackFallbackView");

    LOG_SUCCESS_CAT("RTX", "Black fallback forged — the void is sealed");
    LOG_AMOURANTH("Amouranth: \"Even darkness bows to the lasso.\"");
}