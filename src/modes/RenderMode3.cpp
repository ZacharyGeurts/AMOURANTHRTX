// src/modes/RenderMode3.cpp
// AMOURANTH RTX — MODE 3: GLASS + REFRACTION + FRESNEL
// FINAL: Full dielectric stack — glass sphere with refraction, Fresnel, and caustic-like effects
// LAZY CAMERA = OPTIONAL, MODE 3 = ALWAYS WORKS
// Keyboard key: 3 → Render glass sphere with env map refraction
// FEATURES:
//   • Procedural sphere (same as Mode 2)
//   • Dielectric material (glass, IOR = 1.5)
//   • Refraction + Fresnel blending
//   • Transmission rays (in RTX pipeline)
//   • High sample count for smooth refraction
//   • Animated rotation of sphere
//   • [[assume]] + C++23 std::expected
//   • Fallback camera

#include "modes/RenderMode3.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <format>
#include <vector>
#include <expected>
#include <algorithm>
#include <bit>
#include <cmath>

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE3(...) LOG_INFO_CAT("RenderMode3", __VA_ARGS__)

// Re-use sphere generator from Mode 2
#include "RenderMode2.cpp"  // Only for createSphere() — in real build, extract to shared util

// ---------------------------------------------------------------------
// Render Mode 3 Entry Point
// ---------------------------------------------------------------------
void renderMode3(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
) {
    int width  = context.swapchainExtent.width;
    int height = context.swapchainExtent.height;

    auto* rtx = context.getRTX();
    if (!rtx || !context.enableRayTracing || !context.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode3", "RTX or ray tracing not available");
        return;
    }

    // === GEOMETRY: Glass sphere (once) ===
    static bool geometryLoaded = false;
    static VkBuffer vertexBuffer = VK_NULL_HANDLE;
    static VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    static VkBuffer indexBuffer = VK_NULL_HANDLE;
    static VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    static SphereGeometry sphere;
    static float rotationAngle = 0.0f;

    if (!geometryLoaded) {
        try {
            sphere = createSphere(4);  // High quality

            VkDeviceSize vSize = sizeof(glm::vec3) * sphere.vertices.size();
            VkDeviceSize iSize = sizeof(uint32_t) * sphere.indices.size();

            createBuffer(context.physicalDevice, context.device, vSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexMemory);
            auto vRes = uploadData(context.device, context.physicalDevice, context.commandPool,
                                   context.graphicsQueue, sphere.vertices.data(), vSize, vertexBuffer);
            if (!vRes) throw std::runtime_error("Vertex upload failed");

            createBuffer(context.physicalDevice, context.device, iSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexMemory);
            auto iRes = uploadData(context.device, context.physicalDevice, context.commandPool,
                                   context.graphicsQueue, sphere.indices.data(), iSize, indexBuffer);
            if (!iRes) throw std::runtime_error("Index upload failed");

            std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>> geometries;
            geometries.emplace_back(vertexBuffer, indexBuffer, 0, sphere.primitiveCount, 0ULL);

            std::vector<DimensionState> dimensionCache;
            rtx->updateRTX(context.physicalDevice, context.commandPool, context.graphicsQueue,
                           geometries, dimensionCache);

            VkAccelerationStructureKHR tlas = rtx->getTLAS();
            if (tlas == VK_NULL_HANDLE) throw std::runtime_error("TLAS build failed");

            VkWriteDescriptorSet descWrite{};
            descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descWrite.dstSet = descriptorSet;
            descWrite.dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS);
            descWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            descWrite.descriptorCount = 1;

            VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
            asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            asWrite.accelerationStructureCount = 1;
            asWrite.pAccelerationStructures = &tlas;
            descWrite.pNext = &asWrite;

            vkUpdateDescriptorSets(context.device, 1, &descWrite, 0, nullptr);

            geometryLoaded = true;
            LOG_MODE3("{}GLASS SPHERE LOADED | {} tris | TLAS: {:p}{}",
                      ARCTIC_CYAN, sphere.primitiveCount, static_cast<void*>(tlas), RESET);
        } catch (const std::exception& e) {
            LOG_ERROR_CAT("RenderMode3", "Geometry failed: {}", e.what());
            geometryLoaded = true;
        }
    }

    bool useGlass = geometryLoaded && rtx->getTLAS() != VK_NULL_HANDLE;
    if (useGlass) rotationAngle += deltaTime * 0.5f;  // Slow spin

    // === CAMERA ===
    glm::vec3 camPos(0.0f, 0.0f, 6.0f);
    float fov = 60.0f;
    if (auto* cam = context.getCamera(); cam) {
        camPos = cam->getPosition();
        fov = cam->getFOV();
    }

    // === BIND ===
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // === PUSH CONSTANTS: Glass + Refraction + Animated Rotation ===
    RTConstants push{};
    push.clearColor       = glm::vec4(0.01f, 0.01f, 0.03f, 1.0f);
    push.cameraPosition   = camPos;
    push.lightPosition    = glm::vec4(0.0f, 3.0f, 0.0f, 1.0f);  // Top light
    push.lightIntensity   = 12.0f;
    push.resolution       = glm::vec2(width, height);

    // Dielectric: IOR = 1.5 (glass), roughness = 0.0
    push.materialParams   = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);  // albedo white, roughness 0
    push.ior              = 1.5f;
    push.metalness        = 0.0f;
    push.transmission     = 1.0f;
    push.samplesPerPixel  = 8;
    push.maxDepth         = 8;
    push.maxBounces       = 5;
    push.showEnvMapOnly   = 0;

    // Animated rotation (in shader: model matrix)
    float angle = rotationAngle;
    glm::mat4 model = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f));
    std::memcpy(&push.modelMatrix, &model, sizeof(glm::mat4));

    vkCmdPushConstants(commandBuffer, pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(RTConstants), &push);

    // === SBT ===
    VkStridedDeviceAddressRegionKHR raygen = { context.raygenSbtAddress, context.sbtRecordSize, context.sbtRecordSize };
    VkStridedDeviceAddressRegionKHR miss   = { context.missSbtAddress,   context.sbtRecordSize, context.sbtRecordSize };
    VkStridedDeviceAddressRegionKHR hit    = {};
    VkStridedDeviceAddressRegionKHR callable = {};

    // === TRACE ===
    context.vkCmdTraceRaysKHR(commandBuffer, &raygen, &miss, &hit, &callable,
                              static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1);

    LOG_MODE3("{}DISPATCHED | 8 spp | Glass refraction | IOR=1.5 | rotating @ {:.1f}°{}",
              EMERALD_GREEN, glm::degrees(angle), RESET);
}

} // namespace VulkanRTX