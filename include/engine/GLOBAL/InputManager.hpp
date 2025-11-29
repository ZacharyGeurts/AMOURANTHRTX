// =============================================================================
// AMOURANTH RTX – NOVEMBER 29 2025 – STONEKEY INPUT SUPREMACY v3
// FULL SDL3 — 4 CONTROLLERS — TRIGGERS — RUMBLE — GYRO — KEYBOARD — MOUSE
// NO LAZYCAM — CAM ONLY — CIRCULAR INCLUDE HELL OBLITERATED
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

class Application;  // ← Forward declare only — NO MORE CIRCULAR INCLUDE

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

        LOG_SUCCESS_CAT("INPUT", "{}STONEKEY INPUT v3 ONLINE — SDL3 — 4× CONTROLLER — TRIGGERS — RUMBLE GYRO — PINK PHOTONS ∞×4{}", 
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

    void pumpEvents(Application& app) noexcept {
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

        updateDefaultControls(app);
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
    float rightTrigger(int idx)const noexcept { return getAxis(idx, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER); }

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

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, Sub> callbacks_;
    std::array<Controller, 4> controllers_;
    std::atomic<uint64_t> generation_{1};
    std::atomic<uint64_t> nextId_{0};

    float getAxis(int idx, SDL_GamepadAxis axis) const noexcept {
        if (!isConnected(idx)) return 0.0f;
        Sint16 raw = SDL_GetGamepadAxis(controllers_[idx].gamepad, axis);
        return raw / 32767.0f;
    }

    void openAllControllers() noexcept {
        int n = SDL_GetNumGamepads();
        for (int i = 0; i < n && i < 32; ++i) {
            if (!SDL_IsGamepad(i)) continue;

            SDL_Gamepad* pad = SDL_OpenGamepad(i);
            if (!pad) continue;

            int slot = -1;
            for (int j = 0; j < 4; ++j) {
                if (!controllers_[j].gamepad) { slot = j; break; }
            }
            if (slot == -1) { SDL_CloseGamepad(pad); continue; }

            controllers_[slot] = {
                .gamepad = pad,
                .id = SDL_GetGamepadInstanceID(pad),
                .rumbleSupported = SDL_GamepadHasRumble(pad),
                .gyroSupported = SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO)
            };

            LOG_SUCCESS_CAT("INPUT", "{}CONTROLLER {} → {}{}{}", 
                            ELECTRIC_BLUE, slot+1, SDL_GetGamepadName(pad), 
                            controllers_[slot].rumbleSupported ? " RUMBLE" : "",
                            RESET);
        }
    }

    void updateDefaultControls(Application& app) noexcept {
        const float moveSpeed = 20.0f;
        const float lookSens = 0.1f;

        // Mouse
        if (SDL_GetRelativeMouseMode()) {
            float dx, dy;
            SDL_GetRelativeMouseState(&dx, &dy);
            if (dx || dy) {
                CAM.rotate(-dx * lookSens, -dy * lookSens);
            }
        }

        // Keyboard
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_W]) CAM.moveForward( moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_S]) CAM.moveForward(-moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_A]) CAM.moveRight(  -moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_D]) CAM.moveRight(   moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_SPACE])  CAM.moveUp( moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_LCTRL])   CAM.moveUp(-moveSpeed * app.deltaTime);

        // Controller 0
        if (isConnected(0)) {
            float lx = leftStickX(0), ly = leftStickY(0);
            float rx = rightStickX(0), ry = rightStickY(0);

            if (std::abs(lx) > 0.15f || std::abs(ly) > 0.15f) {
                CAM.moveForward(-ly * moveSpeed * app.deltaTime);
                CAM.moveRight(  lx * moveSpeed * app.deltaTime);
            }
            if (std::abs(rx) > 0.15f || std::abs(ry) > 0.15f) {
                CAM.rotate(rx * 3.0f, ry * 3.0f);
            }
        }

        // Toggle mouse capture with F
        static bool fDown = false;
        if (keys[SDL_SCANCODE_F] && !fDown) {
            bool on = SDL_GetRelativeMouseMode();
            SDL_SetRelativeMouseMode(on ? SDL_FALSE : SDL_TRUE);
            LOG_SUCCESS_CAT("INPUT", "{}MOUSE CAPTURE → {}{}", RASPBERRY_PINK, on ? "OFF" : "ON", RESET);
            fDown = true;
        } else if (!keys[SDL_SCANCODE_F]) {
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

#endif // INCLUDE_GUARD