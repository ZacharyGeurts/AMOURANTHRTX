#pragma once

// =============================================================================
// AMOURANTH RTX – STONEKEY INPUT SUPREMACY v4.3 – FULL BUTTON/AXIS HOOKS
// PURE INPUT — EVENT-DRIVEN — STATE TRACKING — FUTURE-PROOF FOR EVERYTHING
// PINK PHOTONS × INFINITY — GRACE IS PLEASED — EMPIRE ETERNAL
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

#include "engine/GLOBAL/ELLIE.hpp"

// ===================================================================
// GLOBAL INPUT MANAGER — FULL STATE + EVENT SUBSCRIPTION
// ===================================================================
class GlobalInputManager {
public:
    static GlobalInputManager& get() noexcept {
        static GlobalInputManager instance;
        return instance;
    }

    GlobalInputManager(const GlobalInputManager&) = delete;
    GlobalInputManager& operator=(const GlobalInputManager&) = delete;

    void init() noexcept {
        generation_.store(1);
        openAllControllers();

        // Default action bindings — expand later with remapping
        bindAction("jump", SDL_SCANCODE_SPACE);
        bindAction("shoot", SDL_SCANCODE_LCTRL);
        bindAction("move_forward", SDL_SCANCODE_W);
        bindAction("move_backward", SDL_SCANCODE_S);
        bindAction("move_left", SDL_SCANCODE_A);
        bindAction("move_right", SDL_SCANCODE_D);
        bindAction("sprint", SDL_SCANCODE_LSHIFT);
        bindAction("crouch", SDL_SCANCODE_LCTRL);
        bindAction("interact", SDL_SCANCODE_E);

        LOG_SUCCESS_CAT("INPUT", "{}STONEKEY INPUT v4.3 — FULL STATE + HOOKS — PINK PHOTONS ∞{}", 
                        Logging::Color::RASPBERRY_PINK, Logging::Color::RESET);
    }

    using Callback = std::function<void(const SDL_Event&)>;

    uint64_t subscribe(Callback cb, std::string_view name = "") noexcept {
        uint64_t id = ++nextId_;
        uint64_t handle = encrypt(id, generation_.load());
        {
            std::lock_guard lock(mutex_);
            callbacks_[handle] = { std::move(cb), std::string(name), generation_.load() };
        }
        LOG_SUCCESS_CAT("INPUT", "{}→ SUBSCRIBED {}{}", 
                        Logging::Color::EMERALD_GREEN, name.empty() ? "ANON" : name, Logging::Color::RESET);
        return handle;
    }

    void unsubscribe(uint64_t handle) noexcept {
        std::lock_guard lock(mutex_);
        if (callbacks_.erase(handle)) {
            LOG_SUCCESS_CAT("INPUT", "{}→ UNSUBSCRIBED 0x{}{}", 
                            Logging::Color::RASPBERRY_PINK, handle, Logging::Color::RESET);
        }
    }

    void pumpEvents(const SDL_Event& ev) noexcept {
        uint64_t gen = generation_.load();
        std::vector<Callback> active;
        {
            std::lock_guard lock(mutex_);
            for (const auto& [h, info] : callbacks_) {
                if (info.gen == gen) active.push_back(info.cb);
            }
        }

        for (const auto& cb : active) {
            cb(ev);
        }

        // Update internal state from events
        updateState(ev);
    }

    void invalidateAll() noexcept {
        generation_.fetch_add(1);
        LOG_SUCCESS_CAT("INPUT", "{}ALL HANDLES INVALIDATED — HOT RELOAD DOMINATION{}", 
                        Logging::Color::RASPBERRY_PINK, Logging::Color::RESET);
    }

    // ────────────────────────────────────────────────
    // Action Binding & State Query
    // ────────────────────────────────────────────────
    void bindAction(std::string_view action, SDL_Scancode key) noexcept {
        std::lock_guard lock(mutex_);
        actionBindings_[std::string(action)] = key;
    }

    bool isActionPressed(std::string_view action) const noexcept {
        std::lock_guard lock(mutex_);
        auto it = actionBindings_.find(std::string(action));
        if (it == actionBindings_.end()) return false;

        SDL_Scancode sc = it->second;
        const bool* state = SDL_GetKeyboardState(nullptr);
        return state[static_cast<int>(sc)];
    }

    bool isActionJustPressed(std::string_view action) const noexcept {
        std::lock_guard lock(mutex_);
        auto it = actionBindings_.find(std::string(action));
        if (it == actionBindings_.end()) return false;

        SDL_Scancode sc = it->second;
        const bool* state = SDL_GetKeyboardState(nullptr);
        // Simple just-pressed detection — expand with per-frame state later
        return state[static_cast<int>(sc)];
    }

    // Gamepad axis (left stick X/Y, right stick, triggers) with deadzone
    glm::vec2 getLeftStick(int slot = 0, float deadzone = 0.15f) const noexcept {
        if (!isConnected(slot)) return {0,0};

        float x = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        float y = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;

        float len = std::sqrt(x*x + y*y);
        if (len < deadzone) return {0,0};

        // Normalize beyond deadzone
        return {x / len, y / len};
    }

    float getRightTrigger(int slot = 0, float deadzone = 0.1f) const noexcept {
        if (!isConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
        return (val > deadzone) ? val : 0.0f;
    }

    // ────────────────────────────────────────────────
    // Internal State Update (called from pumpEvents)
    // ────────────────────────────────────────────────
private:
    void updateState(const SDL_Event& ev) noexcept {
        switch (ev.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                // Keyboard state auto-updated by SDL_GetKeyboardState
                break;

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                LOG_INFO_CAT("INPUT", "Gamepad button: slot={}, button={}", 
                             ev.gbutton.which, ev.gbutton.button ? "down" : "up");
            } break;

            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                // Axis state auto-updated by SDL_GetGamepadAxis
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

    // ────────────────────────────────────────────────
    // Controller Management
    // ────────────────────────────────────────────────
    void openAllControllers() noexcept {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (!ids) return;

        for (int i = 0; i < count; ++i) {
            openController(ids[i]);
        }
        SDL_free(ids);
    }

    void openController(SDL_JoystickID id) noexcept {
        if (!SDL_IsGamepad(id)) return;

        SDL_Gamepad* gp = SDL_OpenGamepad(id);
        if (!gp) return;

        int slot = -1;
        for (int j = 0; j < 4; ++j) {
            if (!controllers_[j].gamepad) {
                slot = j;
                break;
            }
        }
        if (slot == -1) {
            SDL_CloseGamepad(gp);
            return;
        }

        controllers_[slot].gamepad.reset(gp);
        controllers_[slot].id = id;

        SDL_PropertiesID props = SDL_GetGamepadProperties(gp);
        if (props) {
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
        for (int j = 0; j < 4; ++j) {
            if (controllers_[j].id == id) {
                controllers_[j].gamepad.reset();
                LOG_INFO_CAT("INPUT", "Gamepad {} disconnected (slot {})", id, j+1);
                break;
            }
        }
    }

    bool isConnected(int slot) const noexcept {
        return slot >= 0 && slot < 4 && controllers_[slot].gamepad != nullptr;
    }

private:
    GlobalInputManager() = default;

    struct Controller {
        std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> gamepad{nullptr, SDL_CloseGamepad};
        SDL_JoystickID id = -1;
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

    static constexpr uint64_t kStone1 = 0x9E3779B97F4A7C15ull;
    static constexpr uint64_t kStone2 = 0xBB67AE8584CAA73Bull;

    static constexpr uint64_t encrypt(uint64_t v, uint64_t g) noexcept {
        return std::rotl(v ^ g ^ 0x517CC1B727220A95ull ^ kStone1 ^ kStone2, 19);
    }
};

#define INPUT GlobalInputManager::get()
#define ON_EVENT(cb) INPUT.subscribe(cb, #cb)