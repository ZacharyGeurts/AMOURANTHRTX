// src/handle_app.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: T = tonemap | O = overlay | 1-9 = modes | H = HYPERTRACE | F = FPS TARGET
// FIXED: SDL3 event system — all window events via type range
// FIXED: C++23 turbo — <format>, <chrono>, lambda captures, std::move everywhere
// FIXED: char_traits / iterator errors → #include <string> + <iterator> explicitly (GCC 13 bug workaround)
// GROK PROTIP: "Never raw loop. Use ranges when possible."

#include "handle_app.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <format>
#include <iterator>      // ← FIX: explicit for GCC 13 char_traits
#include <string>        // ← FIX: explicit char_traits

#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Dispose.hpp"
#include "engine/utils.hpp"
#include "engine/core.hpp"

using namespace Logging::Color;

// =============================================================================
//  MESH LOADER — OBJ with fallback triangle (C++23 std::ranges ready)
// =============================================================================
void loadMesh(const std::string& filename, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Mesh", "{}Failed to open {} — using fallback triangle{}", 
                     CRIMSON_MAGENTA, filename, RESET);
        vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        indices = {0, 1, 2};
        return;
    }

    std::vector<glm::vec3> tempVertices;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string type;
        if (!(iss >> type)) continue;

        if (type == "v") {
            glm::vec3 v;
            if (iss >> v.x >> v.y >> v.z) {
                tempVertices.push_back(v);
            }
        } else if (type == "f") {
            std::string token;
            std::vector<uint32_t> face;
            while (iss >> token) {
                std::istringstream tss(token);
                uint32_t idx;
                char discard;
                if (tss >> idx) {
                    face.push_back(idx - 1);
                    tss >> discard; // skip /
                }
            }
            if (face.size() >= 3) {
                for (size_t i = 2; i < face.size(); ++i) {
                    indices.insert(indices.end(), {face[0], face[i-1], face[i]});
                }
            }
        }
    }

    if (tempVertices.size() < 3 || indices.size() < 3 || indices.size() % 3 != 0) {
        LOG_WARN_CAT("Mesh", "{}Invalid .obj → fallback triangle{}", OCEAN_TEAL, RESET);
        vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        indices = {0, 1, 2};
    } else {
        vertices = std::move(tempVertices);
        LOG_INFO_CAT("Mesh", "{}Loaded {}: {} verts, {} tris{}", 
                     EMERALD_GREEN, filename, vertices.size(), indices.size() / 3, RESET);
    }
}

// =============================================================================
//  APPLICATION CTOR
// =============================================================================
Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1), quit_(false),
      sdl_(std::make_unique<SDL3Initializer::SDL3Initializer>(title_, width_, height_)),
      camera_(std::make_unique<VulkanRTX::PerspectiveCamera>(60.0f, static_cast<float>(width) / height)),
      inputHandler_(nullptr), isFullscreen_(false), isMaximized_(false),
      lastFrameTime_(std::chrono::steady_clock::now()),
      showOverlay_(true), tonemapEnabled_(false)
{
    LOG_INFO_CAT("Application", "{}INIT [{}x{}]{}", OCEAN_TEAL, width, height, RESET); 
    camera_->setUserData(this);
    initializeInput();
}

// =============================================================================
//  DESTRUCTOR
// =============================================================================
Application::~Application() {
    LOG_INFO_CAT("Application", "{}SHUTDOWN{}", CRIMSON_MAGENTA, RESET);
    Dispose::quitSDL();
}

// =============================================================================
//  RENDERER OWNERSHIP
// =============================================================================
void Application::setRenderer(std::unique_ptr<VulkanRTX::VulkanRenderer> renderer) {
    renderer_ = std::move(renderer);

    loadMesh("assets/models/scene.obj", vertices_, indices_);
    renderer_->getBufferManager()->uploadMesh(vertices_.data(), vertices_.size(), indices_.data(), indices_.size());

    auto ctx = renderer_->getContext();
    renderer_->getRTX().updateRTX(
        ctx->physicalDevice,
        ctx->commandPool,
        ctx->graphicsQueue,
        renderer_->getBufferManager()->getGeometries(),
        renderer_->getBufferManager()->getDimensionStates()
    );

    renderer_->setRenderMode(mode_);
    updateWindowTitle();

    camera_->setUserData(this);
    LOG_INFO_CAT("CAMERA", "{}camera_->setUserData(this) @ {:p}{}", 
                 EMERALD_GREEN, static_cast<void*>(this), RESET);

    LOG_INFO_CAT("Application", "{}MESH LOADED | 1-9=mode | H=HYPERTRACE | T=tonemap | O=overlay | F=FPS TARGET{}", 
                 EMERALD_GREEN, RESET);
}

VulkanRTX::VulkanRenderer* Application::getRenderer() const {
    if (!renderer_) {
        LOG_ERROR_CAT("APP", "{}getRenderer(): null — call setRenderer() first{}", CRIMSON_MAGENTA, RESET);
        return nullptr;
    }
    return renderer_.get();
}

// =============================================================================
//  INPUT INITIALIZATION (C++23 lambdas with [this, *] capture)
// =============================================================================
void Application::initializeInput() {
    inputHandler_ = std::make_unique<HandleInput>(*camera_);
    inputHandler_->setCallbacks(
        [this](const SDL_KeyboardEvent& key) {
            if (key.type == SDL_EVENT_KEY_DOWN) {
                switch (key.key) {
                    case SDLK_1: setRenderMode(1); LOG_INFO_CAT("INPUT", "{}MODE 1{}", EMERALD_GREEN, RESET); break;
                    case SDLK_2: setRenderMode(2); LOG_INFO_CAT("INPUT", "{}MODE 2{}", EMERALD_GREEN, RESET); break;
                    case SDLK_3: setRenderMode(3); LOG_INFO_CAT("INPUT", "{}MODE 3{}", EMERALD_GREEN, RESET); break;
                    case SDLK_4: setRenderMode(4); LOG_INFO_CAT("INPUT", "{}MODE 4{}", EMERALD_GREEN, RESET); break;
                    case SDLK_5: setRenderMode(5); LOG_INFO_CAT("INPUT", "{}MODE 5{}", EMERALD_GREEN, RESET); break;
                    case SDLK_6: setRenderMode(6); LOG_INFO_CAT("INPUT", "{}MODE 6{}", EMERALD_GREEN, RESET); break;
                    case SDLK_7: setRenderMode(7); LOG_INFO_CAT("INPUT", "{}MODE 7{}", EMERALD_GREEN, RESET); break;
                    case SDLK_8: setRenderMode(8); LOG_INFO_CAT("INPUT", "{}MODE 8{}", EMERALD_GREEN, RESET); break;
                    case SDLK_9: setRenderMode(9); LOG_INFO_CAT("INPUT", "{}MODE 9{}", EMERALD_GREEN, RESET); break;

                    case SDLK_T: toggleTonemap(); break;
                    case SDLK_O: toggleOverlay(); break;
                    case SDLK_H: toggleHypertrace(); break;
                    case SDLK_F: toggleFpsTarget(); break;

                    case SDLK_F11: toggleFullscreen(); break;
                    case SDLK_M:   toggleMaximize();   break;

                    default: inputHandler_->defaultKeyboardHandler(key); break;
                }
            } else {
                inputHandler_->defaultKeyboardHandler(key);
            }
        },
        [this](const SDL_MouseButtonEvent& mb) { inputHandler_->defaultMouseButtonHandler(mb); },
        [this](const SDL_MouseMotionEvent& mm) { inputHandler_->defaultMouseMotionHandler(mm); },
        [this](const SDL_MouseWheelEvent& mw) { inputHandler_->defaultMouseWheelHandler(mw); },
        [this](const SDL_TextInputEvent& ti) { inputHandler_->defaultTextInputHandler(ti); },
        [this](const SDL_TouchFingerEvent& tf) { inputHandler_->defaultTouchHandler(tf); },
        [this](const SDL_GamepadButtonEvent& gb) { inputHandler_->defaultGamepadButtonHandler(gb); },
        [this](const SDL_GamepadAxisEvent& ga) { inputHandler_->defaultGamepadAxisHandler(ga); },
        [this](bool connected, SDL_JoystickID id, SDL_Gamepad* pad) { inputHandler_->defaultGamepadConnectHandler(connected, id, pad); }
    );
}

// =============================================================================
//  TOGGLES
// =============================================================================
void Application::toggleFpsTarget() {
    if (auto* r = getRenderer()) {
        r->toggleFpsTarget();
        LOG_INFO_CAT("INPUT", "{}FPS TARGET: {} FPS{}", 
                     PEACHES_AND_CREAM,
                     r->getFpsTarget() == VulkanRTX::VulkanRenderer::FpsTarget::FPS_60 ? 60 : 120,
                     RESET);
    }
    updateWindowTitle();
}

void Application::toggleHypertrace() {
    if (auto* r = getRenderer()) {
        r->toggleHypertrace();
        LOG_INFO_CAT("INPUT", "{}HYPERTRACE {}{}", 
                     CRIMSON_MAGENTA,
                     r->getRTX().isHypertraceEnabled() ? "ON" : "OFF",
                     RESET);
    }
    updateWindowTitle();
}

void Application::toggleTonemap() {
    tonemapEnabled_ = !tonemapEnabled_;
    int target = tonemapEnabled_ ? 2 : mode_;
    if (auto* r = getRenderer()) r->setRenderMode(target);

    LOG_INFO_CAT("INPUT", "{}TONEMAP {} | MODE {}{}",
                 PEACHES_AND_CREAM,
                 tonemapEnabled_ ? "ON" : "OFF",
                 target,
                 RESET);
    updateWindowTitle();
}

void Application::toggleOverlay() {
    showOverlay_ = !showOverlay_;
    LOG_INFO_CAT("INPUT", "{}OVERLAY {}{}", OCEAN_TEAL, showOverlay_ ? "ON" : "OFF", RESET);
    updateWindowTitle();
}

// =============================================================================
//  WINDOW TITLE (std::format C++23)
// =============================================================================
void Application::updateWindowTitle() {
    std::string title = title_;

    if (showOverlay_) {
        title += std::format(" | Mode {}", mode_);
        if (tonemapEnabled_) title += " (TONEMAP)";
        if (auto* r = getRenderer(); r && r->getRTX().isHypertraceEnabled()) title += " (HYPERTRACE)";
        if (auto* r = getRenderer()) {
            title += std::format(" ({} FPS)", 
                r->getFpsTarget() == VulkanRTX::VulkanRenderer::FpsTarget::FPS_60 ? 60 : 120);
        }
        title += " | 1-9=mode | H=HYPERTRACE | T=tonemap | O=hide | F=FPS | F11=FS | M=MAX";
    }

    SDL_SetWindowTitle(sdl_->getWindow(), title.c_str());
}

// =============================================================================
//  MODE / FULLSCREEN / MAXIMIZE
// =============================================================================
void Application::setRenderMode(int mode) {
    mode_ = mode;
    int finalMode = tonemapEnabled_ ? 2 : mode_;
    if (auto* r = getRenderer()) r->setRenderMode(finalMode);
    LOG_INFO_CAT("Application", "{}Render mode → {}{}", EMERALD_GREEN, finalMode, tonemapEnabled_ ? " (tonemap)" : "");
    updateWindowTitle();
}

void Application::toggleFullscreen() {
    isFullscreen_ = !isFullscreen_;
    isMaximized_ = false;
    SDL_SetWindowFullscreen(sdl_->getWindow(), isFullscreen_);
    LOG_INFO_CAT("APP", "{}FULLSCREEN {}{}", isFullscreen_ ? EMERALD_GREEN : CRIMSON_MAGENTA, 
                 isFullscreen_ ? "ON" : "OFF", RESET);
    updateWindowTitle();
}

void Application::toggleMaximize() {
    if (isFullscreen_) return;
    isMaximized_ = !isMaximized_;
    if (isMaximized_) SDL_MaximizeWindow(sdl_->getWindow());
    else SDL_RestoreWindow(sdl_->getWindow());
    LOG_INFO_CAT("APP", "{}MAXIMIZE {}{}", isMaximized_ ? EMERALD_GREEN : OCEAN_TEAL, 
                 isMaximized_ ? "ON" : "RESTORED", RESET);
    updateWindowTitle();
}

// =============================================================================
//  RESIZE
// =============================================================================
void Application::handleResize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == width_ && height == height_)) return;
    if (SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED) return;

    LOG_INFO_CAT("APP", "{}RESIZE → {}x{}{}", BRIGHT_PINKISH_PURPLE, width, height, RESET);
    width_ = width; height_ = height;
    camera_->setAspectRatio(static_cast<float>(width_) / height_);

    if (auto* r = getRenderer()) {
        r->getSwapchainManager().recreateSwapchain(width_, height_);
        r->handleResize(width_, height_);
    }
    updateWindowTitle();
}

// =============================================================================
//  WINDOW EVENTS
// =============================================================================
void Application::handleWindowEvent(const SDL_WindowEvent& we) {
    switch (we.type) {
        case SDL_EVENT_WINDOW_MINIMIZED:   LOG_INFO_CAT("APP", "{}MINIMIZED{}", OCEAN_TEAL, RESET); break;
        case SDL_EVENT_WINDOW_RESTORED:    LOG_INFO_CAT("APP", "{}RESTORED{}", EMERALD_GREEN, RESET);
            { int w, h; SDL_GetWindowSize(sdl_->getWindow(), &w, &h);
              if (w != width_ || h != height_) handleResize(w, h); } break;
        case SDL_EVENT_WINDOW_MAXIMIZED:   if (!isFullscreen_) { isMaximized_ = true; updateWindowTitle(); } break;
        case SDL_EVENT_WINDOW_RESIZED:     if (!(SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED))
                                               handleResize(we.data1, we.data2); break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: quit_ = true; break;
        default: break;
    }
}

// =============================================================================
//  MAIN LOOP (C++23: structured bindings ready)
// =============================================================================
void Application::run() {
    while (!shouldQuit()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) { quit_ = true; break; }
            if (ev.type >= SDL_EVENT_WINDOW_FIRST && ev.type <= SDL_EVENT_WINDOW_LAST) {
                handleWindowEvent(ev.window);
            }
        }
        if (quit_) break;

        inputHandler_->handleInput(*this);
        render();
    }
}

// =============================================================================
//  RENDER
// =============================================================================
void Application::render() {
    if (!getRenderer() || !camera_) return;
    if (SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return;
    }

    auto now = std::chrono::steady_clock::now();
    float delta = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;

    camera_->update(delta);
    getRenderer()->renderFrame(*camera_, delta);
    updateWindowTitle();
}

bool Application::shouldQuit() const { return quit_; }