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
    LOG_DEBUG_CAT("Renderer", "Updating descriptor set for frame {}", frameIndex);
    if (frameIndex >= frames_.size()) {
        throw std::out_of_range("Invalid frame index");
    }
    VkDescriptorSet descriptorSet = frames_[frameIndex].rayTracingDescriptorSet;
    VkWriteDescriptorSetAccelerationStructureKHR accelDescriptor{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };
    VkDescriptorImageInfo storageImageInfo{
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
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        },
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
    };
    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanRenderer::updateGraphicsDescriptorSet(uint32_t frameIndex) {
    VkDescriptorSet descriptorSet = frames_[frameIndex].graphicsDescriptorSet;
    VkDescriptorImageInfo imageInfo{
        .sampler = denoiseSampler_,
        .imageView = denoiseImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo
    };
    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
}

void VulkanRenderer::updateComputeDescriptorSet(uint32_t frameIndex) {
    VkDescriptorSet descriptorSet = frames_[frameIndex].computeDescriptorSet;
    VkDescriptorImageInfo inputInfo{
        .imageView = context_.storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo outputInfo{
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

void VulkanRenderer::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {
    for (uint32_t i = 0; i < frames_.size(); ++i) {
        VkDescriptorSet ds = frames_[i].rayTracingDescriptorSet;
        VkWriteDescriptorSetAccelerationStructureKHR tlasInfo{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas
        };
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &tlasInfo,
            .dstSet = ds,
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
        vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
    }
}

void VulkanRenderer::renderFrame(const Camera& camera) {
    if (frames_.empty() || currentFrame_ >= frames_.size()) return;

    vkWaitForFences(context_.device, 1, &frames_[currentFrame_].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(context_.device, 1, &frames_[currentFrame_].fence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                            frames_[currentFrame_].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
        return;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire image");
    }

    vkResetCommandBuffer(frames_[currentFrame_].commandBuffer, 0);

    UE::UniformBufferObject ubo{
        .model = glm::mat4(1.0f),
        .view = camera.getViewMatrix(),
        .proj = camera.getProjectionMatrix()
    };
    void* data;
    vkMapMemory(context_.device, bufferManager_->getUniformBufferMemory(currentFrame_), 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_.device, bufferManager_->getUniformBufferMemory(currentFrame_));

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

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    vkBeginCommandBuffer(frames_[currentFrame_].commandBuffer, &beginInfo);

    recordRayTracingCommands(frames_[currentFrame_].commandBuffer, context_.swapchainExtent, context_.storageImage,
                             context_.storageImageView, pushConstants, context_.topLevelAS);
    denoiseImage(frames_[currentFrame_].commandBuffer, context_.storageImage, context_.storageImageView,
                 denoiseImage_, denoiseImageView_);

    VkImageMemoryBarrier graphicsBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = denoiseImage_,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(frames_[currentFrame_].commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &graphicsBarrier);

    VkClearValue clearValue = {{{0.2f, 0.2f, 0.3f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = pipelineManager_->getRenderPass(),
        .framebuffer = context_.framebuffers[imageIndex],
        .renderArea = {{0, 0}, context_.swapchainExtent},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(frames_[currentFrame_].commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(frames_[currentFrame_].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineManager_->getGraphicsPipeline());
    vkCmdBindDescriptorSets(frames_[currentFrame_].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineManager_->getGraphicsPipelineLayout(), 0, 1, &frames_[currentFrame_].graphicsDescriptorSet, 0, nullptr);

    vkCmdPushConstants(frames_[currentFrame_].commandBuffer, pipelineManager_->getGraphicsPipelineLayout(),
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
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = denoiseImage_,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(frames_[currentFrame_].commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &postGraphicsBarrier);

    vkEndCommandBuffer(frames_[currentFrame_].commandBuffer);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &frames_[currentFrame_].commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frames_[currentFrame_].renderFinishedSemaphore
    };
    vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, frames_[currentFrame_].fence);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames_[currentFrame_].renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &imageIndex
    };
    VkResult presentResult = vkQueuePresentKHR(context_.presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to present queue");
    }

    // Frame counter with 1-second logging
    ++frameCount_;
    if (FPS_COUNTER) {
        ++framesThisSecond_;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFPSTime_).count();
        if (elapsed >= 1) {
            LOG_INFO_CAT("FPS", "Frames rendered: {} | Average FPS: {:.2f}", frameCount_, static_cast<double>(framesThisSecond_) / elapsed);
            lastFPSTime_ = now;
            framesThisSecond_ = 0;
        }
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::recordRayTracingCommands(VkCommandBuffer commandBuffer, VkExtent2D extent, VkImage outputImage,
                                             VkImageView outputImageView, const MaterialData::PushConstants& pushConstants,
                                             VkAccelerationStructureKHR tlas) {
    VkImageMemoryBarrier outputBarrier{
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

void VulkanRenderer::denoiseImage(VkCommandBuffer commandBuffer, VkImage inputImage, VkImageView inputImageView,
                                 VkImage outputImage, VkImageView outputImageView) {
    VkImageMemoryBarrier inputBarrier{
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

    VkImageMemoryBarrier outputBarrier{
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
    VkDeviceSize dimensionSize = sizeof(UE::DimensionData) * 1;
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
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 3}
    };
    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT * 3,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes
    };
    vkCreateDescriptorPool(context_.device, &poolInfo, nullptr, &context_.descriptorPool);
    context_.resourceManager.addDescriptorPool(context_.descriptorPool);

    if (computeDescriptorSetLayout_) {
        context_.resourceManager.removeDescriptorSetLayout(computeDescriptorSetLayout_);
        vkDestroyDescriptorSetLayout(context_.device, computeDescriptorSetLayout_, nullptr);
    }
    VkDescriptorSetLayoutBinding computeBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = computeBindings
    };
    vkCreateDescriptorSetLayout(context_.device, &computeLayoutInfo, nullptr, &computeDescriptorSetLayout_);
    context_.resourceManager.addDescriptorSetLayout(computeDescriptorSetLayout_);

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
    vkAllocateDescriptorSets(context_.device, &rtAlloc, rtSets.data());
    vkAllocateDescriptorSets(context_.device, &graphicsAlloc, gSets.data());
    vkAllocateDescriptorSets(context_.device, &computeAlloc, cSets.data());
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].rayTracingDescriptorSet = rtSets[i];
        frames_[i].graphicsDescriptorSet = gSets[i];
        frames_[i].computeDescriptorSet = cSets[i];
        updateDescriptorSetForFrame(i, context_.topLevelAS);
        updateGraphicsDescriptorSet(i);
        updateComputeDescriptorSet(i);
    }

    Dispose::freeCommandBuffers(context_.device, context_.commandPool, context_.commandBuffers);
    createCommandBuffers();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frames_[i].imageAvailableSemaphore = swapchainManager_->getImageAvailableSemaphore(i);
        frames_[i].renderFinishedSemaphore = swapchainManager_->getRenderFinishedSemaphore(i);
        frames_[i].fence = swapchainManager_->getInFlightFence(i);
    }

    width_ = width;
    height_ = height;
}

} // namespace VulkanRTX