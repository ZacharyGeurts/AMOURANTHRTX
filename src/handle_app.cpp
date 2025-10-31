// handle_app.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#include "handle_app.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/SDL3/SDL3_init.hpp"
#include "engine/Dispose.hpp"

// ---------------------------------------------------------------------------
//  Utility: Pointer to Hex
// ---------------------------------------------------------------------------
inline std::string ptr_to_hex(const void* p) { return std::format("{:p}", p); }

// ---------------------------------------------------------------------------
//  Utility: Join vector<T> with separator
// ---------------------------------------------------------------------------
template<typename T>
std::string join(const std::vector<T>& vec, const std::string& sep) {
    if (vec.empty()) return "";
    std::string result = std::format("{}", vec[0]);
    for (size_t i = 1; i < vec.size(); ++i) {
        result += sep + std::format("{}", vec[i]);
    }
    return result;
}

// ---------------------------------------------------------------------------
//  Mesh Loader – EXPLOSIVE EDITION
// ---------------------------------------------------------------------------
void loadMesh(const std::string& filename, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    static std::vector<glm::vec3> cachedVertices;
    static std::vector<uint32_t> cachedIndices;
    static bool isLoaded = false;

    if (isLoaded) {
        LOG_DEBUG_CAT("MeshLoader", "CACHE HIT: '{}' → {} verts, {} tris", filename, cachedVertices.size(), cachedIndices.size() / 3);
        vertices = cachedVertices;
        indices = cachedIndices;
        return;
    }

    LOG_INFO_CAT("MeshLoader", "LOADING MESH FROM: {}", filename);
    vertices.clear();
    indices.clear();

    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_WARNING_CAT("MeshLoader", "FILE NOT FOUND: '{}' → FALLBACK TRIANGLE", filename);
        vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
        indices = {0, 1, 2};
    } else {
        std::vector<glm::vec3> tempVertices;
        std::string line;
        uint32_t lineNum = 0;

        while (std::getline(file, line)) {
            ++lineNum;
            std::istringstream iss(line);
            std::string type;
            if (!(iss >> type)) continue;

            if (type == "v") {
                glm::vec3 v;
                if (iss >> v.x >> v.y >> v.z) {
                    tempVertices.push_back(v);
                } else {
                    LOG_WARNING_CAT("MeshLoader", "Malformed vertex at line {}: '{}'", lineNum, line);
                }
            } else if (type == "f") {
                uint32_t a, b, c;
                if (iss >> a >> b >> c) {
                    indices.insert(indices.end(), {a-1, b-1, c-1});
                } else {
                    LOG_WARNING_CAT("MeshLoader", "Malformed face at line {}: '{}'", lineNum, line);
                }
            }
        }
        file.close();

        if (tempVertices.size() < 3 || indices.size() < 3 || indices.size() % 3 != 0) {
            LOG_WARNING_CAT("MeshLoader", "INVALID MESH: {} verts, {} indices → FALLBACK", tempVertices.size(), indices.size());
            vertices = {{0.0f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
            indices = {0, 1, 2};
        } else {
            vertices = std::move(tempVertices);
            LOG_DEBUG_CAT("MeshLoader", "Parsed {} vertices, {} triangles", vertices.size(), indices.size() / 3);
        }
    }

    cachedVertices = vertices;
    cachedIndices = indices;
    isLoaded = true;
    LOG_INFO_CAT("MeshLoader", "MESH LOADED & CACHED: {} verts, {} tris @ {}", vertices.size(), indices.size() / 3, ptr_to_hex(&vertices));
}

// ---------------------------------------------------------------------------
//  Application Constructor
// ---------------------------------------------------------------------------
Application::Application(const char* title, int width, int height)
    : title_(title), width_(width), height_(height), mode_(1),
      sdl_(std::make_unique<SDL3Initializer::SDL3Initializer>(title, width, height)),
      renderer_(nullptr), camera_(std::make_unique<PerspectiveCamera>(60.0f, static_cast<float>(width) / height)),
      inputHandler_(nullptr), isFullscreen_(false), isMaximized_(false),
      lastFrameTime_(std::chrono::steady_clock::now())
{
    LOG_INFO_CAT("Application", "INITIALIZING: '{}' ({}x{}) @ {}", title_, width_, height_, ptr_to_hex(this));
    try {
        loadMesh("assets/models/scene.obj", vertices_, indices_);

        uint32_t extensionCount = 0;
        const char* const* extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (!extensionNames) {
            LOG_ERROR_CAT("Application", "SDL FAILED: SDL_Vulkan_GetInstanceExtensions returned null");
            throw std::runtime_error("Failed to get Vulkan instance extensions from SDL");
        }

        std::vector<std::string> instanceExtensions(extensionNames, extensionNames + extensionCount);
        LOG_DEBUG_CAT("Application", "Vulkan instance extensions [{}]: {}", extensionCount, join(instanceExtensions, ", "));

        renderer_ = std::make_unique<VulkanRTX::VulkanRenderer>(width_, height_, sdl_->getWindow(), instanceExtensions);
        camera_->setUserData(this);
        initializeInput();

        LOG_INFO_CAT("Application", "APPLICATION FULLY INITIALIZED @ {}", ptr_to_hex(this));
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("Application", "INIT FAILED: {}", e.what());
        throw;
    }
}

// ---------------------------------------------------------------------------
//  Destructor – EXPLOSIVE CLEANUP
// ---------------------------------------------------------------------------
Application::~Application() {
    LOG_INFO_CAT("Application", "DESTRUCTOR STARTED @ {}", ptr_to_hex(this));

    inputHandler_.reset();
    LOG_DEBUG_CAT("Application", "Input handler destroyed");

    camera_.reset();
    LOG_DEBUG_CAT("Application", "Camera destroyed");

    renderer_.reset();
    LOG_DEBUG_CAT("Application", "Vulkan renderer destroyed");

    sdl_.reset();
    LOG_DEBUG_CAT("Application", "SDL window destroyed");

    Dispose::quitSDL();
    LOG_INFO_CAT("Application", "SDL subsystems terminated");

    LOG_INFO_CAT("Application", "APPLICATION DESTROYED @ {}", ptr_to_hex(this));
}

// ---------------------------------------------------------------------------
//  Input Initialization
// ---------------------------------------------------------------------------
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
        [this](bool connected, SDL_JoystickID id, SDL_Gamepad* pad) { 
            inputHandler_->defaultGamepadConnectHandler(connected, id, pad); 
        }
    );
    LOG_DEBUG_CAT("Application", "Input handler initialized with all callbacks");
}

// ---------------------------------------------------------------------------
//  Toggle Fullscreen
// ---------------------------------------------------------------------------
void Application::toggleFullscreen() {
    isFullscreenRef() = !isFullscreenRef();
    isMaximizedRef() = false;

    SDL_Window* win = sdl_->getWindow();
    if (isFullscreenRef()) {
        SDL_SetWindowFullscreen(win, true);
        SDL_DisplayID display = SDL_GetDisplayForWindow(win);
        const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(display);
        if (dm && dm->w > 0 && dm->h > 0) {
            width_ = dm->w;
            height_ = dm->h;
        } else {
            width_ = 1920;
            height_ = 1080;
        }
        LOG_INFO_CAT("Application", "ENTERED FULLSCREEN: {}x{}", width_, height_);
    } else {
        SDL_SetWindowFullscreen(win, false);
        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        if (w > 0 && h > 0) {
            width_ = w;
            height_ = h;
        }
        LOG_INFO_CAT("Application", "EXITED FULLSCREEN: {}x{}", width_, height_);
    }

    if (width_ > 0 && height_ > 0) {
        renderer_->handleResize(width_, height_);
        camera_->setAspectRatio(static_cast<float>(width_) / height_);
    }
}

// ---------------------------------------------------------------------------
//  Toggle Maximize
// ---------------------------------------------------------------------------
void Application::toggleMaximize() {
    if (isFullscreenRef()) return;

    isMaximizedRef() = !isMaximizedRef();
    if (isMaximizedRef()) {
        SDL_MaximizeWindow(sdl_->getWindow());
        LOG_INFO_CAT("Application", "WINDOW MAXIMIZED (resize will follow)");
    } else {
        SDL_RestoreWindow(sdl_->getWindow());
        int w, h;
        SDL_GetWindowSize(sdl_->getWindow(), &w, &h);
        if (w > 0 && h > 0) {
            width_ = w;
            height_ = h;
            renderer_->handleResize(width_, height_);
            camera_->setAspectRatio(static_cast<float>(width_) / height_);
        }
        LOG_INFO_CAT("Application", "WINDOW RESTORED: {}x{}", width_, height_);
    }
}

// ---------------------------------------------------------------------------
//  Resize Handler
// ---------------------------------------------------------------------------
void Application::handleResize(int width, int height) {
    if (width <= 0 || height <= 0) {
        LOG_WARNING_CAT("Application", "INVALID RESIZE IGNORED: {}x{}", width, height);
        return;
    }

    if (width == width_ && height == height_) {
        LOG_DEBUG_CAT("Application", "RESIZE NO-OP: {}x{}", width, height);
        return;
    }

    // Skip if minimized
    Uint32 flags = SDL_GetWindowFlags(sdl_->getWindow());
    if (flags & SDL_WINDOW_MINIMIZED) {
        LOG_DEBUG_CAT("Application", "RESIZE SKIPPED: Window minimized");
        return;
    }

    width_ = width;
    height_ = height;

    LOG_INFO_CAT("Application", "RESIZE → {}x{}", width_, height_);
    renderer_->handleResize(width_, height_);
    camera_->setAspectRatio(static_cast<float>(width_) / height_);
}

// ---------------------------------------------------------------------------
//  Main Loop
// ---------------------------------------------------------------------------
void Application::run() {
    LOG_INFO_CAT("Application", "MAIN LOOP STARTED");
    while (!shouldQuit()) {
        inputHandler_->handleInput(*this);
        render();
    }
    LOG_INFO_CAT("Application", "MAIN LOOP ENDED");
}

// ---------------------------------------------------------------------------
//  Render Frame
// ---------------------------------------------------------------------------
void Application::render() {
    if (!renderer_ || !camera_) {
        LOG_ERROR_CAT("Application", "RENDER ABORTED: renderer or camera missing");
        return;
    }

    // Skip rendering if minimized
    if (SDL_GetWindowFlags(sdl_->getWindow()) & SDL_WINDOW_MINIMIZED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return;
    }

    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime_).count();
    lastFrameTime_ = currentTime;

    camera_->update(deltaTime);
    renderer_->renderFrame(*camera_);

    LOG_DEBUG_CAT("Application", "Frame rendered (Δt={:.3f}ms)", deltaTime * 1000.0f);
}

// ---------------------------------------------------------------------------
//  Input Handler Constructor
// ---------------------------------------------------------------------------
HandleInput::HandleInput(Camera& camera) : camera_(camera) {
    LOG_DEBUG_CAT("Input", "HandleInput constructed @ {}", ptr_to_hex(this));
}

// ---------------------------------------------------------------------------
//  Event Polling
// ---------------------------------------------------------------------------
void HandleInput::handleInput(Application& app) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                LOG_INFO_CAT("Input", "QUIT EVENT → setRenderMode(0)");
                app.setRenderMode(0);
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                LOG_DEBUG_CAT("Input", "RESIZE EVENT: {}x{}", event.window.data1, event.window.data2);
                app.handleResize(event.window.data1, event.window.data2);
                break;

            case SDL_EVENT_WINDOW_MAXIMIZED:
                LOG_INFO_CAT("Input", "WINDOW MAXIMIZED");
                app.isMaximizedRef() = true;
                break;

            case SDL_EVENT_WINDOW_RESTORED:
                if (app.isMaximizedRef()) {
                    LOG_INFO_CAT("Input", "WINDOW RESTORED FROM MAXIMIZED");
                    app.isMaximizedRef() = false;
                    int w, h;
                    SDL_GetWindowSize(app.getWindow(), &w, &h);
                    if (w > 0 && h > 0) {
                        app.handleResize(w, h);
                    }
                }
                break;

            case SDL_EVENT_WINDOW_MINIMIZED:
                LOG_INFO_CAT("Input", "WINDOW MINIMIZED");
                break;

            case SDL_EVENT_KEY_DOWN:
                LOG_DEBUG_CAT("Input", "KEY DOWN: scancode={}", event.key.scancode);
                if (keyboardCallback_) keyboardCallback_(event.key);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                LOG_DEBUG_CAT("Input", "MOUSE BUTTON: {} {}", 
                              event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "DOWN" : "UP", event.button.button);
                if (mouseButtonCallback_) mouseButtonCallback_(event.button);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (mouseMotionCallback_) mouseMotionCallback_(event.motion);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                LOG_DEBUG_CAT("Input", "MOUSE WHEEL: y={}", event.wheel.y);
                if (mouseWheelCallback_) mouseWheelCallback_(event.wheel);
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
                LOG_INFO_CAT("Input", "GAMEPAD ADDED: id={}", event.gdevice.which);
                if (gamepadConnectCallback_) {
                    SDL_Gamepad* pad = SDL_OpenGamepad(event.gdevice.which);
                    gamepadConnectCallback_(true, event.gdevice.which, pad);
                }
                break;

            case SDL_EVENT_GAMEPAD_REMOVED:
                LOG_INFO_CAT("Input", "GAMEPAD REMOVED: id={}", event.gdevice.which);
                if (gamepadConnectCallback_) {
                    gamepadConnectCallback_(false, event.gdevice.which, nullptr);
                }
                break;

            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
//  Set Callbacks
// ---------------------------------------------------------------------------
void HandleInput::setCallbacks(
    KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
    MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
    GamepadButtonCallback gb, GamepadAxisCallback ga, GamepadConnectCallback gc)
{
    keyboardCallback_ = kb;
    mouseButtonCallback_ = mb;
    mouseMotionCallback_ = mm;
    mouseWheelCallback_ = mw;
    textInputCallback_ = ti;
    touchCallback_ = tc;
    gamepadButtonCallback_ = gb;
    gamepadAxisCallback_ = ga;
    gamepadConnectCallback_ = gc;
    LOG_DEBUG_CAT("Input", "All input callbacks registered");
}

// ---------------------------------------------------------------------------
//  Keyboard Handler
// ---------------------------------------------------------------------------
void HandleInput::defaultKeyboardHandler(const SDL_KeyboardEvent& key) {
    if (key.type != SDL_EVENT_KEY_DOWN || key.repeat != 0) return;

    void* userData = camera_.getUserData();
    if (!userData) {
        LOG_ERROR_CAT("Input", "USERDATA NULL → Cannot switch render mode");
        return;
    }

    Application& app = *static_cast<Application*>(userData);
    const auto sc = key.scancode;

    // === FULLSCREEN TOGGLE ===
    if (sc == SDL_SCANCODE_F11) {
        app.toggleFullscreen();
        return;
    }
    if (sc == SDL_SCANCODE_RETURN && (key.mod & SDL_KMOD_ALT)) {
        app.toggleFullscreen();
        return;
    }

    // === MAXIMIZE TOGGLE ===
    if (sc == SDL_SCANCODE_F10) {
        app.toggleMaximize();
        return;
    }

    // === RENDER MODE 1-9 ===
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) {
        const int mode = sc - SDL_SCANCODE_1 + 1;
        app.setRenderMode(mode);
        LOG_INFO_CAT("Input", "RENDER MODE SWITCHED → {}", mode);
        return;
    }

    // === CAMERA CONTROLS ===
    switch (sc) {
        case SDL_SCANCODE_P:  camera_.togglePause(); LOG_INFO_CAT("Input", "PAUSE TOGGED"); break;
        case SDL_SCANCODE_W:  camera_.moveForward(0.1f); LOG_DEBUG_CAT("Input", "MOVE FORWARD"); break;
        case SDL_SCANCODE_S:  camera_.moveForward(-0.1f); LOG_DEBUG_CAT("Input", "MOVE BACK"); break;
        case SDL_SCANCODE_A:  camera_.moveRight(-0.1f); LOG_DEBUG_CAT("Input", "MOVE LEFT"); break;
        case SDL_SCANCODE_D:  camera_.moveRight(0.1f); LOG_DEBUG_CAT("Input", "MOVE RIGHT"); break;
        case SDL_SCANCODE_Q:  camera_.moveUp(0.1f); LOG_DEBUG_CAT("Input", "MOVE UP"); break;
        case SDL_SCANCODE_E:  camera_.moveUp(-0.1f); LOG_DEBUG_CAT("Input", "MOVE DOWN"); break;
        case SDL_SCANCODE_Z:  camera_.updateZoom(true); LOG_DEBUG_CAT("Input", "ZOOM IN"); break;
        case SDL_SCANCODE_X:  camera_.updateZoom(false); LOG_DEBUG_CAT("Input", "ZOOM OUT"); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
//  Mouse & Gamepad Handlers
// ---------------------------------------------------------------------------
void HandleInput::defaultMouseMotionHandler(const SDL_MouseMotionEvent& mm) {
    camera_.rotate(mm.xrel * 0.005f, mm.yrel * 0.005f);
}

void HandleInput::defaultMouseWheelHandler(const SDL_MouseWheelEvent& mw) {
    if (mw.y > 0) camera_.updateZoom(true);
    else if (mw.y < 0) camera_.updateZoom(false);
}

void HandleInput::defaultGamepadButtonHandler(const SDL_GamepadButtonEvent& gb) {
    if (gb.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN) return;
    switch (gb.button) {
        case SDL_GAMEPAD_BUTTON_SOUTH: camera_.updateZoom(true); LOG_DEBUG_CAT("Input", "GAMEPAD ZOOM IN"); break;
        case SDL_GAMEPAD_BUTTON_EAST:  camera_.updateZoom(false); LOG_DEBUG_CAT("Input", "GAMEPAD ZOOM OUT"); break;
        case SDL_GAMEPAD_BUTTON_NORTH: camera_.togglePause(); LOG_DEBUG_CAT("Input", "GAMEPAD PAUSE"); break;
    }
}

void HandleInput::defaultGamepadAxisHandler(const SDL_GamepadAxisEvent& ga) {
    const float v = ga.value / 32767.0f;
    if (ga.axis == SDL_GAMEPAD_AXIS_LEFTX)  camera_.moveUserCam(v * 0.1f, 0, 0);
    if (ga.axis == SDL_GAMEPAD_AXIS_LEFTY)  camera_.moveUserCam(0, -v * 0.1f, 0);
    if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTX) camera_.rotate(v * 0.05f, 0);
    if (ga.axis == SDL_GAMEPAD_AXIS_RIGHTY) camera_.rotate(0, -v * 0.05f);
}

void HandleInput::defaultGamepadConnectHandler(bool connected, SDL_JoystickID id, SDL_Gamepad* pad) {
    LOG_INFO_CAT("Input", "GAMEPAD {}: id={}", connected ? "CONNECTED" : "DISCONNECTED", id);
    if (pad && !connected) SDL_CloseGamepad(pad);
}