// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Application handling for SDL3 and Vulkan integration.
// Loads meshes, initializes renderer, input, and audio; manages main loop.
// Dependencies: SDL3, GLM, VulkanRTX_Setup.hpp, logging.hpp, Dispose.hpp, camera.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "handle_app.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <fmt/format.h>
#include <chrono>
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Dispose.hpp"

void loadMesh(const std::string& filename, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    static std::vector<glm::vec3> cachedVertices;
    static std::vector<uint32_t> cachedIndices;
    static bool isLoaded = false;

    if (isLoaded) {
        LOG_DEBUG_CAT("MeshLoader", "Using cached mesh data for {}", filename);
        vertices = cachedVertices;
        indices = cachedIndices;
        return;
    }

    LOG_DEBUG_CAT("MeshLoader", "Loading mesh from {}", filename);
    vertices.clear();
    indices.clear();

    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_WARNING_CAT("MeshLoader", "Failed to open mesh file: {}, using fallback triangle", filename);
        vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        indices = {0, 1, 2};
    } else {
        std::vector<glm::vec3> tempVertices;
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string type;
            iss >> type;
            if (type == "v") {
                glm::vec3 vertex;
                iss >> vertex.x >> vertex.y >> vertex.z;
                tempVertices.push_back(vertex);
            } else if (type == "f") {
                uint32_t v1, v2, v3;
                iss >> v1 >> v2 >> v3;
                indices.push_back(v1 - 1);
                indices.push_back(v2 - 1);
                indices.push_back(v3 - 1);
            }
        }
        file.close();
        vertices = tempVertices;
        if (vertices.size() < 3 || indices.size() < 3 || indices.size() % 3 != 0) {
            LOG_WARNING_CAT("MeshLoader", "Invalid mesh data in {}, using fallback triangle", filename);
            vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
            indices = {0, 1, 2};
        }
    }

    cachedVertices = vertices;
    cachedIndices = indices;
    isLoaded = true;
    LOG_DEBUG_CAT("MeshLoader", "Loaded and cached mesh: {} vertices, {} indices", vertices.size(), indices.size());
}

Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1),
      sdl_(std::make_unique<SDL3Initializer::SDL3Initializer>(title, width, height)),
      renderer_(nullptr), camera_(std::make_unique<PerspectiveCamera>(60.0f, static_cast<float>(width) / height)),
      inputHandler_(nullptr),
      lastFrameTime_(std::chrono::steady_clock::now()) {
    LOG_INFO_CAT("Application", "Initializing Application: {} ({}x{})", title_, width_, height_);
    try {
        loadMesh("assets/models/scene.obj", vertices_, indices_);
        uint32_t extensionCount = 0;
        const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (!extensionNames) {
            LOG_ERROR_CAT("Application", "Failed to get Vulkan extension count from SDL");
            throw std::runtime_error("Failed to get Vulkan extension count");
        }
        std::vector<std::string> instanceExtensions(extensionNames, extensionNames + extensionCount);
        std::string extensionsStr;
        for (size_t i = 0; i < instanceExtensions.size(); ++i) {
            extensionsStr += instanceExtensions[i];
            if (i < instanceExtensions.size() - 1) extensionsStr += ", ";
        }
        LOG_DEBUG_CAT("Application", "Vulkan instance extensions: {}", extensionsStr);
        renderer_ = std::make_unique<VulkanRTX::VulkanRenderer>(width_, height_, sdl_->getWindow(), instanceExtensions);
        camera_->setUserData(this); // Set Application reference in camera
        initializeInput();
        LOG_INFO_CAT("Application", "Application initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Application", "Initialization failed: {}", e.what());
        throw;
    }
}

Application::~Application() {
    LOG_DEBUG_CAT("Application", "Starting destructor cleanup");
    // Reset input and camera
    inputHandler_.reset();
    camera_.reset();
    // Destroy Vulkan renderer (includes surface and swapchain destruction)
    renderer_.reset();
    // Now safe to destroy SDL window
    sdl_.reset();
    // Finally quit SDL subsystems
    Dispose::quitSDL();
    LOG_INFO_CAT("Application", "Application destroyed");
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
    LOG_DEBUG_CAT("Application", "Input handler initialized");
}

void Application::run() {
    LOG_INFO_CAT("Application", "Starting application main loop");
    while (!shouldQuit()) {
        inputHandler_->handleInput(*this);
        render();
    }
    LOG_INFO_CAT("Application", "Application main loop ended");
}

void Application::render() {
    if (!renderer_ || !camera_) {
        LOG_ERROR_CAT("Application", "Cannot render: renderer or camera not initialized");
        return;
    }
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;
    camera_->update(deltaTime);
    renderer_->renderFrame(*camera_); // Adjusted to match current VulkanRenderer signature
}

void Application::handleResize(int width, int height) {
    if (width <= 0 || height <= 0) {
        LOG_WARNING_CAT("Application", "Invalid resize dimensions: {}x{}", width, height);
        return;
    }
    width_ = width;
    height_ = height;
    renderer_->handleResize(width, height);
    camera_->setAspectRatio(static_cast<float>(width) / height);
    LOG_INFO_CAT("Application", "Resized to {}x{}", width, height);
}

HandleInput::HandleInput(Camera& camera) : camera_(camera) {
    LOG_DEBUG_CAT("Input", "HandleInput initialized");
}

void HandleInput::handleInput(Application& app) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                app.setRenderMode(0);
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                app.handleResize(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (keyboardCallback_) keyboardCallback_(event.key);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouseButtonCallback_) mouseButtonCallback_(event.button);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouseMotionCallback_) mouseMotionCallback_(event.motion);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (mouseWheelCallback_) mouseWheelCallback_(event.wheel);
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (textInputCallback_) textInputCallback_(event.text);
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                if (touchCallback_) touchCallback_(event.tfinger);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (gamepadButtonCallback_) gamepadButtonCallback_(event.gbutton);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (gamepadAxisCallback_) gamepadAxisCallback_(event.gaxis);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (gamepadConnectCallback_) {
                    bool connected = event.type == SDL_EVENT_GAMEPAD_ADDED;
                    SDL_Gamepad* pad = connected ? SDL_OpenGamepad(event.gdevice.which) : nullptr;
                    gamepadConnectCallback_(connected, event.gdevice.which, pad);
                    if (pad && !connected) {
                        SDL_CloseGamepad(pad);
                    }
                }
                break;
        }
    }
}

void HandleInput::setCallbacks(
    KeyboardCallback kb,
    MouseButtonCallback mb,
    MouseMotionCallback mm,
    MouseWheelCallback mw,
    TextInputCallback ti,
    TouchCallback tc,
    GamepadButtonCallback gb,
    GamepadAxisCallback ga,
    GamepadConnectCallback gc) {
    keyboardCallback_ = kb;
    mouseButtonCallback_ = mb;
    mouseMotionCallback_ = mm;
    mouseWheelCallback_ = mw;
    textInputCallback_ = ti;
    touchCallback_ = tc;
    gamepadButtonCallback_ = gb;
    gamepadAxisCallback_ = ga;
    gamepadConnectCallback_ = gc;
    LOG_DEBUG_CAT("Input", "Input callbacks set");
}

void HandleInput::defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
    if (key.type != SDL_EVENT_KEY_DOWN) return;
    void* userData = camera_.getUserData();
    if (!userData) {
        LOG_ERROR_CAT("Input", "Camera userData is null, cannot switch render mode");
        return;
    }
    Application& app = *static_cast<Application*>(userData);
    switch (key.scancode) {
        case SDL_SCANCODE_1:
        case SDL_SCANCODE_2:
        case SDL_SCANCODE_3:
        case SDL_SCANCODE_4:
        case SDL_SCANCODE_5:
        case SDL_SCANCODE_6:
        case SDL_SCANCODE_7:
        case SDL_SCANCODE_8:
        case SDL_SCANCODE_9:
            app.setRenderMode(key.scancode - SDL_SCANCODE_1 + 1);
            // camera_.setMode(key.scancode - SDL_SCANCODE_1 + 1); // Commented out: camera mode not needed for render switching
            LOG_INFO_CAT("Input", "Switched to render mode {}", key.scancode - SDL_SCANCODE_1 + 1);
            break;
        case SDL_SCANCODE_P:
            camera_.togglePause();
            break;
        case SDL_SCANCODE_W:
            camera_.moveForward(0.1f);
            break;
        case SDL_SCANCODE_S:
            camera_.moveForward(-0.1f);
            break;
        case SDL_SCANCODE_A:
            camera_.moveRight(-0.1f);
            break;
        case SDL_SCANCODE_D:
            camera_.moveRight(0.1f);
            break;
        case SDL_SCANCODE_Q:
            camera_.moveUp(0.1f);
            break;
        case SDL_SCANCODE_E:
            camera_.moveUp(-0.1f);
            break;
        case SDL_SCANCODE_Z:
            camera_.updateZoom(true);
            break;
        case SDL_SCANCODE_X:
            camera_.updateZoom(false);
            break;
        default:
            break;
    }
}

void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb) {
    (void)mb;
}

void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    camera_.rotate(mm.xrel * 0.005f, mm.yrel * 0.005f);
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    if (mw.y > 0) {
        camera_.updateZoom(true);
    } else if (mw.y < 0) {
        camera_.updateZoom(false);
    }
}

void HandleInput::defaultTextInputHandler(const SDL_TextInputEvent& ti) {
    (void)ti;
}

void HandleInput::defaultTouchHandler(const SDL_TouchFingerEvent& tf) {
    (void)tf;
}

void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    if (gb.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        switch (gb.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:
                camera_.updateZoom(true);
                break;
            case SDL_GAMEPAD_BUTTON_EAST:
                camera_.updateZoom(false);
                break;
            case SDL_GAMEPAD_BUTTON_NORTH:
                camera_.togglePause();
                break;
            default:
                break;
        }
    }
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    float axisValue = ga.value / 32767.0f;
    if (ga.axis == SDL_GAMEPAD_AXIS_LEFTX) {
        camera_.moveUserCam(axisValue * 0.1f, 0.0f, 0.0f);
    } else if (ga.axis == SDL_GAMEPAD_AXIS_LEFTY) {
        camera_.moveUserCam(0.0f, -axisValue * 0.1f, 0.0f);
    } else if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
        camera_.rotate(axisValue * 0.05f, 0.0f);
    } else if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
        camera_.rotate(0.0f, -axisValue * 0.05f);
    }
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    (void)id;
    if (pad && !connected) {
        SDL_CloseGamepad(pad);
    }
}