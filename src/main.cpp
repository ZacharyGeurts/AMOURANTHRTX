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

    if (RTX::SwapchainManager::minimized_) {
        LOG_AMOURANTH("[PHASE 0] Window minimized — photons meditate in silence");
        return;
    }

    const uint32_t globalFrame = currentFrame_.fetch_add(1, std::memory_order_relaxed);
    const uint32_t slot        = globalFrame % maxFramesInFlight_;

    // PHASE 1: Wait — BestQuality = infinite (perfect sync), Uncapped = 1µs (max FPS)
    if (inFlightFences_[slot] != VK_NULL_HANDLE) {
        if (Options::CURRENT_PRESET == Options::Preset::BestQuality) {
            VK_CHECK(vkWaitForFences(stone_device(), 1, &inFlightFences_[slot], VK_TRUE, UINT64_MAX));
        } else {
            VkResult r = vkWaitForFences(stone_device(), 1, &inFlightFences_[slot], VK_TRUE, 1'000);
            if (r == VK_TIMEOUT) {
                currentFrame_.fetch_sub(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        stone_device(),
        RTX::SwapchainManager::swapchain(),
        Options::CURRENT_PRESET == Options::Preset::BestQuality ? UINT64_MAX : 100'000'000,
        imageAvailableSemaphores_[slot],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            g_resizeRequested.store(true);
            g_resizeWidth.store(stone_width());
            g_resizeHeight.store(stone_height());
        }
        LOG_AMOURANTH("[PHASE 2] Acquire failed: {} → skipping frame", string_VkResult(acquireResult));
        return;
    }

    bool needsResize = (acquireResult == VK_SUBOPTIMAL_KHR);

    VK_CHECK(vkResetFences(stone_device(), 1, &inFlightFences_[slot]));
    VK_CHECK(vkResetCommandBuffer(commandBuffers_[slot], 0));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffers_[slot], &beginInfo));
    VkCommandBuffer cmd = commandBuffers_[slot];

    bool drawPink = (activeRenderMode_ == 0) || g_forcePink.load() || g_resizeRequested.load();

    if (drawPink) {
        VkClearColorValue pink{ {1.0f, 0.2f, 0.8f, 1.0f} };
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageMemoryBarrier toTransfer{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image               = stone_images()[imageIndex],
            .subresourceRange    = range
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &toTransfer);

        vkCmdClearColorImage(cmd, stone_images()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &pink, 1, &range);

        VkImageMemoryBarrier toPresent{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = 0,
            .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image               = stone_images()[imageIndex],
            .subresourceRange    = range
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &toPresent);

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSemaphoreSubmitInfo waitSem{
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = imageAvailableSemaphores_[slot],
            .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };

        VkCommandBufferSubmitInfo cmdInfo{
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd
        };

        VkSemaphoreSubmitInfo signalSem{
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = renderFinishedSemaphores_[slot]
        };

        VkSubmitInfo2 submit{
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &waitSem,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &cmdInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signalSem
        };

        VK_CHECK(vkQueueSubmit2(stone_graphics_queue(), 1, &submit, inFlightFences_[slot]));

        VkPresentInfoKHR present{
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &renderFinishedSemaphores_[slot],
            .swapchainCount     = 1,
            .pSwapchains        = &stone_swapchain(),
            .pImageIndices      = &imageIndex
        };

        VkResult presentResult = vkQueuePresentKHR(stone_present_queue(), &present);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            g_resizeRequested.store(true);
            g_resizeWidth.store(stone_width());
            g_resizeHeight.store(stone_height());
        }

        frameNumber_++;
        return;
    }

    // FULL NORMAL RENDER PATH — UNTOUCHED
    if (resetAccumulation_ || resetAccumNextFrame_) {
        LOG_MAIN("[PHASE 8.1] ACCUMULATION RESET");
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

    updateUniformBuffer(slot, camera, deltaTime);
    updateTonemapUniform(slot);

    VkAccelerationStructureKHR tlas = RTX::LAS::get().getTLAS();
    if (tlas == VK_NULL_HANDLE) tlas = pipelineManager_.dummyTLAS();

    RTX::RTDescriptorUpdate desc{};
    desc.tlas                  = tlas;
    desc.ubo                   = reinterpret_cast<VkBuffer>(uniformBufferEncs_[slot]);
    desc.uboSize               = 368;
    desc.rtOutputViews[slot]   = rtOutputViews_[slot].get();
    desc.accumulationViews[slot] = accumViews_[slot].get();
    desc.envSampler            = envMapSampler_.get();
    desc.envImageView          = envMapImageView_.get();
    desc.materialsBuffer       = reinterpret_cast<VkBuffer>(materialBufferEncs_[0]);
    desc.materialsSize         = 16_MB;
    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreView_.valid())
        desc.nexusScoreViews[slot] = hypertraceScoreView_.get();

    pipelineManager_.updateRTDescriptorSet(slot, desc);
    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING)
        updateNexusDescriptors();

    recordRayTracingCommands(cmd, slot);

    VkImageMemoryBarrier rtToRead{
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

    VkImageMemoryBarrier toPresent{
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

    VkSemaphoreSubmitInfo waitSem{
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = imageAvailableSemaphores_[slot],
        .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkCommandBufferSubmitInfo cmdInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd
    };

    VkSemaphoreSubmitInfo signalSem{
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = renderFinishedSemaphores_[slot]
    };

    VkSubmitInfo2 submit{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = 1,
        .pWaitSemaphoreInfos      = &waitSem,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmdInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signalSem
    };

    VK_CHECK(vkQueueSubmit2(stone_graphics_queue(), 1, &submit, inFlightFences_[slot]));

    VkPresentInfoKHR present{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &renderFinishedSemaphores_[slot],
        .swapchainCount     = 1,
        .pSwapchains        = &stone_swapchain(),
        .pImageIndices      = &imageIndex
    };

    VkResult presentResult = vkQueuePresentKHR(stone_present_queue(), &present);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || needsResize) {
        g_resizeRequested.store(true);
        g_resizeWidth.store(stone_width());
        g_resizeHeight.store(stone_height());
    }

    currentSpp_++;
    accumulationFrame_++;
    frameNumber_++;
    LOG_AMOURANTH("FRAME COMPLETE | #{} | spp={} | accum={}", globalFrame, currentSpp_, accumulationFrame_);
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

inline void AdvanceEternalRing() noexcept
{
    static constexpr uint32_t FRAMES = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    static std::array<VkCommandPool,   FRAMES> g_pools   = {};
    static std::array<VkCommandBuffer, FRAMES> g_cmds    = {};
    static std::array<VkFence,         FRAMES> g_fences  = {};
    static uint32_t                           g_current = 0;
    static bool                               g_initialized = false;

    if (!g_initialized) {
        const VkDevice dev = stone_device();

        VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = RTX::g_ctx().graphicsFamily()
        };

        VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (uint32_t i = 0; i < FRAMES; ++i) {
            VK_CHECK(vkCreateCommandPool(dev, &poolInfo, nullptr, &g_pools[i]));
            VkCommandBufferAllocateInfo allocInfo{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = g_pools[i],
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };
            VK_CHECK(vkAllocateCommandBuffers(dev, &allocInfo, &g_cmds[i]));
            VK_CHECK(vkCreateFence(dev, &fenceInfo, nullptr, &g_fences[i]));
        }

        RTX::g_ctx().commandPool_ = g_pools[0];
        g_initialized = true;

        LOG_AMOURANTH("ETERNAL COMMAND RING FORGED — {} SLOTS — g_ctx().commandPool_ = IMMORTAL", FRAMES);
    }

    // Advance to next frame
    vkWaitForFences(stone_device(), 1, &g_fences[g_current], VK_TRUE, UINT64_MAX);
    vkResetFences(stone_device(), 1, &g_fences[g_current]);
    vkResetCommandPool(stone_device(), g_pools[g_current], 0);

    g_current = (g_current + 1) % FRAMES;
    RTX::g_ctx().commandPool_ = g_pools[g_current];  // ← Keeps all old code working
}

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

void Application::run() noexcept
{
    // ── ETERNAL STATE — IMMORTAL ACROSS CRASHES
    static std::atomic<bool> g_resizeInProgress{false};
    static std::atomic<uint32_t> g_pendingWidth{0};
    static std::atomic<uint32_t> g_pendingHeight{0};

    auto lastTime = std::chrono::steady_clock::now();

    int   frameCount = 0;
    float fpsTimer   = 0.0f;
    float currentFPS = 60.0f;

    float titleTimer = 0.0f;
    constexpr float TITLE_UPDATE_INTERVAL = 0.6f;
    int dotPhase = 0;
    constexpr const char* dots[] = { ".", "..", "...", "...." };

    uint32_t currentMaxFramesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // ── MAIN LOOP — THE EMPIRE NEVER DIES
    while (!quit_)
    {
        const auto frameStart = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        // ── INPUT & EVENTS — UNBREAKABLE
        bool toggleFS = false;
        int winW = 0, winH = 0;
        SDL3Window::pollEvents(winW, winH, quit_, toggleFS);

        width_  = winW > 0 ? winW : width_;
        height_ = winH > 0 ? winH : height_;

        if (width_ > 0 && height_ > 0)
        {
            proj_ = glm::perspective(
                glm::radians(75.0f),
                static_cast<float>(width_) / std::max(height_, 1),
                0.1f, 1000.0f
            );
        }

        if (toggleFS) {
            SDL3Window::toggleFullscreen();
        }

        // ── RESIZE HANDLING — NUCLEAR-PROOF + SELF-HEALING
        if (g_resizeRequested.exchange(false))
        {
            uint32_t w = g_resizeWidth.exchange(0);
            uint32_t h = g_resizeHeight.exchange(0);
            if (w && h)
            {
                g_pendingWidth.store(w, std::memory_order_relaxed);
                g_pendingHeight.store(h, std::memory_order_relaxed);
            }
        }

        if (g_pendingWidth.load(std::memory_order_relaxed) && 
            g_pendingHeight.load(std::memory_order_relaxed))
        {
            uint32_t w = g_pendingWidth.exchange(0, std::memory_order_relaxed);
            uint32_t h = g_pendingHeight.exchange(0, std::memory_order_relaxed);

            LOG_AMOURANTH("[RESIZE] Recreating swapchain: {}×{}", w, h);

            vkDeviceWaitIdle(stone_device());

            RTX::las().notifyResize();
            RTX::SwapchainManager::get().recreate(w, h);

            // Rebuild acceleration structures safely
            for (int i = 0; i < 3; ++i)
                RTX::las().beginFrame();

            // Force pipeline + descriptors to recover if corrupted
            RTX::pipeline().forgeRTXPipeline(RTX::g_ctx().commandPool(), stone_graphics_queue());

            LOG_SUCCESS_CAT("RESIZE", "Swapchain + RTX Crown rebuilt — empire restored");
        }

        // ── INPUT
        processInput(g_deltaTime);

// ── RENDER — SELF-HEALING RENDERER — THE CROWN REFORGES ITSELF
if (renderer_)
{
    renderer_->setMaxFramesInFlight(currentMaxFramesInFlight);

    // SELF-HEALING: If the renderer dies (device lost, driver crash, etc.)
    if (!renderer_->isAlive())
    {
        LOG_FATAL_CAT("RENDERER", "Renderer died — RESURRECTING FROM THE VOID");
        LOG_CAPTAIN_N("[CAPTAIN N] \"...She killed the renderer.\n"
                      "               She thought she won.\n"
                      "               She was wrong.\n"
                      "               We are the empire.\n"
                      "               We rise again.\"");

        // FULL RESURRECTION — CROWN REBORN
        renderer_ = phase7_5_Renderer();  // ← THIS IS THE ONE TRUE WAY
        stone_seal_renderer(renderer_.get());

        LOG_SUCCESS_CAT("RENDERER", "Renderer resurrected — crown restored — Binding 31 lives");
    }

    renderer_->renderFrame(CAM, g_deltaTime);
}

        // ── TITLE — ETERNAL AND BEAUTIFUL
        titleTimer += g_deltaTime;
        if (titleTimer >= TITLE_UPDATE_INTERVAL)
        {
            titleTimer -= TITLE_UPDATE_INTERVAL;
            dotPhase = (dotPhase + 1) % 4;

            const char* modeName = "VOID";
            if (currentRenderMode_ > 0 && currentRenderMode_ <= 9)
            {
                constexpr const char* names[] = {
                    "VOID",
                    "PURE PINK — BINDING 31",
                    "PATH TRACED ACCUM",
                    "HYBRID DENOISED",
                    "RASTER FALLBACK",
                    "DEBUG VIS",
                    "TLAS VIEWER",
                    "SBT DEBUG",
                    "PERF METRICS",
                    "HOT RELOAD TEST"
                };
                modeName = names[currentRenderMode_];
            }

            const std::string title = currentRenderMode_ == 0 ?
                std::format("AMOURANTH RTX | {:.1f} FPS | {}×{} | DEV MODE | PRESS 1-9 TO IGNITE{}", 
                           currentFPS, stone_width(), stone_height(), dots[dotPhase]) :
                std::format("AMOURANTH RTX | {:.1f} FPS | {}×{} | Mode {}: {} | Bounces {} | FIF:{}",
                           currentFPS, stone_width(), stone_height(),
                           currentRenderMode_, modeName,
                           Options::OptionsRTX::MAX_BOUNCES, currentMaxFramesInFlight);

            SDL_SetWindowTitle(stone_window(), title.c_str());
        }

        // ── FPS COUNTER — SMOOTH AND ACCURATE
        ++frameCount;
        fpsTimer += g_deltaTime;
        if (fpsTimer >= 1.0f)
        {
            currentFPS = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer   = 0.0f;
        }

        // ── FRAME PACING — OPTIONAL BUT GODLY
        if (Options::Performance::ENABLE_FRAME_PREDICTION)
        {
            const auto frameTime = std::chrono::steady_clock::now() - frameStart;
            const auto target = std::chrono::duration<float>(1.0f / 240.0f);
            if (frameTime < target)
                std::this_thread::sleep_for(target - frameTime);
        }
    }

    // ── FINAL SHUTDOWN — GRACEFUL AND ETERNAL
    vkDeviceWaitIdle(stone_device());
    LOG_AMOURANTH("[SHUTDOWN] The empire rests. The photons return to the void.");
}

// =============================================================================
// MAIN — THE EMPIRE AWAKENS — DECEMBER 01, 2025
// ONE CALL. ONE TRUTH. ONE RUN.
// =============================================================================
int main(int, char**)
{
    install_apocalypse_handler();

    phase1_preInitialization();
    phase3_sacrificialSplash();
    phase4_merchantShip();
    phase6_sceneAndAccelerationStructures();
    phase7_forgeTheRTX();

    auto renderer = phase7_5_Renderer();
    stone_seal_final();

	AdvanceEternalRing();

    // ========================================================================
    // ETERNAL COMMAND RING — FORGED WITH PURE STATIC MAGIC
    // g_ctx().commandPool_ IS NOW ETERNAL AND ALWAYS VALID
    // NO LOCAL CLASSES. NO STATIC MEMBERS. NO ERRORS.
    // ========================================================================

    static constexpr uint32_t FRAMES = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    static std::array<VkCommandPool,   FRAMES> g_pools   = {};
    static std::array<VkCommandBuffer, FRAMES> g_cmds    = {};
    static std::array<VkFence,         FRAMES> g_fences  = {};
    static uint32_t                            g_current = 0;
    static bool                                g_ringInitialized = false;

    if (!g_ringInitialized) {
        const VkDevice dev = stone_device();

        const VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = RTX::g_ctx().graphicsFamily()  // ← Correct call
        };

        const VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (uint32_t i = 0; i < FRAMES; ++i) {
            VK_CHECK(vkCreateCommandPool(dev, &poolInfo, nullptr, &g_pools[i]));

            VkCommandBufferAllocateInfo allocInfo{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool        = g_pools[i],
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };
            VK_CHECK(vkAllocateCommandBuffers(dev, &allocInfo, &g_cmds[i]));
            VK_CHECK(vkCreateFence(dev, &fenceInfo, nullptr, &g_fences[i]));
        }

        // SACRED PATCH — g_ctx().commandPool_ NOW POINTS TO ETERNAL RING
        RTX::g_ctx().commandPool_ = g_pools[0];

        LOG_AMOURANTH("ETERNAL COMMAND RING FORGED — {} SLOTS — g_ctx().commandPool_ = ETERNAL", FRAMES);
        g_ringInitialized = true;
    }

    // ========================================================================
    // HELPER: Advance ring and keep g_ctx().commandPool_ in sync
    // Call this at the start of each frame if you want perfect sync
    // ========================================================================
    [[maybe_unused]] static const auto advanceEternalRing = []() {
        vkWaitForFences(stone_device(), 1, &g_fences[g_current], VK_TRUE, UINT64_MAX);
        vkResetFences(stone_device(), 1, &g_fences[g_current]);
        vkResetCommandPool(stone_device(), g_pools[g_current], 0);

        g_current = (g_current + 1) % FRAMES;
        RTX::g_ctx().commandPool_ = g_pools[g_current];  // Keep legacy code happy forever
    };

    // Optional: call once per frame
    advanceEternalRing();

    // ========================================================================
    // ASCENSION — NOW GO FULL ROBOT HEAVY
    // ========================================================================
    g_app_ptr = std::make_unique<Application>(
        "AMOURANTH RTX — VALHALLA v∞ TURBO",
        Options::Window::DEFAULT_WIDTH,
        Options::Window::DEFAULT_HEIGHT
    );
    g_app_ptr->setRenderer(std::move(renderer));

    g_app_ptr->run();

    phase9_ballerina("FINAL GRACE: ETERNAL SLIPSTREAM", std::source_location::current());

    // Cleanup on exit
    vkDeviceWaitIdle(stone_device());
    for (uint32_t i = 0; i < FRAMES; ++i) {
        if (g_cmds[i])   vkFreeCommandBuffers(stone_device(), g_pools[i], 1, &g_cmds[i]);
        if (g_fences[i]) vkDestroyFence(stone_device(), g_fences[i], nullptr);
        if (g_pools[i])  vkDestroyCommandPool(stone_device(), g_pools[i], nullptr);
    }

    LOG_AMOURANTH("ETERNAL COMMAND RING — RETURNED TO VALHALLA");

    return 0;
}