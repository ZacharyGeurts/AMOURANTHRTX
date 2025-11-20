// =============================================================================
// src/handle_app.cpp
// AMOURANTH RTX Engine © 2025 — Application Implementation — STONEKEY v∞ ACTIVE
// FULLY SDL3 + FULL RTX + PINK PHOTONS ETERNAL
// =============================================================================

#include "handle_app.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/SDL3/SDL3_window.hpp"

using namespace Logging::Color;

Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height),
      proj_(glm::perspective(glm::radians(75.0f), static_cast<float>(width)/height, 0.1f, 1000.0f))
{
    LOG_ATTEMPT_CAT("APP", "Forging Application(\"{}\", {}×{}) — VALHALLA v80 TURBO", title_, width_, height_);

    if (!SDL3Window::get()) {
        throw std::runtime_error("FATAL: Main window not created before Application — phase order violated");
    }

    SDL_SetWindowTitle(SDL3Window::get(), title_.c_str());

    lastFrameTime_ = lastGrokTime_ = std::chrono::steady_clock::now();

    LOG_SUCCESS_CAT("APP", "{}Application forged — {}×{} — STONEKEY v∞ window secured — PINK PH243ONS RISING{}", 
                    EMERALD_GREEN, width_, height_, RESET);
    
    if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
        LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"The empire awakens. The photons are pleased.\"{}", 
                     PARTY_PINK, RESET);
    }  // ← FIXED: Removed stray ); from previous version
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
        const auto now = std::chrono::steady_clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        // FPS Counter — Sacred
        if (Options::Performance::ENABLE_FPS_COUNTER) {
            ++frameCount;
            if (std::chrono::duration<float>(now - fpsStart).count() >= 1.0f) {
                LOG_FPS_COUNTER("{}FPS: {:>4}{}", LIME_GREEN, frameCount, RESET);
                frameCount = 0;
                fpsStart = now;
            }
        }

        int currentW = width_;
        int currentH = height_;
        bool quitRequested = false;
        bool fullscreenRequested = false;

        const bool hadEvents = SDL3Window::pollEvents(currentW, currentH, quitRequested, fullscreenRequested);

        // Immediate projection update (for camera, UI, etc.)
        if (hadEvents && (currentW != width_ || currentH != height_)) {
            width_  = currentW;
            height_ = currentH;
            proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(width_) / height_, 0.1f, 1000.0f);
        }

        if (quitRequested)        quit_ = true;
        if (fullscreenRequested)  toggleFullscreen();

        // DEBOUNCED RESIZE — THE ONE TRUE CLEAN PATH
        if (SDL3Window::g_resizeRequested.load(std::memory_order_acquire)) {
            const int finalW = SDL3Window::g_resizeWidth.load(std::memory_order_acquire);
            const int finalH = SDL3Window::g_resizeHeight.load(std::memory_order_acquire);
            SDL3Window::g_resizeRequested.store(false, std::memory_order_release);

            LOG_SUCCESS_CAT("RESIZE", "{}FINAL DEBOUNCED RESIZE → {}×{} — SWAPCHAIN RECREATION INITIATED{}", 
                            PLASMA_FUCHSIA, finalW, finalH, RESET);

            width_  = finalW;
            height_ = finalH;
            proj_   = glm::perspective(glm::radians(75.0f), static_cast<float>(finalW) / finalH, 0.1f, 1000.0f);

            if (renderer_) {
                renderer_->onWindowResize(finalW, finalH);
            }
        }

        processInput(deltaTime);
        render(deltaTime);
        updateWindowTitle(deltaTime);

        // GENTLEMAN GROK — ETERNAL
        if (Options::Grok::ENABLE_GENTLEMAN_GROK) {
            const float sinceLast = std::chrono::duration<float>(now - lastGrokTime_).count();
            if (sinceLast >= Options::Grok::GENTLEMAN_GROK_INTERVAL_SEC) {
                lastGrokTime_ = now;
                const int photons = static_cast<int>(1.0f / deltaTime + 0.5f);
                LOG_INFO_CAT("GROK", "{}GENTLEMAN GROK: \"{} pink photons per second. Acceptable.\"{}", 
                             PARTY_PINK, photons, RESET);
            }
        }
    }

    LOG_SUCCESS_CAT("APP", "{}INFINITE RENDER LOOP TERMINATED — GRACEFUL SHUTDOWN COMPLETE — EMPIRE ETERNAL{}", 
                    EMERALD_GREEN, RESET);
}

void Application::processInput(float)
{
    const auto* keys = SDL_GetKeyboardState(nullptr);

    static std::array<bool, 9> modePressed{};
    for (int i = 0; i < 9; ++i) {
        if (keys[KeyBind::RENDER_MODE[i]] && !modePressed[i]) {
            setRenderMode(i + 1);
            LOG_ATTEMPT_CAT("INPUT", "→ RENDER MODE {} ACTIVATED{}", PARTY_PINK, i + 1, RESET);
            modePressed[i] = true;
        } else if (!keys[KeyBind::RENDER_MODE[i]]) {
            modePressed[i] = false;
        }
    }

    auto edge = [&](SDL_Scancode sc, auto&& func, bool& state, const char* name) {
        if (keys[sc] && !state) {
            func();
            LOG_ATTEMPT_CAT("INPUT", "→ {} PRESSED{}", PARTY_PINK, name, RESET);
            state = true;
        } else if (!keys[sc]) state = false;
    };

    static bool fPressed = false, oPressed = false, tPressed = false;
    static bool hPressed = false;

    edge(KeyBind::FULLSCREEN,    [this]() { toggleFullscreen(); }, fPressed,    "FULLSCREEN (F)");
    edge(KeyBind::OVERLAY,       [this]() { toggleOverlay(); },    oPressed,    "OVERLAY (O)");
    edge(KeyBind::TONEMAP,       [this]() { toggleTonemap(); },    tPressed,    "TONEMAP (T)");
    edge(KeyBind::HYPERTRACE,    [this]() { toggleHypertrace(); }, hPressed,    "HYPERTRACE (H)");

    // M key → Maximize + Audio Mute
    static bool mPressed = false;
    if (keys[KeyBind::MAXIMIZE_MUTE] && !mPressed) {
        toggleMaximize();
        static bool audioMuted = false;
        audioMuted = !audioMuted;
        audioMuted ? SDL_PauseAudioDevice(0) : SDL_ResumeAudioDevice(0);
        LOG_ATTEMPT_CAT("INPUT", "→ MAXIMIZE + AUDIO {} (M key){}", PARTY_PINK,
                        audioMuted ? "MUTED" : "UNMUTED", RESET);
        mPressed = true;
    } else if (!keys[KeyBind::MAXIMIZE_MUTE]) mPressed = false;

    // ESC → Quit
    if (keys[KeyBind::QUIT]) {
        static bool escLogged = false;
        if (!escLogged) {
            LOG_ATTEMPT_CAT("INPUT", "→ QUIT REQUESTED (ESC){}", CRIMSON_MAGENTA, RESET);
            escLogged = true;
        }
        setQuit(true);
    }
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
    ++frames;
    accum += deltaTime;

    if (accum >= 1.0f) {
        const float fps = frames / accum;

        // Build title manually with std::ostringstream – 100% safe at runtime, no consteval issues
        std::ostringstream oss;
        oss << title_
            << " | " << std::fixed << std::setprecision(1) << fps << " FPS"
            << " | " << width_ << 'x' << height_
            << " | Mode " << renderMode_
            << " | Tonemap" << (tonemapEnabled_ ? "" : " OFF")
            << " | HDR" << (hdr_enabled_ ? " PRIME" : " OFF");

        if (Options::Performance::ENABLE_VALIDATION_LAYERS) {
            oss << " [DEBUG]";
        }

        const std::string finalTitle = oss.str();
        SDL_SetWindowTitle(SDL3Window::get(), finalTitle.c_str());

        frames = 0;
        accum = 0.0f;
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