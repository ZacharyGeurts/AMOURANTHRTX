#pragma once

// =============================================================================
// AMOURANTH RTX Engine — SDL3.hpp
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
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
#include <optional>
#include <cstdio>

#include "ELLIE.hpp"
#include "FieldDosDisplay.hpp"
#include "FieldRuntimeInfo.hpp"
#include "OptionsMenu.hpp"
#include "AMOURANTHRTX.hpp"

#ifdef _WIN32
    #include <windows.h>
    #include <mmdeviceapi.h>
    #include <endpointvolume.h>
    #pragma comment(lib, "ole32.lib")
#endif

// Returns current system master volume as 0.0–1.0
inline std::optional<float> GetSystemMasterVolume() noexcept
{
#ifdef _WIN32
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) return std::nullopt;

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        CoUninitialize();
        return std::nullopt;
    }

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        enumerator->Release();
        CoUninitialize();
        return std::nullopt;
    }

    IAudioEndpointVolume* endpointVolume = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&endpointVolume));
    if (FAILED(hr)) {
        device->Release();
        enumerator->Release();
        CoUninitialize();
        return std::nullopt;
    }

    float volume = 0.0f;
    hr = endpointVolume->GetMasterVolumeLevelScalar(&volume);

    endpointVolume->Release();
    device->Release();
    enumerator->Release();
    CoUninitialize();

    return SUCCEEDED(hr) ? std::optional<float>(volume) : std::nullopt;

#elif defined(__linux__)
    FILE* pipe = popen("pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null | grep -Po '\\d+(?=%)' | head -n1", "r");
    if (!pipe) return std::nullopt;

    int percent = 50;
    if (fscanf(pipe, "%d", &percent) != 1) {
        pclose(pipe);
        return std::nullopt;
    }
    pclose(pipe);

    return static_cast<float>(percent) / 100.0f;

#else
    return std::nullopt;
#endif
}

// =============================================================================
// CONSTANTS
// =============================================================================
constexpr int     SOFT_MAX_SLOTS       = Options::SDL3::MyAudioFiles; // OptionsMenu.hpp
constexpr int     AUDIO_FREQ           = Options::SDL3::AudioFrequency;
constexpr int     AUDIO_CHANNELS       = Options::SDL3::AudioChannels;
float             DEFAULT_VOLUME       = GetSystemMasterVolume().value_or(0.5f);

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
    bool            looping     = false;

    ~AudioSlot() { if (audio) MIX_DestroyAudio(audio); }

    void reset() noexcept {
        if (audio) {
            MIX_DestroyAudio(audio);
            audio = nullptr;
        }
        filename.clear();
        state = AudioSlotState::Free;
        gain  = DEFAULT_VOLUME;
        looping = false;
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
    static SDL3System& get() noexcept { static SDL3System instance; return instance; }

    SDL3System(const SDL3System&) = delete;
    SDL3System& operator=(const SDL3System&) = delete;
    bool init(SDL_Window* window) noexcept;
    void shutdown() noexcept;
    void onWindowEvent(const SDL_Event& ev) noexcept;
    void onResize() noexcept;
    void applyOptions() noexcept;
    void applyMouseCapture() noexcept;
    bool hasFocus() const noexcept { return hasFocus_; }
    void handleQuit() noexcept;
    void toggleAdaptiveResolution() noexcept;
    void toggleRayTracing() noexcept;
    void handleEscape(const bool* keys) noexcept;

    // Input methods used by Pipeline
    bool down(std::string_view action) const;
    bool gamepadButton(SDL_GamepadButton btn, int slot = 0) const;
    glm::vec2 mouseDelta()             const;

    float  leftStickX(int slot = 0)    const;
    float  leftStickY(int slot = 0)    const;
    float  rightStickX(int slot = 0)   const;
    float  rightStickY(int slot = 0)   const;
    float  leftTrigger(int slot = 0)   const;
    float  rightTrigger(int slot = 0)  const;

    size_t getPlayingCount()           const;
    size_t getActiveSlotCount()        const;
    bool   isTrackPlaying(int slot)    const;
    int    playSound(const std::string& file, const std::string& cmd, int preferred_slot = -1);

    bool       mixerReady() const noexcept { return mixer_ready_; }
    MIX_Mixer* mixer()      const noexcept { return mixer_; }

    void pump(const SDL_Event& ev) noexcept;

private:
    SDL3System() = default;   // Explicit default constructor for singleton (or use INPUT.XXX)

    void updateFocusState() noexcept;
    void preloadAudioFiles();
    int  findOrAllocateSlot(const std::string& file, int preferred = -1);
    int  loadIntoNewSlot(const std::string& file);
    void loadIntoSlot(size_t idx, const std::string& file);

    void openAllControllers();
    void openController(SDL_JoystickID id);
    void closeController(SDL_JoystickID id);

    void bindDefaultActions();
    void bind(std::string_view action, SDL_Scancode code);

    void processEvent(const SDL_Event& ev);

    static void track_stopped_callback(void* userdata, MIX_Track* track);

    bool hasFocus_ = false;
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
    std::unordered_map<std::string, SDL_Scancode> bindings_;

    std::mutex callback_mtx_;
    std::unordered_map<MIX_Track*, std::function<void(int)>> stopped_callbacks_;
};

#define INPUT  SDL3System::get()

// ────────────────────────────────────────────────
// IMPLEMENTATIONS
// ────────────────────────────────────────────────
inline bool SDL3System::init(SDL_Window* window) noexcept {
    if (initialized_) return true;
    window_ = window;

    bool headless = Options::SDL3::HeadlessMode;  // set before INPUT.init in navigator_main via env

    int sdlRes = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
    if (sdlRes == 0) {
        if (headless) {
            LOG_WARNING_CAT("SDL3", "SDL_Init non-zero in HEADLESS (using dummy/offscreen) — continuing (partial init expected; tolerant)");
        } else {
            return LOG_ERROR_RETURN_CAT("SDL3", false, "SDL_Init failed: {}", SDL_GetError());
        }
    }
    // tolerate in headless; original inverted check fixed for clarity here

    applyOptions();

    if (TTF_Init() == 0) {
        if (headless) {
            LOG_WARNING_CAT("TTF", "HEADLESS: TTF_Init failed (picky dummy env) — continuing with ttf_ready=false (no font/UI needed for dispatch/thermo/accountant path)");
            ttf_ready_ = false;
        } else {
            return LOG_ERROR_RETURN_CAT("TTF", false, "TTF_Init failed: {}", SDL_GetError());
        }
    } else {
        ttf_ready_ = true;
    }

    if (MIX_Init() == 0) { 
        // MIX_Init may return init flags; treat non-zero as ok or check further if needed
        LOG_WARN_CAT("MIXER", "MIX_Init returned non-zero (may be flags)");
    }

    SDL_AudioSpec desired{};
    desired.freq     = AUDIO_FREQ;
    desired.format   = SDL_AUDIO_F32;
    desired.channels = AUDIO_CHANNELS;

    mixer_ = MIX_CreateMixerDevice(-1, &desired);
    if (!mixer_) {
        LOG_ERROR_CAT("MIXER", "MIX_CreateMixerDevice failed: {}", SDL_GetError());
        if (headless) {
            LOG_WARNING_CAT("MIXER", "HEADLESS: dummy audio driver — continuing with mixer_ready=false (sounds skipped, dispatch/thermo unaffected; tolerant not fatal)");
            mixer_ready_ = false;
            // do not quit or return fail; allow headless test to proceed to RayCanvas dispatch
        } else {
            MIX_Quit();
            return false;
        }
    } else {
        mixer_ready_ = true;
        MIX_SetMixerGain(mixer_, DEFAULT_VOLUME);
    }

    preloadAudioFiles();
    bindDefaultActions();        
    updateFocusState();
    openAllControllers();

    FieldRuntimeInfo::refreshFromHost();

    initialized_ = true;
    LOG_SUCCESS_CAT("SDL3", "Initialized successfully");
    return true;
}

inline void SDL3System::shutdown() noexcept {
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

inline void SDL3System::onWindowEvent(const SDL_Event& ev) noexcept {
    switch (ev.type) {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_HIDDEN:
            updateFocusState();
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            onResize();
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            handleQuit();
            break;
        default:
            break;
    }
}

inline void SDL3System::onResize() noexcept {
    if (!window_) return;
    int pixelW = 0, pixelH = 0;
    SDL_GetWindowSizeInPixels(window_, &pixelW, &pixelH);
    if (pixelW <= 0 || pixelH <= 0) return;
    int dispW = 3840, dispH = 2160;
    const SDL_DisplayID did = SDL_GetDisplayForWindow(window_);
    SDL_Rect bounds{};
    if (did && SDL_GetDisplayBounds(did, &bounds) == 0) {
        dispW = bounds.w;
        dispH = bounds.h;
    }
    int logW = 0, logH = 0;
    SDL_GetWindowSize(window_, &logW, &logH);
    FieldDosDisplay::setWindowMetrics(pixelW, pixelH, SDL_GetWindowDisplayScale(window_), dispW, dispH,
                                      logW, logH);
    Swapchain::recreate(pixelW, pixelH);
}

inline void SDL3System::applyMouseCapture() noexcept {
    if (!window_) return;
    bool capture = Options::SDL3::EnableInputCapture && hasFocus_ && !Options::Canvas::IsX86Fields();
    SDL_SetWindowRelativeMouseMode(window_, capture);
    if (capture) SDL_HideCursor();
    else SDL_ShowCursor();
}

inline void SDL3System::applyOptions() noexcept {
    if (!window_) return;
    SDL_SetWindowFullscreen(window_, Options::SDL3::StartFullscreen);
    SDL_SetWindowBordered(window_, !Options::SDL3::BorderlessWindow);
    SDL_SetWindowResizable(window_, Options::SDL3::AllowWindowResize);
    applyMouseCapture();
}

inline void SDL3System::updateFocusState() noexcept {
    if (!window_) return;
    Uint64 flags = SDL_GetWindowFlags(window_);
    bool newFocus = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    if (newFocus != hasFocus_) {
        hasFocus_ = newFocus;
        applyMouseCapture();
        bool capture = Options::SDL3::EnableInputCapture && hasFocus_ && !Options::Canvas::IsX86Fields();
        LOG_INFO_CAT("SDL3", "Window focus changed: {} (relative mouse: {})", hasFocus_ ? "gained" : "lost", capture ? "enabled" : "disabled");
    }
}

inline void SDL3System::handleQuit() noexcept {
    SDL_Event quitEvent{};
    quitEvent.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quitEvent);
}

inline void SDL3System::handleEscape(const bool* keys) noexcept {
    if (!keys[SDL_SCANCODE_ESCAPE]) return;
    bool currentlyRelative = SDL_GetWindowRelativeMouseMode(window_);
    if (currentlyRelative) {
        SDL_SetWindowRelativeMouseMode(window_, false);
        LOG_INFO_CAT("SDL3", "ESC: Released mouse capture");
    } else if (Options::Canvas::IsX86Fields()) {
        /* RTX-DOS / Doom die panel — ESC is a game key, not quit. */
    } else {
        handleQuit();
        LOG_INFO_CAT("SDL3", "ESC: Mouse was free → quitting");
    }
}

inline bool SDL3System::gamepadButton(SDL_GamepadButton btn, int slot) const {
    if (!hasFocus_ || !Options::SDL3::EnableGamepad) return false;
    if (slot < 0 || static_cast<size_t>(slot) >= controllers_.size()) return false;
    const auto& c = controllers_[static_cast<size_t>(slot)];
    if (!c.gamepad) return false;
    return SDL_GetGamepadButton(c.gamepad.get(), btn);
}

inline bool SDL3System::down(std::string_view action) const {
    if (!hasFocus_) return false;
    using namespace Options::Input;
    if (action == "gp_south")          return gamepadButton(GP_South);
    if (action == "gp_east")           return gamepadButton(GP_East);
    if (action == "gp_west")           return gamepadButton(GP_West);
    if (action == "gp_north")          return gamepadButton(GP_North);
    if (action == "gp_left_shoulder")  return gamepadButton(GP_LeftShoulder);
    if (action == "gp_right_shoulder") return gamepadButton(GP_RightShoulder);
    if (action == "gp_left_stick")     return gamepadButton(GP_LeftStick);
    if (action == "gp_right_stick")    return gamepadButton(GP_RightStick);
    if (action == "gp_left_paddle1")   return gamepadButton(GP_LeftPaddle1);
    if (action == "gp_left_paddle2")   return gamepadButton(GP_LeftPaddle2);
    if (action == "gp_right_paddle1")  return gamepadButton(GP_RightPaddle1);
    if (action == "gp_right_paddle2")  return gamepadButton(GP_RightPaddle2);
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = bindings_.find(std::string(action));
    if (it == bindings_.end()) return false;
    if (it->second == SDL_SCANCODE_UNKNOWN) return false;
    const bool* state = SDL_GetKeyboardState(nullptr);
    return state && state[static_cast<int>(it->second)];
}

inline glm::vec2 SDL3System::mouseDelta() const {
    if (!hasFocus_) return {0.0f, 0.0f};
    float x = 0, y = 0;
    SDL_GetRelativeMouseState(&x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

inline float SDL3System::leftStickX(int slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= controllers_.size() || !controllers_[slot].gamepad) return 0.0f;
    float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
}

inline float SDL3System::leftStickY(int slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= controllers_.size() || !controllers_[slot].gamepad) return 0.0f;
    float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
}

inline float SDL3System::rightStickX(int slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= controllers_.size() || !controllers_[slot].gamepad) return 0.0f;
    float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
    return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
}

inline float SDL3System::rightStickY(int slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= controllers_.size() || !controllers_[slot].gamepad) return 0.0f;
    float val = SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
    return (std::abs(val) > Options::SDL3::GamepadDeadzone) ? val : 0.0f;
}

inline float SDL3System::leftTrigger(int slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= controllers_.size() || !controllers_[slot].gamepad) return 0.0f;
    return SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
}

inline float SDL3System::rightTrigger(int slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= controllers_.size() || !controllers_[slot].gamepad) return 0.0f;
    return SDL_GetGamepadAxis(controllers_[slot].gamepad.get(), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
}

inline size_t SDL3System::getPlayingCount() const {
    size_t count = 0;
    for (const auto& s : slots_)
        if (s.state == AudioSlotState::Playing) ++count;
    return count;
}

inline size_t SDL3System::getActiveSlotCount() const { return slots_.size(); }

inline bool SDL3System::isTrackPlaying(int slot) const {
    if (slot < 0 || static_cast<size_t>(slot) >= slots_.size()) return false;
    return MIX_TrackPlaying(tracks_[static_cast<size_t>(slot)]);
}

// Audio
inline int SDL3System::playSound(const std::string& file, const std::string& cmd, int preferred_slot) {
    if (!mixer_ready_) return -1;

    std::string cmd_lower = cmd;
    std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(), ::tolower);

    if (cmd_lower == "play" || cmd_lower == "load") {
        int slot = findOrAllocateSlot(file, preferred_slot);
        if (slot < 0) return -1;

        auto& s = slots_[static_cast<size_t>(slot)];

        if (cmd_lower == "play") {
            if (!s.audio) return -1;

            MIX_Track* track = tracks_[static_cast<size_t>(slot)];
            MIX_SetTrackAudio(track, s.audio);
            MIX_SetTrackGain(track, s.gain);
            MIX_PlayTrack(track, s.looping ? -1 : 0);
            s.state = AudioSlotState::Playing;

            LOG_INFO_CAT("AUDIO", "Playing '{}' on slot {}", s.filename, slot);
        }
        return slot;
    }
    else if (cmd_lower == "stop") {
        size_t s = static_cast<size_t>(preferred_slot);
        if (preferred_slot >= 0 && s < slots_.size() && slots_[s].isActive()) {
            MIX_StopTrack(tracks_[s], 0);
            slots_[s].state = AudioSlotState::Loaded;
        }
        return preferred_slot;
    }
    else if (cmd_lower == "pause") {
        size_t s = static_cast<size_t>(preferred_slot);
        if (preferred_slot >= 0 && s < slots_.size() && slots_[s].state == AudioSlotState::Playing) {
            MIX_PauseTrack(tracks_[s]);
            slots_[s].state = AudioSlotState::Paused;
        }
        return preferred_slot;
    }
    else if (cmd_lower == "resume") {
        size_t s = static_cast<size_t>(preferred_slot);
        if (preferred_slot >= 0 && s < slots_.size() && slots_[s].state == AudioSlotState::Paused) {
            MIX_ResumeTrack(tracks_[s]);
            slots_[s].state = AudioSlotState::Playing;
        }
        return preferred_slot;
    }
    else if (cmd_lower == "loop") {
        size_t s = static_cast<size_t>(preferred_slot);
        if (preferred_slot >= 0 && s < slots_.size()) {
            slots_[s].looping = true;
        }
        return preferred_slot;
    }

    LOG_ERROR_CAT("AUDIO", "Unknown audio cmd '{}'", cmd);
    return -1;
}

inline void SDL3System::pump(const SDL_Event& ev) noexcept {
    if (ev.type == SDL_EVENT_WINDOW_FOCUS_GAINED || ev.type == SDL_EVENT_WINDOW_SHOWN
        || ev.type == SDL_EVENT_WINDOW_FOCUS_LOST || ev.type == SDL_EVENT_WINDOW_HIDDEN) {
        updateFocusState();
    }

    if (hasFocus_) {
        if (ev.type == SDL_EVENT_KEY_DOWN) {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            if (keys) handleEscape(keys);
        }
        processEvent(ev);
    }
}

inline void SDL3System::processEvent(const SDL_Event& ev) {
    if (ev.type == SDL_EVENT_GAMEPAD_ADDED)   openController(ev.gdevice.which);
    if (ev.type == SDL_EVENT_GAMEPAD_REMOVED) closeController(ev.gdevice.which);
}

// Private helpers
inline void SDL3System::preloadAudioFiles() {
    const auto& files = Options::SDL3::PreloadedAudioFiles;
    if (files.empty()) return;

    slots_.reserve(files.size() + 16);
    tracks_.reserve(files.size() + 16);

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
            slot.reset();
            continue;
        }

        MIX_SetTrackGain(t, DEFAULT_VOLUME);
        MIX_SetTrackStoppedCallback(t, track_stopped_callback, this);
        tracks_.push_back(t);

        LOG_INFO_CAT("AUDIO", "Preloaded '{}' → slot {}", path, idx);
    }
}

inline int SDL3System::findOrAllocateSlot(const std::string& file, int preferred) {
    for (size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].filename == file && slots_[i].audio) return static_cast<int>(i);
    }

    if (preferred >= 0 && static_cast<size_t>(preferred) < slots_.size() && slots_[preferred].state == AudioSlotState::Free) {
        loadIntoSlot(preferred, file);
        return preferred;
    }

    for (size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].state == AudioSlotState::Free) {
            loadIntoSlot(i, file);
            return static_cast<int>(i);
        }
    }

    if (slots_.size() < SOFT_MAX_SLOTS) {
        return loadIntoNewSlot(file);
    }

    return -1;
}

inline int SDL3System::loadIntoNewSlot(const std::string& file) {
    MIX_Audio* a = MIX_LoadAudio(mixer_, file.c_str(), false);
    if (!a) { return LOG_ERROR_RETURN_CAT("AUDIO", -1, "Dynamic load failed '{}': {}", file, SDL_GetError()); }

    size_t idx = slots_.size();
    slots_.emplace_back();
    auto& slot = slots_.back();
    slot.audio = a;
    slot.filename = file;
    slot.state = AudioSlotState::Loaded;

    MIX_Track* t = MIX_CreateTrack(mixer_);
    if (!t) {
        slot.reset();
        return -1;
    }

    MIX_SetTrackGain(t, DEFAULT_VOLUME);
    MIX_SetTrackStoppedCallback(t, track_stopped_callback, this);
    tracks_.push_back(t);
    return static_cast<int>(idx);
}

inline void SDL3System::loadIntoSlot(size_t idx, const std::string& file) {
    MIX_Audio* a = MIX_LoadAudio(mixer_, file.c_str(), false);
    if (!a) return;
    auto& s = slots_[idx];
    if (s.audio) MIX_DestroyAudio(s.audio);
    s.audio = a;
    s.filename = file;
    s.state = AudioSlotState::Loaded;
}

inline void SDL3System::openAllControllers() {
    int n = 0;
    auto* ids = SDL_GetGamepads(&n);
    if (ids) {
        for (int i = 0; i < n; ++i) openController(ids[i]);
        SDL_free(ids);
    }
}

inline void SDL3System::openController(SDL_JoystickID id) {
    if (!SDL_IsGamepad(id)) return;
    auto* gp = SDL_OpenGamepad(id);
    if (!gp) return;

    size_t port = 0;
    for (; port < controllers_.size(); ++port)
        if (!controllers_[port].gamepad) break;

    if (port >= controllers_.size()) {
        SDL_CloseGamepad(gp);
        return;
    }

    controllers_[port].gamepad.reset(gp);
    controllers_[port].id = id;
    auto props = SDL_GetGamepadProperties(gp);
    controllers_[port].rumble = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false) && Options::SDL3::EnableRumble;
    controllers_[port].gyro   = SDL_GamepadHasSensor(gp, SDL_SENSOR_GYRO) && Options::SDL3::EnableGyro;

    LOG_SUCCESS_CAT("INPUT", "Gamepad connected (Player {})", port + 1);
}

inline void SDL3System::closeController(SDL_JoystickID id) {
    for (auto& c : controllers_) {
        if (c.id == id) {
            c.gamepad.reset();
            LOG_INFO_CAT("INPUT", "Gamepad disconnected (id {})", id);
            break;
        }
    }
}

inline void SDL3System::bindDefaultActions() {
    bind("move_forward",     SDL_SCANCODE_W);
    bind("move_backward",    SDL_SCANCODE_S);
    bind("move_left",        SDL_SCANCODE_A);
    bind("move_right",       SDL_SCANCODE_D);

    bind("jump",             SDL_SCANCODE_SPACE);
    bind("crouch",           SDL_SCANCODE_LCTRL);
    bind("sprint",           SDL_SCANCODE_LSHIFT);
    bind("interact",         SDL_SCANCODE_E);
    bind("reload",           SDL_SCANCODE_R);
    bind("use",              SDL_SCANCODE_F);

    bind("gp_south",         SDL_SCANCODE_UNKNOWN);
    bind("gp_east",          SDL_SCANCODE_UNKNOWN);
    bind("gp_west",          SDL_SCANCODE_UNKNOWN);
    bind("gp_north",         SDL_SCANCODE_UNKNOWN);
    bind("gp_left_shoulder", SDL_SCANCODE_UNKNOWN);
    bind("gp_right_shoulder",SDL_SCANCODE_UNKNOWN);
    bind("gp_left_stick",    SDL_SCANCODE_UNKNOWN);
    bind("gp_right_stick",   SDL_SCANCODE_UNKNOWN);
    bind("gp_left_paddle1",  SDL_SCANCODE_UNKNOWN);
    bind("gp_left_paddle2",  SDL_SCANCODE_UNKNOWN);
    bind("gp_right_paddle1", SDL_SCANCODE_UNKNOWN);
    bind("gp_right_paddle2", SDL_SCANCODE_UNKNOWN);
}

inline void SDL3System::bind(std::string_view action, SDL_Scancode code) {
    std::lock_guard<std::mutex> lock(mtx_);
    bindings_[std::string(action)] = code;
}

inline void SDL3System::track_stopped_callback(void* userdata, MIX_Track* track) {
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