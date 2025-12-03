// =============================================================================
// src/modes/RenderMode3.cpp
// =============================================================================
// RENDERMODE 3 — FULL RTX SCENE — BINDING 31 — PINK ON FRAME 0 — ETERNAL
// 100% public API. 100% compiling. 100% pink.
// =============================================================================

#include "modes/RenderMode3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"

using namespace Logging::Color;

RenderMode3::RenderMode3(uint32_t width, uint32_t height)
    : width_(width), height_(height), frameCount_(0)
{
    LOG_INFO_CAT("RTX", "MODE 3 — FULL RTX SCENE — BINDING 31 IGNITED — {}x{}", width, height);
    LOG_SUCCESS_CAT("RTX", "TLAS ACTIVE. ACCUMULATION ENGAGED. PINK ON FRAME 0. THE EMPIRE IS ALIVE.");
}

void RenderMode3::updateUniforms(float)
{
    alignas(16) struct RTXCommand {
        alignas(16) glm::vec3 cameraPos;
        uint32_t      frame;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 invView;
        alignas(16) glm::mat4 invProj;
        alignas(16) glm::vec4 jitter;
        uint64_t      uKey1       = 0x9E37AF18C64D8A17UL;
        uint64_t      uKey2       = 0xE4F8B29D71A3C56CUL;
        uint64_t      uObfuscator = 0x9E37AF18C64D8A17UL ^ 0xE4F8B29D71A3C56CUL ^ 0x1337C0DE69F00D42UL;
        uint64_t      uMode       = 3ULL;
        float         time        = 0.0f;
        uint32_t      spp         = 0;
        uint32_t      _pad[2]     = {0};
    } cmd{};

    float t = static_cast<float>(frameCount_) * 0.016f;
    glm::vec3 pos(
        glm::sin(t * 0.3f) * 8.0f,
        2.0f + glm::sin(t * 0.7f) * 1.5f,
        glm::cos(t * 0.3f) * 8.0f
    );

    glm::mat4 view = glm::lookAt(pos, glm::vec3(0,1,0), glm::vec3(0,1,0));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), float(width_)/float(height_), 0.1f, 1000.0f);
    proj[1][1] *= -1; // Vulkan Y flip

    cmd.cameraPos = pos;
    cmd.frame     = static_cast<uint32_t>(frameCount_);
    cmd.view      = view;
    cmd.proj      = proj;
    cmd.invView   = glm::inverse(view);
    cmd.invProj   = glm::inverse(proj);
    cmd.jitter    = glm::vec4(0.0f);
    cmd.time      = t;
    cmd.spp       = static_cast<uint32_t>(frameCount_ + 1);

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

    if (frameCount_ == 0) {
        g_rtx().requestAccumulationReset();
    }

    ++frameCount_;
}

void RenderMode3::onResize(uint32_t width, uint32_t height)
{
    if (width == width_ && height == height_) return;

    width_ = width;
    height_ = height;
    frameCount_ = 0;

    g_rtx().requestAccumulationReset();

    LOG_INFO_CAT("RTX", "MODE 3 — RESIZED TO {}x{} — PINK FRAME 0 INCOMING", width, height);
}