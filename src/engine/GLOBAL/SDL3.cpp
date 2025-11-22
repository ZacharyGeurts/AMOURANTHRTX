// source/engine/GLOBAL/SDL3.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL
// THE ZAPPER HAS FIRED — MOTHER BRAIN IS NO MORE
// =============================================================================

#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <source_location>

using namespace Logging::Color;
using namespace std::chrono;

// =============================================================================
// Global state
// =============================================================================
std::unique_ptr<VulkanRenderer> g_vulkanRenderer;
SDLWindowPtr                    g_sdl_window;

std::atomic<int>  g_resizeWidth{0};
std::atomic<int>  g_resizeHeight{0};
std::atomic<bool> g_resizeRequested{false};

// =============================================================================
// SDLWindowDeleter — RAII eternal
// =============================================================================
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept
{
    if (w) {
        LOG_INFO_CAT("Dispose", "{}Returning window to the void @ {:p}{}", OCEAN_TEAL, static_cast<void*>(w), RESET);
        SDL_DestroyWindow(w);
    }
}

// =============================================================================
// Namespace: SDL3Initializer — Input System
// =============================================================================
namespace SDL3Initializer {

std::string SDL3Input::locationString(const std::source_location& loc)
{
    return std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
}

SDL3Input::SDL3Input() = default;

SDL3Input::~SDL3Input()
{
    const size_t count = m_gamepads.size();
    if (count > 0) {
        LOG_INFO_CAT("Dispose", "{}SDL3Input destroyed — {} gamepad(s) auto-closed by RAII{}", 
                     SAPPHIRE_BLUE, count, RESET);
    } else {
        LOG_INFO_CAT("Dispose", "{}SDL3Input destroyed — no gamepads to close{}", SAPPHIRE_BLUE, RESET);
    }
    m_gamepads.clear();
}

void SDL3Input::initialize()
{
    const std::string loc = locationString();
    const std::string_view platform = SDL_GetPlatform();

    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Input", "{}Unsupported platform: {} | {}{}", 
                      OCEAN_TEAL, platform, loc, RESET);
        throw std::runtime_error(std::format("Unsupported platform: {}", platform));
    }

    LOG_SUCCESS_CAT("Input", "{}Initializing SDL3Input | {}{}", OCEAN_TEAL, loc, RESET);

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");

    int num = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&num);
    LOG_INFO_CAT("Input", "{}Found {} joystick(s) at startup | {}{}", OCEAN_TEAL, num, loc, RESET);

    if (joysticks) {
        for (int i = 0; i < num; ++i) {
            if (SDL_IsGamepad(joysticks[i])) {
                if (SDL_Gamepad* gp = SDL_OpenGamepad(joysticks[i])) {
                    m_gamepads[joysticks[i]] = GamepadPtr(gp);

                    if (m_gamepadConnectCallback) {
                        m_gamepadConnectCallback(true, joysticks[i], gp);
                    }

                    if (Options::Audio::ENABLE_HAPTICS_FEEDBACK) {
                        constexpr Uint16 intensity = 32768;
                        SDL_RumbleGamepad(gp, intensity, intensity, 500);
                        LOG_INFO_CAT("Input", "{}HAPTICS: Connect rumble → ID {}{}", OCEAN_TEAL, joysticks[i], RESET);
                    }
                }
            }
        }
        SDL_free(joysticks);
    }
}

bool SDL3Input::pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                LOG_INFO_CAT("Input", "{}Quit requested — goodbye, warrior{}", OCEAN_TEAL, RESET);
                return !exitOnClose;

            case SDL_EVENT_WINDOW_RESIZED:
                LOG_INFO_CAT("Input", "{}Window resized: {}x{}{}", OCEAN_TEAL, ev.window.data1, ev.window.data2, RESET);
                if (m_resizeCallback) m_resizeCallback(ev.window.data1, ev.window.data2);
                break;

            case SDL_EVENT_KEY_DOWN:
                handleKeyboard(ev.key, window, audioDevice, consoleOpen);
                if (m_keyboardCallback) m_keyboardCallback(ev.key);
                break;

            case SDL_EVENT_KEY_UP:
                if (m_keyboardCallback) m_keyboardCallback(ev.key);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                handleMouseButton(ev.button, window);
                if (m_mouseButtonCallback) m_mouseButtonCallback(ev.button);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (m_mouseMotionCallback) m_mouseMotionCallback(ev.motion);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                if (m_mouseWheelCallback) m_mouseWheelCallback(ev.wheel);
                break;

            case SDL_EVENT_TEXT_INPUT:
                if (m_textInputCallback) m_textInputCallback(ev.text);
                break;

            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
                if (m_touchCallback) m_touchCallback(ev.tfinger);
                break;

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                handleGamepadButton(ev.gbutton, audioDevice);
                if (m_gamepadButtonCallback) m_gamepadButtonCallback(ev.gbutton);
                break;

            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if (m_gamepadAxisCallback) m_gamepadAxisCallback(ev.gaxis);
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
                handleGamepadConnection(ev.gdevice);
                break;
        }
    }
    return true;
}

void SDL3Input::setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                             MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                             GamepadButtonCallback gb, GamepadAxisCallback ga,
                             GamepadConnectCallback gc, ResizeCallback resize)
{
    m_keyboardCallback      = std::move(kb);
    m_mouseButtonCallback   = std::move(mb);
    m_mouseMotionCallback   = std::move(mm);
    m_mouseWheelCallback    = std::move(mw);
    m_textInputCallback     = std::move(ti);
    m_touchCallback         = std::move(tc);
    m_gamepadButtonCallback = std::move(gb);
    m_gamepadAxisCallback   = std::move(ga);
    m_gamepadConnectCallback = std::move(gc);
    m_resizeCallback        = std::move(resize);

    LOG_SUCCESS_CAT("Input", "{}All 10 input callbacks registered — READY FOR DOMINATION{}", LIME_GREEN, RESET);
}

void SDL3Input::enableTextInput(SDL_Window* window, bool enable)
{
    if (enable) {
        SDL_StartTextInput(window);
        LOG_INFO_CAT("Input", "{}Text input ENABLED{}", OCEAN_TEAL, RESET);
    } else {
        SDL_StopTextInput(window);
        LOG_INFO_CAT("Input", "{}Text input DISABLED{}", OCEAN_TEAL, RESET);
    }
}

void SDL3Input::exportLog(std::string_view filename) const
{
    const std::string loc = locationString();
    LOG_INFO_CAT("Input", "{}Exporting input log → {} | {}{}", OCEAN_TEAL, filename, loc, RESET);

    std::ofstream f(filename.data(), std::ios::app);
    if (f.is_open()) {
        auto now = system_clock::now();
        auto secs = duration_cast<seconds>(now.time_since_epoch()).count();
        f << "[INPUT LOG] " << secs << " | Gamepads: " << m_gamepads.size() << "\n";
        LOG_SUCCESS_CAT("Input", "{}Log exported → {}{}", OCEAN_TEAL, filename, RESET);
    } else {
        LOG_ERROR_CAT("Input", "{}Failed to export log → {}{}", OCEAN_TEAL, filename, RESET);
    }
}

void SDL3Input::handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen)
{
    if (!k.down) return;

    switch (k.key) {
        case SDLK_F:
            {
                bool fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0;
                SDL_SetWindowFullscreen(window, !fs);
                LOG_INFO_CAT("Input", "{}Fullscreen → {}{}", OCEAN_TEAL, !fs ? "ENABLED" : "DISABLED", RESET);
            }
            break;

        case SDLK_ESCAPE:
            {
                SDL_Event quit{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&quit);
            }
            break;

        case SDLK_SPACE:
            if (audioDevice) {
                bool paused = SDL_AudioDevicePaused(audioDevice);
                paused ? SDL_ResumeAudioDevice(audioDevice) : SDL_PauseAudioDevice(audioDevice);
                LOG_INFO_CAT("Input", "{}Audio {} via SPACE{}", OCEAN_TEAL, paused ? "RESUMED" : "PAUSED", RESET);
            }
            break;

        case SDLK_M:
            if (audioDevice) {
                float gain = SDL_GetAudioDeviceGain(audioDevice);
                SDL_SetAudioDeviceGain(audioDevice, gain > 0.5f ? 0.0f : 1.0f);
                LOG_INFO_CAT("Input", "{}Audio MUTE toggle{}", OCEAN_TEAL, RESET);
            }
            break;

        case SDLK_GRAVE:
            if (Options::Performance::ENABLE_CONSOLE_LOG) {
                consoleOpen = !consoleOpen;
                LOG_INFO_CAT("Input", "{}Console → {}{}", OCEAN_TEAL, consoleOpen ? "OPEN" : "CLOSED", RESET);
            }
            break;
    }
}

void SDL3Input::handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window)
{
    if (b.down && b.button == SDL_BUTTON_RIGHT) {
        bool relative = SDL_GetWindowRelativeMouseMode(window);
        SDL_SetWindowRelativeMouseMode(window, !relative);
        LOG_INFO_CAT("Input", "{}Relative mouse → {}{}", OCEAN_TEAL, !relative ? "ENABLED" : "DISABLED", RESET);
    }
}

void SDL3Input::handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice)
{
    if (!g.down) return;

    switch (g.button) {
        case SDL_GAMEPAD_BUTTON_EAST:
            {
                SDL_Event quit{.type = SDL_EVENT_QUIT};
                SDL_PushEvent(&quit);
            }
            break;

        case SDL_GAMEPAD_BUTTON_START:
            if (audioDevice) {
                bool paused = SDL_AudioDevicePaused(audioDevice);
                paused ? SDL_ResumeAudioDevice(audioDevice) : SDL_PauseAudioDevice(audioDevice);
            }
            break;
    }
}

void SDL3Input::handleGamepadConnection(const SDL_GamepadDeviceEvent& e)
{
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (SDL_Gamepad* gp = SDL_OpenGamepad(e.which)) {
            m_gamepads[e.which] = GamepadPtr(gp);

            LOG_SUCCESS_CAT("Input", "{}Gamepad CONNECTED → ID: {} | Total: {}{}", 
                            LIME_GREEN, e.which, m_gamepads.size(), RESET);

            if (m_gamepadConnectCallback) {
                m_gamepadConnectCallback(true, e.which, gp);
            }

            if (Options::Audio::ENABLE_HAPTICS_FEEDBACK) {
                constexpr Uint16 intensity = 32768;
                SDL_RumbleGamepad(gp, intensity, intensity, 500);
            }
        }
    }
    else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto it = m_gamepads.find(e.which);
        if (it != m_gamepads.end()) {
            LOG_INFO_CAT("Input", "{}Gamepad DISCONNECTED → ID: {} | Remaining: {}{}", 
                         AMBER_YELLOW, e.which, m_gamepads.size() - 1, RESET);

            if (m_gamepadConnectCallback) {
                m_gamepadConnectCallback(false, e.which, it->second.get());
            }

            m_gamepads.erase(it);
        }
    }
}

} // namespace SDL3Initializer

// =============================================================================
// Namespace: SDL3Window — The One True Forge
// =============================================================================
namespace SDL3Window {

namespace detail {
    std::atomic<uint64_t> g_lastResizeTime{0};
    std::atomic<int>      g_pendingWidth{0};
    std::atomic<int>      g_pendingHeight{0};
    std::atomic<bool>     g_resizePending{false};
    constexpr uint64_t    RESIZE_DEBOUNCE_MS = 150;
}

void create(const char* title, int width, int height, Uint32 flags)
{
    LOG_SUCCESS_CAT("MAIN", "{}[PHASE 4] FORGING WINDOW + VULKAN CONTEXT — PINK PHOTONS RISING{}", VALHALLA_GOLD, RESET);

    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;  // Add HIDDEN to control show timing

    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
            LOG_FATAL_CAT("SDL3", "{}SDL_Init FAILED: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            throw std::runtime_error("SDL_Init failed");
        }
    }
    LOG_SUCCESS_CAT("SDL3", "{}SDL3 subsystems ONLINE — B-A-N-A-N-A-S{}", EMERALD_GREEN, RESET);

    if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
        LOG_FATAL_CAT("SDL3", "{}SDL_Vulkan_LoadLibrary FAILED: {} — Check Vulkan installation!{}", BLOOD_RED, SDL_GetError(), RESET);
        throw std::runtime_error("Vulkan load failed — driver missing or corrupted");
    }
    LOG_SUCCESS_CAT("SDL3", "{}Vulkan library loaded successfully via SDL3{}", VALHALLA_GOLD, RESET);

    SDL_Window* win = SDL_CreateWindow(title, width, height, flags);
    if (!win) {
        LOG_FATAL_CAT("SDL3", "{}SDL_CreateWindow FAILED: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        throw std::runtime_error("Window creation failed");
    }

    g_sdl_window.reset(win);
    LOG_SUCCESS_CAT("SDL3", "{}WINDOW FORGED — {}x{} @ {:p} — PHOTONS HAVE A HOME{}", DIAMOND_SPARKLE, width, height, static_cast<void*>(win), RESET);

    uint32_t extCount = 0;
    if (SDL_Vulkan_GetInstanceExtensions(&extCount) == 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL_Vulkan_GetInstanceExtensions(count) failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("Vulkan extensions count failed");
    }

    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!sdlExts || extCount == 0) {
        LOG_FATAL_CAT("VULKAN", "{}SDL returned NULL or zero extensions{}", BLOOD_RED, RESET);
        throw std::runtime_error("Vulkan extensions failed");
    }

    std::vector<const char*> extensions(sdlExts, sdlExts + extCount);
    if (Options::Performance::ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);  // Vulkan 1.4 portability for macOS/moltenVK
        LOG_SUCCESS_CAT("VULKAN", "{}Validation + Portability enabled for Vulkan 1.4{}", VALHALLA_GOLD, RESET);
    }

    // Zero-init + explicit pNext = nullptr for Vulkan 1.4 strictness
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "AMOURANTH RTX";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VALHALLA TURBO";
    appInfo.engineVersion = VK_MAKE_VERSION(80, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    static const char* const validationLayers[] = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = Options::Performance::ENABLE_VALIDATION_LAYERS ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = Options::Performance::ENABLE_VALIDATION_LAYERS ? 1 : 0;
    createInfo.ppEnabledLayerNames = Options::Performance::ENABLE_VALIDATION_LAYERS ? validationLayers : nullptr;

    // Forge all variables — LOG EVERYTHING
    LOG_INFO_CAT("VULKAN", "{}FORGING INSTANCE — API: 1.3 | Extensions: {} | Layers: {}{}", VALHALLA_GOLD, extensions.size(), createInfo.enabledLayerCount, RESET);
    for (const auto& ext : extensions) {
        LOG_INFO_CAT("VULKAN", "  Ext: {}{}", AURORA_PINK, ext, RESET);
    }
    if (createInfo.enabledLayerCount > 0) {
        LOG_INFO_CAT("VULKAN", "  Layer: {}{}", PLASMA_FUCHSIA, validationLayers[0], RESET);
    }
    LOG_INFO_CAT("VULKAN", "{}pNext: nullptr | flags: 0x{:x}{}", EMERALD_GREEN, createInfo.flags, RESET);

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("VULKAN", "{}vkCreateInstance FAILED: {} — Check Vulkan drivers!{}", BLOOD_RED, result, RESET);
        throw std::runtime_error("Vulkan instance creation failed");
    }

    LOG_SUCCESS_CAT("VULKAN", "{}INSTANCE FORGED @ {:p} — PINK PHOTONS ASCEND{}", DIAMOND_SPARKLE, static_cast<void*>(instance), RESET);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(win, instance, nullptr, &surface)) {
        LOG_FATAL_CAT("VULKAN", "{}SDL_Vulkan_CreateSurface FAILED: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("Vulkan surface failed");
    }
    LOG_SUCCESS_CAT("VULKAN", "{}SURFACE FORGED @ {:p} — PATH OPEN{}", AURORA_PINK, static_cast<void*>(surface), RESET);

    // ZAPPER EASTER EGG — UNCHANGED
    for (int i = 0; i < 10; i++) LOG_INFO_CAT("ZAPPER", "{}*PEW* {}{}", RASPBERRY_PINK, "ZAPPER FIRES PINK PHOTON #" + std::to_string(i+1), RESET);

    // Show the window — ensure visible, no close right away
    SDL_ShowWindow(win);
    LOG_SUCCESS_CAT("MAIN", "{}WINDOW SHOWN — PINK PHOTONS FILL THE SCREEN{}", EMERALD_GREEN, RESET);

    // Optional: Hold window open for 5 seconds (for testing — remove in production)
    // SDL_Delay(5000);
}

std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) window = get();
    uint32_t count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&count)) return {};

    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    return exts ? std::vector<std::string>(exts, exts + count) : std::vector<std::string>{};
}

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    SDL_Event ev;
    quit = toggleFS = false;
    bool resized = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) toggleFS = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                detail::g_pendingWidth  = ev.window.data1;
                detail::g_pendingHeight = ev.window.data2;
                detail::g_resizePending = true;
                detail::g_lastResizeTime = SDL_GetTicks();
                resized = true;
                break;
        }
    }

    if (g_sdl_window) {
        SDL_GetWindowSizeInPixels(g_sdl_window.get(), &outW, &outH);
    }

    if (detail::g_resizePending && (SDL_GetTicks() - detail::g_lastResizeTime >= detail::RESIZE_DEBOUNCE_MS)) {
        g_resizeWidth.store(detail::g_pendingWidth);
        g_resizeHeight.store(detail::g_pendingHeight);
        g_resizeRequested.store(true);
        detail::g_resizePending = false;
        LOG_SUCCESS_CAT("Window", "{}RESIZE ACCEPTED → {}x{}{}", VALHALLA_GOLD, detail::g_pendingWidth.load(), detail::g_pendingHeight.load(), RESET);
    }

    return resized;
}

void toggleFullscreen() noexcept
{
    if (!g_sdl_window) return;
    bool isFS = SDL_GetWindowFlags(g_sdl_window.get()) & SDL_WINDOW_FULLSCREEN;
    SDL_SetWindowFullscreen(g_sdl_window.get(), !isFS);
    LOG_SUCCESS_CAT("Window", "{}FULLSCREEN {}{}", isFS ? "OFF" : "ON", isFS ? RASPBERRY_PINK : EMERALD_GREEN, RESET);
}

void destroy() noexcept
{
    LOG_INFO_CAT("VULKAN", "{}Returning photons to the void...{}", SAPPHIRE_BLUE, RESET);
    g_vulkanRenderer.reset();
    RTX::cleanupAll();
    LOG_SUCCESS_CAT("VULKAN", "{}Vulkan shutdown complete — Ellie Fier smiles{}", EMERALD_GREEN, RESET);

    g_sdl_window.reset();
    SDL_Quit();
    LOG_SUCCESS_CAT("Dispose", "{}SDL3 shutdown complete — empire sleeps in pink light{}", AURORA_PINK, RESET);
}

} // namespace SDL3Window

// =============================================================================
// Namespace: SDL3Audio — FULLY SDL3 COMPATIBLE — PINK PHOTONS HAVE VOICE
// =============================================================================
namespace SDL3Audio {

AudioManager::~AudioManager() {
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
    if (device_) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    sounds_.clear();
}

bool AudioManager::initMixer() {
    if (device_) {
        LOG_SUCCESS_CAT("AUDIO", "AudioManager already initialized — photons singing", EMERALD_GREEN, RESET);
        return true;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        LOG_ERROR_CAT("AUDIO", "SDL_InitSubSystem(AUDIO) failed: {}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        return false;
    }

    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device_ == 0) {
        LOG_FATAL_CAT("AUDIO", "SDL_OpenAudioDevice failed: {}", BLOOD_RED, SDL_GetError(), RESET);
        return false;
    }

    SDL_ResumeAudioDevice(device_);
    LOG_SUCCESS_CAT("AUDIO", "AUDIO DEVICE OPENED — ID: {} — PINK PHOTONS HAVE VOICE", VALHALLA_GOLD, device_, RESET);
    return true;
}

bool AudioManager::loadSound(std::string_view path, std::string_view name) {
    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 length = 0;

    if (!SDL_LoadWAV(path.data(), &spec, &buffer, &length)) {
        LOG_ERROR_CAT("AUDIO", "Failed to load WAV: {} | {}", AMBER_YELLOW, path, SDL_GetError(), RESET);
        return false;
    }

    auto sound = std::make_unique<SoundData>();
    sound->buffer = buffer;
    sound->length = length;
    sound->spec = spec;

    auto [it, inserted] = sounds_.try_emplace(std::string(name), std::move(sound));
    if (inserted) {
        LOG_SUCCESS_CAT("AUDIO", "SOUND LOADED → \"{}\" | {} bytes | {}Hz {}ch", 
                        RASPBERRY_PINK, name, length, spec.freq, spec.channels, RESET);
    } else {
        LOG_WARN_CAT("AUDIO", "Sound \"{}\" reloaded", AMBER_YELLOW, name, RESET);
        it->second = std::move(sound);
    }
    return true;
}

void AudioManager::playSound(std::string_view name) {
    auto it = sounds_.find(std::string(name));
    if (it == sounds_.end()) {
        LOG_WARN_CAT("AUDIO", "Sound \"{}\" not found — silence falls", CRIMSON_MAGENTA, name, RESET);
        return;
    }

    if (!device_) {
        LOG_ERROR_CAT("AUDIO", "No audio device — cannot play \"{}\"", BLOOD_RED, name, RESET);
        return;
    }

    const auto& sound = it->second;

    // Create and bind stream once (lazy init)
    if (!stream_) {
        stream_ = SDL_CreateAudioStream(&sound->spec, nullptr);
        if (!stream_) {
            LOG_ERROR_CAT("AUDIO", "SDL_CreateAudioStream failed: {}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            return;
        }

        if (SDL_BindAudioStream(device_, stream_) == 0) {
            LOG_ERROR_CAT("AUDIO", "SDL_BindAudioStream failed: {}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
            return;
        }
    }

    // Feed the WAV data into the stream
    if (SDL_PutAudioStreamData(stream_, sound->buffer, sound->length) == 0) {
        LOG_ERROR_CAT("AUDIO", "SDL_PutAudioStreamData failed: {}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
    } else {
        LOG_INFO_CAT("AUDIO", "PLAY → \"{}\" — PINK PHOTONS SING ACROSS THE VOID", PARTY_PINK, name, RESET);
    }
}

} // namespace SDL3Audio