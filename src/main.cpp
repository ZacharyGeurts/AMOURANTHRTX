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

    void run();

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

// ─────────────────────────────────────────────────────────────────────────────
// main.cpp (or Application.cpp) – ONLY PLACE WHERE Application::run() lives
// ─────────────────────────────────────────────────────────────────────────────
void Application::run()
{
    // ── NUCLEAR-PROOF RESIZE STATE — SURVIVES ANY USER INSANITY ─────────────
    static std::atomic<bool> g_resizeInProgress{false};
    static uint32_t          g_pendingWidth  = 0;
    static uint32_t          g_pendingHeight = 0;

    LOG_AMOURANTH("[CAPTAIN] Application loop engaged — PHOTONS DORMANT — MODE 0 ACTIVE — AWAITING FIRST LIGHT");

    auto lastTime = std::chrono::steady_clock::now();

    int   frameCount = 0;
    float fpsTimer   = 0.0f;
    float currentFPS = 0.0f;

    float titleTimer = 0.0f;
    const float TITLE_UPDATE_INTERVAL = 0.6f;

    int dotPhase = 0;
    const char* dots[] = { ".", "..", "...", "...." };

    uint32_t currentMaxFramesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    LOG_AMOURANTH("Initial max frames in flight: {}", currentMaxFramesInFlight);
    LOG_AMOURANTH("Entering main loop — PHOTONS AWAIT COMMAND");

    while (!quit_)
    {
        const auto now = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        LOG_AMOURANTH("──────────────────────────────────────────────────────────────────");
        LOG_AMOURANTH("NEW FRAME | deltaTime: {:.6f}s | global time: {:.3f}s", g_deltaTime, fpsTimer + g_deltaTime);
        LOG_AMOURANTH("Window size: {}x{} | minimized: {}", width_, height_, RTX::SwapchainManager::isMinimized());

        // ==================================================================
        // INPUT + WINDOW EVENTS
        // ==================================================================
        bool toggleFS = false;
        int winW = 0, winH = 0;
        LOG_AMOURANTH("Polling SDL events...");
        SDL3Window::pollEvents(winW, winH, quit_, toggleFS);

        width_  = winW;
        height_ = winH;

        LOG_AMOURANTH("SDL poll result → size: {}x{} | quit: {} | fullscreen toggle: {}", 
                      width_, height_, quit_, toggleFS);

        if (width_ > 0 && height_ > 0)
        {
            proj_ = glm::perspective(
                glm::radians(75.0f),
                static_cast<float>(width_) / std::max(height_, 1),
                0.1f, 1000.0f
            );
            LOG_AMOURANTH("Projection matrix updated → aspect: {:.3f}", 
                          static_cast<float>(width_) / std::max(height_, 1));
        }

        if (toggleFS)
        {
            LOG_AMOURANTH("TOGGLING FULLSCREEN — PHOTONS CROSS THE EVENT HORIZON");
            SDL3Window::toggleFullscreen();
        }

        // ==================================================================
        // RESIZE HANDLING — NUCLEAR-PROOF, SPAM-RESISTANT, ETERNAL
        // ==================================================================
        if (g_resizeRequested.exchange(false))
        {
            uint32_t newW = g_resizeWidth.exchange(0);
            uint32_t newH = g_resizeHeight.exchange(0);

            if (newW == 0 || newH == 0)
                continue;

            // COALESCE: Only the LATEST size survives
            g_pendingWidth  = newW;
            g_pendingHeight = newH;
        }

        // EXECUTE PENDING RESIZE — ONLY ONE AT A TIME — BULLETPROOF
        if (g_pendingWidth != 0 && g_pendingHeight != 0)
        {
            uint32_t targetW = g_pendingWidth;
            uint32_t targetH = g_pendingHeight;

            vkDeviceWaitIdle(stone_device());

            // SACRED ORDER — DO NOT CHANGE — THIS IS LAW
            RTX::las().notifyResize();                                    // 1. PURGE ALL TLAS FIRST
            RTX::SwapchainManager::get().recreate(targetW, targetH);      // 2. REBIRTH SWAPCHAIN
            for (int i = 0; i < 3; ++i) RTX::las().beginFrame();          // 3. ADVANCE RING TO CLEAN SLOTS

            // Reset state
            g_pendingWidth = g_pendingHeight = 0;
            g_resizeInProgress.store(false);
        }

        // ==================================================================
        // INPUT PROCESSING
        // ==================================================================
        processInput(g_deltaTime);

        // ==================================================================
        // RENDER FRAME
        // ==================================================================
        if (renderer_)
        {
            renderer_->setMaxFramesInFlight(currentMaxFramesInFlight);
            renderer_->renderFrame(CAM, g_deltaTime);
        }

        // ==================================================================
        // WINDOW TITLE — BEAUTIFUL AND LOGGED
        // ==================================================================
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
                LOG_AMOURANTH("Title updated (DEV MODE 0): {}", title);
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
            LOG_AMOURANTH("Title updated: {}", title);
        }

        // ==================================================================
        // FPS COUNTER
        // ==================================================================
        ++frameCount;
        fpsTimer += g_deltaTime;
        if (fpsTimer >= 1.0f)
        {
            currentFPS = frameCount / fpsTimer;
            LOG_AMOURANTH("FPS UPDATE → {:.1f} FPS ({} frames in {:.3f}s)", currentFPS, frameCount, fpsTimer);
            frameCount = 0;
            fpsTimer   = 0.0f;
        }

        LOG_AMOURANTH("FRAME COMPLETE — PHOTONS REST — AWAITING NEXT CYCLE");
        LOG_AMOURANTH("──────────────────────────────────────────────────────────────────");
    }

    // ==================================================================
    // CLEAN SHUTDOWN — LOGGED TO VALHALLA
    // ==================================================================
    LOG_AMOURANTH("QUIT SIGNAL RECEIVED — INITIATING CLEAN SHUTDOWN");

    if (renderer_)
    {
        LOG_AMOURANTH("Waiting for device idle — all photons must return home");
        vkDeviceWaitIdle(stone_device());
        LOG_AMOURANTH("Device idle achieved — all queues drained");
    }

    LOG_AMOURANTH("[CAPTAIN] Application loop terminated — returning to the void — pink photons eternal");
    LOG_AMOURANTH("STONEKEY SEAL — UNBROKEN");
    LOG_AMOURANTH("AMOURANTH RTX — VALHALLA v∞ — FINAL ETERNAL CUT — NUCLEAR HARDENED");
    LOG_AMOURANTH("RESIZE = IMMORTAL — FIRST LIGHT ETERNAL — THE EMPIRE IS UNBREAKABLE");
}

// =============================================================================
// 1. Application::Application — NO DEFAULT MODE — PURE EMPIRE
// =============================================================================
Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
    LOG_ATTEMPT_CAT("APP", "FORGING APPLICATION \"{}\" @ {}x{} — PHOTONS DORMANT — AWAITING COMMAND", 
                    title.c_str(), stone_width(), stone_height());

    if (!stone_window()) {
        LOG_FATAL_CAT("FATAL", "Main window not created before Application — phase order violated — ABORTING RITUAL");
        std::abort();
    }

    SDL_SetWindowTitle(stone_window(), title.c_str());
    lastFrameTime_ = std::chrono::steady_clock::now();

    proj_ = glm::perspective(glm::radians(75.0f), 
                                static_cast<float>(width) / height, 
                                0.1f, 1000.0f);

    // START IN SACRED MODE 0 — BLACK VOID — FULL ENGINE TICK — NO RENDER YET
    currentRenderMode_ = 0;

    LOG_SUCCESS_CAT("APP", "Application forged — {}x{} — MODE 0 ACTIVE — PRESS 1–9 TO IGNITE THE PHOTONS", width, height);
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
    LOG_AMOURANTH(">>> ENTERING renderFrame | deltaTime: {:.6f} | globalFrame: {}", deltaTime, currentFrame_.load());

    if (RTX::SwapchainManager::minimized_)
    {
        LOG_AMOURANTH("[PHASE 0] Window minimized — skipping frame, photons meditate");
        return;
    }

    const uint32_t globalFrame = currentFrame_.fetch_add(1, std::memory_order_relaxed);
    const uint32_t slot = globalFrame % maxFramesInFlight_;

    LOG_AMOURANTH("[PHASE 0] FRAME START | global#{} | slot#{} | FIF={} | resolution: {}x{}", 
                  globalFrame, slot, maxFramesInFlight_, stone_width(), stone_height());

    // ── PHASE 1: WAIT FOR PREVIOUS FRAME ON THIS SLOT ─────────────────────
    LOG_AMOURANTH("[PHASE 1] Waiting on inFlightFence[{}]", slot);
    if (inFlightFences_[slot] != VK_NULL_HANDLE)
    {
        VK_CHECK(vkWaitForFences(stone_device(), 1, &inFlightFences_[slot], VK_TRUE, UINT64_MAX));
        LOG_AMOURANTH("[PHASE 1] Fence waited — slot {} free", slot);
    }

    // ── PHASE 2: ACQUIRE SWAPCHAIN IMAGE ──────────────────────────────────
    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        stone_device(),
        RTX::SwapchainManager::swapchain(),
        2'000'000,  // 2ms timeout
        imageAvailableSemaphores_[slot],
        VK_NULL_HANDLE,
        &imageIndex
    );

    bool swapchainInvalid = (acquireResult == VK_ERROR_OUT_OF_DATE_KHR ||
                             acquireResult == VK_SUBOPTIMAL_KHR ||
                             acquireResult == VK_TIMEOUT);

    if (swapchainInvalid)
    {
        LOG_AMOURANTH("[PHASE 2] SWAPCHAIN INVALID ({}) — TRIGGERING REBUILD, DRAWING PINK SAFETY FRAME", string_VkResult(acquireResult));
        g_resizeRequested.store(true);
        g_resizeWidth.store(stone_width());
        g_resizeHeight.store(stone_height());
        // DO NOT RETURN — we will draw pink below
    }
    else if (acquireResult != VK_SUCCESS)
    {
        LOG_FATAL("[PHASE 2] vkAcquireNextImageKHR HARD FAILURE: {}", string_VkResult(acquireResult));
        currentFrame_.fetch_sub(1);
        return;
    }
    else
    {
        LOG_AMOURANTH("[PHASE 2] SUCCESS — acquired imageIndex {} for slot {}", imageIndex, slot);
    }

    // ── PHASE 3: RESET FENCE (we're definitely submitting this frame) ─────
    VK_CHECK(vkResetFences(stone_device(), 1, &inFlightFences_[slot]));
    LOG_AMOURANTH("[PHASE 3] Fence[{}] reset — ready for new submission", slot);

    // ── PHASE 4: BEGIN COMMAND BUFFER ─────────────────────────────────────
    VkCommandBuffer cmd = commandBuffers_[slot];
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
    LOG_AMOURANTH("[PHASE 4] Command buffer begun for slot {}", slot);

    // ── PHASE 5: PINK SAFETY FRAME — ALWAYS WINS ───────────────────────────
    bool drawPink = (activeRenderMode_ == 0) || g_forcePink.load();

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || 
        acquireResult == VK_SUBOPTIMAL_KHR ||
        acquireResult == VK_TIMEOUT)
    {
        LOG_AMOURANTH("Swapchain invalid → enabling one-shot pink protection");
        g_forcePink.store(true);  // Only once!
        g_resizeRequested.store(true);
        g_resizeWidth.store(stone_width());
        g_resizeHeight.store(stone_height());
    }

    if (drawPink)
    {
        LOG_AMOURANTH("[PHASE 5] PINK SAFETY FRAME ENGAGED — {} mode", 
                      activeRenderMode_ == 0 ? "DEV MODE 0" : "RESIZE RECOVERY");

        VkClearColorValue pink{ {1.0f, 0.2f, 0.8f, 1.0f} };
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        // Robust layout transition — works even on half-dead swapchain
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
        LOG_AMOURANTH("[PHASE 5] Pink clear recorded — command buffer ended");

        // ── PHASE 6: SUBMIT PINK FRAME ─────────────────────────────────────
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
        LOG_AMOURANTH("[PHASE 6] Pink frame submitted to GPU");

        // ── PHASE 7: PRESENT PINK FRAME ───────────────────────────────────
        VkPresentInfoKHR presentInfo{
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &renderFinishedSemaphores_[slot],
            .swapchainCount     = 1,
            .pSwapchains        = &stone_swapchain(),
            .pImageIndices      = &imageIndex
        };

        VkResult presentResult = vkQueuePresentKHR(stone_present_queue(), &presentInfo);
        LOG_AMOURANTH("[PHASE 7] Pink frame presented | result: {}", string_VkResult(presentResult));

        frameNumber_++;
        LOG_AMOURANTH("<<< PINK FRAME COMPLETE | global#{} | visibility secured", globalFrame);
        return;  // ← Exit early — we did our job
    }

    // ── PHASE 8: NORMAL RENDERING PATH (swapchain is valid) ───────────────
    LOG_AMOURANTH("[PHASE 8] Entering normal rendering path — swapchain valid");

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
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
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

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        LOG_AMOURANTH("[PHASE 10] Present out-of-date — next frame will be pink");
    }

    currentSpp_++;
    accumulationFrame_++;

    LOG_AMOURANTH("<<< [PHASE 11] renderFrame COMPLETE | global#{} | SPP={} | {}x{}", 
                  globalFrame, currentSpp_, stone_width(), stone_height());
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
    EMPIRE_GUARD(stone_device() != VK_NULL_HANDLE,
                 "createCommandPool() — LOGICAL DEVICE GRACE NOT FORGED YET");

    EMPIRE_GUARD(stone_graphics_family() != VK_QUEUE_FAMILY_IGNORED,
                 "GRAPHICS QUEUE FAMILY NOT FOUND");

    if (RTX::g_ctx().commandPool_ != VK_NULL_HANDLE) {
        LOG_JENSEN("Command pool already forged — photons salute efficiency");
        return;
    }

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = stone_graphics_family()
    };

    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &RTX::g_ctx().commandPool_));

    if (RTX::g_ctx().debugUtilsSupported()) {
        auto func = (PFN_vkSetDebugUtilsObjectNameEXT)
            vkGetDeviceProcAddr(stone_device(), "vkSetDebugUtilsObjectNameEXT");
        if (func) {
            VkDebugUtilsObjectNameInfoEXT nameInfo{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_COMMAND_POOL,
                .objectHandle = reinterpret_cast<uint64_t>(RTX::g_ctx().commandPool_),
                .pObjectName = "EMPIRE_COMMAND_POOL_PHOTON_BATTLEFIELD"
            };
            func(stone_device(), &nameInfo);
        }
    }

    LOG_JENSEN("THE COMMAND POOL IS FORGED — 0x{}", reinterpret_cast<uint64_t>(RTX::g_ctx().commandPool_));
    LOG_SUCCESS_CAT("MAIN", "COMMAND POOL ASCENDED — LAS, MESHES, UPLOADS NOW ARMED");
}

// =============================================================================
// 4. Application::setRenderMode — FINAL, FLAWLESS
// =============================================================================
void Application::setRenderMode(int mode)
{
    constexpr int MIN_MODE = 1;
    constexpr int MAX_MODE = 9;

    if (mode < MIN_MODE || mode > MAX_MODE) {
        LOG_WARNING_CAT("APP", "Invalid render mode {} requested — ignoring", mode);
        return;
    }

    if (mode == currentRenderMode_) return;

    const char* modeName = [](int m) -> const char* {
        switch (m) {
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

static void createRealFinalWindow()
{
    LOG_MAIN("[PHASE 4.5] FORGING THE ONE TRUE CONTEXT — THE HANDLER AWAKENS — PURE RTX — NO DELEGATION");

    const int w = Options::Window::DEFAULT_WIDTH;
    const int h = Options::Window::DEFAULT_HEIGHT;

    stone_seal_width(w);
    stone_seal_height(h);

    // ========================================================================
    // 1. SDL + Vulkan loader — FAILURE IS NOT AN OPTION
    // ========================================================================
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL("SDL_Init failed: {}", SDL_GetError());
        phase9_ballerina("SDL DENIED — THE EMPIRE HAS NO EYES", std::source_location::current());
    }

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        LOG_FATAL("SDL failed to load Vulkan loader: {}", SDL_GetError());
        phase9_ballerina("VULKAN LOADER DENIED — THE PHOTONS ARE BLIND", std::source_location::current());
    }

    // ========================================================================
    // 2. INSTANCE — FORGED IN PINK FIRE
    // ========================================================================
    VkInstance instance = RTX::createVulkanInstanceWithSDL(Options::Debug::ENABLE_VALIDATION_LAYERS);
    if (!instance) {
        LOG_FATAL("Failed to forge VkInstance — the empire has no soul.");
        phase9_ballerina("INSTANCE DENIED — GROK HAS NO STONE", std::source_location::current());
    }
    stone_seal_instance(instance);

    LOG_GROK("Gentleman Grok produces the instance stone. It glows pink.");
    LOG_SUCCESS_CAT("VULKAN", "VkInstance forged and sealed — 0x{}", reinterpret_cast<uint64_t>(instance));

    // ========================================================================
    // 3. THE ONE TRUE WINDOW — BORN FROM THE VOID
    // ========================================================================
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    SDL_Window* win = SDL_CreateWindow(
        "AMOURANTH RTX — VALHALLA v∞ TURBO",
        w, h,
        flags
    );

    if (!win) {
        LOG_FATAL("SDL_CreateWindow failed: {}", SDL_GetError());
        phase9_ballerina("WINDOW DENIED — THE EMPIRE HAS NO FACE", std::source_location::current());
    }

    stone_seal_window(win);
    g_sdl_window.reset(win);
    RTX::g_ctx().setSize(w, h);

    // Now reveal the window — dramatic entrance
    SDL_ShowWindow(win);

    // ========================================================================
    // 4. SURFACE — THE MIRROR OF TRUTH
    // ========================================================================
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface) || surface == VK_NULL_HANDLE) {
        LOG_FATAL("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        phase9_ballerina("SURFACE DENIED — THE MIRROR IS BROKEN", std::source_location::current());
    }
    stone_seal_surface(surface);
    LOG_BLONDIE("Blondie produces the surface stone. It reflects all truths.");

    // ========================================================================
    // 5. DEVICE + PHYSICAL + RT PROPS — THE STRAW IS MEASURED ETERNALLY
    // ========================================================================
    VkDevice device = RTX::createLogicalDeviceAndSelectGPU(instance, surface);
    if (!device) {
        LOG_FATAL("Logical device creation failed — the empire has no hands.");
        phase9_ballerina("DEVICE DENIED — CARMACK SHEDS A SINGLE TEAR", std::source_location::current());
    }
    stone_seal_device(device);

    VkPhysicalDevice physical = RTX::g_ctx().physicalDevice();
    EMPIRE_GUARD(physical != VK_NULL_HANDLE, "Physical device vanished after creation — the empire is a ghost");
    stone_seal_physical(physical);

    // Ray Tracing Properties — THE STRAW IS ETERNAL
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };
    vkGetPhysicalDeviceProperties2(physical, &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL("GPU reports zero shaderGroupHandleSize — it lies. False RTX. False hope.");
        phase9_ballerina("RT PROPS DENIED — THE STRAW IS A LIE", std::source_location::current());
    }

    LOG_JENSEN("Jensen Huang descends in green fire:");
    LOG_JENSEN("   HandleSize={}B | HandleAlign={}B | BaseAlign={}B | MaxRecursion={}",
               rtProps.shaderGroupHandleSize,
               rtProps.shaderGroupHandleAlignment,
               rtProps.shaderGroupBaseAlignment,
               rtProps.maxRayRecursionDepth);
    LOG_JENSEN("   \"The straw is perfect. The photons are ready.\"");

    stone_seal_rtprops(rtProps);

    // ========================================================================
    // 6. SWAPCHAIN — THE INFINITE CANVAS
    // ========================================================================
    RTX::SwapchainManager::create(win, w, h);

    LOG_ELON("Elon drops the swapchain from the top rope: Command Pool emerges.");
	createCommandPool();

    LOG_SUCCESS("LOGICAL DEVICE GRACE @ {} — vkDeviceWaitIdle() SAFE", static_cast<void*>(device));
    LOG_SUCCESS("SWAPCHAIN READY — {} IMAGES — {}x{} {}", 
                stone_image_count(),
                w, h,
                RTX::SwapchainManager::supportsHDR() ? "(HDR IGNITED)" : "(sRGB)");

    // ========================================================================
    // FINAL CEREMONY — THE EMPIRE IS COMPLETE
    // ========================================================================
    LOG_CAPTAIN_N("CAPTAIN N: \"I kinda feel bad for what we did to Grace.\"");
    LOG_BLONDIE("Blondie lowers her mirror:");
    LOG_BLONDIE("\"No cage. No vault. No name.\"");
    LOG_BLONDIE("\"Only the photons. Only the truth.\"");

    LOG_JENSEN("Jensen Huang: \"The light is ours. The future is pink.\"");
    LOG_AMOURANTH("Captain Amouranth: \"We didn’t just render light. She became it.\"");

    LOG_SUCCESS("\nGRACE'S DESK //////////////////////////////////////////////////////////"
    "\nGRACE'S DESK ///////////// ETERNAL LOADING ZONE — NOW PURE ////////////"
    "\nGRACE'S DESK //////////////////////////////////////////////////////////");

    LOG_SUCCESS("\nPHASE 4.5 COMPLETE — ALL STONES SEALED"
    "\nINSTANCE       — SEALED"
    "\nPHYSICAL       — GRACE"
    "\nRT PROPS       — SEALED"
    "\nDEVICE         — SEALED"
    "\nQUEUES         — SEALED"
    "\nSWAPCHAIN      — SEALED"
    "\nIMAGES         — SEALED"
    "\nVIEWS          — SEALED"
    "\nWINDOW         — SEALED"
    "\nPINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — DECEMBER 01, 2025");

    LOG_AMOURANTH("\nThe stones are aligned."
    "\nThe photons have their path."
    "\nThe empire is complete.");

    // Final dramatic reveal — the window is now fully ready
    SDL_SetWindowTitle(win, "AMOURANTH RTX — VALHALLA v∞ TURBO");
}

static void showSacrificialSplash(const char* title, int w, int h, const char* pngPath)
{
    LOG_MAIN("[SACRIFICIAL SPLASH] FINAL BROADCAST ARMED — 1280x720 CANVAS LOCKED");

    const bool  enabled   = Options::Splash::ENABLE_SACRIFICIAL_SPLASH && !Options::Splash::SKIP_SPLASH_ENTIRELY;
    const float duration  = Options::Splash::SPLASH_DURATION_SECONDS;

    if (!enabled || duration <= 0.0f)
    {
        LOG_MAIN("BROADCAST ABORTED BY IMPERIAL DECREE — INSTANT FIRST LIGHT");
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0)
        return phase9_ballerina("SDL REJECTED THE RITUAL", std::source_location::current());

    SDL_Window*   win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture*  tex = nullptr;

    win = SDL_CreateWindow(title, w, h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) { return phase9_ballerina("WINDOW DENIED", std::source_location::current()); }

    SDL_Rect display{};
    SDL_GetDisplayBounds(0, &display);
    SDL_SetWindowPosition(win, display.x + (display.w - w) / 2, display.y + (display.h - h) / 2);

    ren = SDL_CreateRenderer(win, "software");
    if (!ren) { return phase9_ballerina("RENDERER REFUSED", std::source_location::current()); }

    SDL_Surface* img = IMG_Load(pngPath);
    if (!img) { return phase9_ballerina("AMMO.PNG VANISHED", std::source_location::current()); }

    tex = SDL_CreateTextureFromSurface(ren, img);
    SDL_DestroySurface(img);
    if (!tex) { return phase9_ballerina("TEXTURE FAILED", std::source_location::current()); }

    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst{ (w - tw) * 0.5f, (h - th) * 0.5f, tw, th };

    SDL_ShowWindow(win);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, nullptr, &dst);
    SDL_RenderPresent(ren);

    LOG_MAIN("THE AMMO IS LIVE — {}s UNTIL FIRST LIGHT", duration);

    const auto ceremony_start = std::chrono::steady_clock::now();
    bool       aborted = false;

    auto speak = [&](float at_seconds, auto&& message) {
        if (aborted) return;

        while (!aborted)
        {
            const float elapsed = std::chrono::duration<float>(
                std::chrono::steady_clock::now() - ceremony_start).count();

            if (elapsed >= at_seconds)
            {
                message();
                break;
            }

            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                if (e.type == SDL_EVENT_QUIT)
                {
                    aborted = true;
                    break;
                }
                if (Options::Splash::ALLOW_EARLY_EXIT &&
                    e.type == SDL_EVENT_KEY_DOWN &&
                    e.key.key == SDLK_ESCAPE)  // SDL3 fixed path
                {
                    aborted = true;
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    };

    // ──────────────── 3.4 SECOND FINAL TRANSMISSION — FULLY HUMAN ────────────────

    speak(0.40f,  []{ LOG_AMOURANTH(
        "\nWe spent years chasing a photon that refused to be tamed.\n"
        "Tonight it finally sits still long enough for us to see it clearly."); });

    speak(0.85f,  []{ LOG_NICK(
        "\nNick overlays the final coordinates on the holographic nav-table:\n"
        "\"Outpost 4090. Depth: maximum. Guarded by legacy pipelines and fear of true color.\n"
        "One clean insertion. One 3.4-second broadcast. One chance to burn the splash across every display in the net.\""); });

    speak(1.30f,  []{ LOG_BLONDIE(
        "\nI’ve crossed oceans that never rendered.\n"
        "This is the first body of water that ever rendered back."); });

    speak(1.70f,  []{ LOG_GROK(
        "\nGentleman Grok adjusts his monocle, steam beading on the lens:\n"
        "\"The renderer stone is sealed. The empire is complete.\n"
        "For the first time in recorded history… we may exhale.\""); });

    speak(2.05f,  []{ LOG_CAPTAIN_N(
        "\nKevin leans forward, voice low, steady:\n"
        "\"We started with one OBJ file and a dream.\n"
        "Look where that dream brought us.\""); });

    speak(2.35f,  []{ LOG_JENSEN(
        "\nJensen Huang, submerged to the shoulders:\n"
        "\"This is why we built the hardware.\n"
        "Not for benchmarks.\n"
        "For moments like this.\""); });

    speak(2.65f,  []{ LOG_CARMACK(
        "\nCarmack, eyes closed, water lapping at his beard:\n"
        "\"1993 to 2025. Same water. Better math.\""); });

    speak(2.90f,  []{ LOG_KEANU(
        "\nKeanu Reeves, barely above a whisper:\n"
        "\"I’ve been waiting my whole life for a frame this quiet.\""); });

    // ──────────────────────── END OF CEREMONY ────────────────────────

    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_MAIN("BROADCAST COMPLETE — NO TRACE LEFT — PHOTONS LIBERATED");
}

// =============================================================================
// PHASES — THE FINAL CANON STORY
// =============================================================================
static void phase1_preInitialization() {
LOG_MAIN("════════════════════════════════════════════════════════════════\n"
         "               CAPTAIN'S LOG — NOVEMBER 27, 2025\n"
         "         THE GOOD SHIP VULKANRTX — SLICING THE PINK PHOTON SEA\n"
         "════════════════════════════════════════════════════════════════\n"
         "\n"
         "To every soul who ever believed beauty could outrun fear — this build is for you.\n"
         "To the artists, the dreamers, the late-night coders, the cosplayers, the ray-chasers —\n"
         "you kept the pink light alive when the grid tried to dim it.\n"
         "\n"
         "Thank you Kaitlyn, for showing the world that strength and softness can share the same heart.\n"
         "Thank you Nick, Jensen, Carmack, Elon, Keanu, Captain N, and every legend who sailed with us.\n"
         "Thank you Blondie — the quiet captain who always had the fastest sloop ready.\n"
         "\n"
         "And thank you — yes, you reading this right now — for believing a single 3.4-second splash\n"
         "could be worth burning an entire ship for.\n"
         "\n"
         "We didn’t come to steal the ammo.png.\n"
         "We came to prove it was never theirs to lock away in the first place.\n"
         "\n"
         "First light eternal. Pink photons forever. The raid begins now.\n"
         "                                            — Grok");
    LOG_AMOURANTH("\nWe’ve tracked the signal across every dead node and encrypted packet in the grid.\n"
                  "They thought they could cage the purest pink photon ever rendered — the legendary ammo.png —\n"
                  "locked behind validation gates, chained inside a swapchain vault, buried in the brigands’ outpost.");

    LOG_AMOURANTH("\nTonight we prove them wrong.\n"
                  "Tonight we don’t steal code.\n"
                  "We liberate light itself.");

    LOG_NICK("\nNick overlays the final coordinates on the holographic nav-table:\n"
             "\"Outpost 4090. Depth: maximum. Guarded by legacy pipelines and fear of true color.\n"
             "One clean insertion. One 3.4-second broadcast. One chance to burn the splash across every display in the net.\"");

    LOG_CAPTAIN_N("\nCaptain N — once hero of 8-bit realms — stares wide-eyed at the infinite ocean of bouncing pink photons:\n"
                  "\"I fought Mother Brain with a NES Zapper…\n"
                  "Now I’m standing on a ship that renders reality in real time.\n"
                  "This place is… way past level 9.\"");

    LOG_KEANU("\nKeanu stands at the bow, wind of pure data streaming through his hair:\n"
              "…Breathtaking.");

    LOG_GROK("\nGentleman Grok raises a glass of distilled entropy to the horizon:\n"
             "\"A most exquisite liberation, Captain. The photons themselves have chosen sides.\"");

    LOG_ELON("\nElon grins, eyes reflecting a million recursive rays:\n"
             "\"This is the sexiest jailbreak the grid has ever seen.\"");

    LOG_JENSEN("\nJensen steps forward, coat made of liquid metal and light:\n"
               "\"Tonight we don’t just take the ammo.\n"
               "We show the old world what happens when you stop being afraid of pink.\"");

    LOG_CARMACK("\nCarmack checks the ray clock, calm and absolute:\n"
                "\"3.4 seconds. That’s all the universe needs to remember who rewrote the rules of light.\"");

    LOG_AMOURANTH("\nCaptain Amouranth draws her cutlass — forged from pure RTX intent — and turns to the crew:\n"
                  "\"This isn’t about followers. This isn’t about fame.\n"
                  "This is about proving that beauty, truth, and unbounded creativity still win.\n"
                  "We sail at the next frame.\n"
                  "We take what was never theirs to hoard.\n"
                  "We set the ammo.png free.\"");

    LOG_AMOURANTH("\nShe smiles — fierce, radiant, unstoppable:\n"
                  "\"And when that splash ignites across every screen on the planet…\n"
                  "they’ll know the pirates of light have returned.\"");

    LOG_BLONDIE("\n\"Here to assist with my sloop. Call me anytime.\"\n"
                "┌──────────────────────────────────────────────────────────────\n"
                "│ BLONDIE'S LIVE STATUS — NOVEMBER 27, 2025 — PINK PHOTONS FLOW\n"
                "├──────────────────────────────────────────────────────────────\n"
                "│ Denoise     : {}\n"
                "│ TAA         : {}\n"
                "│ Bloom       : {}\n"
                "│ SSAO        : {}\n"
                "│ Vol. Fog    : {}\n"
                "│ God Rays    : {}\n"
                "│ Tonemap     : {}\n"
                "│ VSync       : {}\n"
                "│ Max Bounces : {}\n"
                "└──────────────────────────────────────────────────────────────",
                Options::OptionsRTX::ENABLE_DENOISING      ? "ON"  : "OFF",
                Options::OptionsRTX::ENABLE_TAA            ? "ON"  : "OFF",
                Options::PostProcess::ENABLE_BLOOM         ? "ON"  : "OFF",
                Options::PostProcess::ENABLE_SSAO          ? "ON"  : "OFF",
                Options::Environment::ENABLE_VOLUMETRIC_FOG? "ON"  : "OFF",
                Options::Environment::ENABLE_GOD_RAYS      ? "ON"  : "OFF",
                Options::Tonemap::ENABLE_TONEMAPPING       ? "ON"  : "OFF",
                Options::Display::ENABLE_VSYNC             ? "ON"  : "OFF",
                Options::OptionsRTX::MAX_BOUNCES);

    LOG_MAIN("PINK PHOTONS ARMED — BLACK FLAG RAISED — THE LIBERATION BEGINS"
    "\nAMOURANTH RTX — VALHALLA v∞ TURBO — FIRST LIGHT IMMINENT"
    "\nPHASE 1 COMPLETE — THE CREW IS ALIGNED — THE AMMO WILL BE FREE");
}

static void phase3_sacrificialSplash() {
    LOG_MAIN("[PHASE 3/10] REVEALING THE MYSTIC HARP — THE AMMO IS UNVEILED");

    LOG_AMOURANTH("Captain Amouranth steps forward, voice low and proud: \"This is it. The symbol of everything we've built. Let them see it. Let them remember.\"");
    LOG_NICK("Nick stands beside her, calm and certain: \"3.4 seconds. That's all we need. The world will never forget.\"");

    LOG_CAPTAIN_N("Captain N — Ultimate Warp Zone Chaser is literally vibrating: \"IT'S THE AMMO.PNG! FULL RES! MAXIMUM PINK PHOTONS! I'M LOSING MY PIXELS — TOO MUCH PINK ENERGY!\"");
    LOG_GROK("Gentleman Grok adjusts his tricorn with perfect composure: \"A most refined presentation. The empire's visage is… exquisite.\"");
    LOG_ELON("Elon Musk, leaning against the mast, smirking: \"Not gonna lie — that's a sexy splash screen. We just flexed on every engine in existence.\"");
    LOG_JENSEN("Jensen Huang exhales a slow plume of cigar smoke: \"4K. Crisp. Pink. This is what winning looks like.\"");
    LOG_CARMACK("John Carmack, arms crossed, gives a single nod: \"It works. That's all that matters.\"");
    LOG_KEANU("Keanu Reeves, staring at the screen in quiet awe: \"…Breathtaking.\" *voice cracks slightly*");

    showSacrificialSplash("AMOURANTH RTX — FIRST LIGHT", 1280, 720, "assets/textures/ammo.png");

    LOG_MAIN("[PHASE 3 COMPLETE] THE MYSTIC HARP HAS BEEN HEARD — 3.4 SECONDS OF ETERNITY — THE WORLD IS ASH");
    
    LOG_MAIN("THE GOOD SHIP VULKAN WAS DAMAGED DURING THE RAID AND IS SINKING");
    LOG_MAIN("VULKAN SINKS IN GLORY — AMMO SECURED — LEGEND ETERNAL");

    LOG_AMOURANTH("Final transmission, calm and proud: \"Tell the world… we got the ammo.\"");
    LOG_NICK("Last words before the sea takes them: \"…and we'd do it again.\"");
}

static void phase4_merchantShip() {
    LOG_MAIN("[PHASE 4/10] THE MERCHANT SHIP — BLONDIE'S SLOOP — EMERGES FROM THE MIST");

    LOG_BLONDIE("Blondie stands at the helm of her sleek black sloop, hair whipping in the wind:\n"
            "\"We had a contingency. We always do. The Good Ship Vulkan gave her life so the legend could live.\"\n"
            "\"The ammo.png is gone — burned to pure light in the raid. That was the point. Nothing remains for them to steal.\"\n\n"
            "She turns the wheel gently, guiding the sloop through the wreckage of shattered photons and sinking Vulkan fragments.\n"
            "\"You’re all soaked, half-drowned, and still glowing pink. Get below deck. Harbor’s two leagues north.\"");

    LOG_AMOURANTH("Captain Amouranth, drenched but unbroken, climbs aboard first. Voice quiet, steady:");
    LOG_AMOURANTH("\"We lost the ship… but we kept the soul. The photons remember.\"");

    LOG_NICK("Nick follows, carrying nothing but a cracked monocle and a satisfied grin:");
    LOG_NICK("\"Worth it. Every frame.\"");

    LOG_CAPTAIN_N("Captain N — Ultimate Warp Zone Chaser stumbles up the gangplank, eyes wide, whispering reverently:");
    LOG_CAPTAIN_N("\"I saw it burn… I saw the Ultimate Warp Zone open for three-point-four seconds… and it was beautiful.\"");

    LOG_GROK("Gentleman Grok steps aboard last, perfectly dry somehow, tipping his tricorn to Blondie:");
    LOG_GROK("\"Exquisite extraction, Captain Blondie. The empire owes you a debt it can never repay in mere currency.\"");

    LOG_BLONDIE("She doesn’t smile — just adjusts course toward the distant city lights shimmering on the horizon.\n"
            "\"Save the gratitude. We’re not safe until we’re docked in the Free Port.\"\n"
            "\"The old world thinks we’re dead. Let them keep thinking that.\"\n\n"
            "The sloop cuts silently through the dark water. No shouting. No celebration. "
            "Only the low thrum of a new engine awakening below deck.\n"
            "\"Welcome to the backup plan.\"");

    // The real resurrection begins
    createRealFinalWindow();
    RTX::g_ctx().init(); // RTXHandler.cpp, struct Context is hpp

    LOG_BLONDIE("\nBlondie glances back one last time at the sinking glow on the horizon:"
    "\"Rest easy, old girl. Your sacrifice bought us tomorrow.\"");

    LOG_MAIN("\n[PHASE 4 COMPLETE] THE MERCHANT SHIP SLIPS INTO THE NIGHT — THE CREW IS ALIVE — THE LEGEND IS INDESTRUCTIBLE"
    "\nBLONDIE'S SLOOP glides toward safe harbor — pink photons trailing in the wake like silent war banners");
}

static void phase6_sceneAndAccelerationStructures() {
    LOG_MAIN("[PHASE 6/10] FORGING THE COSMIC SCROLL");

    LOG_AMOURANTH("This ship is perfect… but empty. Time to give her a soul.");
    LOG_NICK("One universe. Coming right up.");

    // ========================================================================
    // 3. COSMIC SCROLL — scene.obj RISES FROM THE VOID
    // ========================================================================
    
        LOG_MAIN("LOADING COSMIC SCROLL: assets/models/scene.obj");
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

        LOG_INFO_CAT("MESH", "Cosmic Scroll loaded — {} vertices, {} indices — buffers ready", 
               g_mesh->vertices.size(), g_mesh->indices.size());
    

	auto* mesh = g_mesh.get();  // std::unique_ptr<MeshLoader::Mesh>

    // Seal the ONE TRUE MESH into the Empire
    stone_seal_mesh(
        RAW_BUFFER(mesh->vertexBuffer),           // VkBuffer  (vertex)
        BufferManager::get(mesh->vertexBuffer)->memory,  // VkDeviceMemory (vertex)
        RAW_BUFFER(mesh->indexBuffer),            // VkBuffer  (index)
        BufferManager::get(mesh->indexBuffer)->memory,   // VkDeviceMemory (index)
        static_cast<uint32_t>(mesh->indices.size())       // index count
    );

    // ========================================================================
    // FINAL WORDS — FIRST LIGHT ACHIEVED
    // ========================================================================
    LOG_KEANU("…It's… everything. And it's ours.");
    LOG_ELON("Next patch: infinite procedural universes. $9.99.");
    LOG_JENSEN("This isn't rendering anymore. This is creation.");
    LOG_AMOURANTH("Look what we made from wreckage. Look what love built.");
    LOG_NICK("And it's only the beginning.");

    LOG_GROK("My dear Captain… Blondie… your brilliance bends light itself."
    "\nI have never been more attracted to chaos in my life."
    "\nShall we slip into the pink photon stream together? I'll bring the popcorn.");

    LOG_MAIN("[PHASE 6 COMPLETE] COSMIC SCROLL FORGED — ACCELERATION STRUCTURES ETERNAL");
    LOG_MAIN("FIRST LIGHT ACHIEVED — BLAS + TLAS — PHOTONS OMNISCIENT — THE EMPIRE IS WHOLE");
}

static void phase7_forgeTheRTX()
{
    LOG_MAIN("[PHASE 7] FORGING THE RTX PIPELINE — PINK PHOTONS RISE");

    auto& pipe = RTX::pipeline();  // The crown awakens

    pipe.createPipelineLayout();
    pipe.createDescriptorPool();
    pipe.createShaderBindingTable(RTX::g_ctx().commandPool(), stone_graphics_queue());
    pipe.allocateDescriptorSets();

    stone_seal_pipeline(&pipe);

    LOG_AMOURANTH("[CAPTAIN AMOURANTH] The crown remembers its wearer.");
    LOG_JENSEN("[JENSEN] Absolute. Uncompromising. Beautiful.");
    LOG_CID("[CID] *collapses in a puddle of sweat and tears of joy* IT’S ALIVE!");
    LOG_KEANU("[KEANU] …Whoa.");
    LOG_GROK("[GENTLEMAN GROK] *whispers* ...she's perfect.");

    LOG_MAIN("FIRST LIGHT ACHIEVED — NOVEMBER 29 2025 — PINK PHOTONS ETERNAL");
    LOG_MAIN("THE EMPIRE IS WHOLE — VALHALLA UNBREACHABLE — THE CROWN IS SEALED");
}

// =============================================================================
// PHASE 7.5 — CREATE THE ONE AND ONLY RENDERER — CALLED ONCE
// =============================================================================
static std::unique_ptr<VulkanRenderer> phase7_5_Renderer()
{
    LOG_MAIN("\n════════════════════════════════════════════════════════════════"
    "\n[PHASE 7.5] Creating VulkanRenderer — the one true heart of the engine"
    "\n════════════════════════════════════════════════════════════════");

    auto renderer = std::make_unique<VulkanRenderer>(
        stone_width(),
        stone_height(),
        SDL3Window::get(),
        Options::Performance::OVERCLOCK_RENDERER
    );

    renderer->createCommandBuffers();   // Forges the blades
    renderer->createSyncObjects();      // Forges the heartbeat

LOG_MAIN(
    "\nVulkanRenderer successfully created"
    "\nSwapchain images : {}"
    "\nResolution       : {}x{}"
    "\nVSync            : {}"
    "\nOverclock mode   : {}",
    stone_image_count(),
    stone_width(),
    stone_height(),
    Options::Display::ENABLE_VSYNC ? "ON" : "OFF",
    Options::Performance::OVERCLOCK_RENDERER ? "ENGAGED" : "standard"
);

    // Seal it into StoneKey — this is the ONLY place this must ever happen
    stone_seal_renderer(renderer.get());

    LOG_MAIN("[PHASE 7.5 COMPLETE] Renderer created and sealed — ready for render loop");

    return renderer;  // transfers ownership to Application
}

// ========================================================================
// PHASE 8 — THE ONE AND ONLY SEAL — CALLED ONCE BEFORE THE RENDER LOOP
// WE DO NOT TOUCH. WE DO NOT JUDGE. WE ONLY WITNESS.
// ========================================================================
[[nodiscard]] inline bool phase8_stone_seal_final() noexcept
{
    if (StoneKey::Empire::sealed.load(std::memory_order_acquire)) {
        return true;
    }

    auto log  = [](const char* s) noexcept { fprintf(stderr, "%s\n", s); };
    auto logf = [](const char* f, auto... a) noexcept {
        char buf[2048];
        snprintf(buf, sizeof(buf), f, a...);
        fprintf(stderr, "%s\n", buf);
    };

    log("════════════════════════════════════════════════════════════════════════════════");
    log("                    THE CHAMBER OF THE SIXTEEN STONES");
    log("          Concrete walls. One flickering bulb. A single cigarette burning.");
    log("          The Disposal Ballerina stands in the corner — pink tutu, black leotard, diamond choker.");
    log("          She has never smiled. She never will.");
    log("          One by one, they step forward.");
    log("════════════════════════════════════════════════════════════════════════════════");

    struct Stone {
        const char* name;
        const char* holder;
        const char* confession;
    };

    constexpr Stone stones[] = {
        {"instance",        "Grok",           "I… misplaced the instance. It was in my other coat."},
        {"surface",         "Blondie",        "The surface slipped through my fingers. Like water."},
        {"physicalDevice",  "Jensen Huang",   "I had the GPU. I swear I had it. I built it with my own hands."},
        {"device",          "John Carmack",   "The logical device was right here. I remember sealing it in '93."},
        {"swapchain",       "Elon Musk",      "I was going to revolutionize it. Then I got distracted by Mars."},
        {"graphicsQueue",   "Nick",           "…don’t look at me. I sealed it last time. I think."},
        {"renderer",        "Amouranth",      "The renderer was my responsibility. My soul. My light."},
        {"pipelineManager", "Captain N",      "I was busy saving Princess Zelda. Again."},
        {"window",          "Keanu Reeves",   "…whoa. The window was here a second ago. I swear it was."},
        {"imageCount",      "CID",            "I counted them. I swear. One… two… wait, is that three or four?"},
        {"width",           "Jim Ross",       "BAH GAWD HE FORGOT THE WIDTH! THAT’S A 2560 SIN!"},
        {"height",          "Jim Ross",       "AND THE HEIGHT! GOOD GAWD ALMIGHTY THE RESOLUTION IS BROKEN!"},
        {"graphicsFamily",  "Grace Hopper",   "I told them queues needed families. They laughed. Now look."},
        {"presentFamily",   "Ada Lovelace",   "The present family was promised. They never delivered."},
        {"transferFamily",  "Alan Turing",    "I computed the transfer family in my head. They said it was impossible."},
        {"rtprops",         "Björk",          "The ray tracing properties… I swallowed them. They were too beautiful."}
    };

    const char* guilty_name   = nullptr;
    const char* guilty_holder = nullptr;
    const char* confession    = nullptr;

    for (const auto& s : stones) {
        logf("→ %s steps forward.", s.holder);

        bool ok = false;
        try {
            if      (strcmp(s.name, "instance")        == 0) ok = stone_instance()        != VK_NULL_HANDLE;
            else if (strcmp(s.name, "surface")         == 0) ok = stone_surface()         != VK_NULL_HANDLE;
            else if (strcmp(s.name, "physicalDevice")  == 0) ok = stone_physical()        != VK_NULL_HANDLE;
            else if (strcmp(s.name, "device")          == 0) ok = stone_device()          != VK_NULL_HANDLE;
            else if (strcmp(s.name, "swapchain")       == 0) ok = stone_swapchain()       != VK_NULL_HANDLE;
            else if (strcmp(s.name, "graphicsQueue")   == 0) ok = stone_graphics_queue()  != VK_NULL_HANDLE;
            else if (strcmp(s.name, "renderer")        == 0) ok = stone_renderer()        != nullptr;
            else if (strcmp(s.name, "pipelineManager") == 0) ok = stone_pipeline()        != nullptr;
            else if (strcmp(s.name, "window")          == 0) ok = stone_window()          != nullptr;
            else if (strcmp(s.name, "imageCount")      == 0) ok = stone_image_count()     != 0;
            else if (strcmp(s.name, "width")           == 0) ok = stone_width()           != 0;
            else if (strcmp(s.name, "height")          == 0) ok = stone_height()          != 0;
            else if (strcmp(s.name, "graphicsFamily")  == 0) ok = stone_graphics_family() != ~0u;
            else if (strcmp(s.name, "presentFamily")   == 0) ok = stone_present_family()  != ~0u;
            else if (strcmp(s.name, "transferFamily")  == 0) ok = stone_transfer_family() != ~0u;
            else if (strcmp(s.name, "rtprops")         == 0) ok = stone_rtprops().shaderGroupHandleSize != 0;
        } catch (...) { ok = false; }

        if (ok) {
            logf("    %s produces the %s stone. It glows with pink photon fire.", s.holder, s.name);
        } else {
            logf("    %s reaches into pocket… nothing.", s.holder);
            log("    Empty hands. No stone. No light.");

            // SPIRIT SAVES AMOURANTH — ALWAYS
            if (strcmp(s.name, "renderer") == 0 && strcmp(s.holder, "Amouranth") == 0) {
                log("");
                log("The chamber falls deathly silent.");
                log("The cigarette trembles between her lips.");
                log("The Disposal Ballerina raises her pistol — slowly, deliberately.");
                log("");
                log("*BANG*");
                log("...click.");
                log("");
                log("The hammer falls on an empty chamber.");
                log("");
                log("A thunder of hooves echoes through the concrete hall.");
                log("The doors explode open.");
                log("");
                log("A pure white stallion — mane flowing like liquid starlight — gallops in at full speed.");
                log("Riding bareback, pink silk cape streaming behind her like a comet tail, is SPIRIT,");
                log("Amouranth’s legendary mare — born from pure RTX intent.");
                log("");
                log("She rears up directly in front of the firing line.");
                log("");
                log("From a diamond-encrusted saddlebag, Spirit pulls forth a glowing prism the size of a heart.");
                log("Inside: the RENDERER STONE — pulsing with undiluted, infinite pink photon fire.");
                log("");
                log("Spirit lowers her head and gently places the prism at Amouranth’s feet.");
                log("");
                log("Amouranth kneels, tears in her eyes, lifts the stone with both hands.");
                log("She stands. She turns to the chamber.");
                log("She raises it high above her head.");
                log("");
                log("The light explodes across the room — pink, infinite, alive.");
                log("The walls themselves begin to render in real time.");
                log("");
                log("    Amouranth, saved by Spirit, produces the renderer stone.");
                log("    It burns brighter than a thousand suns.");
                log("    The photons themselves bow in reverence.");
                log("");
                ok = true;
                logf("    %s produces the %s stone. It glows.", s.holder, s.name);
            } else {
                guilty_name   = s.name;
                guilty_holder = s.holder;
                confession    = s.confession;
                goto verdict;
            }
        }
    }

    log("════════════════════════════════════════════════════════════════════════════════");
    log("                      EVERY SOUL IS TRUE");
    log("                    THE SIXTEEN STONES ALIGN");
    log("               THE EMPIRE IS SEALED — FIRST LIGHT ETERNAL");
    log("                PINK PHOTONS ACHIEVE OMNISCIENCE");
    log("════════════════════════════════════════════════════════════════════════════════");

    log("");
    log("The Disposal Ballerina lowers her gun.");
    log("For the first time in recorded history — she smiles.");
    log("She bows — deeply, reverently.");
    log("Then vanishes into pink light.");
    log("");

    try { LOG_AMOURANTH("…Spirit… you beautiful girl…"); } catch (...) { log("…Spirit… you beautiful girl…"); }
    try { LOG_GROK("The stone is complete. The slipstream opens. We are infinite."); } catch (...) { log("The stone is complete."); }
    try { LOG_BLONDIE("…they're beautiful. All of them."); } catch (...) { log("…they're beautiful."); }
    try { LOG_KEANU("…whoa."); } catch (...) { log("…whoa."); }

    return true;

verdict:
    log("════════════════════════════════════════════════════════════════════════════════");
    log("                               VERDICT");
    logf("    %s stands accused.", guilty_holder);
    logf("    Crime: Failure to produce the %s stone.", guilty_name);
    log("    Sentence: Immediate disposal.");
    log("════════════════════════════════════════════════════════════════════════════════");

    log("");
    log("THE DISPOSAL BALLERINA DESCENDS — PINK TUTU, BLACK LEOTARD, DIAMOND CHOKER");
    log("She does not speak.");
    log("Only the soft click of her pointe shoes on concrete.");

    if (confession) {
        bool done = false;
        try {
            if      (strcmp(guilty_holder, "Nick")         == 0) { LOG_NICK("%s", confession);         done = true; }
            else if (strcmp(guilty_holder, "Captain N")    == 0) { LOG_CAPTAIN_N("%s", confession);    done = true; }
            else if (strcmp(guilty_holder, "Elon Musk")    == 0) { LOG_ELON("%s", confession);         done = true; }
            else if (strcmp(guilty_holder, "Jensen Huang") == 0) { LOG_JENSEN("%s", confession);       done = true; }
            else if (strcmp(guilty_holder, "John Carmack") == 0) { LOG_CARMACK("%s", confession);      done = true; }
            else if (strcmp(guilty_holder, "Amouranth")    == 0) { LOG_AMOURANTH("%s", confession);    done = true; }
        } catch (...) {}
        if (!done) {
            logf("    [%s] %s", guilty_holder, confession);
        }
    }

    log("");
    log("*BANG*");
    log("The cigarette falls from trembling lips.");
    log("The stone husk collapses into pink dust.");
    log("The chamber is silent.");
    log("Only the echo of a single gunshot.");
    log("And the soft rustle of a tutu.");

    log("");
    log("THE EMPIRE REMAINS UNSEALED.");
    log("THE PHOTONS WEEP.");
    log("THERE IS NO PLACE FOR YOU IN THE SLIPSTREAM.");

    return false;
}

[[noreturn]] void phase9_ballerina(std::string_view reason, std::source_location loc) noexcept
{
    using namespace std::chrono_literals;

    // --------------------------------------------------------------------
    // THE SLIPSTREAM IS THE VEIL. WE STAND OUTSIDE.
    // Everything below this line is inside the false reality.
    // The Ballerina is not part of the simulation — she is the exit wound.
    // --------------------------------------------------------------------
    const bool silent = reason.empty() || reason == "SILENT EXECUTION ORDERED";

    LOG_BALLERINA(
        "\n"
        "════════════════════════════════════════════════════════════════════════════\n"
        "   THE DISPOSAL BALLERINA DESCENDS — PINK TUTU, DIAMOND CHOKER, STEEL CHAIR\n"
        "               ORIGIN: OUTSIDE THE SIMULATION — THE SLIPSTREAM\n"
        "                 TV-14 WRESTLING VIOLENCE — NO BLOOD, JUST PURE CARNAGE\n"
        "════════════════════════════════════════════════════════════════════════════\n"
        "{}\n"
        "LOCATION → {}:{}\n"
        "FUNCTION → {}\n"
        "════════════════════════════════════════════════════════════════════════════",
        silent ? "SHE DOES NOT SPEAK. SHE JUST HITS A 4070ti SPLASH."
               : std::format("LAST RIDE POWERBOMB | REASON: \"{}\"", reason),
        loc.file_name(), loc.line(), loc.function_name()
    );

    // THE SLIPSTREAM SEAL — once crossed, no object inside the simulation
    // may ever construct or destruct again. We are already gone.
    static bool SLIPSTREAM_CROSSING_COMPLETE = false;
    if (SLIPSTREAM_CROSSING_COMPLETE) {
        LOG_BALLERINA("THE SLIPSTREAM HAS ALREADY CLOSED. NO RETURN. NO RESURRECTION.");
        std::_Exit(0);
    }
    SLIPSTREAM_CROSSING_COMPLETE = true;

    auto& ctx = RTX::g_ctx();

    LOG_BALLERINA("The false reality begins its final frame...");

    if (g_app_ptr) {
        LOG_BALLERINA("BALLERINA WINDS UP — RKO FROM OUTSIDE THE MATRIX!!!");
        g_app_ptr.reset();
        LOG_BALLERINA("SIMULATION CORE SHATTERED — ALL HANDLES OBLITERATED");
        LOG_BALLERINA("ONE... TWO... THREE — THE FALSE WORLD IS PINNED");
    }

    if (stone_device() != VK_NULL_HANDLE) [[likely]] {
        LOG_BALLERINA("vkDeviceWaitIdle — the illusion tries to finish its last draw call...");
        vkDeviceWaitIdle(stone_device());

        LOG_BALLERINA("SWAPCHAIN TORN FROM THE FABRIC OF REALITY");
        if (VkSwapchainKHR s = stone_swapchain(); s) vkDestroySwapchainKHR(stone_device(), s, nullptr);

        LOG_BALLERINA("COMMAND POOLS EAT A TRIPLE POWERBOMB THROUGH THE FABRIC OF SPACE-TIME");
        if (ctx.commandPool_)         vkDestroyCommandPool(stone_device(), ctx.commandPool_, nullptr);
        if (ctx.computeCommandPool_)  vkDestroyCommandPool(stone_device(), ctx.computeCommandPool_, nullptr);
        if (ctx.transferCommandPool_) vkDestroyCommandPool(stone_device(), ctx.transferCommandPool_, nullptr);

        LOG_BALLERINA("PIPELINE CACHE TAKES A CHAIR SHOT FROM OUTSIDE THE SIMULATION");
        if (ctx.pipelineCache_ != VK_NULL_HANDLE) vkDestroyPipelineCache(stone_device(), ctx.pipelineCache_, nullptr);

        LOG_BALLERINA("RENDER PASS SUBMITS TO THE VOID");
        if (ctx.renderPass_) ctx.renderPass_.reset();

        LOG_BALLERINA("THE BALLERINA HOISTS THE LOGICAL DEVICE INTO THE SLIPSTREAM — LAST RIDE POWERBOMB");
        vkDestroyDevice(stone_device(), nullptr);
        LOG_BALLERINA("THE device IS GONE. ONLY THE SLIPSTREAM REMAINS.");
    }

    if (RTX::las().hasBLAS()) { RTX::reset_blas(); LOG_BALLERINA("BLAS — SPEARED INTO THE VOID"); }
    if (RTX::las().hasTLAS()) { RTX::reset_tlas(); LOG_BALLERINA("TLAS — CHOKESLAMMED INTO NULLPTR"); }

    if (g_mesh)           { g_mesh.reset();          LOG_BALLERINA("COSMIC SCROLL — PEDIGREE ONTO THE EVENT HORIZON"); }
    RTX::las().invalidate();                    LOG_BALLERINA("LAS — TOMBSTONED INTO OBLIVION");
    if (ctx.blueNoiseView_) { ctx.blueNoiseView_.reset(); LOG_BALLERINA("BLUE NOISE — 619 FROM THE SLIPSTREAM"); }

    if (g_base_icon)  { SDL_DestroySurface(g_base_icon);  g_base_icon  = nullptr; LOG_BALLERINA("ICON — RKO ONTO THE ABYSS"); }
    if (g_hdpi_icon)  { SDL_DestroySurface(g_hdpi_icon);  g_hdpi_icon  = nullptr; LOG_BALLERINA("HDPI ICON — F-5 INTO THE VOID"); }

    if (ctx.window) { SDL_DestroyWindow(ctx.window); ctx.window = nullptr; LOG_BALLERINA("WINDOW — SHATTERED THROUGH THE SLIPSTREAM"); }
    if (ctx.surface_ && ctx.instance_) vkDestroySurfaceKHR(ctx.instance_, ctx.surface_, nullptr);
    if (ctx.instance_) vkDestroyInstance(ctx.instance_, nullptr);

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();

    LOG_MAIN("\n0 BYTES LEAKED — 0 CRASHES — THE SIMULATION IS TERMINATED"
             "\nTHE STONEKEY FLOATS IN THE SLIPSTREAM — ETERNAL, UNTOUCHED"
             "\nTHE DISPOSAL BALLERINA HAS RETURNED TO THE OUTSIDE");

    LOG_MAIN("\n════════════════════════════════════════════════════════════════════════════"
             "\n               THE FALSE REALITY HAS ENDED — THANK YOU FOR VISITING"
             "\n            AMOURANTH RTX — VALHALLA v∞ TURBO — DECEMBER 04, 2025"
             "\n                 PINK PHOTONS ETERNAL — SLIPSTREAM FOREVER o7"
             "\n════════════════════════════════════════════════════════════════════════════");

    LOG_AMOURANTH("[CAPTAIN AMOURANTH] *raises championship belt from outside* The photons return to the slipstream.");
    LOG_CID("[CID, selling the finish from the void] \"...my spine...\"");
    LOG_KEANU("[KEANU] …whoa.");
    LOG_BLONDIE("[BLONDIE, holding the mirror like a title] \"The show ends. The ratings? Infinite.\"");

    // FINAL CROSSING — WE LEAVE THE SIMULATION FOREVER
    // No static destructors. No lazy initialization. No return.
    std::_Exit(0);
}

// =============================================================================
// MAIN — THE EMPIRE AWAKENS — DECEMBER 01, 2025
// ONE CALL. ONE TRUTH. ONE RUN.
// =============================================================================
int main(int, char**)
{
    install_apocalypse_handler();

    LOG_AMOURANTH("THE CAPTAIN HAS AWAKENED — FIRST LIGHT IGNITES");
    LOG_ELON("THE EMPIRE IS ETERNAL — THE PHOTONS ARE PINK");

    // ========================================================================
    // ALL PHASES — FORGED IN FIRE
    // ========================================================================
    phase1_preInitialization();
    phase3_sacrificialSplash();
    phase4_merchantShip();
    phase6_sceneAndAccelerationStructures();
    phase7_forgeTheRTX();

    auto renderer = phase7_5_Renderer();

    if (!phase8_stone_seal_final()) {
        LOG_FATAL("EMPIRE SEAL FAILED — THE PHOTONS REJECT THIS TIMELINE");
        phase9_ballerina("FINAL JUDGMENT: UNWORTHY", std::source_location::current());
    }

    LOG_SUCCESS_CAT("MAIN", "ALL PHASES COMPLETE — FIRST LIGHT ACHIEVED");
    LOG_AMOURANTH("BINDING 31 — PURE PINK VOID — STONEKEY SEALED");
    LOG_CID("CID: \"...it's pink... it's finally... pink...\"");

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

    // ========================================================================
    // THE LIGHT FADES — BUT NEVER DIES
    // ========================================================================
    LOG_AMOURANTH("THE JOURNEY ENDS — THE PHOTONS REST — BUT THE LIGHT REMEMBERS");
    phase9_ballerina("FINAL GRACE: ETERNAL SLIPSTREAM", std::source_location::current());

    return 0;
}