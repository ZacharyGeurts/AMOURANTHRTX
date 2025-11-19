// src/handle_app.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — Application Implementation — FULLY SDL3 FIXED
// =============================================================================

#include "handle_app.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/SDL3/SDL3_window.hpp"

using namespace Logging::Color;

// CRITICAL: DEFINE THE STATIC MEMBERS FROM THE HEADER
SDLWindowPtr SDL3Window::g_sdl_window = nullptr;
std::atomic<int>  SDL3Window::g_resizeWidth{0};
std::atomic<int>  SDL3Window::g_resizeHeight{0};
std::atomic<bool> SDL3Window::g_resizeRequested{false};

Application::Application(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height),
      proj_(glm::perspective(glm::radians(75.0f), static_cast<float>(width)/height, 0.1f, 1000.0f))
{
    LOG_ATTEMPT_CAT("APP", "Forging Application(\"{}\", {}×{}) — VALHALLA v80 TURBO", title_, width_, height_);

    // THE WINDOW WAS ALREADY CREATED IN PHASE 2 — DO NOT CREATE IT AGAIN
    // SDL3Window::create(...) ← HERESY — DELETED FOREVER

    if (!SDL3Window::get()) {
        throw std::runtime_error("FATAL: Main window not created before Application — phase order violated");
    }

    // Optional: update title if you really want (safe)
    SDL_SetWindowTitle(SDL3Window::get(), title_.c_str());

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

        // 1. Let the gentleman SDL3 system do its sacred work
        bool anyEventThisFrame = SDL3Window::pollEvents(w, h, quitReq, toggleFS);

        // 2. Update camera projection on any real size change (harmless, happens during drag)
        if (anyEventThisFrame && (w != width_ || h != height_)) {
            width_ = w;
            height_ = h;
            proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(w) / h, 0.1f, 1000.0f);
            // NO onWindowResize CALL HERE — TRAITOR EXECUTED
        }

        if (quitReq) quit_ = true;
        if (toggleFS) toggleFullscreen();

        // 3. THE ONLY PLACE WE EVER RECREATE SWAPCHAIN — DEBOUNCED, ATOMIC, CIVILIZED
        if (SDL3Window::g_resizeRequested.load()) {
            int finalW = SDL3Window::g_resizeWidth.load();
            int finalH = SDL3Window::g_resizeHeight.load();
            SDL3Window::g_resizeRequested.store(false);

            LOG_SUCCESS_CAT("RESIZE", "FINAL DEBOUNCED RESIZE → {}×{} — EXECUTING CLEAN RECREATE", finalW, finalH);
            
            width_ = finalW;
            height_ = finalH;
            proj_ = glm::perspective(glm::radians(75.0f), static_cast<float>(finalW) / finalH, 0.1f, 1000.0f);
            
            if (renderer_) renderer_->onWindowResize(finalW, finalH);
        }

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
    static bool hPressed = false;

    edge(KeyBind::FULLSCREEN,    [this]() { toggleFullscreen(); }, fPressed,    "FULLSCREEN (F)");
    edge(KeyBind::OVERLAY,       [this]() { toggleOverlay(); },    oPressed,    "OVERLAY (O)");
    edge(KeyBind::TONEMAP,       [this]() { toggleTonemap(); },    tPressed,    "TONEMAP (T)");
    edge(KeyBind::HYPERTRACE,    [this]() { toggleHypertrace(); }, hPressed,    "HYPERTRACE (H)");

    // =============================================================================
    // ~ / ` KEY → TOGGLE FULL IMGUI DEBUG CONSOLE (GOD MODE)
    // =============================================================================
    static bool imguiConsolePressed = false;
    if (keys[KeyBind::IMGUI_CONSOLE] && !imguiConsolePressed) {
        showImGuiDebugConsole_ = !showImGuiDebugConsole_;

        // SDL3 FIX: Use true/false + correct getter
        SDL_SetWindowRelativeMouseMode(getWindow(),
            showImGuiDebugConsole_ ? false : true);

        LOG_ATTEMPT_CAT("IMGUI", "{}IMGUI DEBUG CONSOLE {} — PRESS ~ AGAIN TO CLOSE{}", 
                        PARTY_PINK,
                        showImGuiDebugConsole_ ? "SUMMONED" : "BANISHED",
                        RESET);

        imguiConsolePressed = true;
    } else if (!keys[KeyBind::IMGUI_CONSOLE]) {
        imguiConsolePressed = false;
    }

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

    // =============================================================================
    // IMGUI DEBUG CONSOLE — SUMMONED BY THE `~` KEY
    // =============================================================================
    if (showImGuiDebugConsole_ && Options::Performance::ENABLE_IMGUI) {
        ImGui::ShowDemoWindow(&showImGuiDebugConsole_);

        ImGui::Begin("AMOURANTH RTX — EMPIRE CONSOLE v80", &showImGuiDebugConsole_,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.8f, 1.0f), "PINK PHOTONS ETERNAL — NOVEMBER 17, 2025");
        ImGui::Separator();

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Resolution: %dx%d", width_, height_);
        ImGui::Text("Render Mode: %d", renderMode_);
        ImGui::Text("HDR: %s", hdr_enabled_ ? "PRIME (10-bit)" : "OFF (8-bit peasant)");
        ImGui::Text("Tonemap: %s", tonemapEnabled_ ? "ON" : "OFF");
        ImGui::Text("Overlay: %s", showOverlay_ ? "ON" : "OFF");

        if (ImGui::Button("Recompile All Shaders")) {
            // Call your shader hot-reload here
        }
        ImGui::SameLine();
        if (ImGui::Button("Force Crash (testing)")) *(volatile int*)0 = 0;

        ImGui::End();
    }
}

void Application::updateWindowTitle(float deltaTime)
{
    static int frames = 0;
    static float accum = 0.0f;
    ++frames; accum += deltaTime;

    if (accum >= 1.0f) {
        float fps = frames / accum;
        std::string title = std::format(
            "{} | {:.1f} FPS | {}×{} | Mode {} | Tonemap{} Overlay{} HDR{} {}{}",
            title_, fps, width_, height_, renderMode_,
            tonemapEnabled_ ? "" : " OFF",
            showOverlay_ ? "" : " OFF",
            hdr_enabled_ ? " PRIME" : " OFF",
            showImGuiDebugConsole_ ? " [IMGUI CONSOLE]" : "",
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