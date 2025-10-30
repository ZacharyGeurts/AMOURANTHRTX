// src/core.cpp
// Core rendering dispatcher – picks the correct mode at runtime.
// Copyright Zachary Geurts 2025

#include "engine/core.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/types.hpp"  // <-- Added for DenoisePushConstants & MaterialData

namespace VulkanRTX {

/* --------------------------------------------------------------------- *
 *  renderMode1 – the original “sphere + wisp” mode (UPDATED FOR RTX SHADERS)
 * --------------------------------------------------------------------- */
void renderMode1(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode1", "Rendering mode 1 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.02f, 0.02f, 0.05f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode1", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // Use MaterialData::PushConstants (matches raygen.rgen)
        MaterialData::PushConstants push{
            .clearColor      = glm::vec4(0.02f, 0.02f, 0.05f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(0.0f, -1.0f, 0.0f),
            .lightIntensity  = 8.0f + std::sin(deltaTime * 2.0f) * 2.0f,
            .samplesPerPixel = 4,
            .maxDepth        = 5,
            .maxBounces      = 3,
            .russianRoulette = 0.8f,
            .resolution      = glm::vec2(width, height)
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode1", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode1", "Completed mode 1 render");
}

/* --------------------------------------------------------------------- *
 *  renderMode2 – volumetric mist mode
 * --------------------------------------------------------------------- */
void renderMode2(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode2", "Rendering mode 2 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.05f, 0.01f, 0.08f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode2", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(16) glm::vec3 lightPosition;
            alignas(16) glm::vec3 lightColor;
            alignas(4)  float     lightIntensity;
            alignas(4)  uint32_t  samplesPerPixel;
            alignas(4)  uint32_t  maxDepth;
            alignas(4)  uint32_t  maxBounces;
            alignas(4)  float     russianRoulette;
            alignas(4)  float     density;
        } push{
            .clearColor      = glm::vec4(0.05f, 0.01f, 0.08f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition   = glm::vec3(
                std::sin(deltaTime * 1.0f + wavePhase) * 4.0f,
                std::cos(deltaTime * 0.6f) * 3.0f,
                6.0f + std::sin(deltaTime * 0.9f) * 2.0f
            ),
            .lightColor      = glm::vec3(0.6f, 0.4f, 0.8f),
            .lightIntensity  = 10.0f + std::cos(deltaTime * 1.5f) * 3.0f,
            .samplesPerPixel = 8,
            .maxDepth        = 8,
            .maxBounces      = 4,
            .russianRoulette = 0.7f,
            .density         = 0.1f + std::sin(wavePhase) * 0.05f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR |
                           VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode2", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode2", "Completed mode 2 render");
}

/* --------------------------------------------------------------------- *
 *  renderMode3 – animated plasma mode
 * --------------------------------------------------------------------- */
void renderMode3(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode3", "Rendering mode 3 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.1f, 0.0f, 0.2f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode3", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(16) glm::vec3 lightPosition;
            alignas(16) glm::vec3 lightColor;
            alignas(4)  float     lightIntensity;
            alignas(4)  uint32_t  samplesPerPixel;
            alignas(4)  uint32_t  maxDepth;
            alignas(4)  uint32_t  maxBounces;
            alignas(4)  float     russianRoulette;
            alignas(4)  float     plasmaFreq;
        } push{
            .clearColor      = glm::vec4(0.1f, 0.0f, 0.2f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition   = glm::vec3(
                std::sin(deltaTime * 1.2f + wavePhase) * 2.5f,
                std::cos(deltaTime * 0.8f) * 2.5f + std::sin(deltaTime * 1.5f) * 1.0f,
                4.0f + std::cos(deltaTime * 1.0f) * 1.5f
            ),
            .lightColor      = glm::vec3(1.0f, 0.3f, 0.6f),
            .lightIntensity  = 12.0f + std::sin(deltaTime * 2.5f + wavePhase) * 4.0f,
            .samplesPerPixel = 6,
            .maxDepth        = 6,
            .maxBounces      = 3,
            .russianRoulette = 0.85f,
            .plasmaFreq      = 2.0f + std::sin(wavePhase) * 0.5f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode3", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode3", "Completed mode 3 render");
}

/* --------------------------------------------------------------------- *
 *  renderMode4 – caustic reflections mode
 * --------------------------------------------------------------------- */
void renderMode4(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode4", "Rendering mode 4 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.0f, 0.03f, 0.06f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode4", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(16) glm::vec3 lightPosition;
            alignas(16) glm::vec3 lightColor;
            alignas(4)  float     lightIntensity;
            alignas(4)  uint32_t  samplesPerPixel;
            alignas(4)  uint32_t  maxDepth;
            alignas(4)  uint32_t  maxBounces;
            alignas(4)  float     russianRoulette;
            alignas(4)  float     causticStrength;
        } push{
            .clearColor      = glm::vec4(0.0f, 0.03f, 0.06f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition   = glm::vec3(
                std::sin(deltaTime * 0.9f) * 3.5f,
                std::cos(deltaTime * 0.4f + wavePhase) * 2.5f,
                5.5f + std::sin(deltaTime * 0.6f) * 1.2f
            ),
            .lightColor      = glm::vec3(0.8f, 0.5f, 0.9f),
            .lightIntensity  = 9.0f + std::cos(deltaTime * 1.8f) * 2.5f,
            .samplesPerPixel = 12,
            .maxDepth        = 10,
            .maxBounces      = 5,
            .russianRoulette = 0.75f,
            .causticStrength = 1.5f + std::cos(wavePhase) * 0.5f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode4", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode4", "Completed mode 4 render");
}

/* --------------------------------------------------------------------- *
 *  renderMode5 – subsurface scattering mode
 * --------------------------------------------------------------------- */
void renderMode5(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode5", "Rendering mode 5 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.03f, 0.04f, 0.02f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode5", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(16) glm::vec3 lightPosition;
            alignas(16) glm::vec3 lightColor;
            alignas(4)  float     lightIntensity;
            alignas(4)  uint32_t  samplesPerPixel;
            alignas(4)  uint32_t  maxDepth;
            alignas(4)  uint32_t  maxBounces;
            alignas(4)  float     russianRoulette;
            alignas(4)  float     scatterRadius;
        } push{
            .clearColor      = glm::vec4(0.03f, 0.04f, 0.02f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition   = glm::vec3(
                std::sin(deltaTime * 0.7f) * 2.0f,
                std::cos(deltaTime * 0.9f + wavePhase) * 3.0f + std::sin(deltaTime * 1.1f) * 1.0f,
                4.5f + std::cos(deltaTime * 0.5f) * 0.8f
            ),
            .lightColor      = glm::vec3(0.9f, 0.8f, 0.4f),
            .lightIntensity  = 7.0f + std::sin(deltaTime * 1.2f) * 1.5f,
            .samplesPerPixel = 16,
            .maxDepth        = 12,
            .maxBounces      = 6,
            .russianRoulette = 0.6f,
            .scatterRadius   = 0.2f + std::sin(wavePhase * 0.5f) * 0.1f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR |
                           VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode5", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode5", "Completed mode 5 render");
}

/* --------------------------------------------------------------------- *
 *  renderMode6 – path-traced fireflies mode
 * --------------------------------------------------------------------- */
void renderMode6(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode6", "Rendering mode 6 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.0f, 0.05f, 0.1f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode6", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(16) glm::vec3 lightPosition;
            alignas(16) glm::vec3 lightColor;
            alignas(4)  float     lightIntensity;
            alignas(4)  uint32_t  samplesPerPixel;
            alignas(4)  uint32_t  maxDepth;
            alignas(4)  uint32_t  maxBounces;
            alignas(4)  float     russianRoulette;
            alignas(4)  uint32_t  numFireflies;
        } push{
            .clearColor      = glm::vec4(0.0f, 0.05f, 0.1f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition   = glm::vec3(
                std::sin(deltaTime * 1.1f) * 1.5f,
                std::cos(deltaTime * 1.3f + wavePhase) * 1.5f,
                3.0f + std::sin(deltaTime * 0.4f) * 0.5f
            ),
            .lightColor      = glm::vec3(1.0f, 1.0f, 0.2f),
            .lightIntensity  = 5.0f + std::cos(deltaTime * 3.0f) * 1.0f,
            .samplesPerPixel = 32,
            .maxDepth        = 15,
            .maxBounces      = 8,
            .russianRoulette = 0.5f,
            .numFireflies    = static_cast<uint32_t>(50 + std::sin(wavePhase) * 20)
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR |
                           VK_SHADER_STAGE_CALLABLE_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode6", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode6", "Completed mode 6 render");
}

/* --------------------------------------------------------------------- *
 *  renderMode7 – global illumination mode
 * --------------------------------------------------------------------- */
void renderMode7(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode7", "Rendering mode 7 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.08f, 0.02f, 0.04f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode7", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(16) glm::vec3 lightPosition;
            alignas(16) glm::vec3 lightColor;
            alignas(4)  float     lightIntensity;
            alignas(4)  uint32_t  samplesPerPixel;
            alignas(4)  uint32_t  maxDepth;
            alignas(4)  uint32_t  maxBounces;
            alignas(4)  float     russianRoulette;
            alignas(4)  float     giRadius;
        } push{
            .clearColor      = glm::vec4(0.08f, 0.02f, 0.04f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition   = glm::vec3(
                std::sin(deltaTime * 0.5f + wavePhase) * 4.0f,
                std::cos(deltaTime * 0.7f) * 2.0f,
                7.0f + std::cos(deltaTime * 0.3f) * 1.0f
            ),
            .lightColor      = glm::vec3(0.7f, 0.9f, 0.5f),
            .lightIntensity  = 15.0f + std::sin(deltaTime * 0.8f) * 5.0f,
            .samplesPerPixel = 64,
            .maxDepth        = 20,
            .maxBounces      = 10,
            .russianRoulette = 0.4f,
            .giRadius        = 2.0f + std::cos(wavePhase) * 0.5f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode7", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode7", "Completed mode 7 render");
}

/* --------------------------------------------------------------------- *
 *  renderMode8 – denoiser showcase mode
 * --------------------------------------------------------------------- */
void renderMode8(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode8", "Rendering mode 8 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.04f, 0.02f, 0.07f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode8", "Ray tracing disabled, falling back to rasterization");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(16) glm::vec3 lightPosition;
            alignas(16) glm::vec3 lightColor;
            alignas(4)  float     lightIntensity;
            alignas(4)  uint32_t  samplesPerPixel;
            alignas(4)  uint32_t  maxDepth;
            alignas(4)  uint32_t  maxBounces;
            alignas(4)  float     russianRoulette;
            alignas(4)  float     noiseScale;
        } push{
            .clearColor      = glm::vec4(0.04f, 0.02f, 0.07f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition   = glm::vec3(
                std::sin(deltaTime * 0.6f) * 3.0f,
                std::cos(deltaTime * 0.8f + wavePhase) * 2.0f + std::sin(deltaTime * 1.0f) * 1.2f,
                5.0f + std::cos(deltaTime * 0.4f) * 0.8f
            ),
            .lightColor      = glm::vec3(0.5f, 0.6f, 1.0f),
            .lightIntensity  = 6.0f + std::sin(deltaTime * 2.2f) * 1.8f,
            .samplesPerPixel = 1,
            .maxDepth        = 4,
            .maxBounces      = 2,
            .russianRoulette = 0.9f,
            .noiseScale      = 0.1f + std::sin(wavePhase) * 0.05f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode8", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode8", "Completed mode 8 render");
}

/* --------------------------------------------------------------------- *
 *  renderMode9 – advanced hybrid raster/RT mode
 * --------------------------------------------------------------------- */
void renderMode9(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context
) {
    LOG_DEBUG_CAT("RenderMode9", "Rendering mode 9 with zoomLevel: {}, wavePhase: {}", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.06f, 0.03f, 0.09f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Hybrid: Always use raster for base
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);

    if (context.enableRayTracing) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        struct PushConstants {
            alignas(16) glm::vec4 clearColor;
            alignas(16) glm::vec3 cameraPosition;
            alignas(16) glm::vec3 lightPosition;
            alignas(16) glm::vec3 lightColor;
            alignas(4)  float     lightIntensity;
            alignas(4)  uint32_t  samplesPerPixel;
            alignas(4)  uint32_t  maxDepth;
            alignas(4)  uint32_t  maxBounces;
            alignas(4)  float     russianRoulette;
            alignas(4)  float     hybridBlend;
        } push{
            .clearColor      = glm::vec4(0.06f, 0.03f, 0.09f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            .lightPosition   = glm::vec3(
                std::sin(deltaTime * 0.4f) * 2.5f,
                std::cos(deltaTime * 0.5f + wavePhase) * 3.5f,
                6.0f + std::sin(deltaTime * 0.7f) * 1.5f
            ),
            .lightColor      = glm::vec3(0.3f, 0.9f, 0.7f),
            .lightIntensity  = 11.0f + std::cos(deltaTime * 1.6f) * 3.0f,
            .samplesPerPixel = 4,
            .maxDepth        = 7,
            .maxBounces      = 4,
            .russianRoulette = 0.8f,
            .hybridBlend     = 0.7f + std::sin(wavePhase) * 0.2f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                           VK_SHADER_STAGE_MISS_BIT_KHR,
                           0, sizeof(push), &push);

        VkStridedDeviceAddressRegionKHR raygenEntry{
            .deviceAddress = context.raygenSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR missEntry{
            .deviceAddress = context.missSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR hitEntry{
            .deviceAddress = context.hitSbtAddress,
            .stride = context.sbtRecordSize,
            .size   = context.sbtRecordSize
        };
        VkStridedDeviceAddressRegionKHR callableEntry{};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode9", "context.vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer,
                                  &raygenEntry, &missEntry, &hitEntry, &callableEntry,
                                  width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode9", "Completed mode 9 render");
}

/* --------------------------------------------------------------------- *
 *  dispatchRenderMode – single entry point used by the renderer
 * --------------------------------------------------------------------- */
void dispatchRenderMode(
    uint32_t imageIndex,
    VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    Vulkan::Context& context,
    int renderMode
) {
    constexpr int MIN_MODE = 1;
    constexpr int MAX_MODE = 9;
    int mode = (renderMode >= MIN_MODE && renderMode <= MAX_MODE) ? renderMode : 1;

    LOG_DEBUG_CAT("Dispatcher", "Dispatching render mode {}", mode);

    switch (mode) {
        case 1: renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        case 2: renderMode2(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        case 3: renderMode3(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        case 4: renderMode4(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        case 5: renderMode5(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        case 6: renderMode6(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        case 7: renderMode7(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        case 8: renderMode8(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        case 9: renderMode9(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                            zoomLevel, width, height, wavePhase,
                            pipelineLayout, descriptorSet, device, vertexBufferMemory,
                            pipeline, deltaTime, renderPass, framebuffer, context); break;
        default:
            renderMode1(imageIndex, vertexBuffer, commandBuffer, indexBuffer,
                        zoomLevel, width, height, wavePhase,
                        pipelineLayout, descriptorSet, device, vertexBufferMemory,
                        pipeline, deltaTime, renderPass, framebuffer, context);
            break;
    }
}

} // namespace VulkanRTX