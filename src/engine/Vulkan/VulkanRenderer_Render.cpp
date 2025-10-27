// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "engine/Dispose.hpp"
#include <stdexcept>
#include <algorithm>
#include <format>
#include <cstring>
#include <array>
#include <iomanip>
#include <span>
#include <chrono>

namespace VulkanRTX {

void VulkanRenderer::updateDescriptorSetForFrame(uint32_t frameIndex, VkAccelerationStructureKHR tlas) {
    LOG_DEBUG_CAT("Renderer", "Updating descriptor set for frame {} with TLAS: {:p}", frameIndex, static_cast<void*>(tlas));
    if (frameIndex >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid frame index: {} (max: {})", frameIndex, frames_.size() - 1);
        throw std::out_of_range("Invalid frame index");
    }
    if (tlas == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Invalid TLAS for frame {}: {:p}", frameIndex, static_cast<void*>(tlas));
        throw std::runtime_error("Invalid TLAS");
    }

    VkDescriptorSet descriptorSet = frames_[frameIndex].rayTracingDescriptorSet;
    if (descriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null descriptor set for frame {}", frameIndex);
        throw std::runtime_error("Null descriptor set");
    }

    VkWriteDescriptorSetAccelerationStructureKHR accelDescriptor{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkDescriptorImageInfo storageImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = context_.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorImageInfo envMapInfo{
        .sampler = envMapSampler_,
        .imageView = envMapImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorBufferInfo uniformBufferInfo{
        .buffer = context_.uniformBuffers[frameIndex],
        .offset = 0,
        .range = sizeof(UE::UniformBufferObject)
    };

    VkDescriptorBufferInfo materialBufferInfo{
        .buffer = materialBuffers_[frameIndex],
        .offset = 0,
        .range = sizeof(MaterialData) * 128
    };

    VkDescriptorBufferInfo dimensionBufferInfo{
        .buffer = dimensionBuffers_[frameIndex],
        .offset = 0,
        .range = sizeof(UE::DimensionData) * 1
    };

    std::vector<VkWriteDescriptorSet> descriptorWrites = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &accelDescriptor,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &storageImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &uniformBufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &materialBufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &dimensionBufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::EnvMap),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &envMapInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };

    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    LOG_DEBUG_CAT("Renderer", "Updated descriptor set {:p} for frame {}", static_cast<void*>(descriptorSet), frameIndex);
}

void VulkanRenderer::updateGraphicsDescriptorSet(uint32_t frameIndex) {
    LOG_DEBUG_CAT("Renderer", "Updating graphics descriptor set for frame {}", frameIndex);
    if (frameIndex >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid frame index for graphics update: {}", frameIndex);
        return;
    }
    VkDescriptorSet descSet = frames_[frameIndex].graphicsDescriptorSet;
    if (descSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null graphics descriptor set for frame {}", frameIndex);
        return;
    }
    if (!denoiseImageView_ || !denoiseSampler_) {
        LOG_ERROR_CAT("Renderer", "Invalid denoise resources: imageView={:p}, sampler={:p}",
                      static_cast<void*>(denoiseImageView_), static_cast<void*>(denoiseSampler_));
        return;
    }

    VkDescriptorImageInfo imageInfo{
        .sampler = denoiseSampler_,
        .imageView = denoiseImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
    LOG_DEBUG_CAT("Renderer", "Updated graphics descriptor set {:p} for frame {} with denoise image", static_cast<void*>(descSet), frameIndex);
}

void VulkanRenderer::updateComputeDescriptorSet(uint32_t frameIndex) {
    LOG_DEBUG_CAT("Renderer", "Updating compute descriptor set for frame {}", frameIndex);
    if (frameIndex >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid frame index for compute update: {}", frameIndex);
        return;
    }
    VkDescriptorSet descSet = frames_[frameIndex].computeDescriptorSet;
    if (descSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null compute descriptor set for frame {}", frameIndex);
        return;
    }
    if (!context_.storageImageView || !denoiseImageView_) {
        LOG_ERROR_CAT("Renderer", "Invalid image views: storageImageView={:p}, denoiseImageView={:p}",
                      static_cast<void*>(context_.storageImageView), static_cast<void*>(denoiseImageView_));
        return;
    }

    VkDescriptorImageInfo inputInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = context_.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorImageInfo outputInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = denoiseImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &inputInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descSet,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outputInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };

    vkUpdateDescriptorSets(context_.device, 2, writes, 0, nullptr);
    LOG_DEBUG_CAT("Renderer", "Updated compute descriptor set {:p} for frame {} with input/output images", static_cast<void*>(descSet), frameIndex);
}

void VulkanRenderer::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    LOG_DEBUG_CAT("Renderer", "Updating TLAS descriptor for all frames");
    if (!tlas) {
        LOG_ERROR_CAT("Renderer", "Invalid TLAS handle: {:p}", static_cast<void*>(tlas));
        throw std::runtime_error("Invalid TLAS handle");
    }
    for (uint32_t i = 0; i < frames_.size(); ++i) {
        if (!frames_[i].rayTracingDescriptorSet) {
            LOG_ERROR_CAT("Renderer", "Invalid ray tracing descriptor set for frame {}", i);
            throw std::runtime_error("Invalid ray tracing descriptor set");
        }
        VkWriteDescriptorSetAccelerationStructureKHR tlasInfo{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .pNext = nullptr,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas
        };
        VkWriteDescriptorSet tlasWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &tlasInfo,
            .dstSet = frames_[i].rayTracingDescriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        vkUpdateDescriptorSets(context_.device, 1, &tlasWrite, 0, nullptr);
        LOG_DEBUG_CAT("Renderer", "Updated TLAS descriptor for frame {}: tlas={:p}", i, static_cast<void*>(tlas));
    }
}

void VulkanRenderer::renderFrame(const Camera& camera) {
    if (!context_.device || frames_.empty() || currentFrame_ >= frames_.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid state: device={:p}, frames.size={}, currentFrame={}",
                      static_cast<void*>(context_.device), frames_.size(), currentFrame_);
        throw std::runtime_error("Invalid render state");
    }

    vkWaitForFences(context_.device, 1, &frames_[currentFrame_].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &frames_[currentFrame_].fence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                            frames_[currentFrame_].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        handleResize(width_, height_);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR_CAT("Renderer", "Failed to acquire swapchain image: result={}", static_cast<int>(result));
        throw std::runtime_error("Failed to acquire swapchain image");
    }
    if (imageIndex >= context_.framebuffers.size()) {
        LOG_ERROR_CAT("Renderer", "Invalid framebuffer index: {} (size={})", imageIndex, context_.framebuffers.size());
        throw std::runtime_error("Invalid framebuffer index");
    }

    if (vkResetCommandBuffer(frames_[currentFrame_].commandBuffer, 0) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to reset command buffer for frame {}", currentFrame_);
        throw std::runtime_error("Failed to reset command buffer");
    }

    // Update uniform buffer
    UE::UniformBufferObject ubo{
        .model = glm::mat4(1.0f), // Default identity matrix
        .view = camera.getViewMatrix(),
        .proj = camera.getProjectionMatrix(),
        .mode = 0 // Default mode, as AMOURANTH is removed
    };
    VkDeviceMemory uniformBufferMemory = bufferManager_->getUniformBufferMemory(currentFrame_);
    if (!uniformBufferMemory) {
        LOG_ERROR_CAT("Renderer", "Invalid uniform buffer memory for frame {}", currentFrame_);
        throw std::runtime_error("Invalid uniform buffer memory");
    }
    void* data;
    if (vkMapMemory(context_.device, uniformBufferMemory, 0, sizeof(UE::UniformBufferObject), 0, &data) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to map uniform buffer memory");
        throw std::runtime_error("Failed to map uniform buffer memory");
    }
    memcpy(data, &ubo, sizeof(UE::UniformBufferObject));
    vkUnmapMemory(context_.device, uniformBufferMemory);

    // Common push constants
    MaterialData::PushConstants pushConstants{
        .clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        .cameraPosition = camera.getPosition(),
        .lightDirection = glm::vec3(2.0f, 2.0f, 2.0f),
        .lightIntensity = 5.0f,
        .samplesPerPixel = 1u,
        .maxDepth = 5u,
        .maxBounces = 3u,
        .russianRoulette = 0.8f,
        .resolution = {context_.swapchainExtent.width, context_.swapchainExtent.height}
    };

    // Default rendering path (ray-tracing + graphics)
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(frames_[currentFrame_].commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to begin command buffer");
        throw std::runtime_error("Failed to begin command buffer");
    }

    recordRayTracingCommands(frames_[currentFrame_].commandBuffer, context_.swapchainExtent, context_.storageImage,
                            context_.storageImageView, pushConstants, context_.topLevelAS);
    denoiseImage(frames_[currentFrame_].commandBuffer, context_.storageImage, context_.storageImageView,
                 denoiseImage_, denoiseImageView_);

    VkImageMemoryBarrier graphicsBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = denoiseImage_,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(frames_[currentFrame_].commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &graphicsBarrier);

    VkClearValue clearValue = {{{0.2f, 0.2f, 0.3f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = pipelineManager_->getRenderPass(),
        .framebuffer = context_.framebuffers[imageIndex],
        .renderArea = {{0, 0}, context_.swapchainExtent},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    if (!renderPassInfo.renderPass || !renderPassInfo.framebuffer) {
        LOG_ERROR_CAT("Renderer", "Invalid render pass or framebuffer: renderPass={:p}, framebuffer={:p}",
                      static_cast<void*>(renderPassInfo.renderPass), static_cast<void*>(renderPassInfo.framebuffer));
        throw std::runtime_error("Invalid render pass or framebuffer");
    }
    vkCmdBeginRenderPass(frames_[currentFrame_].commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkPipeline graphicsPipeline = pipelineManager_->getGraphicsPipeline();
    VkPipelineLayout graphicsPipelineLayout = pipelineManager_->getGraphicsPipelineLayout();
    if (!graphicsPipeline || !graphicsPipelineLayout) {
        LOG_ERROR_CAT("Renderer", "Invalid graphics pipeline state: pipeline={:p}, layout={:p}",
                      static_cast<void*>(graphicsPipeline), static_cast<void*>(graphicsPipelineLayout));
        throw std::runtime_error("Invalid graphics pipeline state");
    }
    vkCmdBindPipeline(frames_[currentFrame_].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    if (frames_[currentFrame_].graphicsDescriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("Renderer", "Null graphics descriptor set for frame {}", currentFrame_);
        throw std::runtime_error("Null graphics descriptor set");
    }
    vkCmdBindDescriptorSets(frames_[currentFrame_].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            graphicsPipelineLayout, 0, 1, &frames_[currentFrame_].graphicsDescriptorSet, 0, nullptr);

    vkCmdPushConstants(frames_[currentFrame_].commandBuffer, graphicsPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    VkBuffer vertexBuffer = bufferManager_->getVertexBuffer();
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(frames_[currentFrame_].commandBuffer, 0, 1, &vertexBuffer, offsets);
    VkBuffer indexBuffer = bufferManager_->getIndexBuffer();
    vkCmdBindIndexBuffer(frames_[currentFrame_].commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(frames_[currentFrame_].commandBuffer, indexCount_, 1, 0, 0, 0);
    vkCmdEndRenderPass(frames_[currentFrame_].commandBuffer);

    VkImageMemoryBarrier postGraphicsBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = denoiseImage_,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(frames_[currentFrame_].commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &postGraphicsBarrier);

    if (vkEndCommandBuffer(frames_[currentFrame_].commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to end command buffer");
        throw std::runtime_error("Failed to end command buffer");
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &frames_[currentFrame_].commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frames_[currentFrame_].renderFinishedSemaphore
    };
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, frames_[currentFrame_].fence) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to submit queue");
        throw std::runtime_error("Failed to submit queue");
    }

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };
    VkResult presentResult = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
    } else if (presentResult != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to present queue: VkResult={}", static_cast<int>(presentResult));
        throw std::runtime_error("Failed to present queue");
    }

    frameCount_++;
    if (FPS_COUNTER) {
        framesSinceLastLog_++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime_).count();
        if (elapsed >= 1) {
            double fps = static_cast<double>(framesSinceLastLog_) / static_cast<double>(elapsed);
            LOG_WARNING_CAT("FPS", "Total frame count: {}, Average FPS over {} seconds: {:.2f}", frameCount_, elapsed, fps);
            lastLogTime_ = now;
            framesSinceLastLog_ = 0;
        }
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::recordRayTracingCommands(VkCommandBuffer commandBuffer, VkExtent2D extent, VkImage outputImage,
                                             VkImageView outputImageView, const MaterialData::PushConstants& pushConstants,
                                             VkAccelerationStructureKHR tlas) {
    if (!commandBuffer || !outputImage || !outputImageView || !tlas || !rtPipeline_ || !rtPipelineLayout_) {
        LOG_ERROR_CAT("Renderer", "Invalid parameters: cmd={:p}, outputImage={:p}, outputView={:p}, tlas={:p}, pipeline={:p}, layout={:p}",
                      static_cast<void*>(commandBuffer), static_cast<void*>(outputImage),
                      static_cast<void*>(outputImageView), static_cast<void*>(tlas),
                      static_cast<void*>(rtPipeline_), static_cast<void*>(rtPipelineLayout_));
        throw std::runtime_error("Invalid ray tracing parameters");
    }

    VkImageMemoryBarrier outputBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 0, nullptr, 0, nullptr, 1, &outputBarrier);

    const ShaderBindingTable& sbt = pipelineManager_->getShaderBindingTable();
    if (sbt.raygen.deviceAddress == 0 || sbt.miss.deviceAddress == 0 || sbt.hit.deviceAddress == 0) {
        LOG_ERROR_CAT("Renderer", "Invalid shader binding table: raygen={:x}, miss={:x}, hit={:x}",
                      sbt.raygen.deviceAddress, sbt.miss.deviceAddress, sbt.hit.deviceAddress);
        throw std::runtime_error("Invalid shader binding table");
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_, 0, 1, &frames_[currentFrame_].rayTracingDescriptorSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, rtPipelineLayout_, 
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCmdTraceRaysKHR"));
    if (!vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("Renderer", "Failed to load vkCmdTraceRaysKHR function pointer");
        throw std::runtime_error("vkCmdTraceRaysKHR not loaded");
    }

    vkCmdTraceRaysKHR(commandBuffer, &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
                      extent.width, extent.height, 1);
}

void VulkanRenderer::denoiseImage(VkCommandBuffer commandBuffer, VkImage inputImage, VkImageView inputImageView,
                                 VkImage outputImage, VkImageView outputImageView) {
    if (!commandBuffer || !inputImage || !inputImageView || !outputImage || !outputImageView) {
        LOG_ERROR_CAT("Renderer", "Invalid parameters: cmd={:p}, inputImage={:p}, inputView={:p}, outputImage={:p}, outputView={:p}",
                      static_cast<void*>(commandBuffer), static_cast<void*>(inputImage),
                      static_cast<void*>(inputImageView), static_cast<void*>(outputImage),
                      static_cast<void*>(outputImageView));
        throw std::runtime_error("Invalid denoise parameters");
    }

    VkImageMemoryBarrier inputBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = inputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &inputBarrier);

    VkImageMemoryBarrier outputBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &outputBarrier);

    VkPipeline computePipeline = pipelineManager_->getComputePipeline();
    VkPipelineLayout computePipelineLayout = pipelineManager_->getComputePipelineLayout();
    if (!computePipeline || !computePipelineLayout || !frames_[currentFrame_].computeDescriptorSet) {
        LOG_ERROR_CAT("Renderer", "Invalid compute pipeline state: pipeline={:p}, layout={:p}, descriptorSet={:p}",
                      static_cast<void*>(computePipeline), static_cast<void*>(computePipelineLayout),
                      static_cast<void*>(frames_[currentFrame_].computeDescriptorSet));
        throw std::runtime_error("Invalid compute pipeline state");
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout,
                            0, 1, &frames_[currentFrame_].computeDescriptorSet, 0, nullptr);

    MaterialData::PushConstants pushConstants{
        .clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        .cameraPosition = glm::vec3(0.0f, 0.0f, 0.0f),
        .lightDirection = glm::vec3(2.0f, 2.0f, 2.0f),
        .lightIntensity = 5.0f,
        .samplesPerPixel = 1u,
        .maxDepth = 5u,
        .maxBounces = 3u,
        .russianRoulette = 0.8f,
        .resolution = {context_.swapchainExtent.width, context_.swapchainExtent.height}
    };
    vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(MaterialData::PushConstants), &pushConstants);

    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    uint32_t maxWorkgroupSize = props2.properties.limits.maxComputeWorkGroupInvocations;
    uint32_t groupSizeX = 16;
    uint32_t groupSizeY = 16;
    uint32_t groupCountX = (context_.swapchainExtent.width + groupSizeX - 1) / groupSizeX;
    uint32_t groupCountY = (context_.swapchainExtent.height + groupSizeY - 1) / groupSizeY;
    if (groupSizeX * groupSizeY > maxWorkgroupSize) {
        groupSizeX = std::max(8u, static_cast<uint32_t>(std::sqrt(maxWorkgroupSize)));
        groupSizeY = groupSizeX;
        groupCountX = (context_.swapchainExtent.width + groupSizeX - 1) / groupSizeX;
        groupCountY = (context_.swapchainExtent.height + groupSizeY - 1) / groupSizeY;
    }
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
}

void VulkanRenderer::handleResize(int width, int height) {
    LOG_DEBUG_CAT("Renderer", "Handling resize to {}x{}", width, height);
    if (width <= 0 || height <= 0) {
        LOG_WARNING_CAT("Renderer", "Invalid resize dimensions: {}x{}", width, height);
        return;
    }
    vkDeviceWaitIdle(context_.device);
    LOG_DEBUG_CAT("Renderer", "Device idle for resize");

    swapchainManager_->handleResize(width, height);
    context_.swapchain = swapchainManager_->getSwapchain();
    context_.swapchainImageFormat = swapchainManager_->getSwapchainImageFormat();
    context_.swapchainExtent = swapchainManager_->getSwapchainExtent();
    context_.swapchainImages = swapchainManager_->getSwapchainImages();
    context_.swapchainImageViews = swapchainManager_->getSwapchainImageViews();
    LOG_DEBUG_CAT("Renderer", "Swapchain resized: extent={}x{}, imageCount={}, viewCount={}",
                  context_.swapchainExtent.width, context_.swapchainExtent.height,
                  context_.swapchainImages.size(), context_.swapchainImageViews.size());
    if (context_.swapchainImageViews.empty()) {
        LOG_ERROR_CAT("Renderer", "Swapchain image views are empty after resize");
        throw std::runtime_error("Failed to create swapchain image views after resize");
    }

    Dispose::destroyFramebuffers(context_.device, context_.framebuffers);
    LOG_DEBUG_CAT("Renderer", "Destroyed existing framebuffers");

    createFramebuffers();
    LOG_DEBUG_CAT("Renderer", "Recreated framebuffers");

    if (!bufferManager_) {
        LOG_ERROR_CAT("Renderer", "Buffer manager is null during resize");
        throw std::runtime_error("Buffer manager is null");
    }
    bufferManager_->createUniformBuffers(MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("Renderer", "Recreated uniform buffers for {} frames", MAX_FRAMES_IN_FLIGHT);

    if (context_.storageImage) {
        context_.resourceManager.removeImage(context_.storageImage);
        vkDestroyImage(context_.device, context_.storageImage, nullptr);
        context_.storageImage = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old storage image");
    }
    if (context_.storageImageMemory) {
        context_.resourceManager.removeMemory(context_.storageImageMemory);
        vkFreeMemory(context_.device, context_.storageImageMemory, nullptr);
        context_.storageImageMemory = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Freed old storage image memory");
    }
    if (context_.storageImageView) {
        context_.resourceManager.removeImageView(context_.storageImageView);
        vkDestroyImageView(context_.device, context_.storageImageView, nullptr);
        context_.storageImageView = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old storage image view");
    }
    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, context_.storageImage,
                                         context_.storageImageMemory, context_.storageImageView, width, height,
                                         context_.resourceManager);
    if (!context_.storageImage || !context_.storageImageMemory || !context_.storageImageView) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate storage image: image={:p}, memory={:p}, view={:p}",
                      static_cast<void*>(context_.storageImage), static_cast<void*>(context_.storageImageMemory),
                      static_cast<void*>(context_.storageImageView));
        throw std::runtime_error("Failed to recreate storage image");
    }
    LOG_DEBUG_CAT("Renderer", "Recreated storage image: image={:p}, memory={:p}, view={:p}",
                  static_cast<void*>(context_.storageImage), static_cast<void*>(context_.storageImageMemory),
                  static_cast<void*>(context_.storageImageView));

    if (denoiseImage_) {
        context_.resourceManager.removeImage(denoiseImage_);
        vkDestroyImage(context_.device, denoiseImage_, nullptr);
        denoiseImage_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old denoise image");
    }
    if (denoiseImageMemory_) {
        context_.resourceManager.removeMemory(denoiseImageMemory_);
        vkFreeMemory(context_.device, denoiseImageMemory_, nullptr);
        denoiseImageMemory_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Freed old denoise image memory");
    }
    if (denoiseImageView_) {
        context_.resourceManager.removeImageView(denoiseImageView_);
        vkDestroyImageView(context_.device, denoiseImageView_, nullptr);
        denoiseImageView_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old denoise image view");
    }
    if (denoiseSampler_) {
        vkDestroySampler(context_.device, denoiseSampler_, nullptr);
        denoiseSampler_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old denoise sampler");
    }
    VulkanInitializer::createStorageImage(context_.device, context_.physicalDevice, denoiseImage_,
                                         denoiseImageMemory_, denoiseImageView_, width, height,
                                         context_.resourceManager);
    if (!denoiseImage_ || !denoiseImageMemory_ || !denoiseImageView_) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate denoise image: image={:p}, memory={:p}, view={:p}",
                      static_cast<void*>(denoiseImage_), static_cast<void*>(denoiseImageMemory_),
                      static_cast<void*>(denoiseImageView_));
        throw std::runtime_error("Failed to recreate denoise image");
    }
    LOG_DEBUG_CAT("Renderer", "Recreated denoise image: image={:p}, memory={:p}, view={:p}",
                  static_cast<void*>(denoiseImage_), static_cast<void*>(denoiseImageMemory_),
                  static_cast<void*>(denoiseImageView_));

    VkSamplerCreateInfo denoiseSamplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 0.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(context_.device, &denoiseSamplerInfo, nullptr, &denoiseSampler_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate denoise sampler");
        throw std::runtime_error("Failed to recreate denoise sampler");
    }
    LOG_DEBUG_CAT("Renderer", "Recreated denoise sampler: {:p}", static_cast<void*>(denoiseSampler_));

    if (envMapImage_) {
        context_.resourceManager.removeImage(envMapImage_);
        vkDestroyImage(context_.device, envMapImage_, nullptr);
        envMapImage_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old environment map image");
    }
    if (envMapImageMemory_) {
        context_.resourceManager.removeMemory(envMapImageMemory_);
        vkFreeMemory(context_.device, envMapImageMemory_, nullptr);
        envMapImageMemory_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Freed old environment map image memory");
    }
    if (envMapImageView_) {
        context_.resourceManager.removeImageView(envMapImageView_);
        vkDestroyImageView(context_.device, envMapImageView_, nullptr);
        envMapImageView_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old environment map image view");
    }
    if (envMapSampler_) {
        vkDestroySampler(context_.device, envMapSampler_, nullptr);
        envMapSampler_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old environment map sampler");
    }
    createEnvironmentMap();
    LOG_DEBUG_CAT("Renderer", "Recreated environment map (high-res)");

    for (size_t i = 0; i < materialBuffers_.size(); ++i) {
        if (materialBuffers_[i]) {
            context_.resourceManager.removeBuffer(materialBuffers_[i]);
            vkDestroyBuffer(context_.device, materialBuffers_[i], nullptr);
            materialBuffers_[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Destroyed old material buffer[{}]", i);
        }
        if (materialBufferMemory_[i]) {
            context_.resourceManager.removeMemory(materialBufferMemory_[i]);
            vkFreeMemory(context_.device, materialBufferMemory_[i], nullptr);
            materialBufferMemory_[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Freed old material buffer memory[{}]", i);
        }
    }
    for (size_t i = 0; i < dimensionBuffers_.size(); ++i) {
        if (dimensionBuffers_[i]) {
            context_.resourceManager.removeBuffer(dimensionBuffers_[i]);
            vkDestroyBuffer(context_.device, dimensionBuffers_[i], nullptr);
            dimensionBuffers_[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Destroyed old dimension buffer[{}]", i);
        }
        if (dimensionBufferMemory_[i]) {
            context_.resourceManager.removeMemory(dimensionBufferMemory_[i]);
            vkFreeMemory(context_.device, dimensionBufferMemory_[i], nullptr);
            dimensionBufferMemory_[i] = VK_NULL_HANDLE;
            LOG_DEBUG_CAT("Renderer", "Freed old dimension buffer memory[{}]", i);
        }
    }

    constexpr uint32_t MATERIAL_COUNT = 128;
    constexpr uint32_t DIMENSION_COUNT = 1;
    VkDeviceSize materialBufferSize = sizeof(MaterialData) * MATERIAL_COUNT;
    VkDeviceSize dimensionBufferSize = sizeof(UE::DimensionData) * DIMENSION_COUNT;
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
        .properties = {}
    };
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        .pNext = nullptr,
        .shaderGroupHandleSize = 0,
        .maxRayRecursionDepth = 0,
        .maxShaderGroupStride = 0,
        .shaderGroupBaseAlignment = 0,
        .shaderGroupHandleCaptureReplaySize = 0,
        .maxRayDispatchInvocationCount = 0,
        .shaderGroupHandleAlignment = 0,
        .maxRayHitAttributeSize = 0
    };
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);
    VkDeviceSize minStorageBufferOffsetAlignment = props2.properties.limits.minStorageBufferOffsetAlignment;
    materialBufferSize = (materialBufferSize + minStorageBufferOffsetAlignment - 1) & ~(minStorageBufferOffsetAlignment - 1);
    dimensionBufferSize = (dimensionBufferSize + minStorageBufferOffsetAlignment - 1) & ~(minStorageBufferOffsetAlignment - 1);
    LOG_DEBUG_CAT("Renderer", "Buffer sizes for resize: materialBufferSize={} ({} materials), dimensionBufferSize={}, alignment={}",
                  materialBufferSize, MATERIAL_COUNT, dimensionBufferSize, minStorageBufferOffsetAlignment);

    materialBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    materialBufferMemory_.resize(MAX_FRAMES_IN_FLIGHT);
    dimensionBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    dimensionBufferMemory_.resize(MAX_FRAMES_IN_FLIGHT);
    frames_.resize(MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("Renderer", "Resized frames_ to size: {}", frames_.size());
    if (frames_.size() != MAX_FRAMES_IN_FLIGHT) {
        LOG_ERROR_CAT("Renderer", "Failed to resize frames_ to MAX_FRAMES_IN_FLIGHT ({}), current size: {}", MAX_FRAMES_IN_FLIGHT, frames_.size());
        throw std::runtime_error("Failed to resize frames_ vector");
    }

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, materialBufferSize,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       materialBuffers_[i], materialBufferMemory_[i], &allocFlagsInfo, context_.resourceManager);
        if (!materialBuffers_[i] || !materialBufferMemory_[i]) {
            LOG_ERROR_CAT("Renderer", "Failed to recreate material buffer[{}]: buffer={:p}, memory={:p}",
                          i, static_cast<void*>(materialBuffers_[i]), static_cast<void*>(materialBufferMemory_[i]));
            throw std::runtime_error("Failed to recreate material buffer");
        }
        LOG_DEBUG_CAT("Renderer", "Recreated material buffer[{}]: buffer={:p}, memory={:p}",
                      i, static_cast<void*>(materialBuffers_[i]), static_cast<void*>(materialBufferMemory_[i]));

        VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, dimensionBufferSize,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       dimensionBuffers_[i], dimensionBufferMemory_[i], &allocFlagsInfo, context_.resourceManager);
        if (!dimensionBuffers_[i] || !dimensionBufferMemory_[i]) {
            LOG_ERROR_CAT("Renderer", "Failed to recreate dimension buffer[{}]: buffer={:p}, memory={:p}",
                          i, static_cast<void*>(dimensionBuffers_[i]), static_cast<void*>(dimensionBufferMemory_[i]));
            throw std::runtime_error("Failed to recreate dimension buffer");
        }
        LOG_DEBUG_CAT("Renderer", "Recreated dimension buffer[{}]: buffer={:p}, memory={:p}",
                      i, static_cast<void*>(dimensionBuffers_[i]), static_cast<void*>(dimensionBufferMemory_[i]));

        initializeBufferData(i, materialBufferSize, dimensionBufferSize);
        LOG_DEBUG_CAT("Renderer", "Initialized buffer data for frame {}", i);
    }

    // Reccache index count after potential model reload or resize
    // indexCount_ = static_cast<uint32_t>(getIndices().size());  // Removed: already cached statically

    if (context_.descriptorPool) {
        context_.resourceManager.removeDescriptorPool(context_.descriptorPool);
        vkDestroyDescriptorPool(context_.device, context_.descriptorPool, nullptr);
        context_.descriptorPool = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old descriptor pool");
    }
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 3}
    };
    VkDescriptorPoolCreateInfo poolInfo_desc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = MAX_FRAMES_IN_FLIGHT * 3,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes
    };
    if (vkCreateDescriptorPool(context_.device, &poolInfo_desc, nullptr, &context_.descriptorPool) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate descriptor pool during resize");
        throw std::runtime_error("Failed to recreate descriptor pool");
    }
    context_.resourceManager.addDescriptorPool(context_.descriptorPool);
    LOG_DEBUG_CAT("Renderer", "Recreated descriptor pool: {:p}", static_cast<void*>(context_.descriptorPool));

    if (computeDescriptorSetLayout_) {
        context_.resourceManager.removeDescriptorSetLayout(computeDescriptorSetLayout_);
        vkDestroyDescriptorSetLayout(context_.device, computeDescriptorSetLayout_, nullptr);
        computeDescriptorSetLayout_ = VK_NULL_HANDLE;
        LOG_DEBUG_CAT("Renderer", "Destroyed old compute descriptor set layout");
    }
    VkDescriptorSetLayoutBinding computeBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = computeBindings
    };
    if (vkCreateDescriptorSetLayout(context_.device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout_) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to recreate compute descriptor set layout during resize");
        throw std::runtime_error("Failed to recreate compute descriptor set layout");
    }
    context_.resourceManager.addDescriptorSetLayout(computeDescriptorSetLayout_);
    LOG_DEBUG_CAT("Renderer", "Recreated compute descriptor set layout: {:p}", static_cast<void*>(computeDescriptorSetLayout_));

    std::vector<VkDescriptorSetLayout> rayTracingLayouts(MAX_FRAMES_IN_FLIGHT, context_.rayTracingDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> graphicsLayouts(MAX_FRAMES_IN_FLIGHT, context_.graphicsDescriptorSetLayout);
    std::vector<VkDescriptorSetLayout> computeLayouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo rayTracingAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = rayTracingLayouts.data()
    };
    VkDescriptorSetAllocateInfo graphicsAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = graphicsLayouts.data()
    };
    VkDescriptorSetAllocateInfo computeAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = context_.descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = computeLayouts.data()
    };
    std::vector<VkDescriptorSet> rayTracingSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> graphicsSets(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSet> computeSets(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(context_.device, &rayTracingAllocInfo, rayTracingSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate ray-tracing descriptor sets during resize");
        throw std::runtime_error("Failed to allocate ray-tracing descriptor sets");
    }
    if (vkAllocateDescriptorSets(context_.device, &graphicsAllocInfo, graphicsSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate graphics descriptor sets during resize");
        throw std::runtime_error("Failed to allocate graphics descriptor sets");
    }
    if (vkAllocateDescriptorSets(context_.device, &computeAllocInfo, computeSets.data()) != VK_SUCCESS) {
        LOG_ERROR_CAT("Renderer", "Failed to allocate compute descriptor sets during resize");
        throw std::runtime_error("Failed to allocate compute descriptor sets");
    }
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].rayTracingDescriptorSet = rayTracingSets[i];
        frames_[i].graphicsDescriptorSet = graphicsSets[i];
        frames_[i].computeDescriptorSet = computeSets[i];
        if (context_.topLevelAS != VK_NULL_HANDLE) {
            updateDescriptorSetForFrame(i, context_.topLevelAS);
            updateGraphicsDescriptorSet(i);
            updateComputeDescriptorSet(i);
        }
        LOG_DEBUG_CAT("Renderer", "Reallocated descriptor sets for frame {}: rayTracing={:p}, graphics={:p}, compute={:p}",
                      i, static_cast<void*>(rayTracingSets[i]), static_cast<void*>(graphicsSets[i]), static_cast<void*>(computeSets[i]));
    }

    Dispose::freeCommandBuffers(context_.device, context_.commandPool, context_.commandBuffers);
    LOG_DEBUG_CAT("Renderer", "Freed existing command buffers");
    createCommandBuffers();
    LOG_DEBUG_CAT("Renderer", "Recreated command buffers");

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].imageAvailableSemaphore = swapchainManager_->getImageAvailableSemaphore(i);
        frames_[i].renderFinishedSemaphore = swapchainManager_->getRenderFinishedSemaphore(i);
        frames_[i].fence = swapchainManager_->getInFlightFence(i);
        LOG_DEBUG_CAT("Renderer", "Reassigned sync objects for frame {}: imageSem={:p}, renderSem={:p}, fence={:p}",
                      i, static_cast<void*>(frames_[i].imageAvailableSemaphore),
                      static_cast<void*>(frames_[i].renderFinishedSemaphore),
                      static_cast<void*>(frames_[i].fence));
    }

    width_ = width;
    height_ = height;
    LOG_INFO_CAT("Renderer", "VulkanRenderer resized successfully to {}x{}", width, height);
}

} // namespace VulkanRTX