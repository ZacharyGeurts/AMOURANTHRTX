// AMOURANTH RTX Engine, October 2025 - Full-featured image loading, texture management, and surface handling.
// Thread-safe with C++20 features; no mutexes required (assumes single-threaded renderer access).
// Dependencies: SDL3, SDL3_image, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Enhanced features: RAII Texture class, IO stream loading, format detection, surface loading/saving,
// texture querying, color modulation/blend mode support, full format list logging.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_image.hpp"
#include "engine/logging.hpp"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <source_location>
#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <utility>  // std::move
#include <algorithm> // std::transform
#include <cctype>    // ::toupper

namespace AmouranthRTX::Graphics {

struct ImageConfig {
    bool logSupportedFormats = true;  // Log full list on init
};

struct TextureInfo {
    int width = 0;
    int height = 0;
    Uint32 format = 0;
    int access = 0;  // SDL_TEXTUREACCESS_*
    Uint32 modMode = 0;   // For blend/color mod tracking
    SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
};

// Forward declaration for RAII wrapper
class Texture;

// Free functions for subsystem management
void initImage(const ImageConfig& config);
void cleanupImage();

// Format detection
bool isSupportedImage(const std::string& filePath);
bool detectFormat(SDL_IOStream* src, std::string& format);

// Surface loading/saving (CPU-side)
SDL_Surface* loadSurface(const std::string& file);
SDL_Surface* loadSurfaceIO(SDL_IOStream* src, bool closeIO = true);
bool saveSurface(const SDL_Surface* surface, const std::string& file, const std::string& type = "png");
bool saveSurfaceIO(const SDL_Surface* surface, SDL_IOStream* dst, bool closeIO, const std::string& type);

// Texture loading (GPU-side, requires renderer)
SDL_Texture* loadTextureRaw(SDL_Renderer* renderer, const std::string& file);
SDL_Texture* loadTextureRawIO(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true);
void freeTextureRaw(SDL_Texture* texture);

// Utility: Convert texture to surface (for saving/processing)
SDL_Surface* textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer);

// RAII Texture wrapper for automatic management
class Texture {
private:
    SDL_Texture* m_handle = nullptr;
    TextureInfo m_info;
    std::string m_sourcePath;  // For logging/debug

    void queryInfo();
    void applyDefaultMods();

public:
    // Constructors
    explicit Texture(SDL_Renderer* renderer, const std::string& file);
    explicit Texture(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true);
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    ~Texture();

    // Deleted copy
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Accessors
    SDL_Texture* get() const noexcept { return m_handle; }
    const TextureInfo& info() const noexcept { return m_info; }
    int width() const noexcept { return m_info.width; }
    int height() const noexcept { return m_info.height; }
    Uint32 pixelFormat() const noexcept { return m_info.format; }
    const std::string& source() const noexcept { return m_sourcePath; }

    // Modulation and blending (wrappers for common ops)
    void setColorMod(Uint8 r, Uint8 g, Uint8 b);
    void getColorMod(Uint8& r, Uint8& g, Uint8& b) const;
    void setAlphaMod(Uint8 alpha);
    void getAlphaMod(Uint8& alpha) const;
    void setBlendMode(SDL_BlendMode mode);
    void getBlendMode(SDL_BlendMode& mode) const;

    // Utility: Save this texture to file (via surface conversion)
    bool saveToFile(const std::string& file, const std::string& type = "png", SDL_Renderer* renderer = nullptr) const;
};

// Simple cache (thread-unsafe; use external locking if multi-threaded)
class TextureCache {
private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_cache;
    SDL_Renderer* m_renderer = nullptr;

public:
    explicit TextureCache(SDL_Renderer* renderer);
    ~TextureCache();

    std::shared_ptr<Texture> getOrLoad(const std::string& file);
    void clear();
    size_t size() const noexcept { return m_cache.size(); }
};

}  // namespace AmouranthRTX::Graphics

// Implementation (inline for header-only convenience; split to .cpp if preferred)
namespace AmouranthRTX::Graphics {

const std::vector<std::string> SUPPORTED_FORMATS = {
    "ANI", "AVIF", "BMP", "CUR", "GIF", "ICO", "JPG", "JXL", "LBM", "PCX", "PNG", "PNM", "QOI", "SVG",
    "TGA", "TIF", "WEBP", "XCF", "XPM", "XV"
};

void initImage(const ImageConfig& config) {
    LOG_INFO_CAT("Image", "Initializing SDL_image subsystem", std::source_location::current());

    // Platform verification
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Image", "Unsupported platform: {}", std::source_location::current(), platform);
        throw std::runtime_error(std::string("Unsupported platform for images: ") + std::string(platform));
    }

    // No explicit IMG_Init in SDL3_image; formats auto-load on demand
    if (config.logSupportedFormats) {
        std::string formatsList;
        for (const auto& fmt : SUPPORTED_FORMATS) {
            formatsList += fmt + " ";
        }
        LOG_INFO_CAT("Image", "Supported formats: {}", std::source_location::current(), formatsList);
    }

    LOG_INFO_CAT("Image", "SDL_image fully initialized (all formats ready)", std::source_location::current());
}

void cleanupImage() {
    LOG_INFO_CAT("Image", "Cleaning up SDL_image subsystem", std::source_location::current());
    // No IMG_Quit; SDL handles on app exit
}

bool isSupportedImage(const std::string& filePath) {
    if (filePath.empty()) return false;

    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();
    if (ext.empty()) return false;

    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    ext = ext.substr(1);  // Remove '.'

    auto it = std::find(SUPPORTED_FORMATS.begin(), SUPPORTED_FORMATS.end(), ext);
    bool supported = it != SUPPORTED_FORMATS.end();

    LOG_DEBUG_CAT("Image", "Format check for '{}': {} (ext: {})", std::source_location::current(),
                  filePath, supported ? "supported" : "unsupported", ext);
    return supported;
}

bool detectFormat(SDL_IOStream* src, std::string& format) {
    if (!src) {
        format = "unknown";
        return false;
    }

    // Seek before each detection to read from start
    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isAVIF(src)) { format = "AVIF"; return true; }

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isBMP(src)) { format = "BMP"; return true; }

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isGIF(src)) { format = "GIF"; return true; }

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isJPG(src)) { format = "JPG"; return true; }

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isPNG(src)) { format = "PNG"; return true; }

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isTIF(src)) { format = "TIF"; return true; }

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isWEBP(src)) { format = "WEBP"; return true; }

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isQOI(src)) { format = "QOI"; return true; }

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    if (IMG_isSVG(src)) { format = "SVG"; return true; }

    // Add more as needed (e.g., JXL: IMG_isJXL(src))

    format = "unknown";
    return false;
}

SDL_Surface* loadSurface(const std::string& file) {
    LOG_DEBUG_CAT("Image", "Loading surface from file: {}", std::source_location::current(), file);

    if (!isSupportedImage(file)) {
        LOG_WARNING_CAT("Image", "Potentially unsupported format: {}", std::source_location::current(), file);
    }

    SDL_Surface* surface = IMG_Load(file.c_str());
    if (!surface) {
        LOG_ERROR_CAT("Image", "IMG_Load failed for {}: {}", std::source_location::current(), file, SDL_GetError());
        throw std::runtime_error(std::string("IMG_Load failed for ") + file + ": " + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Surface loaded: {} ({}x{})", std::source_location::current(), file,
                 surface->w, surface->h);
    return surface;
}

SDL_Surface* loadSurfaceIO(SDL_IOStream* src, bool closeIO) {
    if (!src) {
        throw std::invalid_argument("Invalid IO stream for surface load");
    }

    std::string detected;
    if (detectFormat(src, detected)) {
        LOG_DEBUG_CAT("Image", "Detected format: {}", std::source_location::current(), detected);
    } else {
        LOG_WARNING_CAT("Image", "Unknown format in stream", std::source_location::current());
    }

    // Seek to start after detection
    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);

    SDL_Surface* surface = IMG_Load_IO(src, closeIO);
    if (!surface) {
        LOG_ERROR_CAT("Image", "IMG_Load_IO failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("IMG_Load_IO failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Surface loaded from IO: {}x{}", std::source_location::current(),
                 surface->w, surface->h);
    return surface;
}

bool saveSurface(const SDL_Surface* surface, const std::string& file, const std::string& type) {
    if (!surface || file.empty()) return false;

    LOG_DEBUG_CAT("Image", "Saving surface to {} (type: {})", std::source_location::current(), file, type);

    SDL_IOStream* dst = SDL_IOFromFile(file.c_str(), "wb");
    if (!dst) {
        LOG_ERROR_CAT("Image", "Failed to open {} for writing", std::source_location::current(), file);
        return false;
    }

    bool success = IMG_SaveTyped_IO(const_cast<SDL_Surface*>(surface), dst, true, type.c_str());

    if (!success) {
        LOG_ERROR_CAT("Image", "IMG_SaveTyped_IO failed for {}: {}", std::source_location::current(), file, SDL_GetError());
    } else {
        LOG_INFO_CAT("Image", "Surface saved: {}", std::source_location::current(), file);
    }
    return success;
}

bool saveSurfaceIO(const SDL_Surface* surface, SDL_IOStream* dst, bool closeIO, const std::string& type) {
    if (!surface || !dst) return false;

    bool success = IMG_SaveTyped_IO(const_cast<SDL_Surface*>(surface), dst, closeIO, type.c_str());
    if (!success) {
        LOG_ERROR_CAT("Image", "IMG_SaveTyped_IO failed: {}", std::source_location::current(), SDL_GetError());
    } else if (closeIO) {
        LOG_INFO_CAT("Image", "Surface saved to IO stream (closed)", std::source_location::current());
    }
    return success;
}

SDL_Texture* loadTextureRaw(SDL_Renderer* renderer, const std::string& file) {
    if (!renderer) throw std::invalid_argument("Invalid renderer for texture load");

    LOG_DEBUG_CAT("Image", "Loading raw texture from: {}", std::source_location::current(), file);

    SDL_Texture* texture = IMG_LoadTexture(renderer, file.c_str());
    if (!texture) {
        LOG_ERROR_CAT("Image", "IMG_LoadTexture failed for {}: {}", std::source_location::current(), file, SDL_GetError());
        throw std::runtime_error(std::string("IMG_LoadTexture failed for ") + file + ": " + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Raw texture loaded: {}", std::source_location::current(), file);
    return texture;
}

SDL_Texture* loadTextureRawIO(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO) {
    if (!renderer || !src) throw std::invalid_argument("Invalid renderer or IO stream");

    std::string detected;
    detectFormat(src, detected);
    LOG_DEBUG_CAT("Image", "Loading raw texture from IO (format: {})", std::source_location::current(), detected);

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);

    SDL_Texture* texture = IMG_LoadTexture_IO(renderer, src, closeIO);
    if (!texture) {
        LOG_ERROR_CAT("Image", "IMG_LoadTexture_IO failed: {}", std::source_location::current(), SDL_GetError());
        throw std::runtime_error(std::string("IMG_LoadTexture_IO failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Raw texture loaded from IO", std::source_location::current());
    return texture;
}

void freeTextureRaw(SDL_Texture* texture) {
    if (!texture) return;

    LOG_DEBUG_CAT("Image", "Freeing raw texture: {}", std::source_location::current(), static_cast<void*>(texture));
    SDL_DestroyTexture(texture);
}

// Utility to convert texture back to surface (expensive; use sparingly)
SDL_Surface* textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer) {
    if (!texture || !renderer) return nullptr;

    float fw, fh;
    SDL_GetTextureSize(texture, &fw, &fh);
    int w = static_cast<int>(fw);
    int h = static_cast<int>(fh);
    if (w <= 0 || h <= 0) return nullptr;

    // Save current state
    SDL_Texture* prev_target = SDL_GetRenderTarget(renderer);
    SDL_Rect prev_viewport;
    SDL_GetRenderViewport(renderer, &prev_viewport);

    // Set up for rendering
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_Rect viewport = {0, 0, w, h};
    SDL_SetRenderViewport(renderer, &viewport);

    // Render the texture
    SDL_FRect dstrect = {0.f, 0.f, fw, fh};
    SDL_RenderTexture(renderer, texture, nullptr, &dstrect);

    // Read pixels
    SDL_Rect readrect = {0, 0, w, h};
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, &readrect);

    // Restore state
    SDL_SetRenderViewport(renderer, &prev_viewport);
    SDL_SetRenderTarget(renderer, prev_target);

    if (!surface) {
        LOG_ERROR_CAT("Image", "Failed to read rendered texture to surface: {}", std::source_location::current(), SDL_GetError());
    } else {
        LOG_DEBUG_CAT("Image", "Texture converted to surface: {}x{}", std::source_location::current(), w, h);
    }

    return surface;
}

// Texture RAII class implementation
Texture::Texture(SDL_Renderer* renderer, const std::string& file)
    : m_handle(loadTextureRaw(renderer, file)), m_sourcePath(file) {
    if (m_handle) queryInfo();
}

Texture::Texture(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO)
    : m_handle(loadTextureRawIO(renderer, src, closeIO)), m_sourcePath("IO_stream") {
    if (m_handle) queryInfo();
}

Texture::Texture(Texture&& other) noexcept
    : m_handle(other.m_handle), m_info(std::exchange(other.m_info, {})), m_sourcePath(std::move(other.m_sourcePath)) {
    other.m_handle = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        freeTextureRaw(m_handle);
        m_handle = other.m_handle;
        m_info = std::exchange(other.m_info, {});
        m_sourcePath = std::move(other.m_sourcePath);
        other.m_handle = nullptr;
    }
    return *this;
}

Texture::~Texture() {
    freeTextureRaw(m_handle);
}

void Texture::queryInfo() {
    if (!m_handle) return;

    float fw, fh;
    SDL_GetTextureSize(m_handle, &fw, &fh);
    m_info.width = static_cast<int>(fw);
    m_info.height = static_cast<int>(fh);

    auto props = SDL_GetTextureProperties(m_handle);
    m_info.format = static_cast<Uint32>(SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, 0));

    m_info.access = static_cast<int>(SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, 0));

    SDL_GetTextureBlendMode(m_handle, &m_info.blendMode);
    applyDefaultMods();  // Optional defaults
}

void Texture::applyDefaultMods() {
    // Example: Default to alpha blend if format supports
    if (m_info.format == SDL_PIXELFORMAT_RGBA8888) {
        setBlendMode(SDL_BLENDMODE_BLEND);
    }
}

void Texture::setColorMod(Uint8 r, Uint8 g, Uint8 b) {
    if (SDL_SetTextureColorMod(m_handle, r, g, b) == 0) {
        m_info.modMode |= 1;  // Flag set
    } else {
        LOG_WARNING_CAT("Image", "Failed to set color mod", std::source_location::current());
    }
}

void Texture::getColorMod(Uint8& r, Uint8& g, Uint8& b) const {
    SDL_GetTextureColorMod(m_handle, &r, &g, &b);
}

void Texture::setAlphaMod(Uint8 alpha) {
    if (SDL_SetTextureAlphaMod(m_handle, alpha) == 0) {
        m_info.modMode |= 2;
    } else {
        LOG_WARNING_CAT("Image", "Failed to set alpha mod", std::source_location::current());
    }
}

void Texture::getAlphaMod(Uint8& alpha) const {
    SDL_GetTextureAlphaMod(m_handle, &alpha);
}

void Texture::setBlendMode(SDL_BlendMode mode) {
    if (SDL_SetTextureBlendMode(m_handle, mode) == 0) {
        m_info.blendMode = mode;
    } else {
        LOG_WARNING_CAT("Image", "Failed to set blend mode", std::source_location::current());
    }
}

void Texture::getBlendMode(SDL_BlendMode& mode) const {
    SDL_GetTextureBlendMode(m_handle, &mode);
}

bool Texture::saveToFile(const std::string& file, const std::string& type, SDL_Renderer* renderer) const {
    if (!m_handle || !renderer) return false;
    SDL_Surface* surf = textureToSurface(m_handle, renderer);
    if (!surf) return false;
    bool success = saveSurface(surf, file, type);
    SDL_DestroySurface(surf);
    return success;
}

// TextureCache implementation
TextureCache::TextureCache(SDL_Renderer* renderer) : m_renderer(renderer) {}

TextureCache::~TextureCache() {
    clear();
}

std::shared_ptr<Texture> TextureCache::getOrLoad(const std::string& file) {
    auto it = m_cache.find(file);
    if (it != m_cache.end()) {
        LOG_DEBUG_CAT("Image", "Cache hit: {}", std::source_location::current(), file);
        return it->second;
    }

    auto tex = std::make_shared<Texture>(m_renderer, file);
    m_cache[file] = tex;
    LOG_INFO_CAT("Image", "Cache miss/load: {}", std::source_location::current(), file);
    return tex;
}

void TextureCache::clear() {
    m_cache.clear();
    LOG_DEBUG_CAT("Image", "Cache cleared", std::source_location::current());
}

}  // namespace AmouranthRTX::Graphics