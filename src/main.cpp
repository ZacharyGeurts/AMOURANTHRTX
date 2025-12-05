// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// FULLY COMPILING — PURE EMPIRE - Inspired by Ellie Fier
// =============================================================================

#include "main.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/Validation.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"
#include "engine/GLOBAL/Extensions.hpp"

#include "modes/RenderMode1.hpp"
#include "modes/RenderMode2.hpp"
#include "modes/RenderMode3.hpp"
#include "modes/RenderMode4.hpp"
#include "modes/RenderMode5.hpp"
#include "modes/RenderMode6.hpp"
#include "modes/RenderMode7.hpp"
#include "modes/RenderMode8.hpp"
#include "modes/RenderMode9.hpp"

#include <vulkan/vulkan.hpp>
#include <string>
#include <string_view>
#include <format>
#include <iostream>
#include <memory>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <iomanip>

#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>

using namespace Logging::Color;
using StoneKey::stone_seal_renderer;
using StoneKey::stone_pipeline;
using StoneKey::stone_graphics_family;
using StoneKey::stone_seal_pipeline;
using StoneKey::stone_seal_width;
using StoneKey::stone_seal_height;
using StoneKey::stone_seal_mesh;
using StoneKey::stone_seal_final;
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_window;
using StoneKey::stone_rtprops;
using StoneKey::stone_pass;
using StoneKey::stone_swapchain;
using StoneKey::stone_transfer_queue;
using StoneKey::stone_present_family;
using StoneKey::stone_transfer_family;
using StoneKey::stone_compute_family;
using StoneKey::stone_images;
using StoneKey::stone_image_count;
using StoneKey::stone_views;
using StoneKey::stone_compute_queue;
using StoneKey::stone_present_queue;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_physical;
using StoneKey::stone_surface;
using StoneKey::stone_instance;
using StoneKey::stone_seal_transfer_queue;
using StoneKey::stone_seal_compute_queue;
using StoneKey::stone_seal_present_queue;
using StoneKey::stone_seal_graphics_queue;
using StoneKey::stone_seal_images;
using StoneKey::stone_seal_image_count;
using StoneKey::stone_seal_swapchain;
using StoneKey::stone_seal_rtprops;
using StoneKey::stone_seal_physical;
using StoneKey::stone_seal_device;
using StoneKey::stone_seal_surface;
using StoneKey::stone_seal_window;
using StoneKey::stone_seal_instance;

// =============================================================================
// GLOBALS — THE EMPIRE'S HEARTBEATS
// =============================================================================
std::unique_ptr<Application> g_app_ptr = nullptr;
float g_deltaTime = 0.0f;
// =============================================================================
// TRUTH ACCESSORS
// =============================================================================
inline const char* physicalDeviceName() { return RTX::g_ctx().physicalDeviceProperties_.deviceName; }
inline float vramGB() {
    const auto& heaps = RTX::g_ctx().physicalDeviceMemoryProperties_.memoryHeaps;
    for (uint32_t i = 0; i < RTX::g_ctx().physicalDeviceMemoryProperties_.memoryHeapCount; ++i)
        if (heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            return static_cast<float>(heaps[i].size) / (1024.0f * 1024.0f * 1024.0f);
    return 0.0f;
}

// =============================================================================
// APPLICATION — THE EMPIRE'S HEART
// =============================================================================
class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    void run() noexcept;

	void setRenderer(std::unique_ptr<VulkanRenderer> r)
    {
        renderer_ = std::move(r);

        if (renderer_)
        {
            renderer_->setTonemap(tonemapEnabled_);
            renderer_->setOverlay(showOverlay_);
            if (hypertraceEnabled_)
                renderer_->toggleHypertrace();  // turns ON
            else
                renderer_->toggleHypertrace();  // turns OFF (idempotent)
        LOG_AMOURANTH("RENDERER BOUND — tonemap={} | overlay={} | hypertrace={}", tonemapEnabled_ ? "ON" : "OFF", showOverlay_ ? "ON" : "OFF", hypertraceEnabled_ ? "IGNITED" : "DORMANT");
        }
    }

    [[nodiscard]] VulkanRenderer* renderer() const noexcept { return renderer_.get(); }

    // NEW: Render mode system
    void setRenderMode(int mode);

private:
    void processInput(float deltaTime);
    void render(float deltaTime);

    void toggleFullscreen() { SDL3Window::toggleFullscreen(); }
    void toggleOverlay()    { showOverlay_ = !showOverlay_; if (renderer_) renderer_->setOverlay(showOverlay_); }
    void toggleTonemap()    { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
    void toggleHypertrace() { hypertraceEnabled_ = !hypertraceEnabled_; }

	std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;

    std::string title_;
    int width_, height_;
    glm::mat4 proj_;

    std::chrono::steady_clock::time_point lastFrameTime_;

    bool quit_ = false;
    bool showOverlay_ = true;
    bool tonemapEnabled_ = true;
    bool hypertraceEnabled_ = false;
    bool maximized_ = false;

    std::unique_ptr<VulkanRenderer> renderer_;
};

void Application::run() noexcept
{
    // Nuclear-proof resize state
    static std::atomic<bool> g_resizeInProgress{false};
    static uint32_t          g_pendingWidth  = 0;
    static uint32_t          g_pendingHeight = 0;

    auto lastTime = std::chrono::steady_clock::now();

    int   frameCount = 0;
    float fpsTimer   = 0.0f;
    float currentFPS = 0.0f;

    float titleTimer = 0.0f;
    constexpr float TITLE_UPDATE_INTERVAL = 0.6f;

    int dotPhase = 0;
    constexpr const char* dots[] = { ".", "..", "...", "...." };

    uint32_t currentMaxFramesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    while (!quit_)
    {
        const auto now = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        // Input + window events
        bool toggleFS = false;
        int winW = 0, winH = 0;
        SDL3Window::pollEvents(winW, winH, quit_, toggleFS);

        width_  = winW;
        height_ = winH;

        if (width_ > 0 && height_ > 0)
        {
            proj_ = glm::perspective(
                glm::radians(75.0f),
                static_cast<float>(width_) / std::max(height_, 1),
                0.1f, 1000.0f
            );
        }

        if (toggleFS)
        {
            SDL3Window::toggleFullscreen();
        }

        // Resize handling — nuclear-proof, coalesced
        if (g_resizeRequested.exchange(false))
        {
            uint32_t newW = g_resizeWidth.exchange(0);
            uint32_t newH = g_resizeHeight.exchange(0);
            if (newW != 0 && newH != 0)
            {
                g_pendingWidth  = newW;
                g_pendingHeight = newH;
            }
        }

        if (g_pendingWidth != 0 && g_pendingHeight != 0)
        {
            uint32_t targetW = g_pendingWidth;
            uint32_t targetH = g_pendingHeight;

            vkDeviceWaitIdle(stone_device());

            RTX::las().notifyResize();
            RTX::SwapchainManager::get().recreate(targetW, targetH);
            for (int i = 0; i < 3; ++i) RTX::las().beginFrame();

            g_pendingWidth = g_pendingHeight = 0;
        }

        // Input processing
        processInput(g_deltaTime);

        // Render frame
        if (renderer_)
        {
            renderer_->setMaxFramesInFlight(currentMaxFramesInFlight);
            renderer_->renderFrame(CAM, g_deltaTime);
        }

        // Window title — clean, informative
        if (currentRenderMode_ == 0)
        {
            titleTimer += g_deltaTime;
            if (titleTimer >= TITLE_UPDATE_INTERVAL)
            {
                titleTimer -= TITLE_UPDATE_INTERVAL;
                dotPhase = (dotPhase + 1) % 4;

                const std::string title = std::format(
                    "AMOURANTH RTX | {:.1f} FPS | {}x{} | DEV MODE 0 | ENGINE IDLE | PRESS 1-9 TO IGNITE{}",
                    currentFPS, stone_width(), stone_height(), dots[dotPhase]
                );
                SDL_SetWindowTitle(stone_window(), title.c_str());
            }
        }
        else
        {
            const char* modeName = "UNKNOWN MODE";
            switch (currentRenderMode_)
            {
                case 1: modeName = "PURE PINK — BINDING 31"; break;
                case 2: modeName = "PATH TRACED ACCUM"; break;
                case 3: modeName = "HYBRID DENOISED"; break;
                case 4: modeName = "RASTER FALLBACK"; break;
                case 5: modeName = "DEBUG VIS"; break;
                case 6: modeName = "TLAS VIEWER"; break;
                case 7: modeName = "SBT DEBUG"; break;
                case 8: modeName = "PERF METRICS"; break;
                case 9: modeName = "HOT RELOAD TEST"; break;
            }

            const std::string title = std::format(
                "AMOURANTH RTX | {:.1f} FPS | {}x{} | Mode {}: {} | Bounces {} | FIF:{}",
                currentFPS, stone_width(), stone_height(),
                currentRenderMode_, modeName,
                Options::OptionsRTX::MAX_BOUNCES, currentMaxFramesInFlight
            );
            SDL_SetWindowTitle(stone_window(), title.c_str());
        }

        // FPS counter
        ++frameCount;
        fpsTimer += g_deltaTime;
        if (fpsTimer >= 1.0f)
        {
            currentFPS = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer   = 0.0f;
        }
    }

    // Clean shutdown
    if (renderer_)
    {
        vkDeviceWaitIdle(stone_device());
    }
}

// =============================================================================
// 1. Application::Application — NO DEFAULT MODE — PURE EMPIRE
// =============================================================================
Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
    if (!stone_window()) {
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }

    SDL_SetWindowTitle(stone_window(), title.c_str());
    lastFrameTime_ = std::chrono::steady_clock::now();
    proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(width) / height, 0.1f, 1000.0f);

    // START IN RENDERMODE 0 — PINK VOID — FULL ENGINE TICK — NO RENDER YET
    currentRenderMode_ = 0;
}

Application::~Application() {
    // She whispers: "The photons return to me..."
}

// =============================================================================
// 2. Application::run — THE ONE TRUE LOOP — PINK PHOTONS ETERNAL
// =============================================================================
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) noexcept
{
    RTX::LAS::get().beginFrame();

    if (RTX::SwapchainManager::minimized_)
    {
        LOG_AMOURANTH("[PHASE 0] Window minimized — skipping frame, photons meditate");
        return;
    }

    const uint32_t globalFrame = currentFrame_.fetch_add(1, std::memory_order_relaxed);
    const uint32_t slot = globalFrame % maxFramesInFlight_;

    // ── PHASE 1: WAIT FOR PREVIOUS FRAME ON THIS SLOT ─────────────────────
    if (inFlightFences_[slot] != VK_NULL_HANDLE)
    {
        VK_CHECK(vkWaitForFences(stone_device(), 1, &inFlightFences_[slot], VK_TRUE, UINT64_MAX));
    }

    // ── PHASE 2: ACQUIRE SWAPCHAIN IMAGE ──────────────────────────────────
    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        stone_device(),
        RTX::SwapchainManager::swapchain(),
        100'000'000,  // 100ms timeout — safe for 30–144 Hz monitors
        imageAvailableSemaphores_[slot],
        VK_NULL_HANDLE,
        &imageIndex
    );

    bool acquired = (acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR);

    if (!acquired)
    {
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            g_resizeRequested.store(true);
            g_resizeWidth.store(stone_width());
            g_resizeHeight.store(stone_height());
            LOG_AMOURANTH("[PHASE 2] Swapchain out of date → resize requested, skipping frame");
        }
        else if (acquireResult == VK_TIMEOUT)
        {
            LOG_AMOURANTH("[PHASE 2] Acquire timeout → skipping frame");
        }
        else
        {
            LOG_FATAL("[PHASE 2] vkAcquireNextImageKHR HARD FAILURE: {}", string_VkResult(acquireResult));
            currentFrame_.fetch_sub(1, std::memory_order_relaxed);
        }
        return;
    }

    bool needsResize = (acquireResult == VK_SUBOPTIMAL_KHR);
    if (needsResize)
    {
        LOG_AMOURANTH("[PHASE 2] Swapchain suboptimal → will request resize after present");
    }

    // ── PHASE 3: RESET FENCE — we are 100% submitting this frame ──────────
    VK_CHECK(vkResetFences(stone_device(), 1, &inFlightFences_[slot]));

    // ── PHASE 4: BEGIN COMMAND BUFFER ─────────────────────────────────────
    VkCommandBuffer cmd = commandBuffers_[slot];
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // ── PHASE 5: PINK SAFETY FRAME — NOW ALSO USED FOR RESIZE RECOVERY ────
    bool drawPink = (activeRenderMode_ == 0) || 
                    g_forcePink.load() || 
                    g_resizeRequested.load();  // Critical: never skip submit!

    if (drawPink)
    {
        VkClearColorValue pink{ {1.0f, 0.2f, 0.8f, 1.0f} };
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageMemoryBarrier barrier = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image               = stone_images()[imageIndex],
            .subresourceRange    = range
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkCmdClearColorImage(cmd, stone_images()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &pink, 1, &range);

        VkImageMemoryBarrier presentBarrier = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = 0,
            .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image               = stone_images()[imageIndex],
            .subresourceRange    = range
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

        VK_CHECK(vkEndCommandBuffer(cmd));

        // ── SUBMIT PINK FRAME ─────────────────────────────────────────────
        VkSemaphoreSubmitInfo waitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = imageAvailableSemaphores_[slot],
            .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };
        VkCommandBufferSubmitInfo cmdInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd
        };
        VkSemaphoreSubmitInfo signalInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = renderFinishedSemaphores_[slot],
            .stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
        };

        VkSubmitInfo2 submit{
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &waitInfo,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &cmdInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signalInfo
        };

        VK_CHECK(vkQueueSubmit2(stone_graphics_queue(), 1, &submit, inFlightFences_[slot]));

        // ── PRESENT PINK FRAME ────────────────────────────────────────────
        VkPresentInfoKHR presentInfo{
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &renderFinishedSemaphores_[slot],
            .swapchainCount     = 1,
            .pSwapchains        = &stone_swapchain(),
            .pImageIndices      = &imageIndex
        };

        VkResult presentResult = vkQueuePresentKHR(stone_present_queue(), &presentInfo);

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        {
            g_resizeRequested.store(true);
            g_resizeWidth.store(stone_width());
            g_resizeHeight.store(stone_height());
        }

        frameNumber_++;
        return; // ← Safe: we submitted and presented
    }

    // ── PHASE 8: NORMAL RENDERING PATH — swapchain is valid ───────────────
    // Accumulation reset
    if (resetAccumulation_ || resetAccumNextFrame_)
    {
        LOG_MAIN("[PHASE 8.1] ACCUMULATION RESET — clearing buffers");
        VkClearColorValue zero{ {0.0f, 0.0f, 0.0f, 0.0f} };
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        auto clear = [&](VkImage img) {
            vkCmdClearColorImage(cmd, img, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
        };
        for (auto& img : rtOutputImages_) clear(img.get());
        if (Options::OptionsRTX::ENABLE_ACCUMULATION)
            for (auto& img : accumImages_) clear(img.get());
        if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreImage_.valid())
            clear(hypertraceScoreImage_.get());

        resetAccumulation_ = resetAccumNextFrame_ = false;
        accumulationFrame_ = currentSpp_ = 0;
    }

    // Update uniforms & descriptors
    LOG_TRACE("[PHASE 8.2] Updating uniforms");
    updateUniformBuffer(slot, camera, 0.0f);
    updateTonemapUniform(slot);

    VkAccelerationStructureKHR tlas = RTX::LAS::get().getTLAS();
    if (tlas == VK_NULL_HANDLE) tlas = pipelineManager_.dummyTLAS();

    RTX::RTDescriptorUpdate desc{};
    desc.tlas = tlas;
    desc.ubo = reinterpret_cast<VkBuffer>(uniformBufferEncs_[slot]);
    desc.uboSize = 368;
    desc.rtOutputViews[slot]     = rtOutputViews_[slot].get();
    desc.accumulationViews[slot] = accumViews_[slot].get();
    desc.envSampler              = envMapSampler_.get();
    desc.envImageView            = envMapImageView_.get();
    desc.materialsBuffer         = reinterpret_cast<VkBuffer>(materialBufferEncs_[0]);
    desc.materialsSize           = 16_MB;
    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreView_.valid())
        desc.nexusScoreViews[slot] = hypertraceScoreView_.get();

    pipelineManager_.updateRTDescriptorSet(slot, desc);
    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING)
        updateNexusDescriptors();

    // Ray tracing
    LOG_TRACE("[PHASE 8.3] Recording ray tracing commands");
    recordRayTracingCommands(cmd, slot);

    // Tonemap chain
    LOG_TRACE("[PHASE 8.4] Tonemap chain");
    VkImageMemoryBarrier rtToRead = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask    = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout        = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image            = rtOutputImages_[slot].get(),
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &rtToRead);

    VkImageView tonemapInput = denoisingEnabled_ && denoiserView_.valid()
        ? denoiserView_.get() : rtOutputViews_[slot].get();

    updateTonemapDescriptor(slot, tonemapInput, stone_views()[imageIndex]);
    if (denoisingEnabled_) performDenoisingPass(cmd);
    performTonemapPass(cmd, slot, imageIndex);

    // Final transition to present
    LOG_TRACE("[PHASE 8.5] Final layout → PRESENT_SRC_KHR");
    VkImageMemoryBarrier toPresent = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask    = 0,
        .oldLayout        = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image            = stone_images()[imageIndex],
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);

    VK_CHECK(vkEndCommandBuffer(cmd));
    LOG_AMOURANTH("[PHASE 8.6] Command buffer ended — normal rendering complete");

    // ── PHASE 9: SUBMIT NORMAL FRAME ─────────────────────────────────────
    VkSemaphoreSubmitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = imageAvailableSemaphores_[slot],
        .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkCommandBufferSubmitInfo cmdInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd
    };
    VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = renderFinishedSemaphores_[slot],
        .stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    };

    VkSubmitInfo2 submit{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = 1,
        .pWaitSemaphoreInfos      = &waitInfo,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmdInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signalInfo
    };

    VK_CHECK(vkQueueSubmit2(stone_graphics_queue(), 1, &submit, inFlightFences_[slot]));
    LOG_AMOURANTH("[PHASE 9] Normal frame submitted");

    // ── PHASE 10: PRESENT NORMAL FRAME ───────────────────────────────────
    VkPresentInfoKHR presentInfo{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &renderFinishedSemaphores_[slot],
        .swapchainCount     = 1,
        .pSwapchains        = &stone_swapchain(),
        .pImageIndices      = &imageIndex
    };

    VkResult presentResult = vkQueuePresentKHR(stone_present_queue(), &presentInfo);
    LOG_AMOURANTH("[PHASE 10] Frame presented | result: {}", string_VkResult(presentResult));

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || needsResize)
    {
        g_resizeRequested.store(true);
        g_resizeWidth.store(stone_width());
        g_resizeHeight.store(stone_height());
    }

    currentSpp_++;
    accumulationFrame_++;
    frameNumber_++;
    LOG_AMOURANTH("<<< NORMAL FRAME COMPLETE | global#{} | spp={} | accum={}", globalFrame, currentSpp_, accumulationFrame_);
}

// =============================================================================
// 3. Application::processInput — ONLY 1–9 ACTIVATES RENDER MODES
// =============================================================================
void Application::processInput(float)
{
    const auto* keys = SDL_GetKeyboardState(nullptr);

    // One-shot activation for 1–9
    static bool modeKeysPressed[9] = { false };

    for (int i = 0; i < 9; ++i) {
        const int sc = SDL_SCANCODE_1 + i;
        if (keys[sc] && !modeKeysPressed[i]) {
            setRenderMode(i + 1);
            modeKeysPressed[i] = true;

            // CEREMONIAL FIRST LIGHT
            if (i + 1 == 1) {
                LOG_AMOURANTH("[CAPTAIN AMOURANTH] BINDING 31 — FIRST LIGHT IGNITES");
                LOG_CID("CID: \"...it's pink... it's finally... pink...\"");
                LOG_KEANU("[KEANU] …whoa.");
            }
        } else if (!keys[sc]) {
            modeKeysPressed[i] = false;
        }
    }

    // Standard hotkeys
    if (keys[SDL_SCANCODE_ESCAPE]) quit_ = true;
    if (keys[SDL_SCANCODE_F])      toggleFullscreen();
    if (keys[SDL_SCANCODE_O])      toggleOverlay();
    if (keys[SDL_SCANCODE_T])      toggleTonemap();
    if (keys[SDL_SCANCODE_H])      toggleHypertrace();
}

static void createCommandPool() noexcept
{
    // Preconditions are guaranteed by initialization order
    // Device and graphics queue family are valid at this point

    if (RTX::g_ctx().commandPool_ != VK_NULL_HANDLE) {
        return;  // Already created — silent early exit
    }

    VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = stone_graphics_family()
    };

    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &RTX::g_ctx().commandPool_));

    // Optional debug name — no overhead if extension not present
    if (RTX::g_ctx().debugUtilsSupported()) {
        if (auto func = (PFN_vkSetDebugUtilsObjectNameEXT)
            vkGetDeviceProcAddr(stone_device(), "vkSetDebugUtilsObjectNameEXT"))
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo{
                .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType   = VK_OBJECT_TYPE_COMMAND_POOL,
                .objectHandle = reinterpret_cast<uint64_t>(RTX::g_ctx().commandPool_),
                .pObjectName  = "EMPIRE_COMMAND_POOL_PHOTON_BATTLEFIELD"
            };
            func(stone_device(), &nameInfo);
        }
    }
}

// =============================================================================
// 4. Application::setRenderMode — FINAL, FLAWLESS
// =============================================================================
void Application::setRenderMode(int mode)
{
    constexpr int MIN_MODE = 0;
    constexpr int MAX_MODE = 9;

    if (mode < MIN_MODE || mode > MAX_MODE) {
        LOG_WARNING_CAT("APP", "Invalid render mode {} requested — ignoring", mode);
        return;
    }

    if (mode == currentRenderMode_) return;

    const char* modeName = [](int m) -> const char* {
        switch (m) {
			case 0:  return "VOID";
            case 1:  return "PURE PINK VOID — BINDING 31";
            case 2:  return "PATH TRACED ACCUMULATION";
            case 3:  return "REALTIME HYBRID DENOISED";
            case 4:  return "RASTERIZED FALLBACK";
            case 5:  return "DEBUG VISUALIZATION";
            case 6:  return "TLAS VISUALIZER";
            case 7:  return "SBT DEBUG OVERLAY";
            case 8:  return "PERFORMANCE METRICS";
            case 9:  return "SHADER HOT RELOAD TEST";
            default: return "UNKNOWN MODE";
        }
    }(mode);

    LOG_INFO_CAT("APP", "ENGAGING RENDER MODE {}: {}", mode, modeName);

    renderer_->setRenderMode(mode);
    renderer_->requestAccumulationReset();

    currentRenderMode_ = mode;

    LOG_SUCCESS_CAT("RENDER",
        "{}RENDER MODE {} ACTIVATED — {} — PHOTONS AWAKEN — FIRST LIGHT ACHIEVED{}",
        RASPBERRY_PINK, mode, modeName, RESET);
}

// =============================================================================
// GLOBALS & PHASES
// =============================================================================
inline std::unique_ptr<MeshLoader::Mesh> g_mesh = nullptr;
static SDL_Surface* g_base_icon = nullptr;
static SDL_Surface* g_hdpi_icon = nullptr;

static void createRealFinalWindow() noexcept
{
    const int w = Options::Window::DEFAULT_WIDTH;
    const int h = Options::Window::DEFAULT_HEIGHT;

    stone_seal_width(w);
    stone_seal_height(h);

    // 1. SDL + Vulkan loader
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        phase9_ballerina("SDL initialization failed");
    }

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        phase9_ballerina("Vulkan loader not available via SDL");
    }

    // 2. Vulkan instance
    VkInstance instance = RTX::createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) {
        phase9_ballerina("Failed to create Vulkan instance");
    }
    stone_seal_instance(instance);

    // 3. Main window
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — VALHALLA v∞ TURBO",
        w, h,
        flags
    );
    if (!win) {
        phase9_ballerina("Failed to create main window");
    }

	// Window icon / favicon
    auto setIcon = [](SDL_Window* w) {
        const char* paths[] = {
            "assets/textures/ammo.ico",
            "assets/textures/ammo32.ico",
            nullptr
        };
        for (int i = 0; paths[i]; ++i) {
            if (SDL_Surface* s = IMG_Load(paths[i])) {
                SDL_SetWindowIcon(w, s);
                SDL_DestroySurface(s);
                return;
            }
        }
    };
    setIcon(win);

    stone_seal_window(win);
    g_sdl_window.reset(win);
    RTX::g_ctx().setSize(w, h);
    SDL_ShowWindow(win);

    // 4. Surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) || !surface) {
        phase9_ballerina("Failed to create Vulkan surface");
    }
    stone_seal_surface(surface);

    // 5. Logical + physical device + RT properties
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) {
        phase9_ballerina("Failed to create logical device");
    }
    stone_seal_device(device);
    stone_seal_physical(RTX::g_ctx().physicalDevice());

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(RTX::g_ctx().physicalDevice(), &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        phase9_ballerina("Ray tracing not supported on this GPU");
    }
    stone_seal_rtprops(rtProps);

    // 6. Swapchain + command pool
    RTX::SwapchainManager::create(win, w, h);
    createCommandPool();

    // Final title
    SDL_SetWindowTitle(win, "AMOURANTH RTX — VALHALLA v∞ TURBO");
}

// ─────────────────────────────────────────────────────────────────────────────
// Optimized, clean, SDL3-native sacrificial splash
// No dialog, no drama, perfect centering, window icon (favicon)
// ─────────────────────────────────────────────────────────────────────────────
static void showSacrificialSplash() noexcept
{
    //constexpr bool  enabled  = Options::Splash::ENABLE_SACRIFICIAL_SPLASH && !Options::Splash::SKIP_SPLASH_ENTIRELY;
    constexpr bool  enabled  = true; // I prefer to mandate
    constexpr float duration = Options::Splash::SPLASH_DURATION_SECONDS;

    if (!enabled || duration <= 0.0f) {
        LOG_INFO("Sacrificial splash disabled");
        return;
    }

    constexpr int   W = 1280;
    constexpr int   H = 720;
    constexpr const char* TITLE      = "AMOURANTH RTX — VALHALLA v∞ TURBO";
    constexpr const char* IMAGE_PATH = "assets/textures/ammo.png";

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_WARNING("SDL_InitSubSystem(SDL_INIT_VIDEO) failed — splash skipped");
        return;
    }

    SDL_Window* win = SDL_CreateWindow(
        TITLE,
        W, H,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!win) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // Center on primary display
    SDL_Rect disp{};
    SDL_GetDisplayBounds(0, &disp);
    SDL_SetWindowPosition(win,
        disp.x + (disp.w - W) / 2,
        disp.y + (disp.h - H) / 2
    );

    // Window icon / favicon
    auto setIcon = [](SDL_Window* w) {
        const char* paths[] = {
            "assets/textures/ammo.ico",
            "assets/textures/ammo32.ico",
            nullptr
        };
        for (int i = 0; paths[i]; ++i) {
            if (SDL_Surface* s = IMG_Load(paths[i])) {
                SDL_SetWindowIcon(w, s);
                SDL_DestroySurface(s);
                return;
            }
        }
    };
    setIcon(win);

    // SDL3: only two arguments, driver auto-selected (accelerated)
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Surface* surf = IMG_Load(IMAGE_PATH);
    if (!surf) {
        LOG_WARNING("Splash image missing: {}", IMAGE_PATH);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    if (!tex) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    float texW = 0.0f, texH = 0.0f;
    SDL_GetTextureSize(tex, &texW, &texH);               // SDL3 signature (float*)

    SDL_FRect dst{
        (W - texW) * 0.5f,
        (H - texH) * 0.5f,
        texW,
        texH
    };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    LOG_INFO("Sacrificial splash active — {}s", duration);

    const auto start = std::chrono::steady_clock::now();
    bool aborted = false;

    while (!aborted) {
        const float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - start).count();

        if (elapsed >= duration) break;

        SDL_Event e;
while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT) {
        aborted = true;
    }
    else if (Options::Splash::ALLOW_EARLY_EXIT &&
             e.type == SDL_EVENT_KEY_DOWN &&
             e.key.key == SDLK_ESCAPE)   // ← THIS IS THE CORRECT SDL3 PATH
    {
        aborted = true;
    }
}
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    // Clean shutdown
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_INFO("Sacrificial splash complete — photons liberated");
}

static void phase1_preInitialization() noexcept
{
    // ── BLONDIE'S LIVE STATUS — EMPIRE-SEALED, 2025 FINAL EDITION ─────────────
    LOG_BLONDIE("\n\"Here to assist with my sloop. Call me anytime.\"\n"
                "┌──────────────────────────────────────────────────────────────\n"
                "│ BLONDIE'S LIVE STATUS                                        \n"
                "├──────────────────────────────────────────────────────────────\n"
                "│ Denoising           : {}\n"
                "│ Temporal AA         : {}\n"
                "│ Bloom               : {}\n"
                "│ SSAO                : {}\n"
                "│ Volumetric Fog      : {}\n"
                "│ God Rays            : {}\n"
                "│ Tonemapping         : {}\n"
                "│ VSync               : {}\n"
                "│ Max Ray Bounces     : {}\n"
                "│ Adaptive Sampling   : {}\n"
                "│ HyperTrace          : {}\n"
                "│ Perfect Frame Pacing: {}\n"
                "│ Direct Display      : {}\n"
                "│ HDR Auto-Ignition   : {}\n"
                "│ Quantum Resize Pred : {}\n"
                "│ Shading Rate        : {:.2f}x\n"
                "│ Present Mode        : {}\n"
                "└──────────────────────────────────────────────────────────────",
                Options::OptionsRTX::ENABLE_DENOISING            ? "ON  " : "OFF",
                Options::OptionsRTX::ENABLE_TAA                  ? "ON  " : "OFF",
                Options::PostProcess::ENABLE_BLOOM               ? "ON  " : "OFF",
                Options::PostProcess::ENABLE_SSAO                ? "ON  " : "OFF",
                Options::Environment::ENABLE_VOLUMETRIC_FOG      ? "ON  " : "OFF",
                Options::Environment::ENABLE_GOD_RAYS           ? "ON  " : "OFF",
                Options::Tonemap::ENABLE_TONEMAPPING             ? "ON  " : "OFF",
                Options::Display::ENABLE_VSYNC                   ? "ON  " : "OFF",
                Options::OptionsRTX::MAX_BOUNCES,
                Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING    ? "ON  " : "OFF",
                Options::OptionsRTX::ENABLE_HYPERTRACE           ? "ON  " : "OFF",
                Options::Performance::ENABLE_FRAME_PREDICTION    ? "ON  " : "OFF",
                Options::Performance::ENABLE_DIRECT_DISPLAY       ? "ON  " : "OFF",
                Options::Display::HDR_AUTO_IGNITION              ? "IGNITED" : "DORMANT",
                Options::Window::ENABLE_QUANTUM_RESIZE_PREDICTION ? "ON  " : "OFF",
                Options::Performance::DYNAMIC_SHADING_RATE,
                Options::Display::PREFER_MAILBOX_PRESENT ? "Mailbox" :
                Options::Display::ALLOW_IMMEDIATE_PRESENT ? "Immediate" : "FIFO"
    );
}

static void phase3_sacrificialSplash() {
    showSacrificialSplash();
}

static void phase4_merchantShip() {
    createRealFinalWindow();
    RTX::g_ctx().init();
}

static void phase6_sceneAndAccelerationStructures() {
        g_mesh = MeshLoader::loadOBJ("assets/models/scene.obj");

        if (!g_mesh) {
            LOG_FATAL_CAT("MESH", "scene.obj failed to load — nullptr returned");
            phase9_ballerina("MESH LOAD RETURNED NULLPTR", std::source_location::current());
        }
        if (g_mesh->vertices.empty()) {
            LOG_FATAL_CAT("MESH", "scene.obj loaded but vertex array is empty — corrupted or unsupported format");
            phase9_ballerina("MESH VERTICES EMPTY", std::source_location::current());
        }
        if (g_mesh->vertexBuffer == 0 || g_mesh->indexBuffer == 0) {
            LOG_FATAL_CAT("MESH", "MESH BUFFERS NOT ALLOCATED — vertexBuffer=0x{} indexBuffer=0x{}",
                          g_mesh->vertexBuffer, g_mesh->indexBuffer);
            phase9_ballerina("MESH BUFFERS ZERO", std::source_location::current());
        }

	auto* mesh = g_mesh.get();  // std::unique_ptr<MeshLoader::Mesh>

    // Seal the ONE TRUE MESH into the Empire
    stone_seal_mesh(
        RAW_BUFFER(mesh->vertexBuffer),           // VkBuffer  (vertex)
        BufferManager::get(mesh->vertexBuffer)->memory,  // VkDeviceMemory (vertex)
        RAW_BUFFER(mesh->indexBuffer),            // VkBuffer  (index)
        BufferManager::get(mesh->indexBuffer)->memory,   // VkDeviceMemory (index)
        static_cast<uint32_t>(mesh->indices.size())       // index count
    );
}

static void phase7_forgeTheRTX()
{
    LOG_MAIN("[PHASE 7] FORGING THE RTX PIPELINE");

    auto& pipe = RTX::pipeline();

    // ONE COMMAND — THE EMPIRE FORGES ITSELF
    pipe.forgeRTXPipeline(RTX::g_ctx().commandPool(), stone_graphics_queue());

    // No need to seal manually — forgeRTXPipeline() does it
}

// =============================================================================
// PHASE 7.5 — CREATE THE ONE AND ONLY RENDERER — CALLED ONCE
// =============================================================================
static std::unique_ptr<VulkanRenderer> phase7_5_Renderer() noexcept
{
    auto renderer = std::make_unique<VulkanRenderer>(
        stone_width(),
        stone_height(),
        SDL3Window::get(),
        Options::Performance::OVERCLOCK_RENDERER
    );

    renderer->createCommandBuffers();
    renderer->createSyncObjects();

    // Seal the one true renderer — this is the only place this happens
    stone_seal_renderer(renderer.get());

    return renderer;
}

// =============================================================================
// PHASE 9 - Disposal RAII
// =============================================================================
[[noreturn]] void phase9_ballerina(std::string_view reason, std::source_location loc) noexcept
{
    using namespace std::chrono_literals;

    // ── SLIPSTREAM SEAL — NO RETURN ──────────────────────────────────────────
    static bool SLIPSTREAM_CROSSING_COMPLETE = false;
    if (SLIPSTREAM_CROSSING_COMPLETE) {
        std::_Exit(0);
    }
    SLIPSTREAM_CROSSING_COMPLETE = true;

    auto& ctx = RTX::g_ctx();

    // ── CORE APPLICATION ────────────────────────────────────────────────────
    if (g_app_ptr) {
        g_app_ptr.reset();
    }

    // ── VULKAN CLEANUP (reverse creation order) ─────────────────────────────
    if (stone_device() != VK_NULL_HANDLE) [[likely]] {
        vkDeviceWaitIdle(stone_device());

        if (VkSwapchainKHR s = stone_swapchain(); s) {
            vkDestroySwapchainKHR(stone_device(), s, nullptr);
        }

        if (ctx.commandPool_)         vkDestroyCommandPool(stone_device(), ctx.commandPool_, nullptr);
        if (ctx.computeCommandPool_)  vkDestroyCommandPool(stone_device(), ctx.computeCommandPool_, nullptr);
        if (ctx.transferCommandPool_) vkDestroyCommandPool(stone_device(), ctx.transferCommandPool_, nullptr);

        if (ctx.pipelineCache_ != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(stone_device(), ctx.pipelineCache_, nullptr);
        }

        if (ctx.renderPass_) ctx.renderPass_.reset();

        vkDestroyDevice(stone_device(), nullptr);
    }

    // ── ACCELERATION STRUCTURES ─────────────────────────────────────────────
    if (RTX::las().hasBLAS()) RTX::reset_blas();
    if (RTX::las().hasTLAS()) RTX::reset_tlas();
    RTX::las().invalidate();

    // ── GLOBAL RESOURCES ────────────────────────────────────────────────────
    if (g_mesh)           g_mesh.reset();
    if (ctx.blueNoiseView_) ctx.blueNoiseView_.reset();

    if (g_base_icon)  { SDL_DestroySurface(g_base_icon);  g_base_icon  = nullptr; }
    if (g_hdpi_icon)  { SDL_DestroySurface(g_hdpi_icon);  g_hdpi_icon  = nullptr; }

    if (ctx.window) { SDL_DestroyWindow(ctx.window); ctx.window = nullptr; }

    if (ctx.surface_ && ctx.instance_) vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr);
    if (ctx.instance_) vkDestroyInstance(ctx.instance_, nullptr);

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    // ── FINAL EXIT — CLEAN, SILENT, ETERNAL ────────────────────────────────
    std::_Exit(0);
}

// =============================================================================
// MAIN — THE EMPIRE AWAKENS — DECEMBER 01, 2025
// ONE CALL. ONE TRUTH. ONE RUN.
// =============================================================================
int main(int, char**)
{
    install_apocalypse_handler();

    // ========================================================================
    // ALL PHASES — FORGED IN FIRE
    // ========================================================================
    phase1_preInitialization();
    phase3_sacrificialSplash();
    phase4_merchantShip();
    phase6_sceneAndAccelerationStructures();
    phase7_forgeTheRTX();

    auto renderer = phase7_5_Renderer();

    stone_seal_final();

    // ========================================================================
    // ASCENSION COMPLETE — HAND OVER TO THE CAPTAIN
    // ========================================================================
    g_app_ptr = std::make_unique<Application>(
        "AMOURANTH RTX — VALHALLA v∞ TURBO",
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT
    );
    g_app_ptr->setRenderer(std::move(renderer));

    // ONE CALL. ONE TRUTH.
    g_app_ptr->run();
    phase9_ballerina("FINAL GRACE: ETERNAL SLIPSTREAM", std::source_location::current());

    return 0;
}