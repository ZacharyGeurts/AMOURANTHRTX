// src/handle_app.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: T = tonemap | O = overlay | 1-9 = modes | H = HYPERTRACE | F = FPS TARGET
// FIXED: SDL3 event system — SDL_EVENT_WINDOW is NOT a valid enum
// FIXED: Use SDL_EVENT_WINDOW_MINIMIZED etc. directly in main loop
// FIXED: No separate SDL_EVENT_WINDOW type — all window events are SDL_EVENT_WINDOW_*
// FIXED: -Wswitch warnings suppressed with default case

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
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Dispose.hpp"
#include "engine/utils.hpp"
#include "engine/core.hpp"

using namespace Logging::Color;

// =============================================================================
//  MESH LOADER — OBJ with fallback triangle
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
                if (tss >> idx) face.push_back(idx - 1);
            }
            if (face.size() >= 3) {
                for (size_t i = 2; i < face.size(); ++i) {
                    indices.insert(indices.end(), {face[0], face[i-1], face[i]});
                }
            }
        }
    }
    file.close();

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
//  DESTRUCTOR — RAII cleanup
// =============================================================================
Application::~Application() {
    LOG_INFO_CAT("Application", "{}SHUTDOWN{}", CRIMSON_MAGENTA, RESET);
    Dispose::quitSDL();
}

// =============================================================================
//  RENDERER OWNERSHIP — setRenderer()
// =============================================================================
void Application::setRenderer(std::unique_ptr<VulkanRTX::VulkanRenderer> renderer) {
    renderer_ = std::move(renderer);

    // Load mesh
    loadMesh("assets/models/scene.obj", vertices_, indices_);
    renderer_->getBufferManager()->uploadMesh(vertices_.data(), vertices_.size(), indices_.data(), indices_.size());

    auto ctx = renderer_->getContext();

    // Async RTX update
    renderer_->getRTX().updateRTX(
        ctx->physicalDevice,
        ctx->commandPool,
        ctx->graphicsQueue,
        renderer_->getBufferManager()->getGeometries(),
        renderer_->getBufferManager()->getDimensionStates()
    );

    renderer_->setRenderMode(mode_);
    updateWindowTitle();

    // CRITICAL: Set camera.userData_ AFTER renderer is valid
    camera_->setUserData(this);
    LOG_INFO_CAT("CAMERA", "{}camera_->setUserData(this) @ {:p}{}", 
                 EMERALD_GREEN, static_cast<void*>(this), RESET);

    LOG_INFO_CAT("Application", "{}MESH LOADED | 1-9=mode | H=HYPERTRACE | T=tonemap | O=overlay | F=FPS TARGET{}", 
                 EMERALD_GREEN, RESET);
}

// =============================================================================
//  PUBLIC: getRenderer() — SAFE ACCESS
// =============================================================================
VulkanRTX::VulkanRenderer* Application::getRenderer() const {
    if (!renderer_) {
        LOG_ERROR_CAT("APP", "{}getRenderer(): null — call setRenderer() first{}", CRIMSON_MAGENTA, RESET);
        return nullptr;
    }
    return renderer_.get();
}

// =============================================================================
//  INPUT INITIALIZATION
// =============================================================================
void Application::initializeInput() {
    inputHandler_ = std::make_unique<HandleInput>(*camera_);
    inputHandler_->setCallbacks(
        [this](const SDL_KeyboardEvent& key) {
            if (key.type == SDL_EVENT_KEY_DOWN) {
                switch (key.key) {
                    case SDLK_1: setRenderMode(1); LOG_INFO_CAT("INPUT", "{}KEY 1 → MODE 1{}", EMERALD_GREEN, RESET); break;
                    case SDLK_2: setRenderMode(2); LOG_INFO_CAT("INPUT", "{}KEY 2 → MODE 2{}", EMERALD_GREEN, RESET); break;
                    case SDLK_3: setRenderMode(3); LOG_INFO_CAT("INPUT", "{}KEY 3 → MODE 3{}", EMERALD_GREEN, RESET); break;
                    case SDLK_4: setRenderMode(4); LOG_INFO_CAT("INPUT", "{}KEY 4 → MODE 4{}", EMERALD_GREEN, RESET); break;
                    case SDLK_5: setRenderMode(5); LOG_INFO_CAT("INPUT", "{}KEY 5 → MODE 5{}", EMERALD_GREEN, RESET); break;
                    case SDLK_6: setRenderMode(6); LOG_INFO_CAT("INPUT", "{}KEY 6 → MODE 6{}", EMERALD_GREEN, RESET); break;
                    case SDLK_7: setRenderMode(7); LOG_INFO_CAT("INPUT", "{}KEY 7 → MODE 7{}", EMERALD_GREEN, RESET); break;
                    case SDLK_8: setRenderMode(8); LOG_INFO_CAT("INPUT", "{}KEY 8 → MODE 8{}", EMERALD_GREEN, RESET); break;
                    case SDLK_9: setRenderMode(9); LOG_INFO_CAT("INPUT", "{}KEY 9 → MODE 9{}", EMERALD_GREEN, RESET); break;

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
//  FPS TARGET TOGGLE – 'F' KEY
// =============================================================================
void Application::toggleFpsTarget() {
    if (auto* renderer = getRenderer()) {
        renderer->toggleFpsTarget();
        LOG_INFO_CAT("INPUT", "{}KEY F → FPS TARGET: {} FPS{}", 
                     PEACHES_AND_CREAM,
                     renderer->getFpsTarget() == VulkanRTX::VulkanRenderer::FpsTarget::FPS_60 ? 60 : 120,
                     RESET);
    }
    updateWindowTitle();
}

// =============================================================================
//  HYPERTRACE – toggle 12,000+ FPS mode
// =============================================================================
void Application::toggleHypertrace() {
    if (auto* renderer = getRenderer()) {
        renderer->toggleHypertrace();
        LOG_INFO_CAT("INPUT", "{}KEY H → HYPERTRACE {}{}", 
                     CRIMSON_MAGENTA,
                     renderer->getRTX().isHypertraceEnabled() ? "ENABLED" : "DISABLED",
                     RESET);
    }
    updateWindowTitle();
}

// =============================================================================
//  TONEMAP – independent flag, forces render mode 2 when active
// =============================================================================
void Application::toggleTonemap() {
    tonemapEnabled_ = !tonemapEnabled_;

    int targetMode = tonemapEnabled_ ? 2 : mode_;
    if (auto* renderer = getRenderer()) {
        renderer->setRenderMode(targetMode);
    }

    LOG_INFO_CAT("INPUT", "{}KEY T → TONEMAP {} | MODE {}{}",
                 PEACHES_AND_CREAM,
                 tonemapEnabled_ ? "ENABLED" : "DISABLED",
                 targetMode,
                 RESET);

    updateWindowTitle();
}

// =============================================================================
//  OVERLAY TOGGLE
// =============================================================================
void Application::toggleOverlay() {
    showOverlay_ = !showOverlay_;
    LOG_INFO_CAT("INPUT", "{}KEY O → OVERLAY {}{}", 
                 OCEAN_TEAL,
                 showOverlay_ ? "ON" : "OFF",
                 RESET);
    updateWindowTitle();
}

// =============================================================================
//  WINDOW TITLE — real-time state sync
// =============================================================================
void Application::updateWindowTitle() {
    std::string title = title_;

    if (showOverlay_) {
        title += " | Mode " + std::to_string(mode_);
        if (tonemapEnabled_) title += " (TONEMAP)";
        if (auto* renderer = getRenderer(); renderer && renderer->getRTX().isHypertraceEnabled()) {
            title += " (HYPERTRACE)";
        }
        
        if (auto* renderer = getRenderer()) {
            auto target = renderer->getFpsTarget();
            title += std::format(" ({} FPS)", target == VulkanRTX::VulkanRenderer::FpsTarget::FPS_60 ? 60 : 120);
        }

        title += " | 1-9=mode | H=HYPERTRACE | T=tonemap | O=hide | F=FPS | F11=FS | M=MAX";
    }

    SDL_SetWindowTitle(sdl_->getWindow(), title.c_str());
}

// =============================================================================
//  RENDER MODE
// =============================================================================
void Application::setRenderMode(int mode) {
    mode_ = mode;

    int finalMode = tonemapEnabled_ ? 2 : mode_;
    if (auto* renderer = getRenderer()) {
        renderer->setRenderMode(finalMode);
    }

    LOG_INFO_CAT("Application", "{}Render mode set to {}{}",
                 EMERALD_GREEN,
                 finalMode,
                 tonemapEnabled_ ? " (tonemap active)" : "");

    updateWindowTitle();
}

// =============================================================================
//  FULLSCREEN TOGGLE — F11
// =============================================================================
void Application::toggleFullscreen() {
    isFullscreen_ = !isFullscreen_;
    isMaximized_ = false;
    SDL_SetWindowFullscreen(sdl_->getWindow(), isFullscreen_);

    if (isFullscreen_) {
        LOG_INFO_CAT("APP", "{}FULLSCREEN ENABLED{}", EMERALD_GREEN, RESET);
    } else {
        LOG_INFO_CAT("APP", "{}FULLSCREEN DISABLED{}", CRIMSON_MAGENTA, RESET);
    }

    updateWindowTitle();
}

// =============================================================================
//  MAXIMIZE TOGGLE — M key
// =============================================================================
void Application::toggleMaximize() {
    if (isFullscreen_) return;

    isMaximized_ = !isMaximized_;
    if (isMaximized_) {
        SDL_MaximizeWindow(sdl_->getWindow());
        LOG_INFO_CAT("APP", "{}WINDOW MAXIMIZED{}", EMERALD_GREEN, RESET);
    } else {
        SDL_RestoreWindow(sdl_->getWindow());
        LOG_INFO_CAT("APP", "{}WINDOW RESTORED{}", OCEAN_TEAL, RESET);
    }
    updateWindowTitle();
}

// =============================================================================
//  RESIZE HANDLER — sync camera + renderer + swapchain
// =============================================================================
void Application::handleResize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == width_ && height == height_)) return;

    if (SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED) {
        LOG_INFO_CAT("APP", "{}RESIZE IGNORED: Window minimized{}", OCEAN_TEAL, RESET);
        return;
    }

    LOG_INFO_CAT("APP", "{}RESIZE → {}x{}{}", BRIGHT_PINKISH_PURPLE, width, height, RESET);

    width_ = width;
    height_ = height;
    camera_->setAspectRatio(static_cast<float>(width_) / height_);

    if (auto* renderer = getRenderer()) {
        renderer->getSwapchainManager().recreateSwapchain(width_, height_);
        renderer->handleResize(width_, height_);

        LOG_INFO_CAT("APP", "{}Swapchain + renderer resized: {}x{}{}", EMERALD_GREEN, width_, height_, RESET);
    }

    updateWindowTitle();
}

// =============================================================================
//  WINDOW EVENT HANDLER — SDL3: Handle SDL_EVENT_WINDOW_* directly
// =============================================================================
void Application::handleWindowEvent(const SDL_WindowEvent& we) {
    switch (we.type) {
        case SDL_EVENT_WINDOW_MINIMIZED:
            LOG_INFO_CAT("APP", "{}WINDOW MINIMIZED{}", OCEAN_TEAL, RESET);
            break;

        case SDL_EVENT_WINDOW_RESTORED:
            LOG_INFO_CAT("APP", "{}WINDOW RESTORED{}", EMERALD_GREEN, RESET);
            {
                int w, h;
                SDL_GetWindowSize(sdl_->getWindow(), &w, &h);
                if (w != width_ || h != height_) {
                    handleResize(w, h);
                }
            }
            break;

        case SDL_EVENT_WINDOW_MAXIMIZED:
            if (!isFullscreen_) {
                isMaximized_ = true;
                LOG_INFO_CAT("APP", "{}WINDOW MAXIMIZED (OS){}", EMERALD_GREEN, RESET);
                updateWindowTitle();
            }
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            if (!(SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED)) {
                handleResize(we.data1, we.data2);
            }
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            quit_ = true;
            break;

        default:
            break;  // Suppress -Wswitch warnings
    }
}

// =============================================================================
//  MAIN LOOP — SDL3: Check for window events via type range
// =============================================================================
void Application::run() {
    while (!shouldQuit()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                quit_ = true;
                break;
            }

            // SDL3: All window events are in SDL_EVENT_WINDOW_FIRST to SDL_EVENT_WINDOW_LAST
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
//  RENDER FRAME
// =============================================================================
void Application::render() {
    if (!getRenderer() || !camera_) return;

    uint32_t flags = SDL_GetWindowFlags(sdl_->getWindow());
    if (flags & SDL_WINDOW_MINIMIZED) {
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

// =============================================================================
//  QUIT CHECK
// =============================================================================
bool Application::shouldQuit() const {
    return quit_;
}