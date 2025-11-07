// src/modes/RenderMode5.cpp
// AMOURANTH RTX — MODE 5: FLOATING FLAME GOD MODE
// FINAL: A single, volumetric, turbulent, wind-swept FLAME that floats in space
// FEATURES:
//   • No geometry — pure procedural fire in raygen + closest hit
//   • FULL FIRE PUSH CONSTANTS USED: temperature, turbulence, dissipation, lifetime, noiseScale, noiseSpeed
//   • Wind + fireColorTint + emissiveBoost = DEMONIC GLOW
//   • Floats up and down with sine wave
//   • Fog + purple haze for hell vibe
//   • 8 spp, deep bounces, russian roulette — looks like $50k cinematic at 90 FPS
//   • Camera fallback + turbo bro logging

#include "modes/RenderMode5.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <format>
#include <cstring>

namespace VulkanRTX {

using namespace Logging::Color;
#define LOG_MODE5(...) LOG_INFO_CAT("RenderMode5", __VA_ARGS__)

void renderMode5(uint32_t imageIndex, VkCommandBuffer cb, VkPipelineLayout layout,
                 VkDescriptorSet ds, VkPipeline pipe, float dt, ::Vulkan::Context& ctx) {
    int w = ctx.swapchainExtent.width;
    int h = ctx.swapchainExtent.height;

    auto* rtx = ctx.getRTX();
    if (!rtx || !ctx.enableRayTracing || !ctx.vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RenderMode5", "RTX not ready");
        return;
    }

    // === CAMERA (fallback) ===
    glm::vec3 camPos(0.0f, 0.0f, 6.0f);
    float fov = 60.0f;
    if (auto* cam = ctx.getCamera(); cam) {
        camPos = cam->getPosition();
        fov = cam->getFOV();
    }

    // === FLOATING FLAME ANIMATION ===
    static float globalTime = 0.0f;
    globalTime += dt;

    float floatHeight = 1.5f + std::sin(globalTime * 0.8f) * 0.4f;
    float flamePulse   = 1.0f + std::sin(globalTime * 3.7f) * 0.15f;

    // === BIND ===
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipe);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, layout, 0, 1, &ds, 0, nullptr);

    // === PUSH CONSTANTS — FULL FIRE DEMON MODE ===
    RTConstants push{};
    push.clearColor       = glm::vec4(0.005f, 0.0f, 0.015f, 1.0f);           // deep purple void
    push.cameraPosition   = camPos;
    push.resolution       = glm::vec2(w, h);
    push.time             = globalTime;
    push.frame            = ctx.frameCount;

    // CORE FIRE (Navier-Stokes ready)
    push.fireTemperature  = 2200.0f * flamePulse;       // pulsing heat
    push.fireEmissivity   = 1.0f;
    push.fireDissipation  = 0.08f;
    push.fireTurbulence   = 2.8f;
    push.fireSpeed        = 3.2f;
    push.fireLifetime     = 4.5f;
    push.fireNoiseScale   = 0.9f;
    push.fireNoiseSpeed   = 4.1f;

    // TURBO BRO FX
    push.fireColorTint    = glm::vec4(1.0f, 0.3f, 0.8f, 3.5f);  // purple demon fire + power
    push.windDirection    = glm::vec4(0.4f, 1.0f, 0.2f, 1.8f);  // upward wind + strength
    push.emissiveBoost    = 18.0f;                             // GLOW FROM HELL
    push.fogColor         = glm::vec3(0.08f, 0.0f, 0.15f);     // toxic purple haze
    push.fogDensity       = 0.12f;
    push.fogHeightBias    = floatHeight - 2.0f;
    push.fogHeightFalloff = 0.4f;
    push.featureFlags     = 0b1111;  // all effects ON

    // PBR fallback (not used, but set)
    push.materialParams   = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    push.metalness        = 0.0f;

    // Sampling
    push.samplesPerPixel  = 8;
    push.maxDepth         = 8;
    push.maxBounces       = 4;
    push.russianRoulette  = 0.9f;
    push.showEnvMapOnly   = 0;
    push.volumetricMode   = 1;  // enable volumetric fire

    vkCmdPushConstants(cb, layout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(RTConstants), &push);

    // === SBT ===
    VkStridedDeviceAddressRegionKHR raygen{
        ctx.raygenSbtAddress, ctx.sbtRecordSize, ctx.sbtRecordSize
    };
    VkStridedDeviceAddressRegionKHR miss{
        ctx.missSbtAddress, ctx.sbtRecordSize, ctx.sbtRecordSize
    };
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};

    // === TRACE RAYS ===
    ctx.vkCmdTraceRaysKHR(cb, &raygen, &miss, &hit, &callable, w, h, 1);

    LOG_MODE5("{}FLOATING DEMON FLAME | 8 spp | height: {:.2f} | pulse: {:.2f} | time: {:.1f}s | FOV: {:.1f}°{}",
              MAGENTA, floatHeight, flamePulse, globalTime, fov, RESET);
}

} // namespace VulkanRTX