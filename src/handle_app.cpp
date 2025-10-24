// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Application handling implementation for SDL3 + Vulkan integration.
// Dependencies: SDL3, GLM, VulkanRTX_Setup.hpp, logging.hpp, Dispose.hpp, ue_init.hpp
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "handle_app.hpp"
#include "engine/SDL3/SDL3_audio.hpp"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <fmt/format.h>
#include <source_location>
#include "engine/logging.hpp"
#include "ue_init.hpp"  // For AMOURANTH, UniversalEquation, DimensionalNavigator
#include "engine/Vulkan/VulkanRenderer.hpp"  // VulkanRenderer
#include "engine/SDL3/SDL3_init.hpp"

// Simple OBJ loader for demonstration
void loadMesh(const std::string& filename, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    LOG_DEBUG_CAT("MeshLoader", "Loading mesh from {}", filename);
    vertices.clear();
    indices.clear();

    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_WARNING_CAT("MeshLoader", "Failed to open mesh file: {}, using fallback triangle", filename);
        vertices = {
            {0.0f, -0.5f, 0.0f}, // Bottom
            {0.5f, 0.5f, 0.0f},  // Top-right
            {-0.5f, 0.5f, 0.0f}  // Top-left
        };
        indices = {0, 1, 2};
        return;
    }

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
            indices.push_back(v1 - 1); // OBJ indices are 1-based
            indices.push_back(v2 - 1);
            indices.push_back(v3 - 1);
        }
    }
    file.close();

    vertices = tempVertices;
    if (vertices.size() < 3 || indices.size() < 3 || indices.size() % 3 != 0) {
        LOG_WARNING_CAT("MeshLoader", "Invalid mesh data in {}, using fallback triangle", filename);
        vertices = {
            {0.0f, -0.5f, 0.0f}, // Bottom
            {0.5f, 0.5f, 0.0f},  // Top-right
            {-0.5f, 0.5f, 0.0f}  // Top-left
        };
        indices = {0, 1, 2};
    }
    LOG_DEBUG_CAT("MeshLoader", "Loaded mesh: {} vertices, {} indices", vertices.size(), indices.size());
}

Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1),
      sdl_(std::make_unique<SDL3Initializer::SDL3Initializer>(title, width, height)),
      renderer_(nullptr), navigator_(nullptr), amouranth_(std::nullopt),
      inputHandler_(nullptr), audioDevice_(0), audioStream_(nullptr),
      lastFrameTime_(std::chrono::steady_clock::now()) {
    LOG_INFO_CAT("Application", "Initializing Application: {} ({}x{})", title_, width_, height_);
    try {
        // Load mesh from assets folder with fallback
        loadMesh("assets/models/scene.obj", vertices_, indices_);

        // Get Vulkan instance extensions from SDL
        uint32_t extensionCount = 0;
        const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (!extensionNames) {
            LOG_ERROR_CAT("Application", "Failed to get Vulkan extension count from SDL");
            throw std::runtime_error("Failed to get Vulkan extension count");
        }
        std::vector<std::string> instanceExtensions(extensionNames, extensionNames + extensionCount);
        // Manually concatenate extensions for logging
        std::string extensionsStr;
        for (size_t i = 0; i < instanceExtensions.size(); ++i) {
            extensionsStr += instanceExtensions[i];
            if (i < instanceExtensions.size() - 1) extensionsStr += ", ";
        }
        LOG_DEBUG_CAT("Application", "Vulkan instance extensions: {}", extensionsStr);

        // Initialize VulkanRenderer with SDL window and extensions
        renderer_ = std::make_unique<VulkanRTX::VulkanRenderer>(width_, height_, sdl_->getWindow(), instanceExtensions);
        navigator_ = std::make_unique<UE::DimensionalNavigator>("Navigator", width_, height_, *renderer_);
        navigator_->initialize(9, 1024);

        // Initialize AMOURANTH with Vulkan resources
        amouranth_.emplace(navigator_.get(), renderer_->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
        amouranth_->setMode(1); // Removed std::source_location argument
        amouranth_->getUniversalEquation().setNavigator(navigator_.get());
        amouranth_->getUniversalEquation().initializeCalculator(&amouranth_.value());
        initializeInput();
        initializeAudio();
        LOG_INFO_CAT("Application", "Application initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Application", "Initialization failed: {}", e.what());
        throw;
    }
}

Application::~Application() {
    amouranth_.reset();
    LOG_DEBUG_CAT("Application", "AMOURANTH instance destroyed");
    inputHandler_.reset();
    navigator_.reset();
    renderer_.reset();
    if (audioStream_) {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
        LOG_DEBUG_CAT("Application", "Destroyed audio stream");
    }
    if (audioDevice_) {
        SDL_CloseAudioDevice(audioDevice_);
        audioDevice_ = 0;
        LOG_DEBUG_CAT("Application", "Closed audio device");
    }
    Dispose::quitSDL();
    sdl_.reset();
    LOG_INFO_CAT("Application", "Application destroyed");
}

void Application::initializeInput() {
    if (amouranth_.has_value()) {
        inputHandler_ = std::make_unique<HandleInput>(amouranth_->getUniversalEquation(), navigator_.get(), amouranth_.value());
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
    } else {
        LOG_ERROR_CAT("Application", "Cannot initialize input: AMOURANTH not initialized");
        throw std::runtime_error("AMOURANTH not initialized");
    }
}

void Application::initializeAudio() {
    try {
        SDL3Audio::AudioConfig config;
        config.frequency = 44100;  // Standard sample rate for 8-channel audio
        config.format = SDL_AUDIO_S16LE;  // Signed 16-bit format for multi-channel audio
        config.channels = 8;  // 8-channel surround audio
        // Optional: Set callback if needed for custom audio processing
        // config.callback = [](Uint8* buffer, int length) { /* Custom audio generation */ };

        SDL3Audio::initAudio(config, audioDevice_, audioStream_);
        LOG_INFO_CAT("Application", "8-channel audio initialized successfully with device ID: {}", audioDevice_);
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Application", "Failed to initialize 8-channel audio: {}", e.what());
        audioDevice_ = 0;
        audioStream_ = nullptr;
    }
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
    if (!amouranth_.has_value() || !renderer_) {
        LOG_ERROR_CAT("Application", "Cannot render: AMOURANTH or renderer not initialized");
        return;
    }
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;

    amouranth_->update(deltaTime);
    // Assuming navigator_ provides a concrete Camera instance
    renderer_->renderFrame(navigator_->getCamera()); // Ensure getCamera() returns a concrete Camera
    amouranth_->getUniversalEquation().advanceCycle();
}

void Application::handleResize(int width, int height) {
    if (width <= 0 || height <= 0) {
        LOG_WARNING_CAT("Application", "Invalid resize dimensions: {}x{}", width, height);
        return;
    }
    width_ = width;
    height_ = height;
    renderer_->handleResize(width, height);
    if (amouranth_.has_value()) {
        navigator_->setWidth(width);
        navigator_->setHeight(height);
        amouranth_->setCurrentDimension(amouranth_->getCurrentDimension(), std::source_location::current());
    }
    LOG_INFO_CAT("Application", "Resized to {}x{}", width, height);
}

HandleInput::HandleInput(UE::UniversalEquation& universalEquation, UE::DimensionalNavigator* navigator, UE::AMOURANTH& amouranth)
    : universalEquation_(universalEquation), navigator_(navigator), amouranth_(amouranth) {
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
                    if (!connected && pad) {
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
            {
                int mode = key.scancode - SDL_SCANCODE_1 + 1;
                universalEquation_.setMode(mode);
                navigator_->setMode(mode);
                amouranth_.setMode(mode); // Removed std::source_location argument
            }
            break;
        case SDL_SCANCODE_KP_PLUS:
        case SDL_SCANCODE_EQUALS:
            amouranth_.adjustInfluence(0.1f, std::source_location::current());
            break;
        case SDL_SCANCODE_KP_MINUS:
        case SDL_SCANCODE_MINUS:
            amouranth_.adjustInfluence(-0.1f, std::source_location::current());
            break;
        case SDL_SCANCODE_P:
            amouranth_.togglePause(std::source_location::current());
            break;
        case SDL_SCANCODE_W:
            amouranth_.moveUserCam(0.0f, 0.0f, 0.1f, std::source_location::current());
            break;
        case SDL_SCANCODE_S:
            amouranth_.moveUserCam(0.0f, 0.0f, -0.1f, std::source_location::current());
            break;
        case SDL_SCANCODE_A:
            amouranth_.moveUserCam(-0.1f, 0.0f, 0.0f, std::source_location::current());
            break;
        case SDL_SCANCODE_D:
            amouranth_.moveUserCam(0.1f, 0.0f, 0.0f, std::source_location::current());
            break;
        case SDL_SCANCODE_Q:
            amouranth_.moveUserCam(0.0f, 0.1f, 0.0f, std::source_location::current());
            break;
        case SDL_SCANCODE_E:
            amouranth_.moveUserCam(0.0f, -0.1f, 0.0f, std::source_location::current());
            break;
        case SDL_SCANCODE_Z:
            amouranth_.updateZoom(true, std::source_location::current());
            break;
        case SDL_SCANCODE_X:
            amouranth_.updateZoom(false, std::source_location::current());
            break;
        case SDL_SCANCODE_UP:
            amouranth_.setCurrentDimension(amouranth_.getCurrentDimension() + 1, std::source_location::current());
            break;
        case SDL_SCANCODE_DOWN:
            amouranth_.setCurrentDimension(amouranth_.getCurrentDimension() - 1, std::source_location::current());
            break;
        default:
            break;
    }
}

void HandleInput::defaultMouseButtonHandler(const SDL_MouseButtonEvent& mb) {
    (void)mb;
}

void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    amouranth_.rotateCamera(mm.xrel * 0.005f, mm.yrel * 0.005f, std::source_location::current());
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    if (mw.y > 0) {
        amouranth_.updateZoom(true, std::source_location::current());
    } else if (mw.y < 0) {
        amouranth_.updateZoom(false, std::source_location::current());
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
            case SDL_GAMEPAD_BUTTON_SOUTH: // A button
                amouranth_.updateZoom(true, std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_EAST: // B button
                amouranth_.updateZoom(false, std::source_location::current());
                break;
            case SDL_GAMEPAD_BUTTON_NORTH: // Y button
                amouranth_.togglePause(std::source_location::current());
                break;
            default:
                break;
        }
    }
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    float axisValue = ga.value / 32767.0f; // Normalize to [-1, 1]
    if (ga.axis == SDL_GAMEPAD_AXIS_LEFTX) {
        amouranth_.moveUserCam(axisValue * 0.1f, 0.0f, 0.0f, std::source_location::current());
    } else if (ga.axis == SDL_GAMEPAD_AXIS_LEFTY) {
        amouranth_.moveUserCam(0.0f, -axisValue * 0.1f, 0.0f, std::source_location::current());
    } else if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
        amouranth_.rotateCamera(axisValue * 0.05f, 0.0f, std::source_location::current());
    } else if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
        amouranth_.rotateCamera(0.0f, -axisValue * 0.05f, std::source_location::current());
    }
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    (void)id;
    if (!connected && pad) {
        SDL_CloseGamepad(pad);
    }
}