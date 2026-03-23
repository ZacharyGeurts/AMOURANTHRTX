#pragma once

// =============================================================================
// AMOURANTH RTX — SDL3 Core + Image + Mixer + TTF + Full Input
// Full 4-axis controller support + real track finished callbacks
// Updated to use OptionsMenu.hpp for all configuration
// Dynamic audio slots — preload as many files as desired via Options::SDL3::PreloadedAudioFiles
// No hard slot limit (MyAudioSlots is soft/recommended concurrent max)
//
// INPUT BEHAVIOR (2026 standard):
// - Mouse capture: relative mode only when window focused (SDL_SetWindowRelativeMouseMode)
// - Keyboard & gamepad: captured when window has focus (SDL_WINDOW_INPUT_FOCUS)
// - Alt+Tab / lost focus: automatically releases mouse capture & stops relative mode
// - Regain focus: auto-restores relative mouse mode if enabled in options
// - No global/raw input hijacking — respects OS focus rules
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
#include <utility>
#include <cctype>

#include "ELLIE.hpp"
#include "OptionsMenu.hpp"
#include "AMOURANTHRTX.hpp"  // for Swapchain

// =============================================================================
// CONSTANTS — centralized from Options::SDL3
// =============================================================================
constexpr int     AUDIO_FREQ           = Options::SDL3::AudioFrequency;
constexpr int     AUDIO_CHANNELS       = Options::SDL3::AudioChannels;
constexpr float   DEFAULT_VOLUME       = Options::SDL3::DefaultVolume;

// Soft / recommended maximum concurrent playing tracks
constexpr int     SOFT_MAX_SLOTS       = Options::SDL3::MyAudioSlots;

// =============================================================================
// TYPES
// =============================================================================
enum class AudioSlotState : uint8_t {
    Free       = 0,
    Loaded     = 1,
    Playing    = 2,
    Paused     = 3
};

struct AudioSlot {
    MIX_Audio*      audio       = nullptr;
    std::string     filename;
    AudioSlotState  state       = AudioSlotState::Free;
    float           gain        = DEFAULT_VOLUME;

    ~AudioSlot() { if (audio) MIX_DestroyAudio(audio); }

    void reset() noexcept {
        if (audio) {
            MIX_DestroyAudio(audio);
            audio = nullptr;
        }
        filename.clear();
        state = AudioSlotState::Free;
        gain  = DEFAULT_VOLUME;
    }

    bool isActive() const noexcept { return state == AudioSlotState::Playing || state == AudioSlotState::Paused; }
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

        // Initial focus check
        updateFocusState();

        initialized_ = true;
        LOG_SUCCESS_CAT("SDL3", "Initialized — dynamic audio slots (preloaded: {}, soft max: {}) + full controller support",
                        slots_.size(), SOFT_MAX_SLOTS);
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

            for (auto& s : slots_) s.reset();
            slots_.clear();

            MIX_DestroyMixer(mixer_);
            mixer_ = nullptr;
            MIX_Quit();
            mixer_ready_ = false;
        }

        fonts_.clear();
        if (ttf_ready_) TTF_Quit();
        SDL_Quit();

        stopped_callbacks_.clear();

        initialized_ = false;
        LOG_SUCCESS_CAT("SDL3", "Shutdown complete");
    }

    // Called on all window-related events
    void onWindowEvent(const SDL_Event& ev) noexcept {
        switch (ev.type) {
            // Focus events → update mouse capture/grab
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            case SDL_EVENT_WINDOW_SHOWN:
            case SDL_EVENT_WINDOW_HIDDEN:
                updateFocusState();
                break;

            // Size changes → recreate swapchain / adjust render resolution
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                onResize();
                break;
    
            // Position / display changes (optional logging or multi-monitor handling)
            case SDL_EVENT_WINDOW_MOVED:
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                // Optional: log or update multi-monitor aware rendering
                LOG_DEBUG_CAT("SDL3", "Window moved or display changed");
                break;

            // Minimized / restored → pause/resume rendering if desired
            case SDL_EVENT_WINDOW_MINIMIZED:
                LOG_INFO_CAT("SDL3", "Window minimized — pausing rendering");
                // Optional: set a paused flag to skip heavy compute dispatches
                break;

            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                LOG_INFO_CAT("SDL3", "Window restored/maximized");
                onResize();  // Re-check size
                break;

            // User wants to close → handle gracefully
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                LOG_INFO_CAT("SDL3", "Window close requested");
                handleQuit();  // or push SDL_QUIT event
                break;

            case SDL_EVENT_WINDOW_LAST:
                break;

            // Rare / advanced
            case SDL_EVENT_WINDOW_HIT_TEST:
            case SDL_EVENT_WINDOW_ICCPROF_CHANGED:
                // Usually ignore, but log for debugging
                LOG_DEBUG_CAT("SDL3", "Advanced window event: {}", ev.type);
                break;

            default:
                // Not a window event — ignore or forward elsewhere
                break;
        }
    }

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

    void applyOptions() noexcept {
        if (!window_) return;
        SDL_SetWindowFullscreen(window_, Options::SDL3::StartFullscreen);
        SDL_SetWindowBordered(window_, !Options::SDL3::BorderlessWindow);
        SDL_SetWindowResizable(window_, Options::SDL3::AllowWindowResize);

        // Mouse capture only when focused — handled in updateFocusState()
        bool capture = Options::SDL3::EnableInputCapture && hasFocus_;
        SDL_SetWindowRelativeMouseMode(window_, capture);
    }

    // ────────────────────────────────────────────────
    // FOCUS & CAPTURE LOGIC
    // ────────────────────────────────────────────────
private:
    bool hasFocus_ = false;

    void updateFocusState() noexcept {
        if (!window_) return;

        Uint32 flags = SDL_GetWindowFlags(window_);
        bool newFocus = (flags & SDL_WINDOW_INPUT_FOCUS) == 0;

        if (newFocus != hasFocus_) {
            hasFocus_ = newFocus;
            bool capture = Options::SDL3::EnableInputCapture && hasFocus_;
            SDL_SetWindowRelativeMouseMode(window_, capture);
            Options::SDL3::EnableInputCapture = true;

            LOG_INFO_CAT("SDL3", "Window focus changed: {} (relative mouse: {})",
                         hasFocus_ ? "gained" : "lost", capture ? "enabled" : "disabled");
        }
    }

public:
    bool hasFocus() const noexcept { return hasFocus_; }

    // ── Global hotkey handlers ──────────────────────────────────────────────
    void handleQuit() noexcept {
        LOG_INFO_CAT("SDL3", "ESC pressed — quitting application");
        SDL_Event quitEvent{};
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    }

    void toggleAdaptiveResolution() noexcept {
        Options::Rendering::EnableAdaptiveResolution = !Options::Rendering::EnableAdaptiveResolution;
        LOG_INFO_CAT("RENDER", "Adaptive resolution toggled: {}",
                     Options::Rendering::EnableAdaptiveResolution ? "ON" : "OFF");
    }

    void toggleRayTracing() noexcept {
        Options::Rendering::EnableHardwareRayTracing = !Options::Rendering::EnableHardwareRayTracing;
        if (Options::Rendering::EnableHardwareRayTracing && !Options::Rendering::PreferHardwareRT) {
            LOG_WARNING_CAT("RENDER", "Hardware RT toggled ON but PreferHardwareRT is OFF — may use software fallback");
        }
        LOG_INFO_CAT("RENDER", "Hardware Ray Tracing toggled: {}",
                     Options::Rendering::EnableHardwareRayTracing ? "ON" : "OFF");
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

            auto& s = slots_[static_cast<size_t>(slot)];

            if (cmd_lower == "play") {
                if (!s.audio) return -1;

                MIX_Track* track = tracks_[static_cast<size_t>(slot)];
                MIX_SetTrackAudio(track, s.audio);
                MIX_SetTrackGain(track, s.gain);
                MIX_PlayTrack(track, 0);
                s.state = AudioSlotState::Playing;

                if (getPlayingCount() > static_cast<size_t>(SOFT_MAX_SLOTS)) {
                    LOG_WARNING_CAT("AUDIO", "Active playing count ({}) exceeds soft max ({})", getPlayingCount(), SOFT_MAX_SLOTS);
                }

                LOG_SUCCESS_CAT("AUDIO", "Playing '{}' on slot {}", s.filename, slot);
            } else {
                LOG_SUCCESS_CAT("AUDIO", "Loaded '{}' into slot {}", file, slot);
            }
            return slot;
        }
        else if (cmd_lower == "stop") {
            size_t s = static_cast<size_t>(preferred_slot);
            if (preferred_slot >= 0 && s < slots_.size() && slots_[s].isActive()) {
                MIX_StopTrack(tracks_[s], 0);
                slots_[s].state = AudioSlotState::Loaded;
                LOG_INFO_CAT("AUDIO", "Stopped slot {}", preferred_slot);
            }
            return preferred_slot;
        }
        else if (cmd_lower == "pause") {
            size_t s = static_cast<size_t>(preferred_slot);
            if (preferred_slot >= 0 && s < slots_.size() && slots_[s].state == AudioSlotState::Playing) {
                MIX_PauseTrack(tracks_[s]);
                slots_[s].state = AudioSlotState::Paused;
                LOG_INFO_CAT("AUDIO", "Paused slot {}", preferred_slot);
            }
            return preferred_slot;
        }
        else if (cmd_lower == "remove") {
            size_t s = static_cast<size_t>(preferred_slot);
            if (preferred_slot >= 0 && s < slots_.size() && slots_[s].audio) {
                MIX_StopTrack(tracks_[s], 0);
                slots_[s].reset();
                LOG_INFO_CAT("AUDIO", "Removed slot {}", preferred_slot);
            }
            return preferred_slot;
        }

        LOG_ERROR_CAT("AUDIO", "Unknown cmd '{}'", cmd);
        return -1;
    }

    void onTrackFinished(int slot, std::function<void(int)> cb) {
        if (!mixer_ready_ || slot < 0 || static_cast<size_t>(slot) >= slots_.size() || !tracks_[static_cast<size_t>(slot)]) return;

        std::lock_guard<std::mutex> lock(callback_mtx_);
        stopped_callbacks_[tracks_[static_cast<size_t>(slot)]] = std::move(cb);
    }

    bool isTrackPlaying(int slot) const {
        if (slot < 0 || static_cast<size_t>(slot) >= slots_.size() || !tracks_[static_cast<size_t>(slot)]) return false;
        return MIX_TrackPlaying(tracks_[static_cast<size_t>(slot)]);
    }

    size_t getActiveSlotCount() const { return slots_.size(); }

    size_t getPlayingCount() const {
        size_t count = 0;
        for (const auto& s : slots_)
            if (s.state == AudioSlotState::Playing) ++count;
        return count;
    }

private:
    void preloadAudioFiles() {
        const auto& files = Options::SDL3::PreloadedAudioFiles;
        if (files.empty()) return;

        slots_.reserve(files.size() + 16);
        tracks_.reserve(files.size() + 16);

        size_t loaded = 0;
        for (const auto& path : files) {
            MIX_Audio* a = MIX_LoadAudio(mixer_, path.c_str(), false);
            if (!a) {
                LOG_ERROR_CAT("AUDIO", "Preload failed: {}", path);
                continue;
            }

            size_t idx = slots_.size();
            slots_.emplace_back();
            auto& slot = slots_.back();
            slot.audio    = a;
            slot.filename = path;
            slot.state    = AudioSlotState::Loaded;

            MIX_Track* t = MIX_CreateTrack(mixer_);
            if (!t) {
                LOG_ERROR_CAT("AUDIO", "Failed to create track for '{}'", path);
                slot.reset();
                continue;
            }

            MIX_SetTrackGain(t, DEFAULT_VOLUME);
            MIX_SetTrackStoppedCallback(t, track_stopped_callback, this);
            tracks_.push_back(t);

            LOG_INFO_CAT("AUDIO", "Preloaded '{}' → slot {}", path, idx);
            ++loaded;
        }

        LOG_SUCCESS_CAT("AUDIO", "Preloaded {}/{} requested files", loaded, files.size());
    }

    int findOrAllocateSlot(const std::string& file, int preferred = -1) {
        // Reuse if already loaded
        for (size_t i = 0; i < slots_.size(); ++i) {
            if (slots_[i].filename == file && slots_[i].audio) {
                return static_cast<int>(i);
            }
        }

        // Try preferred slot if valid & free
        size_t pref = static_cast<size_t>(preferred);
        if (preferred >= 0 && pref < slots_.size() && slots_[pref].state == AudioSlotState::Free) {
            loadIntoSlot(pref, file);
            return preferred;
        }

        // First free slot
        for (size_t i = 0; i < slots_.size(); ++i) {
            if (slots_[i].state == AudioSlotState::Free) {
                loadIntoSlot(i, file);
                return static_cast<int>(i);
            }
        }

        // Create new
        return loadIntoNewSlot(file);
    }

    int loadIntoNewSlot(const std::string& file) {
        MIX_Audio* a = MIX_LoadAudio(mixer_, file.c_str(), false);
        if (!a) {
            LOG_ERROR_CAT("AUDIO", "Dynamic load failed '{}': {}", file, SDL_GetError());
            return -1;
        }

        size_t idx = slots_.size();
        slots_.emplace_back();
        auto& slot = slots_.back();
        slot.audio    = a;
        slot.filename = file;
        slot.state    = AudioSlotState::Loaded;

        MIX_Track* t = MIX_CreateTrack(mixer_);
        if (!t) {
            LOG_ERROR_CAT("AUDIO", "Failed to create track for '{}'", file);
            slot.reset();
            return -1;
        }

        MIX_SetTrackGain(t, DEFAULT_VOLUME);
        MIX_SetTrackStoppedCallback(t, track_stopped_callback, this);
        tracks_.push_back(t);

        LOG_INFO_CAT("AUDIO", "Allocated new slot {} for '{}'", idx, file);
        return static_cast<int>(idx);
    }

    void loadIntoSlot(size_t idx, const std::string& file) {
        MIX_Audio* a = MIX_LoadAudio(mixer_, file.c_str(), false);
        if (!a) {
            LOG_ERROR_CAT("AUDIO", "Load failed '{}': {}", file, SDL_GetError());
            return;
        }

        auto& s = slots_[idx];
        if (s.audio) MIX_DestroyAudio(s.audio);
        s.audio    = a;
        s.filename = file;
        s.state    = AudioSlotState::Loaded;
    }

    static void track_stopped_callback(void* userdata, MIX_Track* track) {
        auto* self = static_cast<SDL3System*>(userdata);
        if (!self) return;

        std::lock_guard<std::mutex> lock(self->callback_mtx_);
        auto it = self->stopped_callbacks_.find(track);
        if (it == self->stopped_callbacks_.end()) return;

        int slot_idx = -1;
        for (size_t i = 0; i < self->tracks_.size(); ++i) {
            if (self->tracks_[i] == track) {
                slot_idx = static_cast<int>(i);
                self->slots_[i].state = AudioSlotState::Loaded;
                break;
            }
        }

        if (slot_idx >= 0) it->second(slot_idx);
        self->stopped_callbacks_.erase(it);
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
        if (!t) LOG_ERROR_CAT("SDL", "CreateTextureFromSurface failed: {}", SDL_GetError());
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

    void pump(const SDL_Event& ev) noexcept {
        // Handle window focus / visibility changes first
        if (ev.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
            ev.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
            ev.type == SDL_EVENT_WINDOW_SHOWN ||
            ev.type == SDL_EVENT_WINDOW_HIDDEN) {
            updateFocusState();
        }

        // Global hotkeys — always active (even if not focused, for quit)
        if (ev.type == SDL_EVENT_KEY_DOWN) {
            const bool* keys = SDL_GetKeyboardState(nullptr);

            if (keys[SDL_SCANCODE_ESCAPE]) {
                handleQuit();
            }
            if (keys[SDL_SCANCODE_F1]) {
                toggleAdaptiveResolution();
            }
            if (keys[SDL_SCANCODE_F2]) {
                toggleRayTracing();
            }
        }

        // Only process subscriptions & input if window has focus
        if (hasFocus_) {
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
    }

    void invalidateAll() { generation_.fetch_add(1); }

    glm::vec2 mouseDelta() const {
        if (!hasFocus_) return {0.0f, 0.0f};
        float x = 0, y = 0;
        SDL_GetRelativeMouseState(&x, &y);
        return {static_cast<float>(x), static_cast<float>(y)};
    }

    void bind(std::string_view action, SDL_Scancode code) {
        std::lock_guard<std::mutex> lock(mtx_);
        bindings_[std::string(action)] = code;
    }

    bool down(std::string_view action) const {
        if (!hasFocus_) return false;
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = bindings_.find(std::string(action));
        if (it == bindings_.end()) return false;
        const bool* state = SDL_GetKeyboardState(nullptr);
        return state[static_cast<int>(it->second)];
    }

    glm::vec3 movement(float speed, float dt) const {
        if (!hasFocus_) return glm::vec3(0.0f);
        glm::vec3 v(0.0f);
        if (down("move_forward"))  v.z -= 1.0f;
        if (down("move_backward")) v.z += 1.0f;
        if (down("move_left"))     v.x -= 1.0f;
        if (down("move_right"))    v.x += 1.0f;
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
        return slot >= 0 && static_cast<size_t>(slot) < controllers_.size() &&
               controllers_[static_cast<size_t>(slot)].gamepad != nullptr;
    }

    const char* controllerName(int slot = 0) const {
        return SDL_GetGamepadName(controllers_[static_cast<size_t>(slot)].gamepad.get());
    }

    float leftStickX(int slot = 0) const {
        float val = SDL_GetGamepadAxis(controllers_[static_cast<size_t>(slot)].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float leftStickY(int slot = 0) const {
        float val = SDL_GetGamepadAxis(controllers_[static_cast<size_t>(slot)].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float rightStickX(int slot = 0) const {
        float val = SDL_GetGamepadAxis(controllers_[static_cast<size_t>(slot)].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float rightStickY(int slot = 0) const {
        float val = SDL_GetGamepadAxis(controllers_[static_cast<size_t>(slot)].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
        return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
    }

    float leftTrigger(int slot = 0) const {
        return SDL_GetGamepadAxis(controllers_[static_cast<size_t>(slot)].gamepad.get(), SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    }

    float rightTrigger(int slot = 0) const {
        return SDL_GetGamepadAxis(controllers_[static_cast<size_t>(slot)].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    }

    bool buttonDown(int slot, SDL_GamepadButton button) const {
        return SDL_GetGamepadButton(controllers_[static_cast<size_t>(slot)].gamepad.get(), button);
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
        if (!SDL_IsGamepad(id)) return;
        auto* gp = SDL_OpenGamepad(id);
        if (!gp) {
            LOG_WARNING_CAT("INPUT", "SDL_OpenGamepad failed for id {}: {}", id, SDL_GetError());
            return;
        }

        size_t port = 0;
        for (; port < controllers_.size(); ++port)
            if (!controllers_[port].gamepad) break;

        if (port >= controllers_.size()) {
            SDL_CloseGamepad(gp);
            LOG_WARNING_CAT("INPUT", "No free controller port for id {}", id);
            return;
        }

        controllers_[port].gamepad.reset(gp);
        controllers_[port].id = id;

        auto props = SDL_GetGamepadProperties(gp);
        controllers_[port].rumble = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false) && Options::SDL3::EnableRumble;
        controllers_[port].gyro   = SDL_GamepadHasSensor(gp, SDL_SENSOR_GYRO) && Options::SDL3::EnableGyro;

        LOG_SUCCESS_CAT("INPUT", "Gamepad connected (Player {} Port {}) — {}", port+1, port, SDL_GetGamepadName(gp));
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

    SDL3System() = default;

    bool initialized_   = false;
    bool mixer_ready_   = false;
    bool ttf_ready_     = false;

    SDL_Window* window_ = nullptr;
    MIX_Mixer*  mixer_  = nullptr;

    std::vector<MIX_Track*>     tracks_;
    std::vector<AudioSlot>      slots_;

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
#define ON_EVENT(cb) INPUT.subscribe([](const SDL_Event& ev){ cb(ev); }, #cb)