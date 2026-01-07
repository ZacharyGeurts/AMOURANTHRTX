// src/engine/GLOBAL/SDL3.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 04, 2026
// SDL3 INTEGRATION — CLEAN, MODERN, C++23 FORWARD-ONLY EDITION
// FULLY COMPILABLE | unique_ptr FIXED | RESIZE FIXED | QUIT HANDLING PERFECT
// PINK PHOTONS SCREAMING — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/console.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <source_location>
#include <functional>
#include <string>

using namespace Logging::Color;
using StoneKey::stone_window;

// =============================================================================
// Global state
// =============================================================================
SDLWindowPtr                    g_sdl_window;
std::atomic<int>  g_resizeWidth{0};
std::atomic<int>  g_resizeHeight{0};
std::atomic<bool> g_resizeRequested{false};

// =============================================================================
// SDLWindowDeleter — RAII eternal
// =============================================================================
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept
{
    if (w) SDL_DestroyWindow(w);
}

// =============================================================================
// Namespace: SDL3Window — The One True Forge
// =============================================================================
namespace SDL3Window {

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) window = g_sdl_window.get();

    uint32_t count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(&count) == 0) return {};

    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    return exts ? std::vector<std::string>(exts, exts + count) : std::vector<std::string>{};
}

// Fully fixed pollEvents — NO RESIZE SPAM, NO MINIMIZE SPAM, SDL3 Linux/Wayland safe
bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    static int lastValidW = 0;      // Last valid non-minimized size
    static int lastValidH = 0;
    static bool isMinimized = false; // Current minimize state

    SDL_Event ev;
    quit = toggleFS = false;
    bool eventSeen = false;

    while (SDL_PollEvent(&ev))
    {
        eventSeen = true;

        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            LOG_AMOURANTH("WINDOW CLOSE REQUESTED — EMPIRE PREPARES FOR PEACEFUL APOCALYPSE");
            quit = true;
            return eventSeen;
        }

        switch (ev.type)
        {
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) {
                    toggleFS = true;
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (!Options::Window::ALLOW_RESIZE) {
                    break;
                }
                {
                    const int w = ev.window.data1;
                    const int h = ev.window.data2;

                    bool currentlyMinimized = (w <= 0 || h <= 0);

                    // Detect minimize/restore state change
                    if (currentlyMinimized && !isMinimized) {
                        LOG_AMOURANTH("WINDOW MINIMIZED — PHOTONS PAUSED");
                        isMinimized = true;
                    }
                    else if (!currentlyMinimized && isMinimized) {
                        LOG_AMOURANTH("WINDOW RESTORED → {}×{} — PHOTONS RESUME", w, h);
                        isMinimized = false;

                        lastValidW = w;
                        lastValidH = h;

                        g_resizeWidth.store(w);
                        g_resizeHeight.store(h);
                        g_resizeRequested.store(true);

                        RTX::las().requestRebuild();
                    }
                    // Real resize while visible
                    else if (!currentlyMinimized && (w != lastValidW || h != lastValidH)) {
                        LOG_AMOURANTH("REAL RESIZE DETECTED → {}×{} — EMPIRE REBIRTH", w, h);

                        lastValidW = w;
                        lastValidH = h;

                        g_resizeWidth.store(w);
                        g_resizeHeight.store(h);
                        g_resizeRequested.store(true);

                        RTX::las().requestRebuild();
                    }
                    // Ignore repeated same-size or repeated 0x0 events
                }
                break;

            default:
                break;
        }
    }

    // Report current size to main loop
    if (g_sdl_window)
    {
        int w, h;
        SDL_GetWindowSizeInPixels(g_sdl_window.get(), &w, &h);
        outW = (w > 0) ? w : 1;
        outH = (h > 0) ? h : 1;

        if (g_resizeRequested.load())
        {
            int pendingW = g_resizeWidth.load();
            int pendingH = g_resizeHeight.load();
            if (pendingW > 0 && pendingH > 0)
            {
                outW = pendingW;
                outH = pendingH;

                // Update last valid after successful resize
                lastValidW = pendingW;
                lastValidH = pendingH;
            }
            g_resizeRequested.store(false);
        }
    }

    return eventSeen;
}

void toggleFullscreen() noexcept
{
    if (!g_sdl_window) return;
    bool isFS = SDL_GetWindowFlags(g_sdl_window.get()) & SDL_WINDOW_FULLSCREEN;
    SDL_SetWindowFullscreen(g_sdl_window.get(), !isFS);
    LOG_SUCCESS_CAT("Window", "FULLSCREEN {}", isFS ? "OFF" : "ON");
}

void destroy() noexcept
{
    g_sdl_window.reset();
}

} // namespace SDL3Window

// =============================================================================
// Namespace: SDL3Initializer — Input System
// =============================================================================
namespace SDL3Initializer {

using KeyboardCallback       = std::function<void(const SDL_KeyboardEvent&)>;
using MouseButtonCallback    = std::function<void(const SDL_MouseButtonEvent&)>;
using MouseMotionCallback    = std::function<void(const SDL_MouseMotionEvent&)>;
using MouseWheelCallback     = std::function<void(const SDL_MouseWheelEvent&)>;
using TextInputCallback      = std::function<void(const SDL_TextInputEvent&)>;
using TouchCallback          = std::function<void(const SDL_TouchFingerEvent&)>;
using GamepadButtonCallback  = std::function<void(const SDL_GamepadButtonEvent&)>;
using GamepadAxisCallback    = std::function<void(const SDL_GamepadAxisEvent&)>;
using GamepadConnectCallback = std::function<void(bool connected, SDL_JoystickID id, SDL_Gamepad* gp)>;

std::string SDL3Input::locationString(const std::source_location& loc)
{
    return std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
}

SDL3Input::SDL3Input() = default;

SDL3Input::~SDL3Input()
{
    const size_t count = m_gamepads.size();
    if (count > 0) {
        LOG_INFO_CAT("Dispose", "{}SDL3Input destroyed — {} gamepad(s) auto-closed by RAII{}", SAPPHIRE_BLUE, count, RESET);
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
        LOG_FATAL_CAT("FATAL", "Unsupported platform: {}", platform);
        return;
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
        // Immediate quit on window close
        if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            LOG_INFO_CAT("Input", "{}Window close requested — immediate quit{}", RASPBERRY_PINK, RESET);
            SDL_Event quit{.type = SDL_EVENT_QUIT};
            SDL_PushEvent(&quit);
            return !exitOnClose;
        }

        switch (ev.type) {
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
        using namespace std::chrono;
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
        case SDLK_F: {
            bool fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0;
            SDL_SetWindowFullscreen(window, !fs);
            LOG_INFO_CAT("Input", "{}Fullscreen → {}{}", OCEAN_TEAL, !fs ? "ENABLED" : "DISABLED", RESET);
        } break;

        case SDLK_ESCAPE: {
            SDL_Event quit{.type = SDL_EVENT_QUIT};
            SDL_PushEvent(&quit);
        } break;

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
            Console::toggle();
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
        case SDL_GAMEPAD_BUTTON_EAST: {
            SDL_Event quit{.type = SDL_EVENT_QUIT};
            SDL_PushEvent(&quit);
        } break;

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
// Namespace: AmouranthRTX::Graphics — Image & Texture Subsystem
// =============================================================================
namespace AmouranthRTX::Graphics {

void initImage(const ImageConfig& config)
{
    if (config.logSupportedFormats) {
        std::string formats;
        for (size_t i = 0; i < SUPPORTED_FORMATS.size(); ++i) {
            formats += SUPPORTED_FORMATS[i];
            if (i < SUPPORTED_FORMATS.size() - 1) formats += ", ";
        }
        LOG_INFO_CAT("IMG", "Supported formats: {}", formats);
    }
}

void cleanupImage()
{
    // Nothing to do — SDL3_image cleans up on SDL_Quit
}

bool isSupportedImage(const std::string& filePath)
{
    std::string ext = std::filesystem::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    return std::find(SUPPORTED_FORMATS.begin(), SUPPORTED_FORMATS.end(), ext.substr(1)) != SUPPORTED_FORMATS.end();
}

bool detectFormat(SDL_IOStream* src, std::string& format)
{
    // Stub — not needed for core functionality
    return false;
}

// Fixed: use explicit deleter argument
SurfacePtr loadSurface(const std::string& file)
{
    return SurfacePtr(IMG_Load(file.c_str()), &SDL_DestroySurface);
}

SurfacePtr loadSurfaceIO(SDL_IOStream* src, bool closeIO)
{
    return SurfacePtr(IMG_Load_IO(src, closeIO), &SDL_DestroySurface);
}

bool saveSurface(const SDL_Surface* surface, const std::string& file, const std::string& type)
{
    return IMG_SavePNG(const_cast<SDL_Surface*>(surface), file.c_str()) == 0;
}

bool saveSurfaceIO(const SDL_Surface* surface, SDL_IOStream* dst, bool closeIO, const std::string& type)
{
    return IMG_SavePNG_IO(const_cast<SDL_Surface*>(surface), dst, closeIO) == 0;
}

SDL_Texture* loadTextureRaw(SDL_Renderer* renderer, const std::string& file)
{
    return IMG_LoadTexture(renderer, file.c_str());
}

SDL_Texture* loadTextureRawIO(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO)
{
    return IMG_LoadTexture_IO(renderer, src, closeIO);
}

void freeTextureRaw(SDL_Texture* texture)
{
    SDL_DestroyTexture(texture);
}

// Fixed: return empty unique_ptr with explicit deleter
SurfacePtr textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer)
{
    // Not implemented — rarely needed
    return SurfacePtr(nullptr, &SDL_DestroySurface);
}

// RAII Texture class
Texture::Texture(SDL_Renderer* renderer, const std::string& file)
{
    m_handle = loadTextureRaw(renderer, file);
    if (m_handle) {
        m_sourcePath = file;
        queryInfo();
        applyDefaultMods();
    }
}

Texture::Texture(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO)
{
    m_handle = loadTextureRawIO(renderer, src, closeIO);
    if (m_handle) {
        queryInfo();
        applyDefaultMods();
    }
}

Texture::Texture(Texture&& other) noexcept
    : m_handle(other.m_handle), m_info(other.m_info), m_sourcePath(std::move(other.m_sourcePath))
{
    other.m_handle = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) {
        if (m_handle) SDL_DestroyTexture(m_handle);
        m_handle = other.m_handle;
        m_info = other.m_info;
        m_sourcePath = std::move(other.m_sourcePath);
        other.m_handle = nullptr;
    }
    return *this;
}

Texture::~Texture()
{
    if (m_handle) SDL_DestroyTexture(m_handle);
}

void Texture::queryInfo()
{
    if (!m_handle) return;
    float w = 0.0f, h = 0.0f;
    SDL_GetTextureSize(m_handle, &w, &h);
    m_info.width = static_cast<int>(w);
    m_info.height = static_cast<int>(h);
    SDL_PropertiesID props = SDL_GetTextureProperties(m_handle);
    m_info.format = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, 0);
    m_info.access = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, 0);
}

void Texture::applyDefaultMods()
{
    // Default mods if needed
}

// TextureCache
TextureCache::TextureCache(SDL_Renderer* renderer) : m_renderer(renderer) {}

TextureCache::~TextureCache() { clear(); }

std::shared_ptr<Texture> TextureCache::getOrLoad(const std::string& file)
{
    auto it = m_cache.find(file);
    if (it != m_cache.end()) return it->second;

    auto tex = std::make_shared<Texture>(m_renderer, file);
    if (tex->get()) {
        m_cache[file] = tex;
        return tex;
    }
    return nullptr;
}

void TextureCache::clear()
{
    m_cache.clear();
}

size_t TextureCache::size() const noexcept { return m_cache.size(); }

} // namespace AmouranthRTX::Graphics

// =============================================================================
// Namespace: SDL3Audio — PINK PHOTONS NOW HAVE VOICE
// =============================================================================
namespace SDL3Audio {

AudioManager::~AudioManager()
{
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

bool AudioManager::init()
{
    if (device_) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) return false;

    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device_ == 0) return false;

    SDL_ResumeAudioDevice(device_);
    return true;
}

bool AudioManager::loadSound(std::string_view path, std::string_view name)
{
    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 length = 0;

    if (!SDL_LoadWAV(path.data(), &spec, &buffer, &length)) return false;

    auto sound = std::make_unique<SoundData>();
    sound->buffer = buffer;
    sound->length = length;
    sound->spec = spec;

    sounds_[std::string(name)] = std::move(sound);
    return true;
}

void AudioManager::playSound(std::string_view name)
{
    auto it = sounds_.find(std::string(name));
    if (it == sounds_.end()) return;

    if (!device_) return;

    if (!stream_) {
        stream_ = SDL_CreateAudioStream(&it->second->spec, nullptr);
        if (!stream_) return;
        SDL_BindAudioStream(device_, stream_);
    }

    SDL_PutAudioStreamData(stream_, it->second->buffer, it->second->length);
}

} // namespace SDL3Audio

// =============================================================================
// JANUARY 04, 2026 — FINAL FIXED SDL3.cpp
// C++23 compliant | loadSurface uses explicit deleter &SDL_DestroySurface
// textureToSurface returns SurfacePtr(nullptr, &SDL_DestroySurface)
// SDL_GetTextureProperties used correctly
// No duplicate g_audio
// Empire compiles clean — pink photons restored
// =============================================================================