#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Global Input Manager (C) 2025-2026 Zachary Geurts
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include <SDL3/SDL.h>
#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <string>
#include <string_view>
#include <memory>
#include <array>
#include <vector>
#include <optional>
#include <glm/glm.hpp>

#include "ELLIE.hpp"
#include "Camera.hpp"

// ===================================================================
// GLOBAL INPUT MANAGER — FULL STATE + EVENT SUBSCRIPTION + FPS CONTROLS
// ===================================================================
class GlobalInputManager {
public:
    static GlobalInputManager& get() noexcept {
        static GlobalInputManager instance;
        return instance;
    }

    GlobalInputManager(const GlobalInputManager&) = delete;
    GlobalInputManager& operator=(const GlobalInputManager&) = delete;

    // Init now takes the window (called once from navigator_main)
    void init(SDL_Window* window) noexcept {
        window_ = window;
        generation_.store(1);
        openAllControllers();

        // Default FPS bindings (remappable via menu later)
        bindAction("jump",          SDL_SCANCODE_SPACE);
        bindAction("shoot",         SDL_SCANCODE_LCTRL);
        bindAction("move_forward",  SDL_SCANCODE_W);
        bindAction("move_backward", SDL_SCANCODE_S);
        bindAction("move_left",     SDL_SCANCODE_A);
        bindAction("move_right",    SDL_SCANCODE_D);
        bindAction("sprint",        SDL_SCANCODE_LSHIFT);
        bindAction("crouch",        SDL_SCANCODE_LCTRL);
        bindAction("interact",      SDL_SCANCODE_E);

        LOG_SUCCESS_CAT("INPUT", "{}SECURE INPUT — FULL STATE + HOOKS + FPS CONTROLS{}", 
                        Logging::Color::RASPBERRY_PINK, Logging::Color::RESET);
    }

    using Callback = std::function<void(const SDL_Event&)>;

    uint64_t subscribe(Callback cb, std::string_view name = "") noexcept {
        uint64_t id = ++nextId_;
        uint64_t handle = id ^ generation_.load();  // simple XOR handle
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks_[handle] = { std::move(cb), std::string(name), generation_.load() };
        }
        return handle;
    }

    void unsubscribe(uint64_t handle) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(handle);
    }

    void pumpEvents(const SDL_Event& ev) noexcept {
        uint64_t gen = generation_.load();
        std::vector<Callback> active;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& pair : callbacks_) {
                if (pair.second.gen == gen) active.push_back(pair.second.cb);
            }
        }
        for (const auto& cb : active) cb(ev);

        updateState(ev);
    }

    void invalidateAll() noexcept {
        generation_.fetch_add(1);
        LOG_SUCCESS_CAT("INPUT", "{}ALL HANDLES INVALIDATED — HOT RELOAD{}", 
                        Logging::Color::RASPBERRY_PINK, Logging::Color::RESET);
    }

    // ────────────────────────────────────────────────
    // Mouse Capture & Look (SDL3 native)
    // ────────────────────────────────────────────────
    void captureMouse() noexcept {
        if (window_ == nullptr) {
            LOG_ERROR_CAT("INPUT", "Cannot capture mouse — window not set");
            return;
        }
        SDL_SetWindowRelativeMouseMode(window_, true);
    }

    glm::vec2 getMouseDelta() const noexcept {
        float x = 0.0f, y = 0.0f;
        SDL_GetRelativeMouseState(&x, &y);
        return glm::vec2(x, y);
    }

    // ────────────────────────────────────────────────
    // Action Binding & State Query
    // ────────────────────────────────────────────────
    void bindAction(std::string_view action, SDL_Scancode key) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        actionBindings_[std::string(action)] = key;
    }

    bool isActionPressed(std::string_view action) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actionBindings_.find(std::string(action));
        if (it == actionBindings_.end()) return false;
        const bool* state = SDL_GetKeyboardState(nullptr);
        return state[static_cast<int>(it->second)] != 0;
    }

    bool isActionJustPressed(std::string_view action) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actionBindings_.find(std::string(action));
        if (it == actionBindings_.end()) return false;
        const bool* state = SDL_GetKeyboardState(nullptr);
        return state[static_cast<int>(it->second)] != 0;  // expand with frame state later
    }

    // ────────────────────────────────────────────────
    // FPS Movement Vector (WASD + sprint/crouch)
    // ────────────────────────────────────────────────
    glm::vec3 getMovementVector(float speed, float dt) const noexcept {
        glm::vec3 move(0.0f);
        if (isActionPressed("move_forward"))  move.z -= 1.0f;
        if (isActionPressed("move_backward")) move.z += 1.0f;
        if (isActionPressed("move_left"))     move.x -= 1.0f;
        if (isActionPressed("move_right"))    move.x += 1.0f;

        if (glm::length(move) > 0.001f) move = glm::normalize(move);

        if (isActionPressed("sprint")) move *= 2.2f;
        if (isActionPressed("crouch")) move *= 0.5f;

        return move * speed * dt;
    }

    // ────────────────────────────────────────────────
    // Controller Support (Xbox 360 / compatible gamepads)
    // ────────────────────────────────────────────────
    bool isControllerConnected() const noexcept {
        return isConnected(0);  // slot 0 = primary controller
    }

    float getSprintTrigger() const noexcept {
        return getSprintTrigger();  // right trigger for sprint
    }

    bool isControllerSprintPressed() const noexcept {
        return getSprintTrigger() > 0.3f;  // analog trigger > 30% = sprint
    }

    bool isControllerCrouchPressed() const noexcept {
        return SDL_GetGamepadButton(controllers_[0].gamepad.get(), SDL_GAMEPAD_BUTTON_SOUTH) == 0;  // A button = crouch
    }

    bool isControllerJumpPressed() const noexcept {
        return SDL_GetGamepadButton(controllers_[0].gamepad.get(), SDL_GAMEPAD_BUTTON_EAST) == 0;  // B button = jump
    }

    bool isControllerShootPressed() const noexcept {
        return SDL_GetGamepadButton(controllers_[0].gamepad.get(), SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) == 0;  // RB = shoot
    }

    // Mousewheel zoom (notched in/out)
    bool isZoomInJustPressed() const noexcept {
        return isActionJustPressed("zoom_in");
    }

    bool isZoomOutJustPressed() const noexcept {
        return isActionJustPressed("zoom_out");
    }

private:
    void updateState(const SDL_Event& ev) noexcept {
        // Handle real-time input state updates here
        switch (ev.type) {
            case SDL_EVENT_MOUSE_MOTION: {
                mouseDelta = glm::vec2(ev.motion.xrel, ev.motion.yrel);
                break;
            }

            case SDL_EVENT_MOUSE_WHEEL: {
                // Mouse wheel for zoom notches (handled in navigator_main via isZoomIn/OutJustPressed)
                break;
            }

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                // Gamepad states queried directly — no need to cache here
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
                openController(ev.gdevice.which);
                break;

            case SDL_EVENT_GAMEPAD_REMOVED:
                closeController(ev.gdevice.which);
                break;

            default:
                break;
        }
    }

    void openAllControllers() noexcept {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids == nullptr) return;

        for (int i = 0; i < count; ++i) {
            openController(ids[i]);
        }
        SDL_free(ids);
    }

    void openController(SDL_JoystickID id) noexcept {
        if (!SDL_IsGamepad(id)) return;

        SDL_Gamepad* gp = SDL_OpenGamepad(id);
        if (gp == nullptr) return;

        size_t slot = 4;
        for (size_t j = 0; j < controllers_.size(); ++j) {
            if (controllers_[j].gamepad == nullptr) {
                slot = j;
                break;
            }
        }
        if (slot == 4) {
            SDL_CloseGamepad(gp);
            return;
        }

        controllers_[slot].gamepad.reset(gp);
        controllers_[slot].id = id;

        SDL_PropertiesID props = SDL_GetGamepadProperties(gp);
        if (props != 0) {
            controllers_[slot].rumbleSupported = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
        }
        controllers_[slot].gyroSupported = SDL_GamepadHasSensor(gp, SDL_SENSOR_GYRO);

        const char* name = SDL_GetGamepadName(gp);
        LOG_SUCCESS_CAT("INPUT", "{}CONTROLLER {} CONNECTED — {}{}{}", 
                        Logging::Color::ELECTRIC_BLUE, slot+1, name ? name : "Unknown",
                        controllers_[slot].rumbleSupported ? " RUMBLE" : "",
                        controllers_[slot].gyroSupported ? " GYRO" : "",
                        Logging::Color::RESET);
    }

    void closeController(SDL_JoystickID id) noexcept {
        for (size_t j = 0; j < controllers_.size(); ++j) {
            if (controllers_[j].id == id) {
                controllers_[j].gamepad.reset();
                LOG_INFO_CAT("INPUT", "Gamepad {} disconnected (slot {})", id, j+1);
                break;
            }
        }
    }

    bool isConnected(int slot) const noexcept {
        if (slot < 0 || slot >= static_cast<int>(controllers_.size())) return false;
        return controllers_[static_cast<size_t>(slot)].gamepad != nullptr;
    }

private:
    GlobalInputManager() = default;

    struct Controller {
        std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> gamepad{nullptr, SDL_CloseGamepad};
        SDL_JoystickID id = 0;
        bool rumbleSupported = false;
        bool gyroSupported = false;
    };

    struct Subscription {
        Callback cb;
        std::string name;
        uint64_t gen = 0;
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, Subscription> callbacks_;
    std::array<Controller, 4> controllers_;

    std::unordered_map<std::string, SDL_Scancode> actionBindings_;

    std::atomic<uint64_t> generation_{1};
    std::atomic<uint64_t> nextId_{0};

    SDL_Window* window_ = nullptr;  // set at init
    glm::vec2 mouseDelta{0.0f, 0.0f};
};

#define INPUT GlobalInputManager::get()
#define ON_EVENT(cb) INPUT.subscribe(cb, #cb)