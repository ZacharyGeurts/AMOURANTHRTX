// src/handle_app.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — Application Implementation — SPLIT & PURIFIED
// =============================================================================

#include "handle_app.hpp"
#include "engine/Vulkan/Compositor.hpp"
#include "engine/Vulkan/HDR_surface.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"

using namespace Logging::Color;

Application::Application(const std::string& title, int width, int height)
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

Application::~Application()
{
    LOG_TRACE_CAT("APP", "Application::~Application() — beginning graceful shutdown");
    renderer_.reset();
    LOG_SUCCESS_CAT("APP", "{}Application destroyed — Empire preserved. Pink photons eternal.{}", COSMIC_GOLD, RESET);
}

void Application::run()
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

void Application::processInput(float)
{
    const auto* keys = SDL_GetKeyboardState(nullptr);

    // Render Modes 1-9
    static std::array<bool, 9> modePressed{};
    for (int i = 0; i < 9; ++i) {
        if (keys[KeyBind::RENDER_MODE[i]] && !modePressed[i]) {
            setRenderMode(i + 1);
            LOG_ATTEMPT_CAT("INPUT", "{}→ RENDER MODE {} ACTIVATED{}", PARTY_PINK, i + 1, RESET);
            modePressed[i] = true;
        } else if (!keys[KeyBind::RENDER_MODE[i]]) {
            modePressed[i] = false;
        }
    }

    auto edge = [&](SDL_Scancode sc, auto&& func, bool& state, const char* name) {
        if (keys[sc] && !state) {
            func();
            LOG_ATTEMPT_CAT("INPUT", "{}→ {} PRESSED{}", PARTY_PINK, name, RESET);
            state = true;
        } else if (!keys[sc]) state = false;
    };

    static bool fPressed = false, oPressed = false, tPressed = false;
    static bool hPressed = false, hdrPressed = false;

    edge(KeyBind::FULLSCREEN,    [this]() { toggleFullscreen(); }, fPressed,    "FULLSCREEN (F)");
    edge(KeyBind::OVERLAY,       [this]() { toggleOverlay(); },    oPressed,    "OVERLAY (O)");
    edge(KeyBind::TONEMAP,       [this]() { toggleTonemap(); },    tPressed,    "TONEMAP (T)");
    edge(KeyBind::HYPERTRACE,    [this]() { toggleHypertrace(); }, hPressed,    "HYPERTRACE (H)");
    edge(KeyBind::HDR_TOGGLE,    [this]() { toggleHDR(); },        hdrPressed,  "HDR PRIME (F12)");

    // M key → Maximize + Audio Mute
    static bool mPressed = false;
    if (keys[KeyBind::MAXIMIZE_MUTE] && !mPressed) {
        toggleMaximize();
        static bool audioMuted = false;
        audioMuted = !audioMuted;
        audioMuted ? SDL_PauseAudioDevice(0) : SDL_ResumeAudioDevice(0);
        LOG_ATTEMPT_CAT("INPUT", "{}→ MAXIMIZE + AUDIO {} (M key){}", PARTY_PINK,
                        audioMuted ? "MUTED" : "UNMUTED", RESET);
        mPressed = true;
    } else if (!keys[KeyBind::MAXIMIZE_MUTE]) mPressed = false;

    // ESC → Quit
    if (keys[KeyBind::QUIT]) {
        static bool escLogged = false;
        if (!escLogged) {
            LOG_ATTEMPT_CAT("INPUT", "{}→ QUIT REQUESTED (ESC){}", CRIMSON_MAGENTA, RESET);
            escLogged = true;
        }
        setQuit(true);
    }
}

void Application::toggleHDR() noexcept
{
    hdr_enabled_ = !hdr_enabled_;

    if (hdr_enabled_) {
        if (HDRCompositor::try_enable_hdr() && HDRSurface::g_hdr_surface()) {
            HDRSurface::g_hdr_surface()->reprobe();
            LOG_SUCCESS_CAT("APP", "{}HDR PRIME ACTIVATED — 10-bit Glory Locked | Pink Photons Rise{}", PARTY_PINK, RESET);
        } else {
            LOG_WARN_CAT("APP", "HDR Prime Failed — Peasant Fallback (8-bit SDR)");
            hdr_enabled_ = false;
        }
    } else {
        RTX::g_ctx().hdr_format = VK_FORMAT_B8G8R8A8_UNORM;
        RTX::g_ctx().hdr_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        LOG_INFO_CAT("APP", "{}Peasant Mode: 8-bit SDR Mercy Granted{}", LIME_GREEN, RESET);
    }

    // EVERYONE ELSE USES SWAPCHAIN.views(), SWAPCHAIN.extent(), etc.
    // SO WE DO TOO — EXCEPT HERE, WHERE WE CALL A METHOD
    // → ONLY HERE WE USE ->
    SWAPCHAIN.recreate(width_, height_);
}

void Application::render(float deltaTime)
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

void Application::updateWindowTitle(float deltaTime)
{
    static int frames = 0;
    static float accum = 0.0f;
    ++frames; accum += deltaTime;

    if (accum >= 1.0f) {
        float fps = frames / accum;
        std::string title = std::format(
            "{} | {:.1f} FPS | {}×{} | Mode {} | Tonemap{} Overlay{} HDR{} {}",
            title_, fps, width_, height_, renderMode_,
            tonemapEnabled_ ? "" : " OFF",
            showOverlay_ ? "" : " OFF",
            hdr_enabled_ ? " PRIME" : " OFF",
            Options::Performance::ENABLE_VALIDATION_LAYERS ? " [DEBUG]" : ""
        );
        SDL_SetWindowTitle(getWindow(), title.c_str());
        frames = 0; accum = 0.0f;
    }
}

void Application::toggleFullscreen()          { SDL3Window::toggleFullscreen(); }
void Application::toggleOverlay()             { showOverlay_ = !showOverlay_; if (renderer_) renderer_->setOverlay(showOverlay_); }
void Application::toggleTonemap()             { tonemapEnabled_ = !tonemapEnabled_; if (renderer_) renderer_->setTonemap(tonemapEnabled_); }
void Application::toggleHypertrace()          { hypertraceEnabled_ = !hypertraceEnabled_; }
void Application::toggleFpsTarget()           { /* impl */ }
void Application::toggleMaximize()            { maximized_ = !maximized_; }
void Application::setRenderMode(int mode)     { renderMode_ = glm::clamp(mode, 1, 9); }
void Application::setRenderer(std::unique_ptr<VulkanRenderer> r)
{
    renderer_ = std::move(r);
    if (renderer_) {
        renderer_->setTonemap(tonemapEnabled_);
        renderer_->setOverlay(showOverlay_);
    }
}