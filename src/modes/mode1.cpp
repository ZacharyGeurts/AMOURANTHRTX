// mode1.cpp
// Implementation of renderMode1 for AMOURANTH RTX Engine to draw a sphere with enhanced RTX ambient lighting and point light.
// Copyright Zachary Geurts 2025

#include "engine/core.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

struct PushConstants {
    alignas(16) glm::vec4 clearColor;      // 16 bytes
    alignas(16) glm::vec3 cameraPosition;  // 16 bytes (padded)
    alignas(16) glm::vec3 lightPosition;   // 16 bytes (padded) - Changed from lightDirection to support point light
    alignas(4) float lightIntensity;       // 4 bytes
    alignas(4) uint32_t samplesPerPixel;   // 4 bytes
    alignas(4) uint32_t maxDepth;          // 4 bytes
    alignas(4) uint32_t maxBounces;        // 4 bytes
    alignas(4) float russianRoulette;      // 4 bytes
    // Total size: 68 bytes (matches std140 layout in shaders)
};

void renderMode1(const UE::AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, int width, int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const UE::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, [[maybe_unused]] float deltaTime, [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    // Note: Render pass and framebuffer are ignored for ray tracing mode, as vkCmdTraceRaysKHR operates outside of render passes.
    // The output is directed to a storage image bound in the descriptor set.

    LOG_SIMULATION("Initiating Mode 1 Ray-Tracing Render - ImageIndex: {}, Resolution: {}x{}", imageIndex, width, height);

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

    LOG_SIMULATION("Computing dynamic point light parameters for dimension {}", currentDim);

    if (!dimData.empty() && currentDim >= 0 && static_cast<size_t>(currentDim) < dimData.size()) {
        const auto& data = dimData[currentDim];
        
        // Light position: Incorporate dimension position and NURBS-based oscillation
        lightPos = data.position + glm::vec3(
            static_cast<float>(std::sin(simTime * lightMovementSpeed) * data.scale * ue.getTwoD()),
            static_cast<float>(std::cos(simTime * lightMovementSpeed * 1.3L) * data.scale * ue.getThreeDInfluence()),
            static_cast<float>(std::sin(simTime * lightMovementSpeed * 0.7L) * data.scale * ue.getOneDPermeation())
        );

        // Light intensity: Modulated by observable energy, GodWaveEnergy, and influence
        lightIntensity = static_cast<float>(data.observable + data.GodWaveEnergy) * influence;
        lightIntensity = std::clamp(lightIntensity, 0.1f, 10.0f);

        // Light color: Map energies to RGB with dynamic modulation
        lightColor = glm::vec3(
            static_cast<float>(std::min(data.nurbEnergy * nurbEnergyStrength, 1.0L)) * (1.0f + std::sin(simTime * godWaveFreq) * 0.3f), // Red
            static_cast<float>(std::min(data.spinEnergy * spinInteraction, 1.0L)) * (1.0f + std::cos(simTime * godWaveFreq * 1.5L) * 0.4f), // Green
            static_cast<float>(std::min(data.fieldEnergy * emFieldStrength, 1.0L)) * (1.0f + std::sin(simTime * godWaveFreq * 2.0L) * 0.2f) // Blue
        );
        lightColor = glm::clamp(lightColor, glm::vec3(0.0f), glm::vec3(1.0f));

        LOG_SIMULATION("Dynamic Point Light Parameters - Position: ({:.3f}, {:.3f}, {:.3f}), Color: ({:.3f}, {:.3f}, {:.3f}), Intensity: {:.3f}, Movement Speed: {:.3f}",
                       lightPos.x, lightPos.y, lightPos.z, lightColor.r, lightColor.g, lightColor.b, lightIntensity, lightMovementSpeed);
    } else {
        LOG_WARNING("No dimension data available for point light adjustment in Mode 1 - Using default parameters");
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

    // Bind the ray-tracing pipeline
    LOG_SIMULATION("Binding ray-tracing pipeline");
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);

    // Bind descriptor set (contains TLAS and output image)
    LOG_SIMULATION("Binding descriptor set for TLAS and output image");
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Use local PushConstants struct for Vulkan shader compatibility
    PushConstants pushConstants{};  // Zero-initialize for safety

    // Map computed values to header fields
    // - clearColor: Use ambient contribution as base clear/fill color, tinted by lightColor
    pushConstants.clearColor = glm::vec4(ambientContribution, 1.0f);  // RGBA; alpha=1 for opaque
    pushConstants.clearColor += glm::vec4(lightColor * 0.1f, 0.0f);  // Subtle light tint
    pushConstants.clearColor = glm::clamp(pushConstants.clearColor, glm::vec4(0.0f), glm::vec4(1.0f));

    // - cameraPosition: Extract from view matrix (inverse of view[3] for position)
    glm::mat4 invView = glm::inverse(amouranth->getViewMatrix());
    pushConstants.cameraPosition = glm::vec3(invView[3]);  // Translation component

    // - lightPosition: Direct map from computation (for point light)
    pushConstants.lightPosition = lightPos;

    // - lightIntensity: Direct map from computation
    pushConstants.lightIntensity = lightIntensity;

    // - samplesPerPixel: Set to 1 for basic RT (or scale with godWaveFreq for denoising; e.g., uint32_t(1 + godWaveFreq))
    pushConstants.samplesPerPixel = static_cast<uint32_t>(std::clamp(static_cast<float>(godWaveFreq), 1.0f, 4.0f));  // 1-4 spp

    // - maxDepth: UE-based recursion (use currentDim or maxBounces equivalent; default 5 for spheres)
    pushConstants.maxDepth = static_cast<uint32_t>(std::max(ue.getCurrentDimension(), 5));  // Min 5 bounces

    // - maxBounces: Tie to simulation depth (e.g., influence * 10)
    pushConstants.maxBounces = static_cast<uint32_t>(std::clamp(static_cast<float>(influence * 10.0L), 1.0f, 16.0f));

    // - russianRoulette: Probability threshold (0.5 default; modulate with vacuum energy for termination)
    pushConstants.russianRoulette = std::clamp(0.5f + 0.2f * static_cast<float>(ue.getVacuumEnergy()), 0.1f, 0.9f);

    // Log populated constants for debugging
    LOG_SIMULATION("PushConstants - ClearColor: ({:.3f},{:.3f},{:.3f},{:.3f}), CameraPos: ({:.3f},{:.3f},{:.3f}), "
                   "LightPos: ({:.3f},{:.3f},{:.3f}), Intensity: {:.3f}, SPP: {}, MaxDepth: {}, MaxBounces: {}, RR: {:.3f}",
                   pushConstants.clearColor.r, pushConstants.clearColor.g, pushConstants.clearColor.b, pushConstants.clearColor.a,
                   pushConstants.cameraPosition.x, pushConstants.cameraPosition.y, pushConstants.cameraPosition.z,
                   pushConstants.lightPosition.x, pushConstants.lightPosition.y, pushConstants.lightPosition.z,
                   pushConstants.lightIntensity, pushConstants.samplesPerPixel, pushConstants.maxDepth,
                   pushConstants.maxBounces, pushConstants.russianRoulette);

    // Push to command buffer (stages match header expectation)
    vkCmdPushConstants(commandBuffer, pipelineLayout, 
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 
                       0, sizeof(PushConstants), &pushConstants);

    // Dispatch rays using function pointer from amouranth
    LOG_SIMULATION("Preparing to dispatch rays");
    auto vkCmdTraceRaysKHR = amouranth->getVkCmdTraceRaysKHR();
    if (!vkCmdTraceRaysKHR) {
        LOG_ERROR("vkCmdTraceRaysKHR not loaded - Aborting render");
        throw std::runtime_error("vkCmdTraceRaysKHR not loaded");
    }

    // Use SBT regions
    VkStridedDeviceAddressRegionKHR raygenSBT = amouranth->getRaygenSBT();
    VkStridedDeviceAddressRegionKHR missSBT = amouranth->getMissSBT();
    VkStridedDeviceAddressRegionKHR hitSBT = amouranth->getHitSBT();
    VkStridedDeviceAddressRegionKHR callableSBT = amouranth->getCallableSBT();

    LOG_SIMULATION("Dispatching rays - Dimensions: {}x{}x1", width, height);
    vkCmdTraceRaysKHR(commandBuffer, &raygenSBT, &missSBT, &hitSBT, &callableSBT, width, height, 1);

    // Log rendering completion
    LOG_SIMULATION("Mode 1 Ray-Tracing Render Complete - Rays dispatched: {}x{}x1, Point light adjusted with UE energies", width, height);
}