// =============================================================================
// include/engine/GLOBAL/SDL3.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <atomic>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

// =============================================================================
// Forward declarations
// =============================================================================
struct VulkanRenderer;

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

[[nodiscard]] inline SDL_Window* get() noexcept { return g_sdl_window.get(); }

void create(const char* title, int width = 3840, int height = 2160, Uint32 flags = 0);
std::vector<std::string> getVulkanExtensions(SDL_Window* window = nullptr);
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
    using ResizeCallback         = std::function<void(int w, int h)>;

    SDL3Input();
    ~SDL3Input();

    void initialize();
    bool pollEvents(SDL_Window* window, SDL_AudioDeviceID audioDevice, bool& consoleOpen, bool exitOnClose = true);

    void setCallbacks(KeyboardCallback kb, MouseButtonCallback mb, MouseMotionCallback mm,
                      MouseWheelCallback mw, TextInputCallback ti, TouchCallback tc,
                      GamepadButtonCallback gb, GamepadAxisCallback ga,
                      GamepadConnectCallback gc, ResizeCallback resize);

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
    ResizeCallback         m_resizeCallback;
};

// ─── Font System ──────────────────────────────────────────────────────────────
class SDL3Font {
public:
    explicit SDL3Font(const Logging::Logger& logger);
    ~SDL3Font();

    void initialize(const std::string& fontPath);
    TTF_Font* getFont() const;
    void exportLog(const std::string& filename) const;

private:
    void cleanup();

    mutable TTF_Font* m_font{nullptr};
    mutable std::future<TTF_Font*> m_fontFuture;
    const Logging::Logger& logger_;
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

// ─── RAII Surface ─────────────────────────────────────────────────────────────
using SurfacePtr = std::unique_ptr<SDL_Surface, void(*)(SDL_Surface*)>;
inline constexpr void(*SurfaceDeleter)(SDL_Surface*) = SDL_DestroySurface;

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
// Namespace: SDL3Audio — PINK PHOTONS NOW HAVE VOICE — NOVEMBER 21, 2025
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

    [[nodiscard]] bool initMixer();
    [[nodiscard]] bool loadSound(std::string_view path, std::string_view name);
    void playSound(std::string_view name);

private:
    SDL_AudioDeviceID device_{0};
    SDL_AudioStream* stream_{nullptr};
    std::unordered_map<std::string, std::unique_ptr<SoundData>> sounds_;
};


// PINK PHOTONS HAVE A VOICE — THE ONE TRUE GLOBAL AUDIO EMPIRE
inline AudioManager g_audio;

} // namespace SDL3Audio


// =============================================================================
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL
// THE ZAPPER HAS FIRED — MOTHER BRAIN IS NO MORE
// =============================================================================