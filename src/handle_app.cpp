// src/handle_app.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

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
#include <cstdio>
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Dispose.hpp"
#include "engine/utils.hpp"

template<typename T>
std::string join(const std::vector<T>& vec, const std::string& sep) {
    if (vec.empty()) return "";
    std::ostringstream oss;
    oss << vec[0];
    for (size_t i = 1; i < vec.size(); ++i) oss << sep << vec[i];
    return oss.str();
}

void loadMesh(const std::string& filename, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    static std::vector<glm::vec3> cachedVertices;
    static std::vector<uint32_t> cachedIndices;
    static bool isLoaded = false;

    if (isLoaded) {
        vertices = cachedVertices;
        indices = cachedIndices;
        return;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        indices = {0, 1, 2};
    } else {
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
            vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
            indices = {0, 1, 2};
        } else {
            vertices = std::move(tempVertices);
        }
    }

    cachedVertices = vertices;
    cachedIndices = indices;
    isLoaded = true;
}

Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1),
      sdl_(std::make_unique<SDL3Initializer::SDL3Initializer>(title_, width_, height_)),
      renderer_(nullptr),
      camera_(std::make_unique<VulkanRTX::PerspectiveCamera>(60.0f, static_cast<float>(width) / height)),
      inputHandler_(nullptr), isFullscreen_(false), isMaximized_(false),
      lastFrameTime_(std::chrono::steady_clock::now())
{
    LOG_INFO_CAT("Application", "{}APPLICATION INITIALIZATION{}", Logging::Color::DIAMOND_WHITE, Logging::Color::RESET);
    LOG_INFO_CAT("Application", "Title      : {}", title_);
    LOG_INFO_CAT("Application", "Resolution : {}x{}", width_, height_);
    LOG_INFO_CAT("Application", "Starting Render Mode: {}", mode_);

    loadMesh("assets/models/scene.obj", vertices_, indices_);
    globalVertices_ = vertices_;
    globalIndices_ = indices_;

    camera_->setUserData(this);
    initializeInput();

    LOG_INFO_CAT("Application", "{}APPLICATION INITIALIZED SUCCESSFULLY{}", Logging::Color::EMERALD_GREEN, Logging::Color::RESET);
}

void Application::setRenderer(std::unique_ptr<VulkanRTX::VulkanRenderer> renderer) {
    renderer_ = std::move(renderer);
}

Application::~Application() {
    LOG_INFO_CAT("Application", "{}SHUTTING DOWN APPLICATION...{}", Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);

    inputHandler_.reset();
    camera_.reset();
    renderer_.reset();
    sdl_.reset();

    Dispose::quitSDL();  // Now valid
}

void Application::initializeInput() {
    inputHandler_ = std::make_unique<HandleInput>(*camera_);
    inputHandler_->setCallbacks(
        [this](const SDL_KeyboardEvent& key) { inputHandler_->defaultKeyboardHandler(key); },
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

void Application::toggleFullscreen() {
    isFullscreenRef() = !isFullscreenRef();
    isMaximizedRef() = false;
    SDL_SetWindowFullscreen(sdl_->getWindow(), isFullscreenRef());
}

void Application::toggleMaximize() {
    if (isFullscreenRef()) return;
    isMaximizedRef() = !isMaximizedRef();
    if (isMaximizedRef()) SDL_MaximizeWindow(sdl_->getWindow());
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
        inputHandler_->handleInput(*this);
        render();
    }
    LOG_INFO_CAT("Application", "Main loop exited");
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
    renderer_->renderFrame(*camera_);
}

HandleInput::HandleInput(VulkanRTX::Camera& camera) : camera_(camera) {}

void HandleInput::handleInput(Application& app) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT: app.setRenderMode(0); break;
            case SDL_EVENT_WINDOW_RESIZED: app.handleResize(event.window.data1, event.window.data2); break;
            case SDL_EVENT_KEY_DOWN: if (keyboardCallback_) keyboardCallback_(event.key); break;
            case SDL_EVENT_MOUSE_MOTION: if (mouseMotionCallback_) mouseMotionCallback_(event.motion); break;
            case SDL_EVENT_MOUSE_WHEEL: if (mouseWheelCallback_) mouseWheelCallback_(event.wheel); break;
        }
    }
}

void HandleInput::defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
    if (key.type != SDL_EVENT_KEY_DOWN || key.repeat != 0) return;
    void* userData = camera_.getUserData();
    if (!userData) return;
    Application& app = *static_cast<Application*>(userData);
    const auto sc = key.scancode;

    if (sc == SDL_SCANCODE_F11 || (sc == SDL_SCANCODE_RETURN && (key.mod & SDL_KMOD_ALT))) { app.toggleFullscreen(); return; }
    if (sc == SDL_SCANCODE_F10) { app.toggleMaximize(); return; }
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) { 
        int newMode = sc - SDL_SCANCODE_1 + 1;
        app.setRenderMode(newMode); 
        return; 
    }

    switch (sc) {
        case SDL_SCANCODE_P: camera_.togglePause(); break;
        case SDL_SCANCODE_W: camera_.moveForward(0.1f); break;
        case SDL_SCANCODE_S: camera_.moveForward(-0.1f); break;
        case SDL_SCANCODE_A: camera_.moveRight(-0.1f); break;
        case SDL_SCANCODE_D: camera_.moveRight(0.1f); break;
        case SDL_SCANCODE_Q: camera_.moveUp(0.1f); break;
        case SDL_SCANCODE_E: camera_.moveUp(-0.1f); break;
        case SDL_SCANCODE_Z: camera_.updateZoom(true); break;
        case SDL_SCANCODE_X: camera_.updateZoom(false); break;
        default: break;
    }
}

void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    camera_.rotate(mm.xrel * 0.005f, mm.yrel * 0.005f);
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    if (mw.y > 0) camera_.updateZoom(true);
    else if (mw.y < 0) camera_.updateZoom(false);
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    if (pad && !connected) SDL_CloseGamepad(pad);
}

void HandleInput::setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                               MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                               GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc) {
    keyboardCallback_ = kb; mouseButtonCallback_ = mb; mouseMotionCallback_ = mm;
    mouseWheelCallback_ = mw; textInputCallback_ = ti; touchCallback_ = tc;
    gamepadButtonCallback_ = gb; gamepadAxisCallback_ = ga; gamepadConnectCallback_ = gc;
}

void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    if (gb.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN) return;
    switch (gb.button) {
        case SDL_GAMEPAD_BUTTON_SOUTH: camera_.updateZoom(true); break;
        case SDL_GAMEPAD_BUTTON_EAST:  camera_.updateZoom(false); break;
        case SDL_GAMEPAD_BUTTON_NORTH: camera_.togglePause(); break;
    }
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    const float v = ga.value / 32767.0f;
    if (ga.axis == SDL_GAMEPAD_AXIS_LEFTX)  camera_.moveUserCam(v * 0.1f, 0, 0);
    if (ga.axis == SDL_GAMEPAD_AXIS_LEFTY)  camera_.moveUserCam(0, -v * 0.1f, 0);
    if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTX) camera_.rotate(v * 0.05f, 0);
    if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTY) camera_.rotate(0, -v * 0.05f);
}