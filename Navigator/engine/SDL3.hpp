#pragma once

// =============================================================================
// AMOURANTH RTX — SDL3 Core + Image + Mixer + TTF + Full Input (16-slot audio)
// Full 4-axis controller support + real track finished callbacks
// Updated to use OptionsMenu.hpp for all configuration
// =============================================================================

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <algorithm>
#include <utility>
#include <cctype>

#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "AMOURANTHRTX.hpp"  // for Swapchain

// =============================================================================
// CONSTANTS — centralized from Options::SDL3
// =============================================================================
constexpr int     MAX_SLOTS         = Options::SDL3::MaxAudioSlots;     // Number of audio tracks/slots
constexpr int     AUDIO_FREQ        = Options::SDL3::AudioFrequency;    // Sample rate (Hz)
constexpr int     AUDIO_CHANNELS    = Options::SDL3::AudioChannels;     // Mono=1, Stereo=2
constexpr float   DEFAULT_VOLUME    = Options::SDL3::DefaultVolume;     // Default track gain

// =============================================================================
// TYPES
// =============================================================================
struct AudioSlot {
    MIX_Audio*     audio      = nullptr;
    std::string    filename;
    bool           in_use     = false;
    ~AudioSlot() { if (audio) MIX_DestroyAudio(audio); }
};

struct FontEntry {
    TTF_Font* font = nullptr;
    ~FontEntry() { if (font) TTF_CloseFont(font); }
};

struct ControllerState {
    std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)> gamepad{nullptr, SDL_CloseGamepad};
    SDL_JoystickID id = 0;
    bool rumble = false;
    bool gyro   = false;
};

struct Subscription {
    std::function<void(const SDL_Event&)> cb;
    std::string name;
    uint64_t gen = 0;
};

// =============================================================================
// MAIN SYSTEM
// =============================================================================
class SDL3System {
public:
    static SDL3System& get() noexcept {
        static SDL3System instance;
        return instance;
    }

    SDL3System(const SDL3System&) = delete;
    SDL3System& operator=(const SDL3System&) = delete;

    bool init(SDL_Window* window) noexcept {
        if (initialized_) return true;
        window_ = window;

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) == 0) {
            LOG_ERROR_CAT("SDL3", "SDL_Init failed: {}", SDL_GetError());
            return false;
        }

        // Apply runtime SDL3 options from menu
        applyOptions();

        if (TTF_Init() == 0) {
            LOG_ERROR_CAT("TTF", "TTF_Init failed: {}", SDL_GetError());
            return false;
        }
        ttf_ready_ = true;

        if (MIX_Init() == 0) {
            LOG_ERROR_CAT("MIXER", "MIX_Init failed: {}", SDL_GetError());
            return false;
        }

        SDL_AudioSpec desired{};
        desired.freq     = AUDIO_FREQ;
        desired.format   = SDL_AUDIO_F32;
        desired.channels = AUDIO_CHANNELS;

        mixer_ = MIX_CreateMixerDevice(-1, &desired);
        if (!mixer_) {
            LOG_ERROR_CAT("MIXER", "MIX_CreateMixerDevice failed: {}", SDL_GetError());
            MIX_Quit();
            return false;
        }
        mixer_ready_ = true;

        tracks_.resize(MAX_SLOTS);
        slots_.resize(MAX_SLOTS);
        for (int i = 0; i < MAX_SLOTS; ++i) {
            tracks_[i] = MIX_CreateTrack(mixer_);
            if (tracks_[i]) {
                MIX_SetTrackGain(tracks_[i], DEFAULT_VOLUME);
                MIX_SetTrackStoppedCallback(tracks_[i], track_stopped_callback, this);
            }
        }

        bindDefaultActions();

        initialized_ = true;
        LOG_SUCCESS_CAT("SDL3", "Initialized — {} audio slots + full controller support", MAX_SLOTS);
        return true;
    }

    void shutdown() noexcept {
        if (!initialized_) return;

        for (auto& c : controllers_) if (c.gamepad) c.gamepad.reset();

        if (mixer_ready_) {
            for (auto t : tracks_) {
                if (t) {
                    MIX_SetTrackStoppedCallback(t, nullptr, nullptr);
                    MIX_DestroyTrack(t);
                }
            }
            tracks_.clear();

            MIX_DestroyMixer(mixer_);   // Destroy the mixer
            MIX_Quit();                 // Quit mixer subsystem
        }

        fonts_.clear();
        if (ttf_ready_) TTF_Quit();
        SDL_Quit();

        stopped_callbacks_.clear();

        initialized_ = false;
        LOG_SUCCESS_CAT("SDL3", "Shutdown complete");
    }

    // ────────────────────────────────────────────────
    // RESIZE HANDLING — SDL3 owns it, uses pixel size
    // ────────────────────────────────────────────────
    void onResize() noexcept {
        if (!window_) return;

        int pixelW = 0, pixelH = 0;
        SDL_GetWindowSizeInPixels(window_, &pixelW, &pixelH);

        if (pixelW <= 0 || pixelH <= 0) {
            LOG_WARNING_CAT("SDL3", "Window minimized/invalid size");
            return;
        }

        Swapchain::recreate(pixelW, pixelH);
    }

    // Apply runtime SDL3 options from Options::SDL3
    void applyOptions() noexcept {
        if (!window_) return;

        SDL_SetWindowFullscreen(window_, Options::SDL3::StartFullscreen);
        SDL_SetWindowBordered(window_, !Options::SDL3::BorderlessWindow);
        SDL_SetWindowResizable(window_, Options::SDL3::AllowWindowResize);
        SDL_SetWindowRelativeMouseMode(window_, Options::SDL3::EnableInputCapture);
    }

    // ────────────────────────────────────────────────
    // AUDIO
    // ────────────────────────────────────────────────
    int playSound(const std::string& file, const std::string& cmd, int preferred_slot = -1) {
        if (!mixer_ready_) return -1;

        std::string cmd_lower = cmd;
        std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(), ::tolower);

        if (cmd_lower == "load" || cmd_lower == "play") {
            int slot = findOrAllocateSlot(file, preferred_slot);
            if (slot < 0) return -1;

            if (cmd_lower == "play") {
                MIX_SetTrackAudio(tracks_[slot], slots_[slot].audio);
                MIX_PlayTrack(tracks_[slot], 0);
                LOG_SUCCESS_CAT("AUDIO", "Playing '{}' on slot {}", slots_[slot].filename, slot);
            } else {
                LOG_SUCCESS_CAT("AUDIO", "Loaded '{}' into slot {}", file, slot);
            }
            return slot;
        }
        else if (cmd_lower == "stop") {
            if (preferred_slot >= 0 && preferred_slot < MAX_SLOTS && slots_[preferred_slot].in_use) {
                MIX_StopTrack(tracks_[preferred_slot], 0);
                LOG_INFO_CAT("AUDIO", "Stopped slot {}", preferred_slot);
            }
            return preferred_slot;
        }
        else if (cmd_lower == "pause") {
            if (preferred_slot >= 0 && preferred_slot < MAX_SLOTS && slots_[preferred_slot].in_use) {
                MIX_PauseTrack(tracks_[preferred_slot]);
                LOG_INFO_CAT("AUDIO", "Paused slot {}", preferred_slot);
            }
            return preferred_slot;
        }
        else if (cmd_lower == "remove") {
            if (preferred_slot >= 0 && preferred_slot < MAX_SLOTS && slots_[preferred_slot].in_use) {
                MIX_StopTrack(tracks_[preferred_slot], 0);
                slots_[preferred_slot] = {};
                LOG_INFO_CAT("AUDIO", "Removed slot {}", preferred_slot);
            }
            return preferred_slot;
        }

        LOG_ERROR_CAT("AUDIO", "Unknown cmd '{}'", cmd);
        return -1;
    }

    void onTrackFinished(int slot, std::function<void(int)> cb) {
        if (!mixer_ready_ || slot < 0 || slot >= MAX_SLOTS || !tracks_[slot]) return;

        std::lock_guard<std::mutex> lock(callback_mtx_);
        stopped_callbacks_[tracks_[slot]] = std::move(cb);
    }

    bool isTrackPlaying(int slot) const {
        if (slot < 0 || slot >= MAX_SLOTS || !tracks_[slot]) return false;
        return MIX_TrackPlaying(tracks_[slot]);
    }

private:
    static void track_stopped_callback(void* userdata, MIX_Track* track) {
        auto* self = static_cast<SDL3System*>(userdata);
        if (!self) return;

        std::lock_guard<std::mutex> lock(self->callback_mtx_);
        auto it = self->stopped_callbacks_.find(track);
        if (it != self->stopped_callbacks_.end()) {
            int slot = -1;
            for (int i = 0; i < MAX_SLOTS; ++i) {
                if (self->tracks_[i] == track) {
                    slot = i;
                    break;
                }
            }
            if (slot >= 0) it->second(slot);
            self->stopped_callbacks_.erase(it);
        }
    }

    int findOrAllocateSlot(const std::string& file, int preferred) {
        if (preferred >= 0 && preferred < MAX_SLOTS && !slots_[preferred].in_use) {
            loadIntoSlot(preferred, file);
            return preferred;
        }

        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (!slots_[i].in_use) {
                loadIntoSlot(i, file);
                return i;
            }
        }

        int victim = (preferred >= 0 && preferred < MAX_SLOTS) ? preferred : 0;
        MIX_StopTrack(tracks_[victim], 0);
        loadIntoSlot(victim, file);
        LOG_WARNING_CAT("AUDIO", "Overwrote slot {} with '{}'", victim, file);
        return victim;
    }

    void loadIntoSlot(int slot, const std::string& file) {
        MIX_Audio* a = MIX_LoadAudio(mixer_, file.c_str(), false);
        if (!a) {
            LOG_ERROR_CAT("AUDIO", "Load failed '{}': {}", file, SDL_GetError());
            return;
        }
        slots_[slot].audio = a;
        slots_[slot].filename = file;
        slots_[slot].in_use = true;
    }

public:
    // ────────────────────────────────────────────────
    // IMAGE & TEXT
    // ────────────────────────────────────────────────
    SDL_Texture* loadTexture(SDL_Renderer* r, const std::string& path) {
        SDL_Surface* s = IMG_Load(path.c_str());
        if (!s) {
            LOG_ERROR_CAT("IMG", "IMG_Load failed for '{}': {}", path, SDL_GetError());
            return nullptr;
        }
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        SDL_DestroySurface(s);
        if (!t) {
            LOG_ERROR_CAT("SDL", "SDL_CreateTextureFromSurface failed: {}", SDL_GetError());
        }
        return t;
    }

    bool loadFont(const std::string& name, const std::string& path, int size) {
        if (!ttf_ready_) return false;
        TTF_Font* f = TTF_OpenFont(path.c_str(), static_cast<float>(size));
        if (!f) {
            LOG_ERROR_CAT("TTF", "TTF_OpenFont failed for '{}': {}", path, SDL_GetError());
            return false;
        }
        fonts_[name].font = f;
        return true;
    }

    SDL_Texture* renderText(SDL_Renderer* r, const std::string& fontname,
                            const std::string& text, SDL_Color col, int wrap = 0) {
        auto it = fonts_.find(fontname);
        if (it == fonts_.end() || !it->second.font) {
            LOG_ERROR_CAT("TTF", "Font '{}' not found", fontname);
            return nullptr;
        }

        SDL_Surface* s = TTF_RenderText_Blended_Wrapped(it->second.font, text.c_str(), 0, col, wrap);
        if (!s) {
            LOG_ERROR_CAT("TTF", "TTF_RenderText_Blended_Wrapped failed: {}", SDL_GetError());
            return nullptr;
        }

        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        SDL_DestroySurface(s);
        if (!t) {
            LOG_ERROR_CAT("SDL", "SDL_CreateTextureFromSurface failed: {}", SDL_GetError());
        }
        return t;
    }

    // ────────────────────────────────────────────────
    // INPUT
    // ────────────────────────────────────────────────
    using EventCallback = std::function<void(const SDL_Event&)>;

    uint64_t subscribe(EventCallback cb, std::string_view name = "") {
        uint64_t id = ++nextId_;
        uint64_t handle = id ^ generation_.load();
        std::lock_guard<std::mutex> lock(mtx_);
        subscriptions_[handle] = {std::move(cb), std::string(name), generation_.load()};
        return handle;
    }

    void unsubscribe(uint64_t handle) {
        std::lock_guard<std::mutex> lock(mtx_);
        subscriptions_.erase(handle);
    }

    void pump(const SDL_Event& ev) {
        uint64_t gen = generation_.load();
        std::vector<EventCallback> active;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            for (const auto& p : subscriptions_) {
                if (p.second.gen == gen) active.push_back(p.second.cb);
            }
        }
        for (const auto& cb : active) cb(ev);
        processEvent(ev);
    }

    void invalidateAll() { generation_.fetch_add(1); }

    void captureMouse(bool enable) {
        if (window_) SDL_SetWindowRelativeMouseMode(window_, enable);
    }

    glm::vec2 mouseDelta() const {
        float x = 0, y = 0;
        SDL_GetRelativeMouseState(&x, &y);
        return {x, y};
    }

    void bind(std::string_view action, SDL_Scancode code) {
        std::lock_guard<std::mutex> lock(mtx_);
        bindings_[std::string(action)] = code;
    }

    bool down(std::string_view action) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = bindings_.find(std::string(action));
        if (it == bindings_.end()) return false;
        const bool* ks = SDL_GetKeyboardState(nullptr);
        return ks[static_cast<int>(it->second)];
    }

    glm::vec3 movement(float speed, float dt) const {
        glm::vec3 v(0);
        if (down("move_forward"))  v.z -= 1;
        if (down("move_backward")) v.z += 1;
        if (down("move_left"))     v.x -= 1;
        if (down("move_right"))    v.x += 1;
        if (glm::length(v) > 0.01f) v = glm::normalize(v);
        if (down("sprint")) v *= 2.2f;
        if (down("crouch")) v *= 0.5f;
        return v * speed * dt;
    }

    int numControllers() const {
        int count = 0;
        for (const auto& c : controllers_) if (c.gamepad) ++count;
        return count;
    }

    bool controllerConnected(int slot = 0) const {
        return slot >= 0 && slot < static_cast<int>(controllers_.size()) &&
               controllers_[static_cast<size_t>(slot)].gamepad != nullptr;
    }

    const char* controllerName(int slot = 0) const {
        if (!controllerConnected(slot)) return "None";
        return SDL_GetGamepadName(controllers_[slot].gamepad.get());
    }

    float leftStickX(int slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float leftStickY(int slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float rightStickX(int slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float rightStickY(int slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float leftTrigger(int slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        return SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    }

    float rightTrigger(int slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        return SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    }

    bool buttonDown(int slot, SDL_GamepadButton button) const {
        if (!controllerConnected(slot)) return false;
        return SDL_GetGamepadButton(controllers_[slot].gamepad.get(), button);
    }

private:
    void processEvent(const SDL_Event& ev) {
        if (ev.type == SDL_EVENT_GAMEPAD_ADDED) openController(ev.gdevice.which);
        if (ev.type == SDL_EVENT_GAMEPAD_REMOVED) closeController(ev.gdevice.which);
    }

    void openAllControllers() {
        int n = 0;
        auto* ids = SDL_GetGamepads(&n);
        if (ids) {
            for (int i = 0; i < n; ++i) openController(ids[i]);
            SDL_free(ids);
        }
    }

    void openController(SDL_JoystickID id) {
        if (SDL_IsGamepad(id) == 0) return;
        auto* gp = SDL_OpenGamepad(id);
        if (!gp) {
            LOG_WARNING_CAT("INPUT", "SDL_OpenGamepad failed for id {}: {}", id, SDL_GetError());
            return;
        }

        size_t inputport = 0;
        for (; inputport < controllers_.size(); ++inputport)
            if (!controllers_[inputport].gamepad) break;

        if (inputport >= controllers_.size()) {
            SDL_CloseGamepad(gp);
            LOG_WARNING_CAT("INPUT", "No free controller input port for id {}", id);
            return;
        }

        controllers_[inputport].gamepad.reset(gp);
        controllers_[inputport].id = id;

        auto props = SDL_GetGamepadProperties(gp);
        controllers_[inputport].rumble = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false) && Options::SDL3::EnableRumble;
        controllers_[inputport].gyro   = SDL_GamepadHasSensor(gp, SDL_SENSOR_GYRO) && Options::SDL3::EnableGyro;

        LOG_SUCCESS_CAT("INPUT", "Gamepad connected (Player {} Input Port {}) — {}", inputport+1, inputport, SDL_GetGamepadName(gp));
    }

    void closeController(SDL_JoystickID id) {
        for (auto& c : controllers_) {
            if (c.id == id) {
                c.gamepad.reset();
                LOG_INFO_CAT("INPUT", "Gamepad disconnected (id {})", id);
                break;
            }
        }
    }

    void bindDefaultActions() {
        bind("jump",          SDL_SCANCODE_SPACE);
        bind("shoot",         SDL_SCANCODE_LCTRL);
        bind("move_forward",  SDL_SCANCODE_W);
        bind("move_backward", SDL_SCANCODE_S);
        bind("move_left",     SDL_SCANCODE_A);
        bind("move_right",    SDL_SCANCODE_D);
        bind("sprint",        SDL_SCANCODE_LSHIFT);
        bind("crouch",        SDL_SCANCODE_LCTRL);
        bind("interact",      SDL_SCANCODE_E);
    }

private:
    SDL3System() = default;

    bool initialized_   = false;
    bool mixer_ready_   = false;
    bool ttf_ready_     = false;

    SDL_Window* window_ = nullptr;
    MIX_Mixer*  mixer_  = nullptr;

    std::vector<MIX_Track*> tracks_;
    std::vector<AudioSlot> slots_;  // Dynamic size from Options::SDL3::MaxAudioSlots

    std::unordered_map<std::string, FontEntry> fonts_;
    std::array<ControllerState, 4> controllers_;

    mutable std::mutex mtx_;
    std::unordered_map<uint64_t, Subscription> subscriptions_;
    std::unordered_map<std::string, SDL_Scancode> bindings_;

    std::mutex callback_mtx_;
    std::unordered_map<MIX_Track*, std::function<void(int)>> stopped_callbacks_;

    std::atomic<uint64_t> generation_{1};
    std::atomic<uint64_t> nextId_{0};
};

#define INPUT  SDL3System::get()
#define ON_EVENT(cb) SDL3.subscribe([](const SDL_Event& ev){ cb(ev); }, #cb)