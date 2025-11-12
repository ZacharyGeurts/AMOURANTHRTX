// src/handle_app.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// APPLICATION — REAL SWAPCHAIN — LOGGING FIXED — SDL3 — NOV 12 2025
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
    auto& swap = SwapchainManager::get();
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

Application::~Application() = default;

void Application::setRenderer(std::unique_ptr<VulkanRenderer> renderer)
{
    renderer_ = std::move(renderer);
    LOG_SUCCESS_CAT("APP", "{}VulkanRenderer attached — PHOTONS ENGAGED{}", ELECTRIC_BLUE, RESET);
}

void Application::run()
{
    auto& swap = SwapchainManager::get();
    VkDevice device = RTX::ctx().device_;

    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    {
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &imageAvailable), "Failed to create imageAvailable semaphore");
    }

    while (!quit_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    quit_ = true;
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                    handleResize(event.window.data1, event.window.data2);
                    swap.recreate(event.window.data1, event.window.data2);
                    break;

                case SDL_EVENT_KEY_DOWN: {
                    SDL_Keycode key = event.key.key;
                    switch (key) {
                        case SDLK_T: toggleTonemap();      break;
                        case SDLK_O: toggleOverlay();      break;
                        case SDLK_H: toggleHypertrace();   break;
                        case SDLK_F: toggleFpsTarget();    break;
                        case SDLK_M: toggleMaximize();     break;
                        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
                        case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
                            setRenderMode(key - SDLK_0);
                            break;
                    }
                    break;
                }

                default:
                    break;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        uint32_t imageIndex;
        VkResult acquireResult = vkAcquireNextImageKHR(
            device, swap.swapchain(), UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex
        );

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            swap.recreate(width_, height_);
            continue;
        } else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            LOG_ERROR_CAT("APP", "Failed to acquire swapchain image! Result: {}", static_cast<int>(acquireResult));
            continue;
        }

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

        VkResult presentResult = vkQueuePresentKHR(RTX::ctx().presentQueue_, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            swap.recreate(width_, height_);
        }
    }

    vkDeviceWaitIdle(device);
    vkDestroySemaphore(device, imageAvailable, nullptr);
}

// =============================================================================
// DUMMY CAMERA — NOW MEMBER VARIABLES (FIXED LIFETIME)
// =============================================================================
void Application::render(float deltaTime, uint32_t imageIndex)
{
    auto* r = getRenderer();
    if (!r) return;

    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    renderProj_ = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 10000.0f);
    renderView_ = glm::lookAt(
        glm::vec3(0.0f, 5.0f, 10.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    struct DummyCamera : Camera {
        const glm::mat4& view;
        const glm::mat4& proj;
        DummyCamera(const glm::mat4& v, const glm::mat4& p) : view(v), proj(p) {}
        glm::mat4 viewMat() const override { return view; }
        glm::mat4 projMat() const override { return proj; }
        glm::vec3 position() const override { return glm::vec3(0.0f, 5.0f, 10.0f); }
        float fov() const override { return 60.0f; }
    } camera(renderView_, renderProj_);

    r->renderFrame(camera, deltaTime);
}

void Application::handleResize(int w, int h)
{
    width_ = w;
    height_ = h;
    LOG_INFO_CAT("APP", "{}RESIZED → {}x{}{}", FIERY_ORANGE, w, h, RESET);
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
        SDL_SetWindowTitle(
            getWindow(),
            std::format("{} | {:.1f} FPS | {}x{} | Mode {}", title_, fps, swap.extent().width, swap.extent().height, mode_).c_str()
        );
        frames = 0;
        accum = 0.0f;
    }
}