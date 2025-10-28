// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "engine/Vulkan/types.hpp"
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

namespace VulkanRTX {

// -----------------------------------------------------------------------------
// 1. CREATE DESCRIPTOR POOL
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              MAX_FRAMES_IN_FLIGHT * 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             MAX_FRAMES_IN_FLIGHT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             MAX_FRAMES_IN_FLIGHT * 3 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     MAX_FRAMES_IN_FLIGHT * 3 }
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT * 3,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes
    };

    if (vkCreateDescriptorPool(context_.device, &poolInfo, nullptr, &context_.descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    context_.resourceManager.addDescriptorPool(context_.descriptorPool);
    LOG_DEBUG_CAT("Renderer", "Created descriptor pool: %p", static_cast<void*>(context_.descriptorPool));
}

// -----------------------------------------------------------------------------
// 2. CREATE ALL DESCRIPTOR SETS
// -----------------------------------------------------------------------------
void VulkanRenderer::createDescriptorSets() {
    if (context_.descriptorPool == VK_NULL_HANDLE) {
        createDescriptorPool();
    }

    std::vector<VkDescriptorSetLayout> rtLayouts(MAX_FRAMES_IN_FLIGHT, context_.rayTracingDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> graphicsLayouts(MAX_FRAMES_IN_FLIGHT, context_.graphicsDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> computeLayouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo rtAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = rtLayouts.data()
    };
    VkDescriptorSetAllocateInfo graphicsAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = graphicsLayouts.data()
    };
    VkDescriptorSetAllocateInfo computeAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = computeLayouts.data()
    };

    std::vector<VkDescriptorSet> rtSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> gSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> cSets(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateDescriptorSets(context_.device, &rtAlloc, rtSets.data()) != VK_SUCCESS ||
        vkAllocateDescriptorSets(context_.device, &graphicsAlloc, gSets.data()) != VK_SUCCESS ||
        vkAllocateDescriptorSets(context_.device, &computeAlloc, cSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].rayTracingDescriptorSet = rtSets[i];
        frames_[i].graphicsDescriptorSet   = gSets[i];
        frames_[i].computeDescriptorSet    = cSets[i];

        updateDescriptorSetForFrame(i, pipelineManager_->getTLAS());
        updateGraphicsDescriptorSet(i);
        updateComputeDescriptorSet(i);
    }

    LOG_DEBUG_CAT("Renderer", "Created %u descriptor sets per frame", MAX_FRAMES_IN_FLIGHT);
}

// -----------------------------------------------------------------------------
// 3. UPDATE PER-FRAME RAY-TRACING DESCRIPTOR SET
// -----------------------------------------------------------------------------
void VulkanRenderer::updateDescriptorSetForFrame(uint32_t frameIndex, VkAccelerationStructureKHR tlas) {
    LOG_DEBUG_CAT("Renderer", "Updating descriptor set for frame {}", frameIndex);
    if (frameIndex >= frames_.size()) {
        throw std::out_of_range("Invalid frame index");
    }
    VkDescriptorSet descriptorSet = frames_[frameIndex].rayTracingDescriptorSet;
    if (descriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null descriptor set for frame {}", frameIndex);
        return;
    }

    VkAccelerationStructureKHR tlasArray[1] = { tlas };
    VkWriteDescriptorSetAccelerationStructureKHR accelDescriptor = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = tlas ? 1u : 0u,
        .pAccelerationStructures = tlas ? tlasArray : nullptr
    };

    VkDescriptorImageInfo storageImageInfo = {
        .imageView = context_.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo envMapInfo = {
        .sampler = envMapSampler_,
        .imageView = envMapImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkDescriptorBufferInfo uniformBufferInfo = {
        .buffer = context_.uniformBuffers[frameIndex],
        .offset = 0,
        .range = sizeof(UniformBufferObject)
    };
    VkDescriptorBufferInfo materialBufferInfo = {
        .buffer = materialBuffers_[frameIndex],
        .offset = 0,
        .range = sizeof(MaterialData) * 128
    };
    VkDescriptorBufferInfo dimensionBufferInfo = {
        .buffer = dimensionBuffers_[frameIndex],
        .offset = 0,
        .range = sizeof(DimensionData) * 1
    };

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    descriptorWrites.reserve(6);

    if (tlas) {
        descriptorWrites.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &accelDescriptor,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        });
    }

    descriptorWrites.insert(descriptorWrites.end(), {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &storageImageInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &uniformBufferInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &materialBufferInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &dimensionBufferInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::EnvMap),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &envMapInfo
        }
    });

    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

// -----------------------------------------------------------------------------
// 4. UPDATE GRAPHICS DESCRIPTOR SET
// -----------------------------------------------------------------------------
void VulkanRenderer::updateGraphicsDescriptorSet(uint32_t frameIndex) {
    VkDescriptorSet descriptorSet = frames_[frameIndex].graphicsDescriptorSet;
    if (descriptorSet == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("Renderer", "Skipping graphics descriptor update for frame {}: null set", frameIndex);
        return;
    }

    VkDescriptorImageInfo imageInfo = {
        .sampler = denoiseSampler_,
        .imageView = denoiseImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo
    };
    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
}

// -----------------------------------------------------------------------------
// 5. UPDATE COMPUTE DESCRIPTOR SET
// -----------------------------------------------------------------------------
void VulkanRenderer::updateComputeDescriptorSet(uint32_t frameIndex) {
    VkDescriptorSet descriptorSet = frames_[frameIndex].computeDescriptorSet;
    if (descriptorSet == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("Renderer", "Skipping compute descriptor update for frame {}: null set", frameIndex);
        return;
    }

    VkDescriptorImageInfo inputInfo = {
        .imageView = context_.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo outputInfo = {
        .imageView = denoiseImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &inputInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outputInfo
        }
    };
    vkUpdateDescriptorSets(context_.device, 2, writes, 0, nullptr);
}

// -----------------------------------------------------------------------------
// 6. UPDATE TLAS BINDING ACROSS ALL FRAMES
// -----------------------------------------------------------------------------
void VulkanRenderer::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    if (tlas == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("VulkanRenderer", "Attempted to update TLAS descriptor with null handle");
        return;
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorSet ds = frames_[i].rayTracingDescriptorSet;
        if (ds == VK_NULL_HANDLE) {
            LOG_WARNING_CAT("VulkanRenderer", "Skipping TLAS update for frame {}: descriptor set is null", i);
            continue;
        }

        VkAccelerationStructureKHR tlasArray[1] = { tlas };
        VkWriteDescriptorSetAccelerationStructureKHR tlasInfo = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = nullptr,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = tlasArray
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &tlasInfo,
            .dstSet = ds,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };

        LOG_DEBUG_CAT("VulkanRenderer", "Updating TLAS descriptor for frame {}: set=%p, tlas=%p",
                      i, static_cast<void*>(ds), static_cast<void*>(tlas));

        vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
    }
}

// -----------------------------------------------------------------------------
// 7. CREATE ACCELERATION STRUCTURES – WITH FALLBACK + BUFFER CREATION
// -----------------------------------------------------------------------------
void VulkanRenderer::createAccelerationStructures() {
    LOG_INFO_CAT("Renderer", "=== STARTING ACCELERATION STRUCTURE CREATION ===");

    auto vertices = getVertices();
    auto indices = getIndices();

    if (vertices.empty() || indices.empty()) {
        LOG_WARNING_CAT("Renderer", "No geometry loaded. Using default triangle.");
        vertices = {
            glm::vec3(-0.5f, -0.5f, 0.0f),
            glm::vec3( 0.5f, -0.5f, 0.0f),
            glm::vec3( 0.0f,  0.5f, 0.0f)
        };
        indices = { 0, 1, 2 };
    }

    LOG_INFO_CAT("Renderer", "Loaded {} vertices, {} indices", vertices.size(), indices.size());

    bufferManager_->asyncUpdateBuffers(vertices, indices, nullptr);

    VkBuffer vertexBuffer = bufferManager_->getVertexBuffer();
    VkBuffer indexBuffer  = bufferManager_->getIndexBuffer();

    indexCount_ = static_cast<uint32_t>(indices.size());

    LOG_INFO_CAT("Renderer", "Acceleration structures created. TLAS: %p", (void*)pipelineManager_->getTLAS());
}

// -----------------------------------------------------------------------------
// 8. RECORD RAY TRACING COMMANDS – FIXED SYNTAX
// -----------------------------------------------------------------------------
void VulkanRenderer::recordRayTracingCommands(VkCommandBuffer commandBuffer, VkExtent2D extent, VkImage outputImage,
                                             VkImageView outputImageView, const MaterialData::PushConstants& pushConstants,
                                             VkAccelerationStructureKHR tlas) {
    VkImageMemoryBarrier outputBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &outputBarrier);

    const ShaderBindingTable& sbt = pipelineManager_->getShaderBindingTable();

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineManager_->getRayTracingPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineManager_->getRayTracingPipelineLayout(), 0, 1, &frames_[currentFrame_].rayTracingDescriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, pipelineManager_->getRayTracingPipelineLayout(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCmdTraceRaysKHR"));
    vkCmdTraceRaysKHR(commandBuffer, &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
                      extent.width, extent.height, 1);
}

// -----------------------------------------------------------------------------
// 9. DENOISE IMAGE (compute)
// -----------------------------------------------------------------------------
void VulkanRenderer::denoiseImage(VkCommandBuffer commandBuffer, VkImage inputImage, VkImageView inputImageView,
                                 VkImage outputImage, VkImageView outputImageView) {
    VkImageMemoryBarrier inputBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = inputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &inputBarrier);

    VkImageMemoryBarrier outputBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &outputBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineManager_->getComputePipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineManager_->getComputePipelineLayout(),
                            0, 1, &frames_[currentFrame_].computeDescriptorSet, 0, nullptr);

    MaterialData::PushConstants pushConstants{
        .resolution = {context_.swapchainExtent.width, context_.swapchainExtent.height}
    };
    vkCmdPushConstants(commandBuffer, pipelineManager_->getComputePipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    uint32_t maxWorkgroup = props2.properties.limits.maxComputeWorkGroupInvocations;
    uint32_t gx = 16, gy = 16;
    if (gx * gy > maxWorkgroup) {
        gx = gy = std::max(8u, static_cast<uint32_t>(std::sqrt(maxWorkgroup)));
    }
    uint32_t cx = (context_.swapchainExtent.width + gx - 1) / gx;
    uint32_t cy = (context_.swapchainExtent.height + gy - 1) / gy;
    vkCmdDispatch(commandBuffer, cx, cy, 1);
}

// -----------------------------------------------------------------------------
// 10. RENDER FRAME – FULLY INTEGRATED WITH CAMERA
// -----------------------------------------------------------------------------
void VulkanRenderer::renderFrame(const Camera& camera) {
    vkWaitForFences(context_.device, 1, &frames_[currentFrame_].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &frames_[currentFrame_].fence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, swapchainManager_->getSwapchain(), UINT64_MAX,
                                            frames_[currentFrame_].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleResize(width_, height_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // Update UBO with camera data
    UniformBufferObject ubo{};
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.camPos = glm::vec4(camera.getPosition(), 1.0f);
    ubo.time = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0f);

    void* data;
    vkMapMemory(context_.device, bufferManager_->getUniformBufferMemory(currentFrame_), 0, sizeof(ubo), 0, &data);
    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_.device, bufferManager_->getUniformBufferMemory(currentFrame_));

    updateDescriptorSetForFrame(currentFrame_, pipelineManager_->getTLAS());

    VkCommandBuffer cmd = frames_[currentFrame_].commandBuffer;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &beginInfo);

    MaterialData::PushConstants pc{};
    pc.resolution = { context_.swapchainExtent.width, context_.swapchainExtent.height };
    recordRayTracingCommands(cmd, context_.swapchainExtent, context_.storageImage, context_.storageImageView, pc, pipelineManager_->getTLAS());

    denoiseImage(cmd, context_.storageImage, context_.storageImageView, denoiseImage_, denoiseImageView_);

    VkImageMemoryBarrier barriers[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = denoiseImage_,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = swapchainManager_->getSwapchainImages()[imageIndex],
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

    VkImageBlit blit = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = { {0,0,0}, {static_cast<int32_t>(context_.swapchainExtent.width), static_cast<int32_t>(context_.swapchainExtent.height), 1} },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = { {0,0,0}, {static_cast<int32_t>(context_.swapchainExtent.width), static_cast<int32_t>(context_.swapchainExtent.height), 1} }
    };
    vkCmdBlitImage(cmd, denoiseImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainManager_->getSwapchainImages()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_NEAREST);

    VkImageMemoryBarrier presentBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchainManager_->getSwapchainImages()[imageIndex],
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frames_[currentFrame_].renderFinishedSemaphore
    };
    vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, frames_[currentFrame_].fence);

    VkSwapchainKHR swapchain = swapchainManager_->getSwapchain();
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex
    };
    result = vkQueuePresentKHR(context_.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
    }

    if (FPS_COUNTER) {
        ++framesThisSecond_;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFPSTime_).count();
        if (elapsed >= 1) {
            LOG_INFO_CAT("FPS", "{}", framesThisSecond_);
            framesThisSecond_ = 0;
            lastFPSTime_ = now;
        }
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

// -----------------------------------------------------------------------------
// 11. HANDLE RESIZE
// -----------------------------------------------------------------------------
void VulkanRenderer::handleResize(int width, int height) {
    vkDeviceWaitIdle(context_.device);

    swapchainManager_->handleResize(width, height);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();

    Dispose::destroyFramebuffers(context_.device, context_.framebuffers);
    createFramebuffers();

    bufferManager_->createUniformBuffers(MAX_FRAMES_IN_FLIGHT);

    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, context_.storageImage,
                                         context_.storageImageMemory, context_.storageImageView, width, height,
                                         context_.resourceManager);

    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, denoiseImage_,
                                         denoiseImageMemory_, denoiseImageView_, width, height,
                                         context_.resourceManager);

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    };
    vkCreateSampler(context_.device, &samplerInfo, nullptr, &denoiseSampler_);

    createEnvironmentMap();

    VkDeviceSize materialSize = sizeof(MaterialData) * 128;
    VkDeviceSize dimensionSize = sizeof(DimensionData) * 1;
    VkPhysicalDeviceProperties2 props2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    VkDeviceSize alignment = props2.properties.limits.minStorageBufferOffsetAlignment;
    materialSize = (materialSize + alignment - 1) & ~(alignment - 1);
    dimensionSize = (dimensionSize + alignment - 1) & ~(alignment - 1);

    materialBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    materialBufferMemory_.resize(MAX_FRAMES_IN_FLIGHT);
    dimensionBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    dimensionBufferMemory_.resize(MAX_FRAMES_IN_FLIGHT);

    VkMemoryAllocateFlagsInfo flags{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, materialSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            materialBuffers_[i], materialBufferMemory_[i], &flags, context_.resourceManager
        );
        VulkanInitializer::createBuffer(
            context_.device, context_.physicalDevice, dimensionSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            dimensionBuffers_[i], dimensionBufferMemory_[i], &flags, context_.resourceManager
        );
        initializeBufferData(i, materialSize, dimensionSize);
    }

    if (context_.descriptorPool) {
        context_.resourceManager.removeDescriptorPool(context_.descriptorPool);
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
    }
    createDescriptorPool();
    createDescriptorSets();

    camera_->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));

    width_ = width;
    height_ = height;
}

} // namespace VulkanRTX