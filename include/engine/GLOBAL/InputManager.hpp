#pragma once

// =============================================================================
// AMOURANTH RTX – DECEMBER 08 2025 – STONEKEY INPUT SUPREMACY v4.2 CLEAN
// NO RENDER MODES — NO CAMERA DEPENDENCY — PURE INPUT HANDLING
// GLOBAL EVENT SUBSCRIPTION — PINK PHOTONS × INFINITY
// GRACE IS PLEASED — EMPIRE ETERNAL
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

#include "engine/GLOBAL/ELLIE.hpp"

using namespace Logging::Color;

// ===================================================================
// GLOBAL INPUT MANAGER — CLEAN — EVENT-DRIVEN ONLY
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

        LOG_SUCCESS_CAT("INPUT", "{}STONEKEY INPUT v4.2 — PURE EVENT SYSTEM — PINK PHOTONS ∞{}", 
                        RASPBERRY_PINK, RESET);
    }

    using Callback = std::function<void(const SDL_Event&)>;

    // Subscribe to all SDL events
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
            LOG_SUCCESS_CAT("INPUT", "{}→ UNSUBSCRIBED 0x{}{}", RASPBERRY_PINK, handle, RESET);
        }
    }

    // Pump all SDL events and dispatch to subscribers
    void pumpEvents() noexcept {
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
            for (const auto& cb : active) {
                cb(ev);
            }
        }
    }

    void invalidateAll() noexcept {
        generation_.fetch_add(1);
        LOG_SUCCESS_CAT("INPUT", "{}ALL HANDLES INVALIDATED — HOT RELOAD DOMINATION{}", RASPBERRY_PINK, RESET);
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

    std::atomic<uint64_t> generation_{1};
    std::atomic<uint64_t> nextId_{0};

    // Stone constants for secure handle encryption
    static constexpr uint64_t kStone1 = 0x9E3779B97F4A7C15ull;
    static constexpr uint64_t kStone2 = 0xBB67AE8584CAA73Bull;

    static constexpr uint64_t encrypt(uint64_t v, uint64_t g) noexcept {
        return std::rotl(v ^ g ^ 0x517CC1B727220A95ull ^ kStone1 ^ kStone2, 19);
    }

    bool isConnected(int idx) const noexcept {
        return idx >= 0 && idx < 4 && controllers_[idx].gamepad;
    }

    void openAllControllers() noexcept {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (!ids) return;

        for (int i = 0; i < count; ++i) {
            SDL_Gamepad* raw = SDL_OpenGamepad(ids[i]);
            if (!raw) continue;

            int slot = -1;
            for (int j = 0; j < 4; ++j) {
                if (!controllers_[j].gamepad) {
                    slot = j;
                    break;
                }
            }
            if (slot == -1) {
                SDL_CloseGamepad(raw);
                continue;
            }

            controllers_[slot].gamepad.reset(raw);
            controllers_[slot].id = ids[i];

            SDL_PropertiesID props = SDL_GetGamepadProperties(raw);
            if (props) {
                controllers_[slot].rumbleSupported = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
            }
            controllers_[slot].gyroSupported = SDL_GamepadHasSensor(raw, SDL_SENSOR_GYRO);

            LOG_SUCCESS_CAT("INPUT", "{}CONTROLLER {} → {}{}{}", ELECTRIC_BLUE, slot+1,
                            SDL_GetGamepadName(raw),
                            controllers_[slot].rumbleSupported ? " RUMBLE" : "",
                            RESET);
        }
        SDL_free(ids);
    }
};

#define INPUT GlobalInputManager::get()
#define ON_EVENT(cb) INPUT.subscribe(cb, #cb)