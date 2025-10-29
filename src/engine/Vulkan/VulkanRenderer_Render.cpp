// src/engine/Vulkan/VulkanRenderer_Render.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <algorithm>
#include <format>
#include <cstring>
#include <array>
#include <iomanip>
#include <span>
#include <chrono>
#include <tinyobjloader/tiny_obj_loader.h>
#include "stb/stb_image.h"

namespace VulkanRTX {

#define VK_CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) { LOG_ERROR_CAT("Renderer", "{} failed: {}", #x, static_cast<int>(r)); throw std::runtime_error(std::format("{} failed", #x)); } } while(0)

// -----------------------------------------------------------------------------
// 3. CREATE ENVIRONMENT MAP (USING CONTEXT-BASED HELPERS)
// -----------------------------------------------------------------------------
void VulkanRenderer::createEnvironmentMap() {
    int width = 512, height = 256, channels = 4;
    stbi_uc* pixels = stbi_load("assets/textures/envmap.hdr", &width, &height, &channels, STBI_rgb_alpha);
    bool useFallback = false;
    if (!pixels) {
        LOG_WARNING_CAT("Renderer", "Failed to load envmap.hdr, creating procedural blue fallback");
        useFallback = true;
        // Procedural bright blue gradient (visible sky)
        pixels = new stbi_uc[width * height * 4];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = (y * width + x) * 4;
                float normY = static_cast<float>(y) / height;
                pixels[idx + 0] = static_cast<stbi_uc>(0.2f + normY * 0.3f) * 255;  // R: dark to light blue
                pixels[idx + 1] = static_cast<stbi_uc>(0.4f + normY * 0.2f) * 255;  // G
                pixels[idx + 2] = static_cast<stbi_uc>(0.8f + normY * 0.2f) * 255;  // B: bright
                pixels[idx + 3] = 255;  // A
            }
        }
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory, nullptr, context_.resourceManager
    );

    void* data;
    VK_CHECK(vkMapMemory(context_.device, stagingMemory, 0, imageSize, 0, &data));
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(context_.device, stagingMemory);
    if (useFallback) {
        delete[] pixels;
    } else {
        stbi_image_free(pixels);
    }

    // Create image
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(context_.device, &imageInfo, nullptr, &envMapImage_));
    context_.resourceManager.addImage(envMapImage_);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(context_.device, envMapImage_, &memReqs);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = VulkanInitializer::findMemoryType(context_.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &envMapImageMemory_));
    context_.resourceManager.addMemory(envMapImageMemory_);
    VK_CHECK(vkBindImageMemory(context_.device, envMapImage_, envMapImageMemory_, 0));

    // Transition to transfer dst
    VulkanInitializer::transitionImageLayout(
        context_, envMapImage_, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    // Copy buffer to image
    VulkanInitializer::copyBufferToImage(
        context_, stagingBuffer, envMapImage_, static_cast<uint32_t>(width), static_cast<uint32_t>(height)
    );

    // Transition to shader read
    VulkanInitializer::transitionImageLayout(
        context_, envMapImage_, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // Cleanup staging
    context_.resourceManager.removeBuffer(stagingBuffer);
    context_.resourceManager.removeMemory(stagingMemory);
    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingMemory, nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = envMapImage_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VK_CHECK(vkCreateImageView(context_.device, &viewInfo, nullptr, &envMapImageView_));
    context_.resourceManager.addImageView(envMapImageView_);

    // Create sampler
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .compareEnable = VK_FALSE,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    VK_CHECK(vkCreateSampler(context_.device, &samplerInfo, nullptr, &envMapSampler_));

    LOG_INFO_CAT("Renderer", "Loaded environment map: {}x{}", width, height);
}

// -----------------------------------------------------------------------------
// 5. INITIALIZE PER-FRAME BUFFER DATA (USING CORRECT STRUCT FIELDS)
// -----------------------------------------------------------------------------
void VulkanRenderer::initializeBufferData(uint32_t frameIndex, VkDeviceSize materialSize, VkDeviceSize dimensionSize) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;

    VkMemoryAllocateFlagsInfo flags{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    if (materialBuffers_[frameIndex] == VK_NULL_HANDLE) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, materialSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            materialBuffers_[frameIndex], materialBufferMemory_[frameIndex], &flags, context_.resourceManager
        );
    }

    if (dimensionBuffers_[frameIndex] == VK_NULL_HANDLE) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, dimensionSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            dimensionBuffers_[frameIndex], dimensionBufferMemory_[frameIndex], &flags, context_.resourceManager
        );
    }

    std::vector<MaterialData> materials(128, MaterialData{
        .diffuse   = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f),
        .specular  = 0.0f,
        .roughness = 0.5f,
        .metallic  = 0.0f,
        .emission  = glm::vec4(0.0f)
    });

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, materialSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory, nullptr, context_.resourceManager
    );

    void* data;
    VK_CHECK(vkMapMemory(context_.device, stagingMemory, 0, materialSize, 0, &data));
    std::memcpy(data, materials.data(), materialSize);
    vkUnmapMemory(context_.device, stagingMemory);

    VulkanInitializer::copyBuffer(
        context_.device, context_.commandPool, context_.graphicsQueue,
        stagingBuffer, materialBuffers_[frameIndex], materialSize
    );

    context_.resourceManager.removeBuffer(stagingBuffer);
    context_.resourceManager.removeMemory(stagingMemory);
    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingMemory, nullptr);

    // Dimension data
    DimensionData dim{};
    dim.screenWidth = context_.swapchainExtent.width;
    dim.screenHeight = context_.swapchainExtent.height;

    stagingBuffer = VK_NULL_HANDLE;
    stagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice, dimensionSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory, nullptr, context_.resourceManager
    );

    VK_CHECK(vkMapMemory(context_.device, stagingMemory, 0, dimensionSize, 0, &data));
    std::memcpy(data, &dim, dimensionSize);
    vkUnmapMemory(context_.device, stagingMemory);

    VulkanInitializer::copyBuffer(
        context_.device, context_.commandPool, context_.graphicsQueue,
        stagingBuffer, dimensionBuffers_[frameIndex], dimensionSize
    );

    context_.resourceManager.removeBuffer(stagingBuffer);
    context_.resourceManager.removeMemory(stagingMemory);
    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingMemory, nullptr);
}

} // namespace VulkanRTX