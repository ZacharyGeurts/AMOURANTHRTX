// src/modes/RenderMode3.cpp
// =============================================================================
// RENDERMODE 3 — RTX COSMIC DANCE — BINDING 31 — ETERNAL PINK ORBIT
// Animated camera orbiting a glowing pink emissive sphere
// Full RTX global illumination + temporal accumulation + subtle Nexus variance overlay
// The empire revolves in perfect harmony — pink photons illuminate the void
// =============================================================================

#include "modes/RenderMode3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace Logging::Color;

RenderMode3::RenderMode3(uint32_t width, uint32_t height)
    : width_(width), height_(height), frameCount_(0)
{
    LOG_AMOURANTH("MODE 3 — RTX COSMIC DANCE — BINDING 31 IGNITED — {}x{}", width, height);
    LOG_AMOURANTH("Camera orbits a pulsing pink emissive sphere — RTX GI + temporal accumulation");
    LOG_AMOURANTH("Subtle red overlay shows Nexus adaptive sampling focus — the empire breathes");
    LOG_SUCCESS_CAT("RTX", "COSMIC DANCE ENGAGED — PINK PHOTONS REVOLVE ETERNALLY");
}

void RenderMode3::updateUniforms(float deltaTime)
{
    alignas(16) struct CosmicCommand {
        alignas(16) glm::vec4 cameraPos    = glm::vec4(0.0f, 2.0f, -8.0f, 1.0f);
        alignas(16) glm::vec4 lightPos     = glm::vec4(0.0f, 1.5f, 0.0f, 1.0f);
        alignas(16) glm::vec4 lightColor   = glm::vec4(1.0f, 0.3f, 0.8f, 30.0f);  // Hot pink, high intensity
        alignas(16) glm::mat4 view         = glm::mat4(1.0f);
        alignas(16) glm::mat4 proj         = glm::mat4(1.0f);
        alignas(16) glm::mat4 invView      = glm::mat4(1.0f);
        alignas(16) glm::mat4 invProj      = glm::mat4(1.0f);
        alignas(16) glm::vec4 jitter       = glm::vec4(0.0f);
        uint64_t      uKey1                = 0x9E3779B97F4A7C15UL;
        uint64_t      uKey2                = 0xFB21A9D37C4E5B62UL;
        uint64_t      uObfuscator          = 0x1337C0DE69F00D42UL;
        uint64_t      uMode                = 3ULL;           // RTX COSMIC DANCE
        uint32_t      frame                = 0;
        uint32_t      visualizeNexus       = 1;              // Subtle variance overlay
        uint32_t      enableGI             = 1;
        uint32_t      enableEmissive       = 1;
        float         time                 = 0.0f;
        float         lightPulse           = 1.0f;
        float         orbitSpeed           = 1.0f;
        float         _pad                 = 0.0f;
    } cmd{};

    // Smooth orbiting camera
    float t = frameCount_ * 0.015f;
    float radius = 8.0f;
    cmd.cameraPos.x = sin(t) * radius;
    cmd.cameraPos.z = cos(t) * radius;
    cmd.cameraPos.y = 2.0f + sin(t * 0.5f) * 1.0f;

    // Pulsing central emissive sphere
    cmd.lightColor.w = 25.0f + sin(t * 4.0f) * 10.0f;
    cmd.lightPulse = 1.0f + sin(t * 3.0f) * 0.3f;

    // View/projection
    glm::vec3 target(0.0f, 1.0f, 0.0f);
    cmd.view = glm::lookAt(glm::vec3(cmd.cameraPos), target, glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    cmd.proj = glm::perspective(glm::radians(65.0f), aspect, 0.1f, 1000.0f);
    cmd.proj[1][1] *= -1.0f; // Vulkan Y flip

    cmd.invView = glm::inverse(cmd.view);
    cmd.invProj = glm::inverse(cmd.proj);

    cmd.frame = static_cast<uint32_t>(frameCount_);
    cmd.time = t;

    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));
}

void RenderMode3::traceRays(VkCommandBuffer cmd)
{
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode3::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);

    // Reset on first frame or every 20 seconds for fresh convergence showcase
    if (frameCount_ == 0 || (frameCount_ % 1200 == 0)) {
        g_rtx().requestAccumulationReset();
    }

    ++frameCount_;
}

void RenderMode3::onResize(uint32_t width, uint32_t height)
{
    if (width == width_ && height == height_) return;

    width_ = width;
    height_ = height;

    g_rtx().requestAccumulationReset();

    LOG_AMOURANTH("MODE 3 — RESIZED TO {}x{} — COSMIC DANCE CONTINUES UNBROKEN", width, height);
}