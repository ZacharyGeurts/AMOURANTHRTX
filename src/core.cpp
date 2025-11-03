// src/core.cpp
// Copyright Zachary Geurts 2025
// FINAL: C++11, NO std::span, NO std::format, NO threading, FULL LOGGING

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/core.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <cmath>
#include <cstdio>

namespace VulkanRTX {

// ---------------------------------------------------------------------
//  Push Constants – 80 bytes, matches ShaderBindingTable
// ---------------------------------------------------------------------
struct alignas(16) RTConstants {
    alignas(16) glm::vec4 clearColor;
    alignas(16) glm::vec3 cameraPosition;
    alignas(4)  float     _pad0;
    alignas(16) glm::vec3 lightDirection;
    alignas(4)  float     lightIntensity;
    alignas(4)  uint32_t  samplesPerPixel;
    alignas(4)  uint32_t  maxDepth;
    alignas(4)  uint32_t  maxBounces;
    alignas(4)  float     russianRoulette;
    alignas(8)  glm::vec2 resolution;
    alignas(4)  uint32_t  showEnvMapOnly;
};
static_assert(sizeof(RTConstants) == 80, "RTConstants must be 80 bytes");

// ---------------------------------------------------------------------
//  Helper: Log with snprintf (C++11 safe)
// ---------------------------------------------------------------------
#define LOG_PUSH(mode, ...) do { \
    char buf[256]; \
    std::snprintf(buf, sizeof(buf), __VA_ARGS__); \
    LOG_DEBUG_CAT("RenderMode" #mode, "%s", buf); \
} while(0)

// ---------------------------------------------------------------------
//  renderMode1 – ENV MAP ONLY
// ---------------------------------------------------------------------
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
    LOG_PUSH(1, "Rendering mode 1 (Env Map Only) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.02f, 0.02f, 0.05f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode1", "Ray tracing disabled → raster fallback");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.02f, 0.02f, 0.05f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(0.0f, -1.0f, 0.0f),
            .lightIntensity  = 8.0f + std::sin(deltaTime * 2.0f) * 2.0f,
            .samplesPerPixel = 4,
            .maxDepth        = 5,
            .maxBounces      = 3,
            .russianRoulette = 0.8f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 1
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        [[maybe_unused]] VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) {
            LOG_ERROR_CAT("RenderMode1", "vkCmdTraceRaysKHR is null");
            throw std::runtime_error("Ray tracing extension not loaded");
        }
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &raygen, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode1", "Completed");
}

// ---------------------------------------------------------------------
//  renderMode2 – volumetric mist
// ---------------------------------------------------------------------
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
    LOG_PUSH(2, "Rendering mode 2 (Volumetric Mist) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.05f, 0.01f, 0.08f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode2", "Ray tracing disabled → raster fallback");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.05f, 0.01f, 0.08f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(
                std::sin(deltaTime * 1.0f + wavePhase) * 4.0f,
                std::cos(deltaTime * 0.6f) * 3.0f,
                6.0f + std::sin(deltaTime * 0.9f) * 2.0f
            ),
            .lightIntensity  = 10.0f + std::cos(deltaTime * 1.5f) * 3.0f,
            .samplesPerPixel = 8,
            .maxDepth        = 8,
            .maxBounces      = 4,
            .russianRoulette = 0.7f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 0
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) throw std::runtime_error("Ray tracing extension not loaded");
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode2", "Completed");
}

// ---------------------------------------------------------------------
//  renderMode3 – animated plasma
// ---------------------------------------------------------------------
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
    LOG_PUSH(3, "Rendering mode 3 (Plasma) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.1f, 0.0f, 0.2f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode3", "Ray tracing disabled → raster fallback");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.1f, 0.0f, 0.2f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(
                std::sin(deltaTime * 1.2f + wavePhase) * 2.5f,
                std::cos(deltaTime * 0.8f) * 2.5f + std::sin(deltaTime * 1.5f) * 1.0f,
                4.0f + std::cos(deltaTime * 1.0f) * 1.5f
            ),
            .lightIntensity  = 12.0f + std::sin(deltaTime * 2.5f + wavePhase) * 4.0f,
            .samplesPerPixel = 6,
            .maxDepth        = 6,
            .maxBounces      = 3,
            .russianRoulette = 0.85f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 0
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) throw std::runtime_error("Ray tracing extension not loaded");
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode3", "Completed");
}

// ---------------------------------------------------------------------
//  renderMode4 – caustic reflections
// ---------------------------------------------------------------------
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
    LOG_PUSH(4, "Rendering mode 4 (Caustics) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.0f, 0.03f, 0.06f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode4", "Ray tracing disabled → raster fallback");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.0f, 0.03f, 0.06f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(
                std::sin(deltaTime * 0.9f) * 3.5f,
                std::cos(deltaTime * 0.4f + wavePhase) * 2.5f,
                5.5f + std::sin(deltaTime * 0.6f) * 1.2f
            ),
            .lightIntensity  = 9.0f + std::cos(deltaTime * 1.8f) * 2.5f,
            .samplesPerPixel = 12,
            .maxDepth        = 10,
            .maxBounces      = 5,
            .russianRoulette = 0.75f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 0
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) throw std::runtime_error("Ray tracing extension not loaded");
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode4", "Completed");
}

// ---------------------------------------------------------------------
//  renderMode5 – subsurface scattering
// ---------------------------------------------------------------------
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
    LOG_PUSH(5, "Rendering mode 5 (SSS) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.03f, 0.04f, 0.02f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode5", "Ray tracing disabled → raster fallback");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.03f, 0.04f, 0.02f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(
                std::sin(deltaTime * 0.7f) * 2.0f,
                std::cos(deltaTime * 0.9f + wavePhase) * 3.0f + std::sin(deltaTime * 1.1f) * 1.0f,
                4.5f + std::cos(deltaTime * 0.5f) * 0.8f
            ),
            .lightIntensity  = 7.0f + std::sin(deltaTime * 1.2f) * 1.5f,
            .samplesPerPixel = 16,
            .maxDepth        = 12,
            .maxBounces      = 6,
            .russianRoulette = 0.6f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 0
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) throw std::runtime_error("Ray tracing extension not loaded");
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode5", "Completed");
}

// ---------------------------------------------------------------------
//  renderMode6 – path-traced fireflies
// ---------------------------------------------------------------------
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
    LOG_PUSH(6, "Rendering mode 6 (Fireflies) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.0f, 0.05f, 0.1f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode6", "Ray tracing disabled → raster fallback");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.0f, 0.05f, 0.1f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(
                std::sin(deltaTime * 1.1f) * 1.5f,
                std::cos(deltaTime * 1.3f + wavePhase) * 1.5f,
                3.0f + std::sin(deltaTime * 0.4f) * 0.5f
            ),
            .lightIntensity  = 5.0f + std::cos(deltaTime * 3.0f) * 1.0f,
            .samplesPerPixel = 32,
            .maxDepth        = 15,
            .maxBounces      = 8,
            .russianRoulette = 0.5f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 0
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) throw std::runtime_error("Ray tracing extension not loaded");
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode6", "Completed");
}

// ---------------------------------------------------------------------
//  renderMode7 – global illumination
// ---------------------------------------------------------------------
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
    LOG_PUSH(7, "Rendering mode 7 (GI) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.08f, 0.02f, 0.04f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode7", "Ray tracing disabled → raster fallback");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.08f, 0.02f, 0.04f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(
                std::sin(deltaTime * 0.5f + wavePhase) * 4.0f,
                std::cos(deltaTime * 0.7f) * 2.0f,
                7.0f + std::cos(deltaTime * 0.3f) * 1.0f
            ),
            .lightIntensity  = 15.0f + std::sin(deltaTime * 0.8f) * 5.0f,
            .samplesPerPixel = 64,
            .maxDepth        = 20,
            .maxBounces      = 10,
            .russianRoulette = 0.4f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 0
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) throw std::runtime_error("Ray tracing extension not loaded");
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode7", "Completed");
}

// ---------------------------------------------------------------------
//  renderMode8 – denoiser showcase
// ---------------------------------------------------------------------
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
    LOG_PUSH(8, "Rendering mode 8 (Denoiser) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.04f, 0.02f, 0.07f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (!context.enableRayTracing) {
        LOG_WARNING_CAT("RenderMode8", "Ray tracing disabled → raster fallback");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);
    } else {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.04f, 0.02f, 0.07f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(
                std::sin(deltaTime * 0.6f) * 3.0f,
                std::cos(deltaTime * 0.8f + wavePhase) * 2.0f + std::sin(deltaTime * 1.0f) * 1.2f,
                5.0f + std::cos(deltaTime * 0.4f) * 0.8f
            ),
            .lightIntensity  = 6.0f + std::sin(deltaTime * 2.2f) * 1.8f,
            .samplesPerPixel = 1,
            .maxDepth        = 4,
            .maxBounces      = 2,
            .russianRoulette = 0.9f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 0
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) throw std::runtime_error("Ray tracing extension not loaded");
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode8", "Completed");
}

// ---------------------------------------------------------------------
//  renderMode9 – hybrid raster/RT
// ---------------------------------------------------------------------
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
    LOG_PUSH(9, "Rendering mode 9 (Hybrid) | zoom: %.2f | wave: %.2f", zoomLevel, wavePhase);

    VkClearValue clearValue = {{{0.06f, 0.03f, 0.09f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Raster base
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, 3, 1, 0, 0, 0);

    if (context.enableRayTracing) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        RTConstants push = {
            .clearColor      = glm::vec4(0.06f, 0.03f, 0.09f, 1.0f),
            .cameraPosition  = glm::vec3(0.0f, 0.0f, 5.0f + zoomLevel),
            ._pad0           = 0.0f,
            .lightDirection  = glm::vec3(
                std::sin(deltaTime * 0.4f) * 2.5f,
                std::cos(deltaTime * 0.5f + wavePhase) * 3.5f,
                6.0f + std::sin(deltaTime * 0.7f) * 1.5f
            ),
            .lightIntensity  = 11.0f + std::cos(deltaTime * 1.6f) * 3.0f,
            .samplesPerPixel = 4,
            .maxDepth        = 7,
            .maxBounces      = 4,
            .russianRoulette = 0.8f,
            .resolution      = glm::vec2(width, height),
            .showEnvMapOnly  = 0
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            0, sizeof(RTConstants), &push);

        VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR hit    = { context.hitSbtAddress,    context.sbtRecordSize, context.sbtRecordSize };
        VkStridedDeviceAddressRegionKHR callable = {};

        if (!context.vkCmdTraceRaysKHR) throw std::runtime_error("Ray tracing extension not loaded");
        context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable, width, height, 1);
    }

    vkCmdEndRenderPass(commandBuffer);
    LOG_DEBUG_CAT("RenderMode9", "Completed");
}

} // namespace VulkanRTX