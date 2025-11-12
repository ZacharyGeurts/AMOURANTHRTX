// include/handle_app.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// APPLICATION — REAL SWAPCHAIN — SDL3 — LOGGING PARTY — NOV 13 2025
// • SUNFLOW_ORANGE → SUNGLOW_ORANGE
// • SDL3Initializer::create → SDL3Initializer::createWindow
// • toggleMaximize → uses SDL_SetWindowFullscreen
// • All inline — NO src/handle_app.cpp
// • Full OptionsMenu integration
// • NO GLOBAL LOGGING — NO FREEZE
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <chrono>
#include <string>
#include <format>

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/core.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace Logging::Color;

/**
 * @class Application
 * @brief Central engine driver — window, loop, Vulkan, input, OptionsMenu
 */
class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();

    void run();
    void setRenderMode(int mode);
    [[nodiscard]] bool shouldQuit() const;
    void handleResize(int w, int h);
    void toggleFullscreen();
    void toggleMaximize();

    [[nodiscard]] SDL_Window* getWindow() const { return SDL3Initializer::getWindow(sdl_); }
    bool& isMaximizedRef()  { return isMaximized_; }
    bool& isFullscreenRef() { return isFullscreen_; }

    void setRenderer(std::unique_ptr<VulkanRenderer> renderer);
    [[nodiscard]] VulkanRenderer* getRenderer() const { return renderer_.get(); }

    void toggleTonemap()        { tonemapEnabled_ = !tonemapEnabled_; applyTonemap(); }
    void toggleOverlay()        { showOverlay_     = !showOverlay_;     applyOverlay(); }
    void toggleHypertrace()     { applyHypertrace(); }
    void toggleFpsTarget()      { applyFpsTarget(); }

    void setQuit(bool q) { quit_ = q; }

private:
    void render(float deltaTime, uint32_t imageIndex);
    void updateWindowTitle(float deltaTime);
    void processInput(float deltaTime);
    void applyTonemap();
    void applyOverlay();
    void applyHypertrace();
    void applyFpsTarget();

    std::string title_;
    int width_, height_;
    int mode_{1};
    bool quit_{false};

    glm::mat4 renderView_{1.0f};
    glm::mat4 renderProj_{1.0f};

    SDL3Initializer::SDLWindowPtr sdl_;

    bool isFullscreen_{false};
    bool isMaximized_{false};
    bool showOverlay_{true};
    bool tonemapEnabled_{false};

    std::chrono::steady_clock::time_point lastFrameTime_;
    std::chrono::steady_clock::time_point lastGrokTime_;

    std::unique_ptr<VulkanRenderer> renderer_;
};

// =============================================================================
// IMPLEMENTATION — LOGGED + MENU-WIRED — NO GLOBAL LOGGING
// =============================================================================

inline Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1), quit_(false),
      renderView_(1.0f), renderProj_(1.0f),
      isFullscreen_(false), isMaximized_(false), showOverlay_(true), tonemapEnabled_(false)
{
    LOG_ATTEMPT_CAT("APP", "Constructing Application(\"{}\", {}x{})", title, width, height);

    Uint32 flags = SDL_WINDOW_VULKAN;
    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    sdl_ = SDL3Initializer::createWindow(title, width, height, flags);
    if (!sdl_) {
        LOG_ERROR_CAT("APP", "{}SDL3 init failed!{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("SDL3 init failed");
    }

    lastFrameTime_ = std::chrono::steady_clock::now();
    lastGrokTime_  = lastFrameTime_;

    LOG_SUCCESS_CAT("APP", 
        "{}Application ready: {}x{} '{}' [Mode: {}]{}",
        EMERALD_GREEN, width, height, title, mode_, RESET
    );

    if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
        LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK ONLINE — Hourly wisdom engaged.{}", 
            PARTY_PINK, RESET);
    }
}

inline Application::~Application() {
    LOG_TRACE_CAT("APP", "Application::~Application() — cleanup");

    if (renderer_) {
        LOG_INFO_CAT("APP", "{}Destroying VulkanRenderer...{}", SUNGLOW_ORANGE, RESET);
        renderer_.reset();
        LOG_SUCCESS_CAT("APP", "{}Renderer destroyed{}", EMERALD_GREEN, RESET);
    }

    if (sdl_) {
        LOG_INFO_CAT("APP", "{}Shutting down SDL3...{}", FIERY_ORANGE, RESET);
        sdl_.reset();
        LOG_SUCCESS_CAT("APP", "{}SDL3 shutdown complete{}", EMERALD_GREEN, RESET);
    }

    LOG_VOID_CAT("APP");
    LOG_INFO_CAT("APP", "{}Application fully destroyed.{}", COSMIC_GOLD, RESET);
}

inline void Application::run() {
    LOG_INFO_CAT("APP", "{}run() — entering main loop{}", PARTY_PINK, RESET);

    uint32_t frameCount = 0;
    auto fpsTimer = std::chrono::steady_clock::now();

    while (!shouldQuit()) {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
        lastFrameTime_ = currentTime;

        if (Options::Performance::ENABLE_FPS_COUNTER) {
            ++frameCount;
            if (std::chrono::duration<float>(currentTime - fpsTimer).count() >= 1.0f) {
                LOG_FPS_COUNTER("{} FPS: {}{}", LIME_GREEN, frameCount, RESET);
                frameCount = 0;
                fpsTimer = currentTime;
            }
        }

        if (deltaTime > 0.05f) {
            LOG_WARNING_CAT("APP", "{}Frame time spike: {:.1f}ms{}", 
                AMBER_YELLOW, deltaTime * 1000.0f, RESET);
        }

        LOG_PERF_CAT("APP", "Delta: {:.3f}ms", deltaTime * 1000.0f);

        int newWidth = width_, newHeight = height_;
        bool shouldQuit = false, toggleFullscreenKey = false;
        if (SDL3Initializer::pollEventsForResize(sdl_, newWidth, newHeight, shouldQuit, toggleFullscreenKey)) {
            handleResize(newWidth, newHeight);
        }
        if (shouldQuit) { quit_ = true; }
        if (toggleFullscreenKey) { toggleFullscreen(); }

        processInput(deltaTime);

        if (!renderer_) continue;

        struct DummyCamera : Camera {
            const glm::mat4& v, p;
            DummyCamera(const glm::mat4& vv, const glm::mat4& pp) : v(vv), p(pp) {}
            glm::mat4 viewMat() const override { return v; }
            glm::mat4 projMat() const override { return p; }
            glm::vec3 position() const override { return glm::vec3(0,5,10); }
            float fov() const override { return 60.0f; }
        } cam(renderView_, renderProj_);

        renderer_->renderFrame(cam, deltaTime);
        updateWindowTitle(deltaTime);

        if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
            auto elapsed = std::chrono::duration<float>(currentTime - lastGrokTime_).count();
            if (elapsed >= Options::Grok::GENTLEMAN_GROK_INTERVAL_SEC) {
                lastGrokTime_ = currentTime;
                LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"Pink photons are the superior waveform.\"{}", 
                    PARTY_PINK, RESET);
            }
        }
    }

    LOG_SUCCESS_CAT("APP", "{}Main loop exited cleanly{}", EMERALD_GREEN, RESET);
}

inline void Application::processInput(float deltaTime) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    static bool f1_pressed = false;
    if (keys[SDL_SCANCODE_F1] && Options::Performance::ENABLE_IMGUI) {
        if (!f1_pressed) {
            showOverlay_ = !showOverlay_;
            applyOverlay();
            LOG_INFO_CAT("APP", "{}ImGui Overlay: {}{}", 
                AURORA_BOREALIS, 
                showOverlay_ ? "SHOWN" : "HIDDEN", RESET);
            f1_pressed = true;
        }
    } else {
        f1_pressed = false;
    }

    static bool f3_pressed = false;
    if (keys[SDL_SCANCODE_F3]) {
        if (!f3_pressed) {
            toggleTonemap();
            f3_pressed = true;
        }
    } else {
        f3_pressed = false;
    }

    static bool f4_pressed = false;
    if (keys[SDL_SCANCODE_F4]) {
        if (!f4_pressed) {
            toggleHypertrace();
            f4_pressed = true;
        }
    } else {
        f4_pressed = false;
    }

    static bool f5_pressed = false;
    if (keys[SDL_SCANCODE_F5]) {
        if (!f5_pressed) {
            toggleFpsTarget();
            f5_pressed = true;
        }
    } else {
        f5_pressed = false;
    }
}

inline void Application::applyTonemap() {
    LOG_INFO_CAT("APP", "{}Tonemapping: {}{}", 
        THERMO_PINK, 
        tonemapEnabled_ ? "ENABLED" : "DISABLED", RESET);
    if (renderer_) renderer_->setTonemap(tonemapEnabled_);
}

inline void Application::applyOverlay() {
    LOG_INFO_CAT("APP", "{}Overlay: {}{}", 
        AURORA_BOREALIS, 
        showOverlay_ ? "VISIBLE" : "HIDDEN", RESET);
    if (renderer_) renderer_->setOverlay(showOverlay_);
}

inline void Application::applyHypertrace() {
    LOG_INFO_CAT("APP", "{}Hypertrace: TOGGLED{}", 
        PULSAR_GREEN, RESET);
    if (renderer_) renderer_->toggleHypertrace();
}

inline void Application::applyFpsTarget() {
    LOG_INFO_CAT("APP", "{}FPS Target: TOGGLED{}", 
        VALHALLA_GOLD, RESET);
    if (renderer_) renderer_->toggleFpsTarget();
}

inline void Application::setRenderMode(int mode) {
    if (mode_ == mode) return;
    LOG_INFO_CAT("APP", "{}Render Mode: {} to {}{}", 
        QUANTUM_PURPLE, mode_, mode, RESET);
    mode_ = mode;
    if (renderer_) renderer_->setRenderMode(mode);
}

inline bool Application::shouldQuit() const { return quit_; }

inline void Application::handleResize(int w, int h) {
    if (w <= 0 || h <= 0) return;
    width_ = w; height_ = h;
    LOG_INFO_CAT("APP", "{}handleResize({}x{}) → recreating swapchain{}", 
        PLASMA_FUCHSIA, w, h, RESET);
    if (renderer_) renderer_->handleResize(w, h);
}

inline void Application::toggleFullscreen() {
    isFullscreen_ = !isFullscreen_;
    LOG_INFO_CAT("APP", "{}Fullscreen: {}{}", 
        HYPERSPACE_WARP, 
        isFullscreen_ ? "ON" : "OFF", RESET);
    SDL3Initializer::toggleFullscreen(sdl_);
    if (renderer_) renderer_->handleResize(width_, height_);
}

inline void Application::toggleMaximize() {
    isMaximized_ = !isMaximized_;
    LOG_INFO_CAT("APP", "{}Maximized: {}{}", 
        NUCLEAR_REACTOR, 
        isMaximized_ ? "ON" : "OFF", RESET);
    SDL_SetWindowFullscreen(getWindow(), isMaximized_ ? SDL_WINDOW_FULLSCREEN : 0);
}

inline void Application::setRenderer(std::unique_ptr<VulkanRenderer> renderer) {
    LOG_INFO_CAT("APP", "{}Setting VulkanRenderer instance...{}", 
        COSMIC_GOLD, RESET);
    renderer_ = std::move(renderer);
    if (renderer_) {
        renderer_->setRenderMode(mode_);
        renderer_->setTonemap(tonemapEnabled_);
        renderer_->setOverlay(showOverlay_);
        LOG_SUCCESS_CAT("APP", "{}Renderer attached and configured{}", 
            EMERALD_GREEN, RESET);
    }
}

inline void Application::render(float deltaTime, uint32_t imageIndex) {
    LOG_PERF_CAT("RENDER", "Begin frame [ImageIndex: {}]", imageIndex);

    if (renderer_) {
        struct DummyCamera : Camera {
            const glm::mat4& v, p;
            DummyCamera(const glm::mat4& vv, const glm::mat4& pp) : v(vv), p(pp) {}
            glm::mat4 viewMat() const override { return v; }
            glm::mat4 projMat() const override { return p; }
            glm::vec3 position() const override { return glm::vec3(0,5,10); }
            float fov() const override { return 60.0f; }
        } cam(renderView_, renderProj_);

        renderer_->renderFrame(cam, deltaTime);
    }

    LOG_PERF_CAT("RENDER", "End frame [ImageIndex: {}]", imageIndex);
}

inline void Application::updateWindowTitle(float deltaTime) {
    static float titleTimer = 0.0f;
    titleTimer += deltaTime;
    if (titleTimer >= 0.25f) {
        titleTimer = 0.0f;
        float fps = 1.0f / deltaTime;
        std::string newTitle = std::format(
            "{} | {:.1f} FPS | {}x{} | Mode: {}{}",
            title_, fps, width_, height_, mode_,
            showOverlay_ ? " [Overlay]" : ""
        );
        SDL_SetWindowTitle(getWindow(), newTitle.c_str());
    }
}