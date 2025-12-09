// =============================================================================
// AMOURANTH RTX – DECEMBER 08 2025 – STONEKEY INPUT SUPREMACY v4 FINAL
// MODE 1 (PRESS 1) = PURE HDR ENVMAP DISPLAY — FIRST TRUE LIGHT ACHIEVED
// NO APPLICATION CLASS ACCESS — USES GLOBAL RENDERER POINTER INSTEAD
// PINK PHOTONS × INFINITY × 4 — GRACE IS PLEASED — EMPIRE ETERNAL
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
#include <cmath>

#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/logging.hpp"

// Forward declarations only
class VulkanRenderer;
extern std::unique_ptr<class Application> g_app_ptr;

// NEW: Global direct pointer to renderer — set once in main.cpp
extern VulkanRenderer* g_renderer_ptr;

using namespace Logging::Color;

// ===================================================================
// GLOBAL INPUT MANAGER — FINAL — MODE 1 = HDR SKY — NO APPLICATION DEPENDENCY
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

        LOG_SUCCESS_CAT("INPUT", "{}STONEKEY INPUT v4 FINAL — MODE 1 = HDR SKY — PINK PHOTONS ∞×4{}", 
                        RASPBERRY_PINK, RESET);
    }

    using Callback = std::function<void(const SDL_Event&)>;

    uint64_t subscribe(Callback cb, std::string_view name = "") noexcept {
        uint64_t id = ++nextId_;
        uint64_t handle = encrypt(id, generation_.load());
        { std::lock_guard lock(mutex_); callbacks_[handle] = { std::move(cb), std::string(name), generation_.load() }; }
        LOG_SUCCESS_CAT("INPUT", "{}→ SUBSCRIBED {}{}", EMERALD_GREEN, name.empty() ? "ANON" : name, RESET);
        return handle;
    }

    void unsubscribe(uint64_t handle) noexcept {
        std::lock_guard lock(mutex_);
        if (callbacks_.erase(handle)) {
            LOG_SUCCESS_CAT("INPUT", "{}→ UNSUBSCRIBED 0x{:016X}{}", RASPBERRY_PINK, handle, RESET);
        }
    }

    void pumpEvents(float dt, std::function<void(int)> setRenderMode, SDL_Window* window) noexcept {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            uint64_t gen = generation_.load();
            std::vector<Callback> active;
            { std::lock_guard lock(mutex_); for (const auto& [h, info] : callbacks_) if (info.gen == gen) active.push_back(info.cb); }
            for (const auto& cb : active) cb(ev);
        }
        updateDefaultControls(dt, setRenderMode, window);
    }

    void invalidateAll() noexcept {
        generation_.fetch_add(1);
        LOG_SUCCESS_CAT("INPUT", "{}ALL HANDLES INVALIDATED — HOT RELOAD DOMINATION{}", RASPBERRY_PINK, RESET);
    }

private:
    GlobalInputManager() = default;

    struct Controller { SDL_Gamepad* gamepad = nullptr; SDL_JoystickID id = -1; bool rumbleSupported = false; bool gyroSupported = false; };
    struct Sub { Callback cb; std::string name; uint64_t gen = 0; };
    struct ModeKeyState { bool down = false; float timer = 0.0f; static constexpr float maxPressTime = 0.5f; };

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, Sub> callbacks_;
    std::array<Controller, 4> controllers_;
    std::array<ModeKeyState, 9> modeKeys_{};  // Keys 2–9
    std::atomic<uint64_t> generation_{1};
    std::atomic<uint64_t> nextId_{0};

    bool isConnected(int idx) const noexcept { return idx >= 0 && idx < 4 && controllers_[idx].gamepad; }

    float getAxis(int idx, SDL_GamepadAxis axis) const noexcept {
        if (!isConnected(idx)) return 0.0f;
        return SDL_GetGamepadAxis(controllers_[idx].gamepad, axis) / 32767.0f;
    }

    void openAllControllers() noexcept {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        for (int i = 0; i < count && i < 32; ++i) {
            SDL_Gamepad* pad = SDL_OpenGamepad(ids[i]);
            if (!pad) continue;
            int slot = -1;
            for (int j = 0; j < 4; ++j) if (!controllers_[j].gamepad) { slot = j; break; }
            if (slot == -1) { SDL_CloseGamepad(pad); continue; }

            controllers_[slot] = { pad, ids[i] };
            SDL_PropertiesID props = SDL_GetGamepadProperties(pad);
            if (props) controllers_[slot].rumbleSupported = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
            controllers_[slot].gyroSupported = SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO);

            LOG_SUCCESS_CAT("INPUT", "{}CONTROLLER {} → {}{}{}", ELECTRIC_BLUE, slot+1, SDL_GetGamepadName(pad),
                            controllers_[slot].rumbleSupported ? " RUMBLE" : "", RESET);
        }
        if (ids) SDL_free(ids);
    }

    void updateDefaultControls(float dt, std::function<void(int)> setRenderMode, SDL_Window* window) noexcept {
        const float moveSpeed = 20.0f;
        const float lookSens  = 0.1f;

        int numKeys = 0;
        const bool* keys = SDL_GetKeyboardState(&numKeys);

        // MODE 1: PURE HDR ENVMAP — PRESS 1
        {
            static bool key1Down = false;
            bool pressed = (SDL_SCANCODE_1 < numKeys) && keys[SDL_SCANCODE_1];

            if (pressed && !key1Down) {
                key1Down = true;
                if (g_renderer_ptr) {
                    g_renderer_ptr->debugShowEnvMapOnly_ = !g_renderer_ptr->debugShowEnvMapOnly_;
                    g_renderer_ptr->requestAccumulationReset();

                    LOG_AMOURANTH(
                        "{}MODE 1 — RAW HDR ENVMAP {} — FIRST TRUE LIGHT ACHIEVED{}",
                        RASPBERRY_PINK,
                        g_renderer_ptr->debugShowEnvMapOnly_ ? "ENGAGED" : "DISENGAGED",
                        RESET
                    );
                }
            } else if (!pressed) {
                key1Down = false;
            }
        }

        // MODES 2–9
        for (int i = 1; i < 9; ++i) {
            int sc = SDL_SCANCODE_1 + i;
            if (sc >= numKeys) continue;
            bool pressed = keys[sc];
            if (pressed && !modeKeys_[i].down) { modeKeys_[i].down = true; modeKeys_[i].timer = 0.0f; }
            else if (!pressed && modeKeys_[i].down) {
                if (modeKeys_[i].timer <= ModeKeyState::maxPressTime && setRenderMode) setRenderMode(i + 1);
                modeKeys_[i] = {};
            }
            if (modeKeys_[i].down) modeKeys_[i].timer += dt;
        }

        // Mouse, WASD, Controller, F key — unchanged
        if (window && SDL_GetWindowRelativeMouseMode(window)) {
            float dx = 0, dy = 0;
            SDL_GetRelativeMouseState(&dx, &dy);
            if (dx || dy) CAM.rotate(-dx * lookSens, -dy * lookSens);
        }

        if (keys[SDL_SCANCODE_W]) CAM.moveForward( moveSpeed * dt);
        if (keys[SDL_SCANCODE_S]) CAM.moveForward(-moveSpeed * dt);
        if (keys[SDL_SCANCODE_A]) CAM.moveRight(   -moveSpeed * dt);
        if (keys[SDL_SCANCODE_D]) CAM.moveRight(    moveSpeed * dt);
        if (keys[SDL_SCANCODE_SPACE])  CAM.moveUp( moveSpeed * dt);
        if (keys[SDL_SCANCODE_LCTRL])  CAM.moveUp(-moveSpeed * dt);

        if (isConnected(0)) {
            float lx = getAxis(0, SDL_GAMEPAD_AXIS_LEFTX);
            float ly = getAxis(0, SDL_GAMEPAD_AXIS_LEFTY);
            float rx = getAxis(0, SDL_GAMEPAD_AXIS_RIGHTX);
            float ry = getAxis(0, SDL_GAMEPAD_AXIS_RIGHTY);
            if (std::abs(lx) > 0.15f || std::abs(ly) > 0.15f) {
                CAM.moveForward(-ly * moveSpeed * dt);
                CAM.moveRight(  lx * moveSpeed * dt);
            }
            if (std::abs(rx) > 0.15f || std::abs(ry) > 0.15f) {
                CAM.rotate(rx * 3.0f, ry * 3.0f);
            }
        }

        static bool fDown = false;
        if (keys[SDL_SCANCODE_F] && !fDown) {
            if (window) {
                bool captured = SDL_GetWindowRelativeMouseMode(window);
                SDL_SetWindowRelativeMouseMode(window, !captured);
                LOG_SUCCESS_CAT("INPUT", "{}MOUSE CAPTURE → {}{}", RASPBERRY_PINK, captured ? "OFF" : "ON", RESET);
            }
            fDown = true;
        } else if (!keys[SDL_SCANCODE_F]) fDown = false;
    }

    static constexpr uint64_t encrypt(uint64_t v, uint64_t g) noexcept {
        return std::rotl(v ^ g ^ 0x517CC1B727220A95ull ^ kStone1 ^ kStone2, 19);
    }
};

#define INPUT GlobalInputManager::get()
#define ON_EVENT(cb) INPUT.subscribe(cb, #cb)