#pragma once

// =============================================================================
// AMOURANTH RTX — SDL3 Core + Image + Mixer + TTF + Full Input
// Full controller support (dynamic count) + real track finished callbacks
// Uses OptionsMenu.hpp for configuration
// Dynamic audio channels — preload via Options::SDL3::PreloadedAudioFiles
// SOFT_MAX_SLOTS is recommended concurrent playing limit (not enforced)
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
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <algorithm>
#include <cctype>

#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "AMOURANTHRTX.hpp"
#include "Camera.hpp"

// =============================================================================
// CONSTANTS — from Options::SDL3
// =============================================================================
constexpr int   AUDIO_FREQ       = Options::SDL3::AudioFrequency;
constexpr int   AUDIO_CHANNELS   = Options::SDL3::AudioChannels;
constexpr float DEFAULT_VOLUME   = Options::SDL3::DefaultVolume;

// Recommended (soft) maximum concurrent playing sounds
constexpr int   SOFT_MAX_SLOTS   = Options::SDL3::MyAudioSlots;

// =============================================================================
// TYPES
// =============================================================================
enum class AudioState : uint8_t {
    Free    = 0,
    Loaded  = 1,
    Playing = 2,
    Paused  = 3
};

struct AudioChannel {
    MIX_Track*              track     = nullptr;
    MIX_Audio*              audio     = nullptr;
    std::string             filename;
    AudioState              state     = AudioState::Free;
    float                   gain      = DEFAULT_VOLUME;
    std::function<void(int)> on_finish;

    ~AudioChannel() {
        if (audio) MIX_DestroyAudio(audio);
        // track is owned by mixer — do NOT destroy here
    }

    void reset() noexcept {
        if (audio) {
            MIX_DestroyAudio(audio);
            audio = nullptr;
        }
        filename.clear();
        state = AudioState::Free;
        gain  = DEFAULT_VOLUME;
        on_finish = nullptr;
    }

    bool isActive() const noexcept {
        return state == AudioState::Playing || state == AudioState::Paused;
    }
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
    std::function<void(const SDL_Event&)> callback;
    std::string name;
    uint64_t    id = 0;
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

        preloadAudioFiles();

        bindDefaultActions();

        initialized_ = true;
        LOG_SUCCESS_CAT("SDL3", "Initialized — audio channels: {} preloaded, soft max playing: {}",
                        channels_.size(), SOFT_MAX_SLOTS);
        return true;
    }

    void shutdown() noexcept {
        if (!initialized_) return;

        controllers_.clear();

        if (mixer_ready_) {
            for (auto& ch : channels_) {
                if (ch.track) {
                    MIX_SetTrackStoppedCallback(ch.track, nullptr, nullptr);
                    MIX_DestroyTrack(ch.track);
                }
            }
            channels_.clear();

            MIX_DestroyMixer(mixer_);
            mixer_ = nullptr;
            MIX_Quit();
            mixer_ready_ = false;
        }

        fonts_.clear();
        if (ttf_ready_) TTF_Quit();

        SDL_Quit();

        subscriptions_.clear();

        initialized_ = false;
        LOG_SUCCESS_CAT("SDL3", "Shutdown complete");
    }

    void onResize() noexcept {
        if (!window_) return;
        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(window_, &pw, &ph);
        if (pw <= 0 || ph <= 0) {
            LOG_WARNING_CAT("SDL3", "Window minimized or invalid size");
            return;
        }
        Swapchain::recreate(pw, ph);
    }

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
            int slot = findOrAllocateChannel(file, preferred_slot);
            if (slot < 0) return -1;

            auto& ch = channels_[static_cast<size_t>(slot)];

            if (cmd_lower == "play") {
                if (!ch.audio || !ch.track) return -1;

                MIX_SetTrackAudio(ch.track, ch.audio);
                MIX_SetTrackGain(ch.track, ch.gain);
                MIX_PlayTrack(ch.track, 0);
                ch.state = AudioState::Playing;

                size_t active = getPlayingCount();
                if (active > static_cast<size_t>(SOFT_MAX_SLOTS)) {
                    LOG_WARNING_CAT("AUDIO", "Playing count ({}) exceeds soft limit ({})", active, SOFT_MAX_SLOTS);
                }

                LOG_SUCCESS_CAT("AUDIO", "Playing '{}' on channel {}", ch.filename, slot);
            } else {
                LOG_SUCCESS_CAT("AUDIO", "Loaded '{}' into channel {}", file, slot);
            }
            return slot;
        }
        else if (cmd_lower == "stop") {
            size_t idx = static_cast<size_t>(preferred_slot);
            if (preferred_slot < 0 || idx >= channels_.size() || !channels_[idx].isActive()) {
                return preferred_slot;
            }
            MIX_StopTrack(channels_[idx].track, 0);
            channels_[idx].state = AudioState::Loaded;
            LOG_INFO_CAT("AUDIO", "Stopped channel {}", preferred_slot);
            return preferred_slot;
        }
        else if (cmd_lower == "pause") {
            size_t idx = static_cast<size_t>(preferred_slot);
            if (preferred_slot < 0 || idx >= channels_.size() || channels_[idx].state != AudioState::Playing) {
                return preferred_slot;
            }
            MIX_PauseTrack(channels_[idx].track);
            channels_[idx].state = AudioState::Paused;
            LOG_INFO_CAT("AUDIO", "Paused channel {}", preferred_slot);
            return preferred_slot;
        }
        else if (cmd_lower == "remove") {
            size_t idx = static_cast<size_t>(preferred_slot);
            if (preferred_slot < 0 || idx >= channels_.size() || !channels_[idx].audio) {
                return preferred_slot;
            }
            MIX_StopTrack(channels_[idx].track, 0);
            channels_[idx].reset();
            LOG_INFO_CAT("AUDIO", "Removed channel {}", preferred_slot);
            return preferred_slot;
        }

        LOG_ERROR_CAT("AUDIO", "Unknown command '{}'", cmd);
        return -1;
    }

    void onTrackFinished(int slot, std::function<void(int)> cb) {
        if (!mixer_ready_ || slot < 0 || static_cast<size_t>(slot) >= channels_.size()) return;
        auto& ch = channels_[static_cast<size_t>(slot)];
        if (!ch.track) return;
        ch.on_finish = std::move(cb);
    }

    bool isTrackPlaying(int slot) const {
        if (slot < 0 || static_cast<size_t>(slot) >= channels_.size()) return false;
        return MIX_TrackPlaying(channels_[static_cast<size_t>(slot)].track);
    }

    size_t getActiveChannelCount() const noexcept { return channels_.size(); }

    size_t getPlayingCount() const noexcept {
        size_t count = 0;
        for (const auto& ch : channels_)
            if (ch.state == AudioState::Playing) ++count;
        return count;
    }

private:
    void preloadAudioFiles() {
        const auto& files = Options::SDL3::PreloadedAudioFiles;
        if (files.empty()) return;

        channels_.reserve(files.size() + 16);

        size_t loaded = 0;
        for (const auto& path : files) {
            MIX_Audio* a = MIX_LoadAudio(mixer_, path.c_str(), false);
            if (!a) {
                LOG_ERROR_CAT("AUDIO", "Preload failed: {}", path);
                continue;
            }

            size_t idx = channels_.size();
            channels_.emplace_back();
            auto& ch = channels_.back();
            ch.audio    = a;
            ch.filename = path;
            ch.state    = AudioState::Loaded;

            ch.track = MIX_CreateTrack(mixer_);
            if (!ch.track) {
                LOG_ERROR_CAT("AUDIO", "Failed to create track for '{}'", path);
                ch.reset();
                continue;
            }

            MIX_SetTrackGain(ch.track, DEFAULT_VOLUME);
            MIX_SetTrackStoppedCallback(ch.track, track_finished_callback, this);

            LOG_INFO_CAT("AUDIO", "Preloaded '{}' → channel {}", path, idx);
            ++loaded;
        }

        LOG_SUCCESS_CAT("AUDIO", "Preloaded {}/{} files", loaded, files.size());
    }

    int findOrAllocateChannel(const std::string& file, int preferred = -1) {
        // Reuse exact file if already loaded
        for (size_t i = 0; i < channels_.size(); ++i) {
            if (channels_[i].filename == file && channels_[i].audio) {
                return static_cast<int>(i);
            }
        }

        // Try preferred slot if free
        if (preferred >= 0) {
            size_t idx = static_cast<size_t>(preferred);
            if (idx < channels_.size() && channels_[idx].state == AudioState::Free) {
                loadIntoChannel(idx, file);
                return preferred;
            }
        }

        // Find first free channel
        for (size_t i = 0; i < channels_.size(); ++i) {
            if (channels_[i].state == AudioState::Free) {
                loadIntoChannel(i, file);
                return static_cast<int>(i);
            }
        }

        // Allocate new channel
        return allocateNewChannel(file);
    }

    int allocateNewChannel(const std::string& file) {
        MIX_Audio* a = MIX_LoadAudio(mixer_, file.c_str(), false);
        if (!a) {
            LOG_ERROR_CAT("AUDIO", "Load failed '{}': {}", file, SDL_GetError());
            return -1;
        }

        size_t idx = channels_.size();
        channels_.emplace_back();
        auto& ch = channels_.back();
        ch.audio    = a;
        ch.filename = file;
        ch.state    = AudioState::Loaded;

        ch.track = MIX_CreateTrack(mixer_);
        if (!ch.track) {
            LOG_ERROR_CAT("AUDIO", "Failed to create track for '{}'", file);
            ch.reset();
            return -1;
        }

        MIX_SetTrackGain(ch.track, DEFAULT_VOLUME);
        MIX_SetTrackStoppedCallback(ch.track, track_finished_callback, this);

        LOG_INFO_CAT("AUDIO", "Allocated new channel {} for '{}'", idx, file);
        return static_cast<int>(idx);
    }

    void loadIntoChannel(size_t idx, const std::string& file) {
        MIX_Audio* a = MIX_LoadAudio(mixer_, file.c_str(), false);
        if (!a) {
            LOG_ERROR_CAT("AUDIO", "Load failed '{}': {}", file, SDL_GetError());
            return;
        }

        auto& ch = channels_[idx];
        if (ch.audio) MIX_DestroyAudio(ch.audio);
        ch.audio    = a;
        ch.filename = file;
        ch.state    = AudioState::Loaded;
    }

    static void track_finished_callback(void* userdata, MIX_Track* track) {
        auto* self = static_cast<SDL3System*>(userdata);
        if (!self) return;

        for (size_t i = 0; i < self->channels_.size(); ++i) {
            auto& ch = self->channels_[i];
            if (ch.track == track) {
                ch.state = AudioState::Loaded;
                if (ch.on_finish) {
                    ch.on_finish(static_cast<int>(i));
                    ch.on_finish = nullptr;  // one-shot
                }
                return;
            }
        }
    }

public:
    // ────────────────────────────────────────────────
    // IMAGE & TEXT
    // ────────────────────────────────────────────────
    SDL_Texture* loadTexture(SDL_Renderer* r, const std::string& path) {
        SDL_Surface* s = IMG_Load(path.c_str());
        if (!s) {
            LOG_ERROR_CAT("IMG", "IMG_Load failed '{}': {}", path, SDL_GetError());
            return nullptr;
        }
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        SDL_DestroySurface(s);
        if (!t) LOG_ERROR_CAT("SDL", "CreateTextureFromSurface failed: {}", SDL_GetError());
        return t;
    }

    bool loadFont(const std::string& name, const std::string& path, int size) {
        if (!ttf_ready_) return false;
        TTF_Font* f = TTF_OpenFont(path.c_str(), static_cast<float>(size));
        if (!f) {
            LOG_ERROR_CAT("TTF", "TTF_OpenFont failed '{}': {}", path, SDL_GetError());
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
            LOG_ERROR_CAT("TTF", "RenderText failed: {}", SDL_GetError());
            return nullptr;
        }

        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        SDL_DestroySurface(s);
        if (!t) LOG_ERROR_CAT("SDL", "CreateTextureFromSurface failed: {}", SDL_GetError());
        return t;
    }

    // ────────────────────────────────────────────────
    // INPUT
    // ────────────────────────────────────────────────
    using EventCallback = std::function<void(const SDL_Event&)>;

    uint64_t subscribe(EventCallback cb, std::string_view name = "") {
        uint64_t id = ++next_subscription_id_;
        std::lock_guard<std::mutex> lock(mtx_);
        subscriptions_[id] = {std::move(cb), std::string(name), id};
        return id;
    }

    void unsubscribe(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        subscriptions_.erase(id);
    }

    void pump(const SDL_Event& ev) {
        std::vector<EventCallback> active;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            for (const auto& [id, sub] : subscriptions_) {
                active.push_back(sub.callback);
            }
        }
        for (const auto& cb : active) cb(ev);
        processEvent(ev);
    }

    void invalidateAll() {
        // With stable IDs we no longer need generation invalidation
        // (but kept method for API compatibility)
    }

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

        float len2 = glm::dot(v, v);
        if (len2 > 0.0001f) v = glm::normalize(v);

        if (down("sprint")) v *= 2.2f;
        if (down("crouch")) v *= 0.5f;
        return v * speed * dt;
    }

    size_t numControllers() const noexcept {
        size_t count = 0;
        for (const auto& c : controllers_) if (c.gamepad) ++count;
        return count;
    }

    bool controllerConnected(size_t slot = 0) const {
        return slot < controllers_.size() && controllers_[slot].gamepad != nullptr;
    }

    const char* controllerName(size_t slot = 0) const {
        if (!controllerConnected(slot)) return "None";
        return SDL_GetGamepadName(controllers_[slot].gamepad.get());
    }

    float leftStickX(size_t slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float leftStickY(size_t slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float rightStickX(size_t slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float rightStickY(size_t slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float leftTrigger(size_t slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        return SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    }

    float rightTrigger(size_t slot = 0) const {
        if (!controllerConnected(slot)) return 0.0f;
        return SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    }

    bool buttonDown(size_t slot, SDL_GamepadButton button) const {
        if (!controllerConnected(slot)) return false;
        return SDL_GetGamepadButton(controllers_[slot].gamepad.get(), button);
    }

private:
    void processEvent(const SDL_Event& ev) {
        if (ev.type == SDL_EVENT_GAMEPAD_ADDED)    openController(ev.gdevice.which);
        if (ev.type == SDL_EVENT_GAMEPAD_REMOVED)  closeController(ev.gdevice.which);
    }

    void openController(SDL_JoystickID id) {
        if (!SDL_IsGamepad(id)) return;

        auto* gp = SDL_OpenGamepad(id);
        if (!gp) {
            LOG_WARNING_CAT("INPUT", "SDL_OpenGamepad failed id {}: {}", id, SDL_GetError());
            return;
        }

        size_t slot = 0;
        for (; slot < controllers_.size(); ++slot) {
            if (!controllers_[slot].gamepad) break;
        }

        if (slot == controllers_.size()) {
            controllers_.emplace_back();
        }

        controllers_[slot].gamepad.reset(gp);
        controllers_[slot].id = id;

        auto props = SDL_GetGamepadProperties(gp);
        controllers_[slot].rumble = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false)
                                    && Options::SDL3::EnableRumble;
        controllers_[slot].gyro = SDL_GamepadHasSensor(gp, SDL_SENSOR_GYRO)
                                  && Options::SDL3::EnableGyro;

        LOG_SUCCESS_CAT("INPUT", "Gamepad connected (slot {}) — {}", slot, SDL_GetGamepadName(gp));
    }

    void closeController(SDL_JoystickID id) {
        for (auto& c : controllers_) {
            if (c.id == id) {
                c.gamepad.reset();
                LOG_INFO_CAT("INPUT", "Gamepad disconnected (id {})", id);
                return;
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

    std::vector<AudioChannel>           channels_;
    std::unordered_map<std::string, FontEntry> fonts_;
    std::vector<ControllerState>        controllers_;

    mutable std::mutex mtx_;
    std::unordered_map<uint64_t, Subscription> subscriptions_;
    std::unordered_map<std::string, SDL_Scancode> bindings_;

    std::atomic<uint64_t> next_subscription_id_{1};
};

#define INPUT  SDL3System::get()
#define ON_EVENT(cb) INPUT.subscribe([](const SDL_Event& ev){ cb(ev); }, #cb)