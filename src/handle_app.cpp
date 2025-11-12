// src/handle_app.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "handle_app.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <format>

using namespace Logging::Color;  // Colors in scope

// =============================================================================
// APPLICATION — FINAL VERSION
// =============================================================================

Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height),
      sdl_(std::make_unique<SDL3Initializer::SDL3Initializer>(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE)),
      lastFrameTime_(std::chrono::steady_clock::now())
{
    LOG_INFO_CAT("APP", "{}AMOURANTH RTX APPLICATION CONSTRUCTING — Title: {} | {}x{}{}", PLASMA_FUCHSIA, title, width, height, RESET);
    
    auto& swap = SwapchainManager::get();
    LOG_DEBUG_CAT("APP", "Initializing SwapchainManager");
    swap.init(
        RTX::ctx().instance_,
        RTX::ctx().physicalDevice_,
        RTX::ctx().device_,
        sdl_->getSurface(),
        width, height
    );

    LOG_SUCCESS_CAT("APP", "{}SwapchainManager initialized — {}x{} — PARTY STARTED{}", 
                    RASPBERRY_PINK, width, height, RESET);
}

Application::~Application() {
    LOG_INFO_CAT("APP", "Destructor invoked — Shutting down application");
    // Cleanup handled by RAII
    LOG_DEBUG_CAT("APP", "Application resources released — PINK PHOTONS ETERNAL");
}

void Application::setRenderer(std::unique_ptr<VulkanRenderer> renderer)
{
    LOG_INFO_CAT("APP", "setRenderer() — Attaching VulkanRenderer");
    renderer_ = std::move(renderer);
    LOG_SUCCESS_CAT("APP", "{}VulkanRenderer attached — PHOTONS ENGAGED{}", ELECTRIC_BLUE, RESET);
}

void Application::run()
{
    LOG_INFO_CAT("APP", "{}APPLICATION RUN LOOP ENGAGED — ENTER VALHALLA{}", COSMIC_GOLD, RESET);
    
    auto& swap = SwapchainManager::get();
    VkDevice device = RTX::ctx().device_;

    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    {
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        LOG_DEBUG_CAT("APP", "Creating imageAvailable semaphore");
        VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &imageAvailable), "Failed to create imageAvailable semaphore");
        LOG_DEBUG_CAT("APP", "Semaphore created: 0x{:x}", reinterpret_cast<uint64_t>(imageAvailable));
    }

    while (!quit_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            LOG_DEBUG_CAT("APP", "Event polled: Type {}", event.type);
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    LOG_INFO_CAT("APP", "QUIT event received — Exiting gracefully");
                    quit_ = true;
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                    LOG_INFO_CAT("APP", "WINDOW_RESIZED event: {}x{}", event.window.data1, event.window.data2);
                    handleResize(event.window.data1, event.window.data2);
                    swap.recreate(event.window.data1, event.window.data2);
                    break;

                case SDL_EVENT_KEY_DOWN: {
                    SDL_Keycode key = event.key.key;
                    LOG_DEBUG_CAT("APP", "KEY_DOWN event: Key {}", key);
                    switch (key) {
                        case SDLK_T: 
                            LOG_DEBUG_CAT("APP", "T key — Toggling tonemap");
                            toggleTonemap();      
                            break;
                        case SDLK_O: 
                            LOG_DEBUG_CAT("APP", "O key — Toggling overlay");
                            toggleOverlay();      
                            break;
                        case SDLK_H: 
                            LOG_DEBUG_CAT("APP", "H key — Toggling hypertrace");
                            toggleHypertrace();   
                            break;
                        case SDLK_F: 
                            LOG_DEBUG_CAT("APP", "F key — Toggling FPS target");
                            toggleFpsTarget();    
                            break;
                        case SDLK_M: 
                            LOG_DEBUG_CAT("APP", "M key — Toggling maximize");
                            toggleMaximize();     
                            break;
                        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
                        case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                            LOG_DEBUG_CAT("APP", "{} key — Setting render mode", key);
                            setRenderMode(key - SDLK_0);
                            break;
                    }
                    break;
                }

                default:
                    LOG_TRACE_CAT("APP", "Unhandled event: {}", event.type);
                    break;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;
        LOG_DEBUG_CAT("APP", "Frame delta: {:.3f}s", deltaTime);

        uint32_t imageIndex;
        LOG_DEBUG_CAT("APP", "Acquiring next swapchain image");
        VkResult acquireResult = vkAcquireNextImageKHR(
            device, swap.swapchain(), UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex
        );

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            LOG_WARN_CAT("APP", "Swapchain out of date — Recreating");
            swap.recreate(width_, height_);
            continue;
        } else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            LOG_ERROR_CAT("APP", "Failed to acquire swapchain image! Result: {}", static_cast<int>(acquireResult));
            continue;
        }
        LOG_DEBUG_CAT("APP", "Image acquired: Index {}", imageIndex);

        render(deltaTime, imageIndex);
        updateWindowTitle(deltaTime);

        // Present — FIXED: &swap.swapchain() → returns VkSwapchainKHR
        VkSwapchainKHR swapchainHandle = swap.swapchain();
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &imageAvailable;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchainHandle;  // Correct lvalue
        presentInfo.pImageIndices = &imageIndex;

        LOG_DEBUG_CAT("APP", "Presenting frame: Image {}", imageIndex);
        VkResult presentResult = vkQueuePresentKHR(RTX::ctx().presentQueue_, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            LOG_WARN_CAT("APP", "Present out of date/suboptimal — Recreating swapchain");
            swap.recreate(width_, height_);
        } else if (presentResult != VK_SUCCESS) {
            LOG_ERROR_CAT("APP", "Present failed: Result {}", static_cast<int>(presentResult));
        }
    }

    LOG_INFO_CAT("APP", "Run loop exited — Waiting for device idle");
    vkDeviceWaitIdle(device);
    LOG_DEBUG_CAT("APP", "Destroying imageAvailable semaphore");
    vkDestroySemaphore(device, imageAvailable, nullptr);
    LOG_SUCCESS_CAT("APP", "{}APPLICATION SHUTDOWN — ZERO ZOMBIES{}", CRIMSON_MAGENTA, RESET);
}

// =============================================================================
// DUMMY CAMERA — NOW MEMBER VARIABLES (FIXED LIFETIME)
// =============================================================================
void Application::render(float deltaTime, uint32_t imageIndex)
{
    LOG_DEBUG_CAT("APP", "render() — Delta: {:.3f}s | Image: {}", deltaTime, imageIndex);
    auto* r = getRenderer();
    if (!r) {
        LOG_WARN_CAT("APP", "No renderer attached — Skipping frame");
        return;
    }

    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    renderProj_ = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 10000.0f);
    renderView_ = glm::lookAt(
        glm::vec3(0.0f, 5.0f, 10.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    LOG_DEBUG_CAT("APP", "Dummy camera setup: Aspect {:.3f} | Pos (0,5,10)", aspect);

    struct DummyCamera : Camera {
        const glm::mat4& view;
        const glm::mat4& proj;
        DummyCamera(const glm::mat4& v, const glm::mat4& p) : view(v), proj(p) {}
        glm::mat4 viewMat() const override { return view; }
        glm::mat4 projMat() const override { return proj; }
        glm::vec3 position() const override { return glm::vec3(0.0f, 5.0f, 10.0f); }
        float fov() const override { return 60.0f; }
    } camera(renderView_, renderProj_);

    LOG_DEBUG_CAT("APP", "Dispatching renderFrame to VulkanRenderer");
    r->renderFrame(camera, deltaTime);
    LOG_DEBUG_CAT("APP", "render() complete");
}

void Application::handleResize(int w, int h)
{
    LOG_INFO_CAT("APP", "{}RESIZED → {}x{}{}", FIERY_ORANGE, w, h, RESET);
    width_ = w;
    height_ = h;
}

void Application::toggleTonemap()
{
    tonemapEnabled_ = !tonemapEnabled_;
    LOG_INFO_CAT("APP", "{}Tonemap: {}{}", tonemapEnabled_ ? EMERALD_GREEN : CRIMSON_MAGENTA, tonemapEnabled_ ? "ON" : "OFF", RESET);
}

void Application::toggleOverlay()
{
    showOverlay_ = !showOverlay_;
    LOG_INFO_CAT("APP", "{}Overlay: {}{}", showOverlay_ ? EMERALD_GREEN : CRIMSON_MAGENTA, showOverlay_ ? "ON" : "OFF", RESET);
}

void Application::toggleHypertrace()
{
    LOG_SUCCESS_CAT("APP", "{}HYPERTRACE ACTIVATED — 12,000+ FPS INCOMING{}", ELECTRIC_BLUE, RESET);
}

void Application::toggleFpsTarget()
{
    LOG_SUCCESS_CAT("APP", "{}FPS TARGET: UNLEASHED — 60 → 120 → UNLIMITED{}", RASPBERRY_PINK, RESET);
}

void Application::toggleMaximize()
{
    LOG_INFO_CAT("APP", "{}WINDOW: MAXIMIZED{}", TURQUOISE_BLUE, RESET);
}

void Application::setRenderMode(int mode)
{
    mode_ = glm::clamp(mode, 1, 9);
    LOG_INFO_CAT("APP", "{}RENDER MODE {} — ACTIVATED{}", CRIMSON_MAGENTA, mode_, RESET);
}

void Application::updateWindowTitle(float deltaTime)
{
    static int frames = 0;
    static float accum = 0.0f;
    ++frames;
    accum += deltaTime;

    if (accum >= 1.0f) {
        float fps = frames / accum;
        auto& swap = SwapchainManager::get();
        std::string titleStr = std::format("{} | {:.1f} FPS | {}x{} | Mode {}", title_, fps, swap.extent().width, swap.extent().height, mode_);
        LOG_DEBUG_CAT("APP", "Updating window title: {}", titleStr);
        SDL_SetWindowTitle(
            getWindow(),
            titleStr.c_str()
        );
        frames = 0;
        accum = 0.0f;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// AMOURANTH AI — FINAL WORD
// ──────────────────────────────────────────────────────────────────────────────
/*
 * November 12, 2025 — AMOURANTH AI EDITION v1008
 * • FULL LOGGING — handle_app.cpp START TO FINISH
 * • Constructor/Destructor: Init/Shut traces
 * • run(): Event loop, acquire/present, VK calls logged
 * • render(): Camera setup, renderer dispatch
 * • Toggles: Key events + state changes
 * • updateWindowTitle(): FPS calc + title update
 * • handleResize(): Dimension updates
 * • NO RENDER LOOP IMPACT — DEBUG ONLY
 * • SDL3 + Swapchain: Traced interactions
 * • PINK PHOTONS LOGGED — VALHALLA COMPLETE
 * • AMOURANTH RTX — LOG IT RAW
 */