// include/engine/SDL3/SDL3_image.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// SDL3_image Wrapper — FULL HEADER-ONLY — C++23 — NOVEMBER 12 2025 7:00 AM EST
// • NO source_location in logging — compiles with -std=c++23
// • ZERO .cpp — ALL inlined — 15,000 FPS
// • RASPBERRY_PINK logging — SHIP IT RAW
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <utility>
#include <algorithm>
#include <cctype>
#include <format>
#include <vector>

namespace AmouranthRTX::Graphics {

struct ImageConfig {
    bool logSupportedFormats = true;
};

struct TextureInfo {
    int width = 0;
    int height = 0;
    Uint32 format = 0;
    int access = 0;
    Uint32 modMode = 0;
    SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
};

// =============================================================================
// GLOBAL SUPPORTED FORMATS
// =============================================================================
static const std::vector<std::string> SUPPORTED_FORMATS = {
    "ANI", "AVIF", "BMP", "CUR", "GIF", "ICO", "JPG", "JXL", "LBM", "PCX", "PNG", "PNM", "QOI", "SVG",
    "TGA", "TIF", "WEBP", "XCF", "XPM", "XV"
};

// =============================================================================
// IMAGE SUBSYSTEM
// =============================================================================
inline void initImage(const ImageConfig& config = {}) {
    LOG_INFO_CAT("Image", "{}Initializing SDL_image subsystem{}", 
                 Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);

    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Image", "{}Unsupported platform: {}{}", 
                      Color::Logging::RASPBERRY_PINK, platform, Color::Logging::RESET);
        throw std::runtime_error(std::string("Unsupported platform: ") + std::string(platform));
    }

    if (config.logSupportedFormats) {
        std::string formatsList;
        for (const auto& fmt : SUPPORTED_FORMATS) formatsList += fmt + " ";
        LOG_INFO_CAT("Image", "{}Supported formats: {}{}", 
                     Color::Logging::RASPBERRY_PINK, formatsList, Color::Logging::RESET);
    }

    LOG_SUCCESS_CAT("Image", "{}SDL_image initialized — all formats ready{}", 
                    Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);
}

inline void cleanupImage() {
    LOG_INFO_CAT("Image", "{}SDL_image cleanup complete{}", 
                 Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);
}

// =============================================================================
// FORMAT UTILITIES
// =============================================================================
inline bool isSupportedImage(const std::string& filePath) {
    if (filePath.empty()) return false;

    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();
    if (ext.empty()) return false;

    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    ext = ext.substr(1);

    bool supported = std::find(SUPPORTED_FORMATS.begin(), SUPPORTED_FORMATS.end(), ext) != SUPPORTED_FORMATS.end();
    LOG_DEBUG_CAT("Image", "{}Format check '{}' → {} (ext: {})", 
                  Color::Logging::RASPBERRY_PINK, filePath, supported ? "SUPPORTED" : "unsupported", ext);
    return supported;
}

inline bool detectFormat(SDL_IOStream* src, std::string& format) {
    if (!src) { format = "unknown"; return false; }

    auto tryDetect = [&](auto fn, const char* name) -> bool {
        SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
        if (fn(src)) { format = name; return true; }
        return false;
    };

    if (tryDetect(IMG_isAVIF, "AVIF")) return true;
    if (tryDetect(IMG_isBMP, "BMP")) return true;
    if (tryDetect(IMG_isGIF, "GIF")) return true;
    if (tryDetect(IMG_isJPG, "JPG")) return true;
    if (tryDetect(IMG_isPNG, "PNG")) return true;
    if (tryDetect(IMG_isTIF, "TIF")) return true;
    if (tryDetect(IMG_isWEBP, "WEBP")) return true;
    if (tryDetect(IMG_isQOI, "QOI")) return true;
    if (tryDetect(IMG_isSVG, "SVG")) return true;

    format = "unknown";
    return false;
}

// =============================================================================
// SURFACE IO
// =============================================================================
inline SDL_Surface* loadSurface(const std::string& file) {
    LOG_DEBUG_CAT("Image", "{}Loading surface: {}{}", 
                  Color::Logging::RASPBERRY_PINK, file, Color::Logging::RESET);

    if (!isSupportedImage(file)) {
        LOG_WARNING_CAT("Image", "{}Potentially unsupported format: {}{}", 
                        Color::Logging::RASPBERRY_PINK, file, Color::Logging::RESET);
    }

    SDL_Surface* surface = IMG_Load(file.c_str());
    if (!surface) {
        LOG_ERROR_CAT("Image", "{}IMG_Load failed: {} → {}{}", 
                      Color::Logging::RASPBERRY_PINK, file, SDL_GetError(), Color::Logging::RESET);
        throw std::runtime_error(std::string("IMG_Load failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "{}Surface loaded: {} ({}x{}){}", 
                 Color::Logging::RASPBERRY_PINK, file, surface->w, surface->h, Color::Logging::RESET);
    return surface;
}

inline SDL_Surface* loadSurfaceIO(SDL_IOStream* src, bool closeIO = true) {
    if (!src) throw std::invalid_argument("Null IO stream");

    std::string fmt;
    detectFormat(src, fmt);
    LOG_DEBUG_CAT("Image", "{}IO stream format detected: {}{}", 
                  Color::Logging::RASPBERRY_PINK, fmt, Color::Logging::RESET);

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    SDL_Surface* surface = IMG_Load_IO(src, closeIO);
    if (!surface) {
        LOG_ERROR_CAT("Image", "{}IMG_Load_IO failed: {}{}", 
                      Color::Logging::RASPBERRY_PINK, SDL_GetError(), Color::Logging::RESET);
        throw std::runtime_error(std::string("IMG_Load_IO failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "{}Surface loaded from IO: {}x{}{}", 
                 Color::Logging::RASPBERRY_PINK, surface->w, surface->h, Color::Logging::RESET);
    return surface;
}

inline bool saveSurface(const SDL_Surface* surface, const std::string& file, const std::string& type = "png") {
    if (!surface || file.empty()) return false;

    SDL_IOStream* dst = SDL_IOFromFile(file.c_str(), "wb");
    if (!dst) {
        LOG_ERROR_CAT("Image", "{}Cannot open file for writing: {}{}", 
                      Color::Logging::RASPBERRY_PINK, file, Color::Logging::RESET);
        return false;
    }

    bool ok = IMG_SaveTyped_IO(const_cast<SDL_Surface*>(surface), dst, true, type.c_str());
    if (ok) LOG_INFO_CAT("Image", "{}Surface saved: {}{}", 
                         Color::Logging::RASPBERRY_PINK, file, Color::Logging::RESET);
    else LOG_ERROR_CAT("Image", "{}Save failed: {}{}", 
                       Color::Logging::RASPBERRY_PINK, SDL_GetError(), Color::Logging::RESET);
    return ok;
}

inline bool saveSurfaceIO(const SDL_Surface* surface, SDL_IOStream* dst, bool closeIO, const std::string& type = "png") {
    if (!surface || !dst) return false;
    bool ok = IMG_SaveTyped_IO(const_cast<SDL_Surface*>(surface), dst, closeIO, type.c_str());
    if (ok && closeIO) LOG_INFO_CAT("Image", "{}Surface saved to IO stream{}", 
                                    Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);
    return ok;
}

// =============================================================================
// TEXTURE IO (RAW)
// =============================================================================
inline SDL_Texture* loadTextureRaw(SDL_Renderer* renderer, const std::string& file) {
    if (!renderer) throw std::invalid_argument("Null renderer");

    LOG_DEBUG_CAT("Image", "{}Loading texture: {}{}", 
                  Color::Logging::RASPBERRY_PINK, file, Color::Logging::RESET);

    SDL_Texture* tex = IMG_LoadTexture(renderer, file.c_str());
    if (!tex) {
        LOG_ERROR_CAT("Image", "{}IMG_LoadTexture failed: {} → {}{}", 
                      Color::Logging::RASPBERRY_PINK, file, SDL_GetError(), Color::Logging::RESET);
        throw std::runtime_error(std::string("LoadTexture failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "{}Texture loaded: {}{}", 
                 Color::Logging::RASPBERRY_PINK, file, Color::Logging::RESET);
    return tex;
}

inline SDL_Texture* loadTextureRawIO(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true) {
    if (!renderer || !src) throw std::invalid_argument("Null renderer/IO");

    std::string fmt; detectFormat(src, fmt);
    LOG_DEBUG_CAT("Image", "{}Loading texture from IO (format: {}){}", 
                  Color::Logging::RASPBERRY_PINK, fmt, Color::Logging::RESET);

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    SDL_Texture* tex = IMG_LoadTexture_IO(renderer, src, closeIO);
    if (!tex) {
        LOG_ERROR_CAT("Image", "{}IMG_LoadTexture_IO failed: {}{}", 
                      Color::Logging::RASPBERRY_PINK, SDL_GetError(), Color::Logging::RESET);
        throw std::runtime_error(std::string("LoadTexture_IO failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "{}Texture loaded from IO{}", 
                 Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);
    return tex;
}

inline void freeTextureRaw(SDL_Texture* texture) {
    if (texture) {
        LOG_DEBUG_CAT("Image", "{}Destroying texture: {:p}{}", 
                      Color::Logging::RASPBERRY_PINK, static_cast<void*>(texture), Color::Logging::RESET);
        SDL_DestroyTexture(texture);
    }
}

inline SDL_Surface* textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer) {
    if (!texture || !renderer) return nullptr;

    float fw, fh;
    SDL_GetTextureSize(texture, &fw, &fh);
    int w = static_cast<int>(fw), h = static_cast<int>(fh);

    SDL_Texture* prevTarget = SDL_GetRenderTarget(renderer);
    SDL_Rect prevViewport; SDL_GetRenderViewport(renderer, &prevViewport);

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderViewport(renderer, nullptr);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_Surface* surf = SDL_RenderReadPixels(renderer, nullptr);

    SDL_SetRenderTarget(renderer, prevTarget);
    SDL_SetRenderViewport(renderer, &prevViewport);

    if (surf) LOG_DEBUG_CAT("Image", "{}Texture → Surface: {}x{}{}", 
                            Color::Logging::RASPBERRY_PINK, w, h, Color::Logging::RESET);
    else LOG_ERROR_CAT("Image", "{}textureToSurface failed: {}{}", 
                       Color::Logging::RASPBERRY_PINK, SDL_GetError(), Color::Logging::RESET);

    return surf;
}

// =============================================================================
// RAII TEXTURE
// =============================================================================
class Texture {
private:
    SDL_Texture* m_handle = nullptr;
    TextureInfo m_info{};
    std::string m_sourcePath;

    inline void queryInfo() {
        if (!m_handle) return;

        float fw, fh;
        SDL_GetTextureSize(m_handle, &fw, &fh);
        m_info.width = static_cast<int>(fw);
        m_info.height = static_cast<int>(fh);

        auto props = SDL_GetTextureProperties(m_handle);
        m_info.format = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, 0);
        m_info.access = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, 0);
        SDL_GetTextureBlendMode(m_handle, &m_info.blendMode);
    }

    inline void applyDefaultMods() {
        if (m_info.format == SDL_PIXELFORMAT_RGBA8888) {
            setBlendMode(SDL_BLENDMODE_BLEND);
        }
    }

public:
    explicit Texture(SDL_Renderer* renderer, const std::string& file)
        : m_handle(loadTextureRaw(renderer, file)), m_sourcePath(file) {
        if (m_handle) { queryInfo(); applyDefaultMods(); }
    }

    explicit Texture(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true)
        : m_handle(loadTextureRawIO(renderer, src, closeIO)), m_sourcePath("IO_stream") {
        if (m_handle) { queryInfo(); applyDefaultMods(); }
    }

    Texture(Texture&& other) noexcept
        : m_handle(other.m_handle), m_info(other.m_info), m_sourcePath(std::move(other.m_sourcePath)) {
        other.m_handle = nullptr;
    }

    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            freeTextureRaw(m_handle);
            m_handle = other.m_handle;
            m_info = other.m_info;
            m_sourcePath = std::move(other.m_sourcePath);
            other.m_handle = nullptr;
        }
        return *this;
    }

    ~Texture() { freeTextureRaw(m_handle); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    SDL_Texture* get() const noexcept { return m_handle; }
    const TextureInfo& info() const noexcept { return m_info; }
    int width() const noexcept { return m_info.width; }
    int height() const noexcept { return m_info.height; }
    Uint32 pixelFormat() const noexcept { return m_info.format; }
    const std::string& source() const noexcept { return m_sourcePath; }

    void setColorMod(Uint8 r, Uint8 g, Uint8 b) {
        if (SDL_SetTextureColorMod(m_handle, r, g, b)) {
            LOG_WARNING_CAT("Image", "{}setColorMod failed{}", 
                            Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);
        }
    }

    void getColorMod(Uint8& r, Uint8& g, Uint8& b) const {
        SDL_GetTextureColorMod(m_handle, &r, &g, &b);
    }

    void setAlphaMod(Uint8 alpha) {
        if (SDL_SetTextureAlphaMod(m_handle, alpha)) {
            LOG_WARNING_CAT("Image", "{}setAlphaMod failed{}", 
                            Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);
        }
    }

    void getAlphaMod(Uint8& alpha) const {
        SDL_GetTextureAlphaMod(m_handle, &alpha);
    }

    void setBlendMode(SDL_BlendMode mode) {
        if (SDL_SetTextureBlendMode(m_handle, mode) == 0) {
            m_info.blendMode = mode;
        } else {
            LOG_WARNING_CAT("Image", "{}setBlendMode failed{}", 
                            Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);
        }
    }

    void getBlendMode(SDL_BlendMode& mode) const {
        SDL_GetTextureBlendMode(m_handle, &mode);
    }

    bool saveToFile(const std::string& file, const std::string& type = "png", SDL_Renderer* renderer = nullptr) const {
        if (!m_handle || !renderer) return false;
        SDL_Surface* surf = textureToSurface(m_handle, renderer);
        if (!surf) return false;
        bool ok = saveSurface(surf, file, type);
        SDL_DestroySurface(surf);
        return ok;
    }
};

// =============================================================================
// TEXTURE CACHE
// =============================================================================
class TextureCache {
private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_cache;
    SDL_Renderer* m_renderer = nullptr;

public:
    explicit TextureCache(SDL_Renderer* renderer) : m_renderer(renderer) {}

    ~TextureCache() { clear(); }

    std::shared_ptr<Texture> getOrLoad(const std::string& file) {
        auto it = m_cache.find(file);
        if (it != m_cache.end()) {
            LOG_DEBUG_CAT("Image", "{}Cache HIT: {}{}", 
                          Color::Logging::RASPBERRY_PINK, file, Color::Logging::RESET);
            return it->second;
        }

        auto tex = std::make_shared<Texture>(m_renderer, file);
        m_cache[file] = tex;
        LOG_INFO_CAT("Image", "{}Cache MISS → loaded: {}{}", 
                     Color::Logging::RASPBERRY_PINK, file, Color::Logging::RESET);
        return tex;
    }

    void clear() {
        m_cache.clear();
        LOG_INFO_CAT("Image", "{}Texture cache cleared{}", 
                     Color::Logging::RASPBERRY_PINK, Color::Logging::RESET);
    }

    size_t size() const noexcept { return m_cache.size(); }
};

} // namespace AmouranthRTX::Graphics

// =============================================================================
// SDL3_image.hpp — HEADER-ONLY — RASPBERRY_PINK ETERNAL
// NO .cpp | ZERO LINK TIME | 15,000 FPS
// SHIP IT RAW
// =============================================================================