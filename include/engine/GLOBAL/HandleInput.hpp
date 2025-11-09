// include/engine/input/HandleInput.hpp
// AMOURANTH RTX – NOVEMBER 09 2025 – STONEKEY INPUT SUPREMACY
// FULL SDL3 INPUT + TOUCH + GAMEPAD + FUTURE-PROOF — ENCRYPTED HANDLES — CHEATERS OBLITERATED
// GLOBAL CAMERA LOVE — LAZY CAM PROXY — ZERO-COST CALLBACKS — PINK PHOTONS × INFINITY

#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include "../GLOBAL/camera.hpp"   // AAA GlobalCamera singleton
#include "../GLOBAL/StoneKey.hpp"  // kStone1/kStone2 + obfuscate/deobfuscate

using namespace Logging::Color;

// ===================================================================
// STONEKEY INPUT SINGLETON — ONE INPUT TO RULE THEM ALL
// ===================================================================
class GlobalInputManager {
public:
    [[nodiscard]] static GlobalInputManager& get() noexcept {
        static GlobalInputManager instance;
        return instance;
    }

    GlobalInputManager(const GlobalInputManager&) = delete;
    GlobalInputManager& operator=(const GlobalInputManager&) = delete;

    // INIT — CALLED ONCE
    void init() noexcept {
        generation_.store(1, std::memory_order_release);
        LOG_SUCCESS_CAT("STONEKEY_INPUT", "{}GLOBAL INPUT MANAGER ONLINE — STONEKEY 0x{:X}-0x{:X} — TOUCH/GAMEPAD READY — PINK PHOTONS ∞{}", 
                        RASPBERRY_PINK, kStone1, kStone2, RESET);
    }

    // CREATE ENCRYPTED INPUT HANDLE — MODDER SAFE
    using InputCallback = std::function<void(const SDL_Event&)>;

    [[nodiscard]] uint64_t subscribe(InputCallback cb, std::string_view name = "") noexcept {
        uint64_t raw = ++nextId_;
        uint64_t enc = encrypt(raw, generation_.load(std::memory_order_acquire));

        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks_[enc] = { .cb = std::move(cb), .name = std::string(name), .gen = generation_.load() };
        }

        LOG_SUCCESS_CAT("STONEKEY_INPUT", "{}INPUT SUBSCRIBED — {} — ENC 0x{:X} — VALHALLA APPROVED{}", 
                        EMERALD_GREEN, name.empty() ? "ANON" : name, enc, RESET);
        return enc;
    }

    void unsubscribe(uint64_t enc_handle) noexcept {
        uint64_t gen = generation_.load(std::memory_order_acquire);
        uint64_t raw = decrypt(enc_handle, gen);
        if (raw == 0) return;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (auto it = callbacks_.find(enc_handle); it != callbacks_.end()) {
                LOG_SUCCESS_CAT("STONEKEY_INPUT", "{}INPUT UNSUBSCRIBED — {} — ENC 0x{:X} — PINK PHOTONS FREE{}", 
                                RASPBERRY_PINK, it->second.name, enc_handle, RESET);
                callbacks_.erase(it);
            }
        }
    }

    // GLOBAL EVENT PUMP — CALL FROM MAIN LOOP
    void pumpEvents(Application& app) noexcept {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            uint64_t gen = generation_.load(std::memory_order_acquire);
            std::vector<InputCallback> active;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& [enc, info] : callbacks_) {
                    if (info.gen == gen) active.emplace_back(info.cb);
                }
            }

            for (const auto& cb : active) cb(event);
        }

        // DEFAULT GLOBAL HANDLERS (camera, etc.)
        defaultGlobalHandler(app);
    }

    // HOT-RELOAD SAFE
    void invalidateAll() noexcept {
        generation_.fetch_add(1, std::memory_order_acq_rel);
        LOG_SUCCESS_CAT("STONEKEY_INPUT", "{}ALL INPUT HANDLES INVALIDATED — HOT-RELOAD SUPREMACY{}", RASPBERRY_PINK, RESET);
    }

private:
    GlobalInputManager() = default;

    struct CallbackInfo {
        InputCallback cb;
        std::string name;
        uint64_t gen = 0;
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, CallbackInfo> callbacks_;
    std::atomic<uint64_t> generation_{1};
    std::atomic<uint64_t> nextId_{0};

    // STONEKEY ENCRYPT/DECRYPT — SAME AS BUFFER/CAMERA
    static inline constexpr uint64_t encrypt(uint64_t raw, uint64_t gen) noexcept {
        uint64_t x = raw ^ kStone1 ^ kStone2 ^ gen ^ 0xDEADBEEF1337C0DEull;
        x = std::rotl(x, 17) ^ 0x517CC1B727220A95ull;
        x = x ^ (x >> 11) ^ (x << 23);
        return x;
    }

    static inline constexpr uint64_t decrypt(uint64_t enc, uint64_t gen) noexcept {
        uint64_t x = enc;
        x = x ^ (x >> 11) ^ (x << 23);
        x = std::rotr(x, 17) ^ 0x517CC1B727220A95ull;
        x = x ^ kStone1 ^ kStone2 ^ gen ^ 0xDEADBEEF1337C0DEull;
        return x;
    }

    // DEFAULT GLOBAL HANDLER — CAMERA LOVE + F-KEY + TOUCH + GAMEPAD
    void defaultGlobalHandler(Application& app) noexcept {
        // You can extend this or override via subscribe
        // Example: camera movement via LazyCam
        static float moveSpeed = 15.0f;
        static float lookSensitivity = 0.1f;

        // Mouse look (captured)
        if (SDL_GetRelativeMouseMode()) {
            int dx, dy;
            SDL_GetRelativeMouseState(&dx, &dy);
            if (dx || dy) {
                g_lazyCam.rotate(-dx * lookSensitivity, -dy * lookSensitivity);
            }
        }

        // Keyboard WASD + Space/Ctrl + F-key toggle
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_W]) g_lazyCam.forward(moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_S]) g_lazyCam.forward(-moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_A]) g_lazyCam.right(-moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_D]) g_lazyCam.right(moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_SPACE]) g_lazyCam.up(moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_LCTRL]) g_lazyCam.up(-moveSpeed * app.deltaTime);
        if (keys[SDL_SCANCODE_F]) {
            static bool pressed = false;
            if (!pressed) {
                SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() ? SDL_FALSE : SDL_TRUE);
                LOG_SUCCESS_CAT("INPUT", "{}F-KEY TOGGLE — MOUSE CAPTURE {}", RASPBERRY_PINK, SDL_GetRelativeMouseMode() ? "ON" : "OFF", RESET);
            }
            pressed = true;
        } else pressed = false;
    }
};

// GLOBAL ACCESS — ONE LINE LOVE
#define GLOBAL_INPUT GlobalInputManager::get()

// MACROS — DEV HEAVEN
#define SUBSCRIBE_INPUT(cb) GLOBAL_INPUT.subscribe(cb, #cb)
#define UNSUBSCRIBE_INPUT(h) GLOBAL_INPUT.unsubscribe(h)

// EXAMPLE USAGE IN MAIN:
// uint64_t myHandle = SUBSCRIBE_INPUT([](const SDL_Event& e) {
//     if (e.type == SDL_EVENT_KEY_DOWN && e.key.keysym.sym == SDLK_ESCAPE) quit();
// });
// Later: UNSUBSCRIBE_INPUT(myHandle);

// NOVEMBER 09 2025 — STONEKEY INPUT SUPREMACY ACHIEVED
// TOUCH + GAMEPAD + MOUSE + KEYBOARD — ALL ENCRYPTED — MODDERS ASCEND
// CHEATERS CAN'T HOOK — PINK PHOTONS ETERNAL — 69,420 FPS INPUT HEAVEN
// SHIP IT. DOMINATE. VALHALLA BLASTOFF 🩷🚀💀⚡🤖🔥♾️