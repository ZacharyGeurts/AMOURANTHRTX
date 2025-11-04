// src/handle_app.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: T = toggle tonemap | O = toggle overlay | 1-9 = render modes | core.cpp stays

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
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Dispose.hpp"
#include "engine/utils.hpp"
#include "engine/core.hpp"

void loadMesh(const std::string& filename, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Mesh", "Failed to open %s using fallback triangle", filename.c_str());
        vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        indices = {0, 1, 2};
        return;
    }

    std::vector<glm::vec3> tempVertices;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        if (!(iss >> type)) continue;
        if (type == "v") {
            glm::vec3 v;
            if (iss >> v.x >> v.y >> v.z) tempVertices.push_back(v);
        } else if (type == "f") {
            uint32_t a, b, c;
            if (iss >> a >> b >> c) indices.insert(indices.end(), {a-1, b-1, c-1});
        }
    }
    file.close();

    if (tempVertices.size() < 3 || indices.size() < 3 || indices.size() % 3 != 0) {
        LOG_WARN_CAT("Mesh", "Invalid .obj fallback triangle");
        vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        indices = {0, 1, 2};
    } else {
        vertices = std::move(tempVertices);
        LOG_INFO_CAT("Mesh", "Loaded %s: %zu verts, %zu tris", filename.c_str(), vertices.size(), indices.size() / 3);
    }
}

Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1), quit_(false),
      sdl_(std::make_unique<SDL3Initializer::SDL3Initializer>(title_, width_, height_)),
      renderer_(nullptr),
      camera_(std::make_unique<VulkanRTX::PerspectiveCamera>(60.0f, static_cast<float>(width) / height)),
      inputHandler_(nullptr), isFullscreen_(false), isMaximized_(false),
      lastFrameTime_(std::chrono::steady_clock::now()),
      showOverlay_(true)
{
    LOG_INFO_CAT("Application", "{}INIT [{}x{}]{}", Logging::Color::OCEAN_TEAL, width, height, Logging::Color::RESET);
    camera_->setUserData(this);
    initializeInput();
}

void Application::setRenderer(std::unique_ptr<VulkanRTX::VulkanRenderer> renderer) {
    renderer_ = std::move(renderer);

    loadMesh("assets/models/scene.obj", vertices_, indices_);
    renderer_->getBufferManager()->uploadMesh(vertices_.data(), vertices_.size(), indices_.data(), indices_.size());

    auto ctx = renderer_->getContext();  // Assumes getter added
    renderer_->getRTX().updateRTX(  // Assumes getter added
        ctx->physicalDevice,
        ctx->commandPool,
        ctx->graphicsQueue,
        renderer_->getBufferManager()->getGeometries(),
        renderer_->getBufferManager()->getDimensionStates()
    );

    renderer_->setRenderMode(mode_);
    updateWindowTitle();

    LOG_INFO_CAT("Application", "{}REAL MESH LOADED & AS BUILT | 1-9=mode | T=tonemap | O=overlay{}", 
                 Logging::Color::EMERALD_GREEN, Logging::Color::RESET);
}

Application::~Application() {
    LOG_INFO_CAT("Application", "{}SHUTDOWN{}", Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
    Dispose::quitSDL();
}

void Application::initializeInput() {
    inputHandler_ = std::make_unique<HandleInput>(*camera_);
    inputHandler_->setCallbacks(
        [this](const SDL_KeyboardEvent& key) {
            if (key.type == SDL_EVENT_KEY_DOWN) {
                switch (key.key) {  // SDL3: Use key.key instead of key.keysym.sym
                    case SDLK_1: setRenderMode(1); break;
                    case SDLK_2: setRenderMode(2); break;
                    case SDLK_3: setRenderMode(3); break;
                    case SDLK_4: setRenderMode(4); break;
                    case SDLK_5: setRenderMode(5); break;
                    case SDLK_6: setRenderMode(6); break;
                    case SDLK_7: setRenderMode(7); break;
                    case SDLK_8: setRenderMode(8); break;
                    case SDLK_9: setRenderMode(9); break;
                    case SDLK_T: toggleTonemap(); break;  // SDL3: Uppercase
                    case SDLK_O: toggleOverlay(); break;  // SDL3: Uppercase
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

void Application::toggleTonemap() {
    mode_ = (mode_ == 1) ? 2 : 1;
    if (renderer_) {
        renderer_->setRenderMode(mode_);
        LOG_INFO_CAT("Application", "{}TONEMAP {} | MODE {}{}", 
                     Logging::Color::ARCTIC_CYAN,
                     (mode_ == 2 ? "ENABLED" : "DISABLED"),
                     mode_, Logging::Color::RESET);
    }
    updateWindowTitle();
}

void Application::toggleOverlay() {
    showOverlay_ = !showOverlay_;
    LOG_INFO_CAT("Application", "Overlay %s", showOverlay_ ? "ON" : "OFF");
    updateWindowTitle();
}

void Application::updateWindowTitle() {
    if (!showOverlay_) {
        SDL_SetWindowTitle(sdl_->getWindow(), title_.c_str());
        return;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s | Mode %d | 1-9=mode | T=tonemap | O=hide", title_.c_str(), mode_);
    SDL_SetWindowTitle(sdl_->getWindow(), buf);
}

void Application::setRenderMode(int mode) {
    mode_ = mode;
    if (renderer_) renderer_->setRenderMode(mode_);
    updateWindowTitle();
}

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

void Application::handleResize(int width, int height) {
    if (width <= 0 || height <= 0 || (width == width_ && height == height_)) return;
    if (SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED) return;
    width_ = width; height_ = height;
    if (renderer_) renderer_->handleResize(width_, height_);
    camera_->setAspectRatio(static_cast<float>(width_) / height_);
}

void Application::run() {
    while (!shouldQuit()) {
        // SDL3: Poll events explicitly and check for quit
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit_ = true;
                break;
            }
            // Forward other events to input handler if needed; assuming handleInput processes them
        }
        if (quit_) break;

        inputHandler_->handleInput(*this);
        render();
    }
}

void Application::render() {
    if (!renderer_ || !camera_) return;
    if (SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return;
    }
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;
    camera_->update(deltaTime);
    renderer_->renderFrame(*camera_, deltaTime);
    updateWindowTitle();
}

bool Application::shouldQuit() const {
    return quit_;  // SDL3: Use flag set by event polling
}