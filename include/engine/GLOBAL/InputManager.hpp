// =============================================================================
// AMOURANTH RTX – DECEMBER 08 2025 – STONEKEY INPUT SUPREMACY v4 (SDL3 FIXED)
// FULL SDL3 — 4 CONTROLLERS — TRIGGERS — RUMBLE — GYRO — KEYBOARD — MOUSE
// ONE-SHOT MODE KEYS (1–9) WITH 0.5s PRESS WINDOW — NO FLICKER
// PINK PHOTONS × INFINITY × 4 — FIRST LIGHT ETERNAL
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <atomic>
#include <mutex>
#include <array>
#include <unordered_map>
#include <string>
#include <string_view>

#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

// ===================================================================
// GLOBAL INPUT MANAGER — SDL3 NATIVE — FULLY STONEKEYED
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

        LOG_SUCCESS_CAT("INPUT", "{}STONEKEY INPUT v4 ONLINE — SDL3 — 4× CONTROLLERS — ONE-SHOT KEYS 1-9 — PINK PHOTONS ∞×4{}", 
                        RASPBERRY_PINK, RESET);
    }

    using Callback = std::function<void(const SDL_Event&)>;

    uint64_t subscribe(Callback cb, std::string_view name = "") noexcept {
        uint64_t id = ++nextId_;
        uint64_t handle = encrypt(id, generation_.load());

        {
            std::lock_guard lock(mutex_);
            callbacks_[handle] = { std::move(cb), std::string(name), generation_.load() };
        }

        LOG_SUCCESS_CAT("INPUT", "{}→ SUBSCRIBED {}{}", EMERALD_GREEN, name.empty() ? "ANON" : name, RESET);
        return handle;
    }

    void unsubscribe(uint64_t handle) noexcept {
        std::lock_guard lock(mutex_);
        if (callbacks_.erase(handle)) {
            LOG_SUCCESS_CAT("INPUT", "{}→ UNSUBSCRIBED 0x{:016X}{}", RASPBERRY_PINK, handle, RESET);
        }
    }

    // Updated signature to avoid incomplete type issues and pass required SDL3 params
    void pumpEvents(float dt, std::function<void(int)> setRenderMode, SDL_Window* window) noexcept {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            uint64_t gen = generation_.load();
            std::vector<Callback> active;

            {
                std::lock_guard lock(mutex_);
                for (const auto& [h, info] : callbacks_) {
                    if (info.gen == gen) active.push_back(info.cb);
                }
            }

            for (const auto& cb : active) cb(ev);
        }

        updateDefaultControls(dt, setRenderMode, window);
    }

    void invalidateAll() noexcept {
        generation_.fetch_add(1);
        LOG_SUCCESS_CAT("INPUT", "{}ALL HANDLES INVALIDATED — HOT RELOAD DOMINATION{}", RASPBERRY_PINK, RESET);
    }

    // ── CONTROLLER API — FULL SDL3 SUPPORT ─────────────────────────────────
    bool isConnected(int idx) const noexcept { return idx >= 0 && idx < 4 && controllers_[idx].gamepad; }

    float leftStickX(int idx)  const noexcept { return getAxis(idx, SDL_GAMEPAD_AXIS_LEFTX); }
    float leftStickY(int idx)  const noexcept { return getAxis(idx, SDL_GAMEPAD_AXIS_LEFTY); }
    float rightStickX(int idx) const noexcept { return getAxis(idx, SDL_GAMEPAD_AXIS_RIGHTX); }
    float rightStickY(int idx) const noexcept { return getAxis(idx, SDL_GAMEPAD_AXIS_RIGHTY); }
    float leftTrigger(int idx) const noexcept { return getAxis(idx, SDL_GAMEPAD_AXIS_LEFT_TRIGGER); }
    float rightTrigger(int idx) const noexcept { return getAxis(idx, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER); }

    bool button(int idx, SDL_GamepadButton btn) const noexcept {
        return isConnected(idx) && SDL_GetGamepadButton(controllers_[idx].gamepad, btn);
    }

    void rumble(int idx, float strength, uint32_t ms = 300) noexcept {
        if (isConnected(idx) && controllers_[idx].rumbleSupported) {
            SDL_RumbleGamepad(controllers_[idx].gamepad, Uint16(strength * 65535), Uint16(strength * 65535), ms);
        }
    }

private:
    GlobalInputManager() = default;

    struct Controller {
        SDL_Gamepad* gamepad = nullptr;
        SDL_JoystickID id = -1;
        bool rumbleSupported = false;
        bool gyroSupported = false;
    };

    struct Sub {
        Callback cb;
        std::string name;
        uint64_t gen = 0;
    };

    // One-shot state for keys 1–9 (0.5 second press window)
    struct ModeKeyState {
        bool  down  = false;
        float timer = 0.0f;
        static constexpr float maxPressTime = 0.5f;
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, Sub> callbacks_;
    std::array<Controller, 4> controllers_;
    std::array<ModeKeyState, 9> modeKeys_{};  // Keys 1–9
    std::atomic<uint64_t> generation_{1};
    std::atomic<uint64_t> nextId_{0};

    float getAxis(int idx, SDL_GamepadAxis axis) const noexcept {
        if (!isConnected(idx)) return 0.0f;
        Sint16 raw = SDL_GetGamepadAxis(controllers_[idx].gamepad, axis);
        return raw / 32767.0f;
    }

    void openAllControllers() noexcept {
        // SDL3: Use SDL_GetGamepads instead of SDL_GetNumGamepads
        int numJoysticks = 0;
        SDL_JoystickID* joysticks = SDL_GetGamepads(&numJoysticks);

        for (int i = 0; i < numJoysticks && i < 32; ++i)
        {
            SDL_Gamepad* pad = SDL_OpenGamepad(joysticks[i]);
            if (!pad) continue;

            int slot = -1;
            for (int j = 0; j < 4; ++j)
            {
                if (!controllers_[j].gamepad) {
                    slot = j;
                    break;
                }
            }
            if (slot == -1) {
                SDL_CloseGamepad(pad);
                continue;
            }

            controllers_[slot].gamepad          = pad;
            controllers_[slot].id               = joysticks[i];
            controllers_[slot].rumbleSupported  = false;
            SDL_PropertiesID props = SDL_GetGamepadProperties(pad);
            if (props != 0) {
                controllers_[slot].rumbleSupported = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
            }
            controllers_[slot].gyroSupported    = SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO);

            LOG_SUCCESS_CAT("INPUT", "{}CONTROLLER {} → {}{}{}", 
                            ELECTRIC_BLUE, slot+1, SDL_GetGamepadName(pad), 
                            controllers_[slot].rumbleSupported ? " RUMBLE" : "", RESET);
        }

        if (joysticks) SDL_free(joysticks);
    }

    // Updated signature to avoid incomplete type issues and pass required SDL3 params
    void updateDefaultControls(float dt, std::function<void(int)> setRenderMode, SDL_Window* window) noexcept {
        const float moveSpeed = 20.0f;
        const float lookSens  = 0.1f;

        // ── ONE-SHOT MODE KEYS 1–9 (0.5s press window) ──
        int numKeys = 0;
        const bool* keys = SDL_GetKeyboardState(&numKeys);  // SDL3: returns const bool*

        for (int i = 0; i < 9; ++i)
        {
            const int sc = SDL_SCANCODE_1 + i;
            if (sc >= numKeys) continue;

            bool pressed = keys[sc];

            if (pressed && !modeKeys_[i].down)
            {
                modeKeys_[i].down  = true;
                modeKeys_[i].timer = 0.0f;
            }
            else if (!pressed && modeKeys_[i].down)
            {
                if (modeKeys_[i].timer <= ModeKeyState::maxPressTime)
                {
                    if (setRenderMode) setRenderMode(i + 1);
                }

                modeKeys_[i] = {};  // Reset
            }

            if (modeKeys_[i].down)
            {
                modeKeys_[i].timer += dt;
            }
        }

        // ── MOUSE LOOK ──
        if (window && SDL_GetWindowRelativeMouseMode(window))
        {
            float dx = 0.0f, dy = 0.0f;
            SDL_GetRelativeMouseState(&dx, &dy);
            if (dx != 0.0f || dy != 0.0f)
            {
                CAM.rotate(-dx * lookSens, -dy * lookSens);
            }
        }

        // ── KEYBOARD MOVEMENT ──
        if (keys[SDL_SCANCODE_W]) CAM.moveForward( moveSpeed * dt);
        if (keys[SDL_SCANCODE_S]) CAM.moveForward(-moveSpeed * dt);
        if (keys[SDL_SCANCODE_A]) CAM.moveRight(   -moveSpeed * dt);
        if (keys[SDL_SCANCODE_D]) CAM.moveRight(    moveSpeed * dt);
        if (keys[SDL_SCANCODE_SPACE])  CAM.moveUp( moveSpeed * dt);
        if (keys[SDL_SCANCODE_LCTRL])  CAM.moveUp(-moveSpeed * dt);

        // ── CONTROLLER 0 MOVEMENT & LOOK ──
        if (isConnected(0))
        {
            float lx = leftStickX(0), ly = leftStickY(0);
            float rx = rightStickX(0), ry = rightStickY(0);

            if (std::abs(lx) > 0.15f || std::abs(ly) > 0.15f)
            {
                CAM.moveForward(-ly * moveSpeed * dt);
                CAM.moveRight(  lx * moveSpeed * dt);
            }
            if (std::abs(rx) > 0.15f || std::abs(ry) > 0.15f)
            {
                CAM.rotate(rx * 3.0f, ry * 3.0f);
            }
        }

        // ── TOGGLE MOUSE CAPTURE (F key) ──
        static bool fDown = false;
        if (keys[SDL_SCANCODE_F] && !fDown)
        {
            if (window) {
                bool captured = SDL_GetWindowRelativeMouseMode(window);
                SDL_SetWindowRelativeMouseMode(window, !captured);
                LOG_SUCCESS_CAT("INPUT", "{}MOUSE CAPTURE → {}{}", RASPBERRY_PINK, captured ? "OFF" : "ON", RESET);
            }
            fDown = true;
        }
        else if (!keys[SDL_SCANCODE_F])
        {
            fDown = false;
        }
    }

    static constexpr uint64_t encrypt(uint64_t v, uint64_t g) noexcept {
        return std::rotl(v ^ g ^ 0x517CC1B727220A95ull ^ kStone1 ^ kStone2, 19);
    }
};

// GLOBAL MACROS — CLEAN AND ETERNAL
#define INPUT GlobalInputManager::get()
#define ON_EVENT(cb) INPUT.subscribe(cb, #cb)