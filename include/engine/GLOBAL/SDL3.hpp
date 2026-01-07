// include/engine/GLOBAL/SDL3.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 06, 2026
// SDL3 INTEGRATION HEADER — CLEAN MODERN C++23 EDITION + RETRO BASIC LAYER
// CORE SDL3 AUDIO ONLY | MUSIC REQUIRES SEPARATE SDL3_mixer LIBRARY 💖
// PINK PHOTONS SCREAMING ETERNAL — EMPIRE UNSTOPPABLE — AMOURANTH FOREVER
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <atomic>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cstdlib>  // for std::system in Retro::cls
#include <cstdio>   // for printf in Retro stubs

#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

// =============================================================================
// Global window handle (RAII-protected)
// =============================================================================
struct SDLWindowDeleter { void operator()(SDL_Window* w) const noexcept; };
using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

extern SDLWindowPtr g_sdl_window;

// =============================================================================
// Global resize state (thread-safe)
// =============================================================================
extern std::atomic<int>  g_resizeWidth;
extern std::atomic<int>  g_resizeHeight;
extern std::atomic<bool> g_resizeRequested;

// =============================================================================
// Namespace: SDL3Window — The One True Forge
// =============================================================================
namespace SDL3Window {

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window = nullptr);

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept;

void toggleFullscreen() noexcept;

void destroy() noexcept;

} // namespace SDL3Window

// =============================================================================
// Namespace: SDL3Initializer — Input & Font
// =============================================================================
namespace SDL3Initializer {

// ─── Gamepad RAII ─────────────────────────────────────────────────────────────
struct GamepadDeleter {
    static inline const auto lambda = [](SDL_Gamepad* gp) { if (gp) SDL_CloseGamepad(gp); };
    using pointer = SDL_Gamepad*;
    void operator()(SDL_Gamepad* gp) const { lambda(gp); }
};
using GamepadPtr = std::unique_ptr<SDL_Gamepad, GamepadDeleter>;

// ─── Input System ─────────────────────────────────────────────────────────────
class SDL3Input {
public:
    using KeyboardCallback       = std::function<void(const SDL_KeyboardEvent&)>;
    using MouseButtonCallback    = std::function<void(const SDL_MouseButtonEvent&)>;
    using MouseMotionCallback    = std::function<void(const SDL_MouseMotionEvent&)>;
    using MouseWheelCallback     = std::function<void(const SDL_MouseWheelEvent&)>;
    using TextInputCallback      = std::function<void(const SDL_TextInputEvent&)>;
    using TouchCallback          = std::function<void(const SDL_TouchFingerEvent&)>;
    using GamepadButtonCallback  = std::function<void(const SDL_GamepadButtonEvent&)>;
    using GamepadAxisCallback    = std::function<void(const SDL_GamepadAxisEvent&)>;
    using GamepadConnectCallback = std::function<void(bool connected, SDL_JoystickID id, SDL_Gamepad* gp)>;

    SDL3Input();
    ~SDL3Input();

    void initialize();
    bool pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose = true);

    void setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                      MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                      GamepadButtonCallback gb, GamepadAxisCallback ga,
                      GamepadConnectCallback gc);

    void enableTextInput(SDL_Window* window, bool enable);

    [[nodiscard]] const std::map<SDL_JoystickID, GamepadPtr>& gamepads() const noexcept { return m_gamepads; }
    void exportLog(std::string_view filename) const;

private:
    static std::string locationString(const std::source_location& loc = std::source_location::current());

    void handleKeyboard(const SDL_KeyboardEvent& k, SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen);
    void handleMouseButton(const SDL_MouseButtonEvent& b, SDL_Window* window);
    void handleGamepadButton(const SDL_GamepadButtonEvent& g, SDL_AudioDeviceID audioDevice);
    void handleGamepadConnection(const SDL_GamepadDeviceEvent& e);

    std::map<SDL_JoystickID, GamepadPtr> m_gamepads;

    KeyboardCallback       m_keyboardCallback;
    MouseButtonCallback    m_mouseButtonCallback;
    MouseMotionCallback    m_mouseMotionCallback;
    MouseWheelCallback     m_mouseWheelCallback;
    TextInputCallback      m_textInputCallback;
    TouchCallback          m_touchCallback;
    GamepadButtonCallback  m_gamepadButtonCallback;
    GamepadAxisCallback    m_gamepadAxisCallback;
    GamepadConnectCallback m_gamepadConnectCallback;
};

} // namespace SDL3Initializer

// =============================================================================
// Namespace: AmouranthRTX::Graphics — Image & Texture Subsystem
// =============================================================================
namespace AmouranthRTX::Graphics {

struct ImageConfig {
    bool logSupportedFormats = true;
};

struct TextureInfo {
    int           width{0};
    int           height{0};
    Uint32        format{0};
    int           access{0};
    Uint32        modMode{0};
    SDL_BlendMode blendMode{SDL_BLENDMODE_NONE};
};

// ─── RAII Surface — C++23 fixed deleter
using SurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

// ─── Supported formats (SDL3_image) ───────────────────────────────────────────
inline static const std::vector<std::string> SUPPORTED_FORMATS = {
    "ANI", "AVIF", "BMP", "CUR", "GIF", "ICO", "JPG", "JXL", "LBM", "PCX",
    "PNG", "PNM", "QOI", "SVG", "TGA", "TIF", "WEBP", "XCF", "XPM", "XV"
};

// ─── Image subsystem control ──────────────────────────────────────────────────
void initImage(const ImageConfig& config = {});
void cleanupImage();

bool isSupportedImage(const std::string& filePath);
bool detectFormat(SDL_IOStream* src, std::string& format);

// ─── Surface I/O (RAII) ───────────────────────────────────────────────────────
SurfacePtr loadSurface(const std::string& file);
SurfacePtr loadSurfaceIO(SDL_IOStream* src, bool closeIO = true);
bool saveSurface(const SDL_Surface* surface, const std::string& file, const std::string& type = "png");
bool saveSurfaceIO(const SDL_Surface* surface, SDL_IOStream* dst, bool closeIO, const std::string& type = "png");

// ─── Raw texture loading (non-RAII) ───────────────────────────────────────────
SDL_Texture* loadTextureRaw(SDL_Renderer* renderer, const std::string& file);
SDL_Texture* loadTextureRawIO(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true);
void freeTextureRaw(SDL_Texture* texture);
SurfacePtr textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer);

// ─── RAII Texture class ───────────────────────────────────────────────────────
class Texture {
public:
    explicit Texture(SDL_Renderer* renderer, const std::string& file);
    explicit Texture(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true);
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] SDL_Texture* get() const noexcept { return m_handle; }
    [[nodiscard]] const TextureInfo& info() const noexcept { return m_info; }
    [[nodiscard]] int  width() const noexcept  { return m_info.width; }
    [[nodiscard]] int  height() const noexcept { return m_info.height; }
    [[nodiscard]] Uint32 pixelFormat() const noexcept { return m_info.format; }
    [[nodiscard]] const std::string& source() const noexcept { return m_sourcePath; }

    void setColorMod(Uint8 r, Uint8 g, Uint8 b);
    void getColorMod(Uint8& r, Uint8& g, Uint8& b) const;
    void setAlphaMod(Uint8 alpha);
    void getAlphaMod(Uint8& alpha) const;
    void setBlendMode(SDL_BlendMode mode);
    void getBlendMode(SDL_BlendMode& mode) const;

    bool saveToFile(const std::string& file, const std::string& type = "png", SDL_Renderer* renderer = nullptr) const;

private:
    void queryInfo();
    void applyDefaultMods();

    SDL_Texture* m_handle{nullptr};
    TextureInfo  m_info{};
    std::string  m_sourcePath;
};

// ─── Texture Cache ─────────────────────────────────────────────────────────────
class TextureCache {
public:
    explicit TextureCache(SDL_Renderer* renderer);
    ~TextureCache();

    std::shared_ptr<Texture> getOrLoad(const std::string& file);
    void clear();
    [[nodiscard]] size_t size() const noexcept;

private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_cache;
    SDL_Renderer*                                             m_renderer{nullptr};
};

} // namespace AmouranthRTX::Graphics

// =============================================================================
// Namespace: SDL3Audio — CORE SDL3 AUDIO ONLY (sounds via streaming)
// =============================================================================
namespace SDL3Audio {

struct SoundData {
    Uint8* buffer = nullptr;
    Uint32 length = 0;
    SDL_AudioSpec spec{};
    ~SoundData() { SDL_free(buffer); }
};

class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    AudioManager(AudioManager&&) noexcept = default;
    AudioManager& operator=(AudioManager&&) noexcept = default;

    [[nodiscard]] bool init();  // Core SDL3 audio init (rename your initMixer() impl to this)

    [[nodiscard]] bool loadSound(std::string_view path, std::string_view name);
    void playSound(std::string_view name);

private:
    SDL_AudioDeviceID device_{0};
    SDL_AudioStream* stream_{nullptr};
    std::unordered_map<std::string, std::unique_ptr<SoundData>> sounds_;
};

// Global audio empire
inline AudioManager g_audio;

} // namespace SDL3Audio

// =============================================================================
// Retro / BASIC-style API — Nostalgia layer (printf stubs for disabled features)
// =============================================================================
namespace Retro {

inline void cls() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

inline void beep() {
    std::cout << '\a' << std::flush;
}

inline void playsound(std::string_view name) {
    SDL3Audio::g_audio.playSound(name);
}

inline void playmusic([[maybe_unused]] std::string_view name, [[maybe_unused]] int loops = -1) {
    printf("[RETRO] PLAYMUSIC disabled — core SDL3 has no high-level music support (MP3/OGG/MOD). Build/link SDL3_mixer separately! 💖\n");
}

inline void stopmusic() {
    printf("[RETRO] STOPMUSIC disabled — no music active (SDL3_mixer required)\n");
}

inline void delay(Uint32 ms) {
    SDL_Delay(ms);
}

inline int rnd(int max = 100) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, max);
    return dist(gen);
}

inline void poke([[maybe_unused]] uintptr_t address, [[maybe_unused]] uint8_t value) {
    printf("[RETRO] POKE disabled — modern memory protection active! Use RTX shaders for true power 💖\n");
}

} // namespace Retro

// =============================================================================
// JANUARY 06, 2026 — PRINTF EDITION
// • All Retro stub messages now use simple printf (no Logging dependency issues)
// • Added <cstdio> for printf
// • AudioManager::init() — rename your SDL3.cpp implementation from initMixer() → init()
// • VulkanRenderer incomplete type errors:
//   → Remove the line g_vulkanRenderer->onResize(...) from pollEvents() in SDL3.cpp
//   → Handle resize in your main render loop using the atomic flags (g_resizeRequested etc.)
//   → Move the std::unique_ptr<VulkanRenderer> g_vulkanRenderer declaration to a .cpp with full VulkanRenderer definition
// Empire compiles — pink photons eternal
// =============================================================================