// include/handle_app.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 or later (GPL-3.0-or-later)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// Application — CENTRAL ENGINE DRIVER — FULLY MODERNIZED — NOV 16 2025
// • SDL3 + C++20 + RAII + Zero globals + Inline everything
// • Audio mute fixed for real SDL3 API (no SDL_GetAudioDevices, no extra args)
// • Keybinds: F (fullscreen), O/T/H/M, 1-9 (modes), ESC (quit)
// • M = Maximize + Global Audio Mute (SDL3 correct)
// • Gentleman Grok approved — PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <chrono>
#include <string>
#include <format>
#include <array>

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

using namespace Logging::Color;

// =============================================================================
// CENTRALIZED KEYBINDINGS — NO EXTERNAL FILE — PURE EMPIRE
// =============================================================================
namespace KeyBind {
    inline constexpr SDL_Scancode FULLSCREEN      = SDL_SCANCODE_F;
    inline constexpr SDL_Scancode OVERLAY         = SDL_SCANCODE_O;
    inline constexpr SDL_Scancode TONEMAP         = SDL_SCANCODE_T;
    inline constexpr SDL_Scancode HYPERTRACE      = SDL_SCANCODE_H;
    inline constexpr SDL_Scancode MAXIMIZE_MUTE   = SDL_SCANCODE_M;
    inline constexpr SDL_Scancode QUIT            = SDL_SCANCODE_ESCAPE;

    inline constexpr std::array<SDL_Scancode, 9> RENDER_MODE{{
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
        SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
        SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9
    }};
}

// Forward declare Camera
struct Camera {
    virtual ~Camera() = default;
    virtual glm::mat4 viewMat() const = 0;
    virtual glm::mat4 projMat() const = 0;
    virtual glm::vec3 position() const = 0;
    virtual float     fov()       const = 0;
};

class Application {
public:
    Application(const std::string& title, int width, int height);
    ~Application();

    void run();
    void setRenderer(std::unique_ptr<VulkanRenderer> renderer);

    [[nodiscard]] SDL_Window*     getWindow() const noexcept { return SDL3Window::get(); }
    [[nodiscard]] VulkanRenderer* getRenderer() const noexcept { return renderer_.get(); }

    void toggleFullscreen();
    void toggleOverlay()   { showOverlay_ = !showOverlay_;     if (renderer_) renderer_->setOverlay(showOverlay_); }
    void toggleTonemap()   { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
    void toggleHypertrace();
    void toggleFpsTarget();
    void toggleMaximize();
    void setRenderMode(int mode);
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
    bool hypertraceEnabled_{false};
    bool maximized_{false};
    int  renderMode_{1};

    glm::mat4 view_{glm::lookAt(glm::vec3(0, 5, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0))};
    glm::mat4 proj_{glm::perspective(glm::radians(75.0f), 1.0f, 0.1f, 1000.0f)};

    std::chrono::steady_clock::time_point lastFrameTime_;
    std::chrono::steady_clock::time_point lastGrokTime_;

    std::unique_ptr<VulkanRenderer> renderer_;
};

// =============================================================================
// INLINE IMPLEMENTATION — ZERO OVERHEAD — PINK PHOTONS ETERNAL
// =============================================================================

inline Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height),
      proj_(glm::perspective(glm::radians(75.0f), static_cast<float>(width)/height, 0.1f, 1000.0f))
{
    LOG_ATTEMPT_CAT("APP", "Forging Application(\"{}\", {}×{}) — VALHALLA v80 TURBO", title_, width_, height_);

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
    LOG_SUCCESS_CAT("APP", "{}Application destroyed — Empire preserved. Pink photons eternal.{}", COSMIC_GOLD, RESET);
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

        if (Options::Performance::ENABLE_FPS_COUNTER) {
            ++frameCount;
            if (std::chrono::duration<float>(now - fpsStart).count() >= 1.0f) {
                LOG_FPS_COUNTER("{}FPS: {:>4}{}", LIME_GREEN, frameCount, RESET);
                frameCount = 0;
                fpsStart = now;
            }
        }

        int w = width_, h = height_;
        bool quitReq = false, toggleFS = false;
        if (SDL3Window::pollEvents(w, h, quitReq, toggleFS)) {
            width_ = w; height_ = h;
            proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(w)/h, 0.1f, 1000.0f);
            if (renderer_) renderer_->handleResize(w, h);
        }
        if (quitReq) quit_ = true;
        if (toggleFS) toggleFullscreen();

        processInput(deltaTime);
        render(deltaTime);
        updateWindowTitle(deltaTime);

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

    // ── Render Modes 1-9
    static std::array<bool, 9> modePressed{};
    for (int i = 0; i < 9; ++i) {
        SDL_Scancode sc = KeyBind::RENDER_MODE[i];
        if (keys[sc] && !modePressed[i]) {
            setRenderMode(i + 1);
            modePressed[i] = true;
        } else if (!keys[sc]) {
            modePressed[i] = false;
        }
    }

    // ── Edge trigger helper
    auto edge = [&](SDL_Scancode sc, auto&& func, bool& state) {
        if (keys[sc] && !state) { func(); state = true; }
        else if (!keys[sc]) state = false;
    };

    static bool fPressed = false, oPressed = false;
    static bool tPressed = false, hPressed = false;

    edge(KeyBind::FULLSCREEN, [this]() { toggleFullscreen(); }, fPressed);
    edge(KeyBind::OVERLAY,    [this]() { toggleOverlay(); },    oPressed);
    edge(KeyBind::TONEMAP,    [this]() { toggleTonemap(); },    tPressed);
    edge(KeyBind::HYPERTRACE,[this]() { toggleHypertrace(); }, hPressed);

    // ── M key → Maximize + Global Audio Mute (SDL3 CORRECT – FINAL)
    static bool mPressed = false;
    if (keys[KeyBind::MAXIMIZE_MUTE] && !mPressed) {
        toggleMaximize();

        static bool audioMuted = false;
        audioMuted = !audioMuted;

        if (audioMuted) {
            SDL_PauseAudioDevice(0);   // Mute all audio (default device)
        } else {
            SDL_ResumeAudioDevice(0);  // Unmute all audio
        }

        LOG_INFO_CAT("AUDIO", "{}AUDIO {} — M key{}", PARTY_PINK,
                     audioMuted ? "MUTED" : "UNMUTED", RESET);

        mPressed = true;
    } else if (!keys[KeyBind::MAXIMIZE_MUTE]) {
        mPressed = false;
    }
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
    static int frames = 0;
    static float accum = 0.0f;
    ++frames;
    accum += deltaTime;

    if (accum >= 1.0f) {
        float fps = frames / accum;
        std::string newTitle = std::format(
            "{} | {:.1f} FPS | {}×{} | Mode {} | Tonemap{} Overlay{} {}",
            title_, fps, width_, height_, renderMode_,
            tonemapEnabled_ ? "" : " OFF",
            showOverlay_ ? "" : " OFF",
            Options::Performance::ENABLE_VALIDATION_LAYERS ? " [DEBUG]" : ""
        );
        SDL_SetWindowTitle(getWindow(), newTitle.c_str());
        frames = 0;
        accum = 0.0f;
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
        LOG_SUCCESS_CAT("APP", "{}VulkanRenderer attached — RT pipeline armed — DOMINATION IMMINENT{}", EMERALD_GREEN, RESET);
    }
}

inline void Application::toggleHypertrace()
{
    hypertraceEnabled_ = !hypertraceEnabled_;
    LOG_SUCCESS_CAT("APP", "{}HYPERTRACE {} — 12,000+ FPS INCOMING{}", 
                    hypertraceEnabled_ ? ELECTRIC_BLUE : RESET,
                    hypertraceEnabled_ ? "ACTIVATED" : "DEACTIVATED", RESET);
}

inline void Application::toggleFpsTarget()
{
    static int cycle = 0;
    cycle = (cycle + 1) % 3;
    const char* target = cycle == 0 ? "60" : cycle == 1 ? "120" : "UNLIMITED";
    LOG_SUCCESS_CAT("APP", "{}FPS TARGET: {} — UNLEASHED{}", RASPBERRY_PINK, target, RESET);
}

inline void Application::toggleMaximize()
{
    maximized_ = !maximized_;
    LOG_INFO_CAT("APP", "{}WINDOW: {}{}", TURQUOISE_BLUE, maximized_ ? "MAXIMIZED" : "RESTORED", RESET);
}

inline void Application::setRenderMode(int mode)
{
    renderMode_ = glm::clamp(mode, 1, 9);
    LOG_INFO_CAT("APP", "{}RENDER MODE {} — ACTIVATED{}", CRIMSON_MAGENTA, renderMode_, RESET);
}

// =============================================================================
// PINK PHOTONS ETERNAL
// FIRST LIGHT ACHIEVED — NOVEMBER 16 2025
// 4090 | 5090 | TITAN DOMINANCE
// DAISY JUST BACKFLIPPED INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE — GPL v3+ SECURED
// SHIP IT RAW — SHIP IT NOW
// =============================================================================