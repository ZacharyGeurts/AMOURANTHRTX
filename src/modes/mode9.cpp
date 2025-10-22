// mode9.cpp
// Implementation of renderMode9 for AMOURANTH RTX Engine to draw a simple scene with rasterization and UE-enhanced ambient lighting.
// Copyright Zachary Geurts 2025

#include "engine/core.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

struct GraphicsPushConstants {
    alignas(16) glm::vec4 clearColor;      // 16 bytes
    alignas(4) float lightIntensity;       // 4 bytes
    float padding[3] = {0.0f, 0.0f, 0.0f}; // 12 bytes padding to align total to 32 bytes
    // Matches shader's sizeof(MaterialData::PushConstants) expectation
};

void renderMode9(const UE::AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, int width, int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, [[maybe_unused]] float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer) {
    // Note: This mode uses the graphics pipeline for rasterization, integrating UniversalEquation for dynamic lighting.

    LOG_SIMULATION("Initiating Mode 9 Rasterization Render - ImageIndex: {}, Resolution: {}x{}", imageIndex, width, height);

    // Access UniversalEquation for dynamic parameters
    const auto& ue = amouranth->getUniversalEquation();
    const auto& dimData = ue.getDimensionData();
    int currentDim = ue.getCurrentDimension();
    float simTime = ue.getSimulationTime();
    long double influence = ue.getInfluence();
    long double godWaveFreq = ue.getGodWaveFreq();
    long double nurbEnergyStrength = ue.getNurbEnergyStrength();
    long double spinInteraction = ue.getSpinInteraction();
    long double emFieldStrength = ue.getEMFieldStrength();

    LOG_SIMULATION("UniversalEquation Parameters - Dimension: {}, Simulation Time: {:.3f}s, Influence: {:.3f}, GodWaveFreq: {:.3f}, Vertices: {}",
                   currentDim, simTime, influence, godWaveFreq, ue.getCurrentVertices());

    // Dynamic light adjustment based on UniversalEquation values
    glm::vec3 lightPos = glm::vec3(0.0f);
    glm::vec3 lightColor = glm::vec3(1.0f); // Default white
    float lightIntensity = 1.0f;
    float lightMovementSpeed = static_cast<float>(godWaveFreq * 0.5L); // Modulate with GodWaveFreq

    LOG_SIMULATION("Computing dynamic light parameters for dimension {}", currentDim);

    if (!dimData.empty() && currentDim >= 0 && static_cast<size_t>(currentDim) < dimData.size()) {
        const auto& data = dimData[currentDim];
        
        // Light position: Incorporate dimension position and NURBS-based oscillation
        lightPos = data.position + glm::vec3(
            static_cast<float>(std::sin(simTime * lightMovementSpeed) * data.scale * ue.getTwoD()),
            static_cast<float>(std::cos(simTime * lightMovementSpeed * 1.3L) * data.scale * ue.getThreeDInfluence()),
            static_cast<float>(std::sin(simTime * lightMovementSpeed * 0.7L) * data.scale * ue.getOneDPermeation())
        );

        // Light intensity: Modulated by observable energy, GodWaveEnergy, and influence
        lightIntensity = static_cast<float>(data.observable + data.GodWaveEnergy) * static_cast<float>(influence);
        lightIntensity = std::clamp(lightIntensity, 0.1f, 10.0f);

        // Light color: Map energies to RGB with dynamic modulation
        lightColor = glm::vec3(
            static_cast<float>(std::min(data.nurbEnergy * nurbEnergyStrength, 1.0L)) * (1.0f + std::sin(simTime * godWaveFreq) * 0.3f), // Red
            static_cast<float>(std::min(data.spinEnergy * spinInteraction, 1.0L)) * (1.0f + std::cos(simTime * godWaveFreq * 1.5L) * 0.4f), // Green
            static_cast<float>(std::min(data.fieldEnergy * emFieldStrength, 1.0L)) * (1.0f + std::sin(simTime * godWaveFreq * 2.0L) * 0.2f) // Blue
        );
        lightColor = glm::clamp(lightColor, glm::vec3(0.0f), glm::vec3(1.0f));

        LOG_SIMULATION("Dynamic Light Parameters - Position: ({:.3f}, {:.3f}, {:.3f}), Color: ({:.3f}, {:.3f}, {:.3f}), Intensity: {:.3f}, Movement Speed: {:.3f}",
                       lightPos.x, lightPos.y, lightPos.z, lightColor.r, lightColor.g, lightColor.b, lightIntensity, lightMovementSpeed);
    } else {
        LOG_WARNING("No dimension data available for light adjustment in Mode 9 - Using default parameters");
    }

    // Incorporate vertex data for ambient lighting contribution
    const auto& projectedVerts = ue.getProjectedVerts();
    glm::vec3 ambientContribution = glm::vec3(0.0f);
    uint64_t vertexCount = std::min(ue.getCurrentVertices(), static_cast<uint64_t>(projectedVerts.size()));
    
    LOG_SIMULATION("Processing {} projected vertices for ambient lighting", vertexCount);

    for (uint64_t i = 0; i < vertexCount; ++i) {
        try {
            ue.validateVertexIndex(i);
            const auto& vert = projectedVerts[i];
            long double waveAmplitude = ue.getVertexWaveAmplitude(i);
            ambientContribution += glm::vec3(vert) * static_cast<float>(waveAmplitude * 0.01L);
        } catch (const std::exception& e) {
            LOG_WARNING("Failed processing vertex {} for ambient contribution: {}", i, e.what());
        }
    }
    if (vertexCount > 0) {
        ambientContribution /= static_cast<float>(vertexCount);
        ambientContribution = glm::clamp(ambientContribution, glm::vec3(0.0f), glm::vec3(0.2f));
        lightColor += ambientContribution;
        lightColor = glm::clamp(lightColor, glm::vec3(0.0f), glm::vec3(1.0f));
        LOG_SIMULATION("Ambient Contribution Added - Color Adjustment: ({:.3f}, {:.3f}, {:.3f})", 
                       ambientContribution.r, ambientContribution.g, ambientContribution.b);
    } else {
        LOG_WARNING("No valid vertices for ambient contribution");
    }

    // Begin render pass
    VkClearValue clearValue = {{{ambientContribution.r, ambientContribution.g, ambientContribution.b, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
        },
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline
    LOG_SIMULATION("Binding graphics pipeline");
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind descriptor set (contains texture and MVP UBO)
    LOG_SIMULATION("Binding descriptor set for texture and MVP");
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Bind vertex and index buffers (assuming sphere mesh is provided)
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Use local GraphicsPushConstants struct for Vulkan shader compatibility
    GraphicsPushConstants pushConstants{};
    pushConstants.clearColor = glm::vec4(ambientContribution, 1.0f);
    pushConstants.clearColor += glm::vec4(lightColor * 0.1f, 0.0f);
    pushConstants.clearColor = glm::clamp(pushConstants.clearColor, glm::vec4(0.0f), glm::vec4(1.0f));
    pushConstants.lightIntensity = lightIntensity;

    // Log populated constants for debugging
    LOG_SIMULATION("GraphicsPushConstants - ClearColor: ({:.3f},{:.3f},{:.3f},{:.3f}), Intensity: {:.3f}",
                   pushConstants.clearColor.r, pushConstants.clearColor.g, pushConstants.clearColor.b, pushConstants.clearColor.a,
                   pushConstants.lightIntensity);

    // Push to command buffer (stages for vertex and fragment)
    vkCmdPushConstants(commandBuffer, pipelineLayout, 
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                       0, sizeof(GraphicsPushConstants), &pushConstants);

    // Draw the sphere mesh (assume indexed draw; adjust index count as needed)
    // For a simple triangle demo: vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    // For sphere: vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    uint32_t indexCount = 36; // Example for simple icosphere; fetch dynamically if needed (e.g., from UE mesh data)
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);

    // Log rendering completion
    LOG_SIMULATION("Mode 9 Rasterization Render Complete - Indices drawn: {}, Light adjusted with UE energies", indexCount);
}