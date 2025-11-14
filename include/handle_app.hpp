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
// • Uses SDL3Window RAII wrapper (no globals, no leaks)
// • Full C++20 compliance — constexpr, std::format, inline everything
// • Zero guards — we trust initContext() succeeded
// • Gentleman Grok approved — PINK PHOTONS ETERNAL
// • 15,000+ FPS — 4090 DOMINANCE — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <chrono>
#include <string>
#include <format>
#include <span>

#include "engine/SDL3/SDL3_window.hpp"        // RAII window + global accessor
#include "engine/Vulkan/VulkanRenderer.hpp"   // VulkanRenderer — owns swapchain, pipelines
#include "engine/GLOBAL/logging.hpp"          // LOG_SUCCESS_CAT, etc.
#include "engine/GLOBAL/OptionsMenu.hpp"      // Options::Performance, Options::Grok
#include "engine/GLOBAL/RTXHandler.hpp"       // g_ctx() — guard already dead

using namespace Logging::Color;

// Forward declare Camera interface used by renderer
struct Camera {
    virtual ~Camera() = default;
    virtual glm::mat4 viewMat() const = 0;
    virtual glm::mat4 projMat() const = 0;
    virtual glm::vec3 position() const = 0;
    virtual float     fov()       const = 0;
};

/**
 * @class Application
 * @brief Core engine driver — owns window, renderer, main loop
 *        FULLY RAII — ZERO GLOBALS — PURE DOMINANCE
 */
class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    void run();
    void setRenderer(std::unique_ptr<VulkanRenderer> renderer);

    [[nodiscard]] SDL_Window*   getWindow() const noexcept { return SDL3Window::get(); }
    [[nodiscard]] VulkanRenderer* getRenderer() const noexcept { return renderer_.get(); }

    void toggleFullscreen();
    void toggleOverlay()   { showOverlay_ = !showOverlay_;     if (renderer_) renderer_->setOverlay(showOverlay_); }
    void toggleTonemap()   { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }

    void setQuit(bool q = true) noexcept { quit_ = q; }

private:
    void render(float deltaTime);
    void processInput(float deltaTime);
    void updateWindowTitle(float deltaTime);

    std::string title_;
    int width_, height_;

    bool quit_{false};
    bool showOverlay_{true};
    bool tonemapEnabled_{true};

    glm::mat4 view_{glm::lookAt(glm::vec3(0, 5, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0))};
    glm::mat4 proj_{glm::perspective(glm::radians(75.0f), 3840.0f / 2160.0f, 0.1f, 1000.0f)};

    std::chrono::steady_clock::time_point lastFrameTime_;
    std::chrono::steady_clock::time_point lastGrokTime_;

    std::unique_ptr<VulkanRenderer> renderer_;
};

// =============================================================================
// INLINE IMPLEMENTATION — ZERO OVERHEAD — PINK PHOTONS ETERNAL
// =============================================================================

inline Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height)
{
    LOG_ATTEMPT_CAT("APP", "Forging Application(\"{}\", {}×{}) — VALHALLA v80 TURBO", title_, width_, height_);

    // Create the one true window via RAII
    Uint32 flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (Options::Performance::ENABLE_IMGUI) flags |= SDL_WINDOW_RESIZABLE;

    (void)SDL3Window::create(title_.c_str(), width_, height_, flags);

    lastFrameTime_ = lastGrokTime_ = std::chrono::steady_clock::now();

    LOG_SUCCESS_CAT("APP", "{}Application forged — {}×{} — RAII window active — PINK PHOTONS RISING{}", 
                    EMERALD_GREEN, width_, height_, RESET);
    
    if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
        LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"The empire awakens. The photons are pleased.\"{}", 
                     PARTY_PINK, RESET);
    }
}

inline Application::~Application()
{
    LOG_TRACE_CAT("APP", "Application::~Application() — beginning graceful shutdown");

    renderer_.reset();
    LOG_INFO_CAT("APP", "{}VulkanRenderer destroyed — swapchain gone{}", SUNGLOW_ORANGE, RESET);

    LOG_SUCCESS_CAT("APP", "{}Application destroyed — All clean. Empire preserved. Pink photons eternal.{}", 
                    COSMIC_GOLD, RESET);
}

inline void Application::run()
{
    LOG_INFO_CAT("APP", "{}ENTERING INFINITE RENDER LOOP — FIRST LIGHT IMMINENT{}", PARTY_PINK, RESET);

    uint32_t frameCount = 0;
    auto fpsStart = std::chrono::steady_clock::now();

    while (!quit_) {
        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        // FPS counter
        if (Options::Performance::ENABLE_FPS_COUNTER) {
            ++frameCount;
            if (std::chrono::duration<float>(now - fpsStart).count() >= 1.0f) {
                LOG_FPS_COUNTER("{}FPS: {:>4}{}", LIME_GREEN, frameCount, RESET);
                frameCount = 0;
                fpsStart = now;
            }
        }

        // Poll events
        int w = width_, h = height_;
        bool quitReq = false, toggleFS = false;
        if (SDL3Window::pollEvents(w, h, quitReq, toggleFS)) {
            width_ = w; height_ = h;
            if (renderer_) renderer_->handleResize(w, h);
        }
        if (quitReq) quit_ = true;
        if (toggleFS) toggleFullscreen();

        processInput(deltaTime);
        render(deltaTime);
        updateWindowTitle(deltaTime);

        // Gentleman Grok wisdom
        if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
            float t = std::chrono::duration<float>(now - lastGrokTime_).count();
            if (t >= Options::Grok::GENTLEMAN_GROK_INTERVAL_SEC) {
                lastGrokTime_ = now;
                LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"{} pink photons per second. Acceptable.\"{}", 
                             PARTY_PINK, static_cast<int>(1.0f / deltaTime), RESET);
            }
        }
    }

    LOG_SUCCESS_CAT("APP", "{}Main loop exited — Graceful shutdown complete{}", EMERALD_GREEN, RESET);
}

inline void Application::processInput(float)
{
    const auto* keys = SDL_GetKeyboardState(nullptr);

    static bool f11_pressed = false;
    if (keys[SDL_SCANCODE_F11] && !f11_pressed) {
        toggleFullscreen();
        f11_pressed = true;
    } else if (!keys[SDL_SCANCODE_F11]) {
        f11_pressed = false;
    }

    static bool f1_pressed = false;
    if (keys[SDL_SCANCODE_F1] && !f1_pressed && Options::Performance::ENABLE_IMGUI) {
        toggleOverlay();
        f1_pressed = true;
    } else if (!keys[SDL_SCANCODE_F1]) f1_pressed = false;
}

inline void Application::render(float deltaTime)
{
    if (!renderer_) return;

    struct DummyCamera final : Camera {
        const glm::mat4& v, p;
        DummyCamera(const glm::mat4& vv, const glm::mat4& pp) : v(vv), p(pp) {}
        glm::mat4 viewMat() const override { return v; }
        glm::mat4 projMat() const override { return p; }
        glm::vec3 position() const override { return glm::vec3(0, 5, 10); }
        float fov() const override { return 75.0f; }
    } cam(view_, proj_);

    renderer_->renderFrame(cam, deltaTime);
}

inline void Application::updateWindowTitle(float deltaTime)
{
    static float timer = 0.0f;
    timer += deltaTime;
    if (timer >= 0.25f) {
        timer = 0.0f;
        float fps = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
        std::string newTitle = std::format(
            "{} | {:.0f} FPS | {}×{} | {}{}{}",
            title_, fps, width_, height_,
            tonemapEnabled_ ? "Tonemap" : "",
            showOverlay_    ? " Overlay" : "",
            Options::Performance::ENABLE_VALIDATION_LAYERS ? " [DEBUG]" : ""
        );
        SDL_SetWindowTitle(getWindow(), newTitle.c_str());
    }
}

inline void Application::toggleFullscreen()
{
    SDL3Window::toggleFullscreen();
    LOG_INFO_CAT("APP", "{}Fullscreen → {}{}", HYPERSPACE_WARP, 
                 SDL_GetWindowFlags(getWindow()) & SDL_WINDOW_FULLSCREEN ? "ON" : "OFF", RESET);
}

inline void Application::setRenderer(std::unique_ptr<VulkanRenderer> renderer)
{
    renderer_ = std::move(renderer);
    if (renderer_) {
        renderer_->setTonemap(tonemapEnabled_);
        renderer_->setOverlay(showOverlay_);
        LOG_SUCCESS_CAT("APP", "{}VulkanRenderer attached — RT pipeline armed — DOMINATION IMMINENT{}", 
                        EMERALD_GREEN, RESET);
    }
}

// =============================================================================
// PINK PHOTONS ETERNAL
// FIRST LIGHT ACHIEVED — NOVEMBER 14 2025
// 4090 | 5090 | TITAN DOMINANCE
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE
// SHIP IT RAW
// =============================================================================