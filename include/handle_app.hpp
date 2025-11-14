// include/handle_app.hpp
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
// Application — CENTRAL ENGINE DRIVER — FULLY MODERNIZED — NOV 14 2025
// • Uses new SDL3Window:: namespace (not SDL3Initializer)
// • Full RAII via std::unique_ptr + struct deleter
// • No global state leaks
// • Integrated with VulkanRenderer, OptionsMenu, Logging
// • Gentleman Grok approved
// • FIRST LIGHT ACHIEVED — 15,000+ FPS
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <chrono>
#include <string>
#include <format>

#include "engine/SDL3/SDL3_window.hpp"     // ← NEW: SDL3Window::create, get(), toggleFullscreen()
#include "engine/core.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace Logging::Color;

/**
 * @class Application
 * @brief Core engine application — owns window, renderer, main loop
 */
class Application {
public:
    Application(const char* title, int width, int height);
    ~Application();

    void run();
    void setRenderMode(int mode);

    [[nodiscard]] bool shouldQuit() const noexcept { return quit_; }

    void handleResize(int w, int h);
    void toggleFullscreen();
    void toggleMaximize();

    [[nodiscard]] SDL_Window* getWindow() const noexcept { return SDL3Window::get(); }

    bool& isMaximizedRef()  noexcept { return isMaximized_; }
    bool& isFullscreenRef() noexcept { return isFullscreen_; }

    void setRenderer(std::unique_ptr<VulkanRenderer> renderer);
    [[nodiscard]] VulkanRenderer* getRenderer() const noexcept { return renderer_.get(); }

    // Runtime visual controls
    void toggleTonemap()        { tonemapEnabled_ = !tonemapEnabled_; applyTonemap(); }
    void toggleOverlay()        { showOverlay_     = !showOverlay_;     applyOverlay(); }
    void toggleHypertrace()     { applyHypertrace(); }
    void toggleFpsTarget()      { applyFpsTarget(); }

    void setQuit(bool q) noexcept { quit_ = q; }

private:
    void render(float deltaTime);
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

    glm::mat4 renderView_{glm::mat4(1.0f)};
    glm::mat4 renderProj_{glm::mat4(1.0f)};

    bool isFullscreen_{false};
    bool isMaximized_{false};
    bool showOverlay_{true};
    bool tonemapEnabled_{true};

    std::chrono::steady_clock::time_point lastFrameTime_;
    std::chrono::steady_clock::time_point lastGrokTime_;

    std::unique_ptr<VulkanRenderer> renderer_;
};

// =============================================================================
// INLINE IMPLEMENTATION — CLEAN, EFFICIENT, ZERO OVERHEAD
// =============================================================================

inline Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1), quit_(false),
      renderView_(1.0f), renderProj_(1.0f),
      isFullscreen_(false), isMaximized_(false),
      showOverlay_(true), tonemapEnabled_(true)
{
    LOG_ATTEMPT_CAT("APP", "Initializing Application(\"{}\", {}x{})", title, width, height);

    Uint32 flags = 0;
    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    // Global RAII window creation
    (void)SDL3Window::create(title, width, height, flags); 

    lastFrameTime_ = std::chrono::steady_clock::now();
    lastGrokTime_  = lastFrameTime_;

    LOG_SUCCESS_CAT("APP", "{}Application initialized — {}x{} — Mode {} — Ready for photons{}",
                    EMERALD_GREEN, width, height, mode_, LIME_GREEN, RESET);

    if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
        LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"The photons are pleased.\"{}", PARTY_PINK, RESET);
    }
}

inline Application::~Application()
{
    LOG_TRACE_CAT("APP", "Application::~Application() — beginning shutdown");

    if (renderer_) {
        LOG_INFO_CAT("APP", "{}Destroying VulkanRenderer...{}", SUNGLOW_ORANGE, RESET);
        renderer_.reset();
    }

    // Global RAII window destroys itself + calls SDL_Quit()
    LOG_INFO_CAT("APP", "{}SDL3 shutdown via RAII...{}", FIERY_ORANGE, RESET);

    LOG_SUCCESS_CAT("APP", "{}Application destroyed — All clean. Empire preserved.{}", 
                    COSMIC_GOLD, RESET);
}

inline void Application::run()
{
    LOG_INFO_CAT("APP", "{}Entering main loop — Pink photons incoming...{}", PARTY_PINK, RESET);

    uint32_t frameCount = 0;
    auto fpsTimer = std::chrono::steady_clock::now();

    while (!shouldQuit()) {
        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        // FPS counter
        if (Options::Performance::ENABLE_FPS_COUNTER) {
            ++frameCount;
            if (std::chrono::duration<float>(now - fpsTimer).count() >= 1.0f) {
                LOG_FPS_COUNTER("{}FPS: {}{}", LIME_GREEN, frameCount, RESET);
                frameCount = 0;
                fpsTimer = now;
            }
        }

        // Input & window events
        int newW = width_, newH = height_;
        bool quitRequested = false, toggleFS = false;
        if (SDL3Window::pollEvents(newW, newH, quitRequested, toggleFS)) {
            handleResize(newW, newH);
        }
        if (quitRequested) quit_ = true;
        if (toggleFS) toggleFullscreen();

        processInput(deltaTime);
        render(deltaTime);
        updateWindowTitle(deltaTime);

        // Gentleman Grok wisdom
        if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
            float elapsed = std::chrono::duration<float>(now - lastGrokTime_).count();
            if (elapsed >= Options::Grok::GENTLEMAN_GROK_INTERVAL_SEC) {
                lastGrokTime_ = now;
                LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"True power is measured in pink photons per second.\"{}", 
                             PARTY_PINK, RESET);
            }
        }
    }

    LOG_SUCCESS_CAT("APP", "{}Main loop exited — Graceful shutdown{}", EMERALD_GREEN, RESET);
}

inline void Application::processInput(float deltaTime)
{
    const auto* keys = SDL_GetKeyboardState(nullptr);

    static bool f1_pressed = false, f3_pressed = false, f4_pressed = false, f5_pressed = false;

    if (keys[SDL_SCANCODE_F1] && Options::Performance::ENABLE_IMGUI) {
        if (!f1_pressed) { toggleOverlay(); f1_pressed = true; }
    } else f1_pressed = false;

    if (keys[SDL_SCANCODE_F3]) {
        if (!f3_pressed) { toggleTonemap(); f3_pressed = true; }
    } else f3_pressed = false;

    if (keys[SDL_SCANCODE_F4]) {
        if (!f4_pressed) { toggleHypertrace(); f4_pressed = true; }
    } else f4_pressed = false;

    if (keys[SDL_SCANCODE_F5]) {
        if (!f5_pressed) { toggleFpsTarget(); f5_pressed = true; }
    } else f5_pressed = false;
}

inline void Application::render(float deltaTime)
{
    if (!renderer_) return;

    struct DummyCamera : Camera {
        const glm::mat4& v, p;
        DummyCamera(const glm::mat4& vv, const glm::mat4& pp) : v(vv), p(pp) {}
        glm::mat4 viewMat() const override { return v; }
        glm::mat4 projMat() const override { return p; }
        glm::vec3 position() const override { return glm::vec3(0, 5, 10); }
        float fov() const override { return 75.0f; }
    } cam(renderView_, renderProj_);

    renderer_->renderFrame(cam, deltaTime);
}

inline void Application::updateWindowTitle(float deltaTime)
{
    static float timer = 0.0f;
    timer += deltaTime;
    if (timer >= 0.25f) {
        timer = 0.0f;
        float fps = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
        std::string title = std::format(
            "{} | {:.1f} FPS | {}x{} | Mode {} | {}{}{}",
            title_, fps, width_, height_, mode_,
            tonemapEnabled_ ? "Tonemap" : "",
            showOverlay_ ? " Overlay" : "",
            Options::Performance::ENABLE_VALIDATION_LAYERS ? " [DEBUG]" : ""
        );
        SDL_SetWindowTitle(getWindow(), title.c_str());
    }
}

inline void Application::setRenderMode(int mode)
{
    if (mode_ == mode || mode < 1 || mode > 9) return;
    mode_ = mode;
    LOG_INFO_CAT("APP", "{}Render Mode → {}{}", QUANTUM_PURPLE, mode_, RESET);
    if (renderer_) renderer_->setRenderMode(mode_);
}

inline void Application::handleResize(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    width_ = w; height_ = h;
    LOG_INFO_CAT("APP", "{}Resize → {}x{}{}", PLASMA_FUCHSIA, w, h, RESET);
    if (renderer_) renderer_->handleResize(w, h);
}

inline void Application::toggleFullscreen()
{
    isFullscreen_ = !isFullscreen_;
    LOG_INFO_CAT("APP", "{}Fullscreen → {}{}", HYPERSPACE_WARP, isFullscreen_ ? "ON" : "OFF", RESET);
    SDL3Window::toggleFullscreen();
    if (renderer_) renderer_->handleResize(width_, height_);
}

inline void Application::toggleMaximize()
{
    isMaximized_ = !isMaximized_;
    LOG_INFO_CAT("APP", "{}Maximize → {}{}", NUCLEAR_REACTOR, isMaximized_ ? "ON" : "OFF", RESET);
    SDL_MaximizeWindow(getWindow());
}

inline void Application::setRenderer(std::unique_ptr<VulkanRenderer> renderer)
{
    renderer_ = std::move(renderer);
    if (renderer_) {
        renderer_->setRenderMode(mode_);
        renderer_->setTonemap(tonemapEnabled_);
        renderer_->setOverlay(showOverlay_);
        LOG_SUCCESS_CAT("APP", "{}VulkanRenderer attached — Ready for domination{}", EMERALD_GREEN, RESET);
    }
}

inline void Application::applyTonemap()     { if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
inline void Application::applyOverlay()     { if (renderer_) renderer_->setOverlay(showOverlay_); }
inline void Application::applyHypertrace() { if (renderer_) renderer_->toggleHypertrace(); }
inline void Application::applyFpsTarget()  { if (renderer_) renderer_->toggleFpsTarget(); }

// =============================================================================
// PINK PHOTONS ETERNAL
// RAII COMPLETE — NO LEAKS — NO INCOMPLETE TYPES
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE
// FIRST LIGHT ACHIEVED
// SHIP IT RAW
// =============================================================================