// src/handle_app.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: T = toggle tonemap | O = toggle overlay | 1-9 = render modes | H = HYPERTRACE
// PROTIP: Use RAII for resource management in Vulkan to ensure proper cleanup on scope exit.

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
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Dispose.hpp"
#include "engine/utils.hpp"
#include "engine/core.hpp"

// ---------------------------------------------------------------------
// IMPORTANT: Vulkan::Context is used by VulkanRenderer::getContext()
// We do NOT use VulkanRTX::Context here!
// ---------------------------------------------------------------------

// PROTIP: When loading meshes from OBJ files, validate input data early to prevent crashes from malformed files.
//         Consider using a dedicated parsing library like tinyobjloader for production code.
void loadMesh(const std::string& filename, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Mesh", std::format("Failed to open {} — using fallback triangle", filename).c_str());
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
        LOG_WARN_CAT("Mesh", "Invalid .obj — serving fallback triangle");
        vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        indices = {0, 1, 2};
    } else {
        vertices = std::move(tempVertices);
        LOG_INFO_CAT("Mesh", std::format("Loaded {}: {} verts, {} tris", filename, vertices.size(), indices.size() / 3).c_str());
    }
}

Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1), quit_(false),
      sdl_(std::make_unique<SDL3Initializer::SDL3Initializer>(title_, width_, height_)),
      renderer_(nullptr),
      camera_(std::make_unique<VulkanRTX::PerspectiveCamera>(60.0f, static_cast<float>(width) / height)),
      inputHandler_(nullptr), isFullscreen_(false), isMaximized_(false),
      lastFrameTime_(std::chrono::steady_clock::now()),
      showOverlay_(true), tonemapEnabled_(false)
{
    LOG_INFO_CAT("Application", std::format("{}INIT [{}x{}]{}", 
                 Logging::Color::OCEAN_TEAL, width, height, Logging::Color::RESET).c_str()); 
    camera_->setUserData(this);
    initializeInput();
}

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

    LOG_INFO_CAT("Application", std::format("{}MESH LOADED | 1-9=mode | H=HYPERTRACE | T=tonemap | O=overlay{}", 
                 Logging::Color::EMERALD_GREEN, Logging::Color::RESET).c_str());
}

Application::~Application() {
    LOG_INFO_CAT("Application", std::format("{}SHUTDOWN{}", 
                 Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET).c_str());
    Dispose::quitSDL();
}

void Application::initializeInput() {
    inputHandler_ = std::make_unique<HandleInput>(*camera_);
    inputHandler_->setCallbacks(
        [this](const SDL_KeyboardEvent& key) {
            if (key.type == SDL_EVENT_KEY_DOWN) {
                switch (key.key) {
                    case SDLK_1: setRenderMode(1); break;
                    case SDLK_2: setRenderMode(2); break;
                    case SDLK_3: setRenderMode(3); break;
                    case SDLK_4: setRenderMode(4); break;
                    case SDLK_5: setRenderMode(5); break;
                    case SDLK_6: setRenderMode(6); break;
                    case SDLK_7: setRenderMode(7); break;
                    case SDLK_8: setRenderMode(8); break;
                    case SDLK_9: setRenderMode(9); break;

                    case SDLK_T: toggleTonemap(); break;
                    case SDLK_O: toggleOverlay(); break;
                    case SDLK_H: toggleHypertrace(); break;  // HYPERTRACE TOGGLE

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

/* --------------------------------------------------------------- */
/*  HYPERTRACE – toggle 12,000+ FPS mode                          */
/* --------------------------------------------------------------- */
void Application::toggleHypertrace() {
    if (renderer_) {
        renderer_->toggleHypertrace();
    }
    updateWindowTitle();
}

/* --------------------------------------------------------------- */
/*  TONEMAP – independent flag, forces render mode 2 when active   */
/* --------------------------------------------------------------- */
void Application::toggleTonemap() {
    tonemapEnabled_ = !tonemapEnabled_;

    int targetMode = tonemapEnabled_ ? 2 : mode_;
    if (renderer_) {
        renderer_->setRenderMode(targetMode);
    }

    LOG_INFO_CAT("TONEMAP",
                 std::format("{}TONEMAP {} | RENDER MODE {}{}",
                 Logging::Color::PEACHES_AND_CREAM,
                 tonemapEnabled_ ? "ENABLED" : "DISABLED",
                 targetMode,
                 Logging::Color::RESET).c_str());

    updateWindowTitle();
}

/* --------------------------------------------------------------- */
void Application::toggleOverlay() {
    showOverlay_ = !showOverlay_;
    LOG_INFO_CAT("Application", std::format("Overlay {}", showOverlay_ ? "ON" : "OFF").c_str());
    updateWindowTitle();
}

/* --------------------------------------------------------------- */
void Application::updateWindowTitle() {
    std::string title = title_;

    if (showOverlay_) {
        title += " | Mode " + std::to_string(mode_);
        if (tonemapEnabled_) title += " (TONEMAP)";
        if (renderer_ && renderer_->getRTX().isHypertraceEnabled()) title += " (HYPERTRACE)";
        title += " | 1-9=mode | H=HYPERTRACE | T=tonemap | O=hide";
    }

    SDL_SetWindowTitle(sdl_->getWindow(), title.c_str());
}

/* --------------------------------------------------------------- */
void Application::setRenderMode(int mode) {
    mode_ = mode;

    int finalMode = tonemapEnabled_ ? 2 : mode_;
    if (renderer_) {
        renderer_->setRenderMode(finalMode);
    }

    LOG_INFO_CAT("Application", std::format("Render mode set to {}{}",
                 finalMode,
                 tonemapEnabled_ ? " (tonemap active)" : "").c_str());

    updateWindowTitle();
}

/* --------------------------------------------------------------- */
void Application::toggleFullscreen() {
    isFullscreen_ = !isFullscreen_;
    isMaximized_ = false;
    SDL_SetWindowFullscreen(sdl_->getWindow(), isFullscreen_);
}

void Application::toggleMaximize() {
    if (isFullscreen_) return;
    isMaximized_ = !isMaximized_;
    if (isMaximized_) SDL_MaximizeWindow(sdl_->getWindow());
    else SDL_RestoreWindow(sdl_->getWindow());
}

/* --------------------------------------------------------------- */
void Application::handleResize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == width_ && height == height_)) return;
    if (SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED) return;
    width_ = width; height_ = height;
    if (renderer_) renderer_->handleResize(width_, height_);
    camera_->setAspectRatio(static_cast<float>(width_) / height_);
}

/* --------------------------------------------------------------- */
void Application::run() {
    while (!shouldQuit()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                quit_ = true;
                break;
            }
        }
        if (quit_) break;

        inputHandler_->handleInput(*this);
        render();
    }
}

/* --------------------------------------------------------------- */
void Application::render() {
    if (!renderer_ || !camera_) return;
    if (SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return;
    }

    auto now = std::chrono::steady_clock::now();
    float delta = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;

    camera_->update(delta);
    renderer_->renderFrame(*camera_, delta);
    updateWindowTitle();
}

/* --------------------------------------------------------------- */
bool Application::shouldQuit() const {
    return quit_;
}