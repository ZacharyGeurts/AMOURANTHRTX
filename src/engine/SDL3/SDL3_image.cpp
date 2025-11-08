// src/engine/SDL3/SDL3_image.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// FIXED FOR CMAKE + GCC 14 + C++23 — NOVEMBER 08 2025
// NO "s" literal → no more operator""s errors

#include "engine/SDL3/SDL3_image.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>       // ← REQUIRED FOR std::string + "s" literal
#include <algorithm>
#include <cctype>
#include <format>
#include <filesystem>

namespace AmouranthRTX::Graphics {

const std::vector<std::string> SUPPORTED_FORMATS = {
    "ANI", "AVIF", "BMP", "CUR", "GIF", "ICO", "JPG", "JXL", "LBM", "PCX", "PNG", "PNM", "QOI", "SVG",
    "TGA", "TIF", "WEBP", "XCF", "XPM", "XV"
};

void initImage(const ImageConfig& config) {
    LOG_INFO_CAT("Image", "Initializing SDL_image subsystem");

    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Image", "Unsupported platform: {}", platform);
        throw std::runtime_error(std::string("Unsupported platform: ") + std::string(platform));
    }

    if (config.logSupportedFormats) {
        std::string formatsList;
        for (const auto& fmt : SUPPORTED_FORMATS) formatsList += fmt + " ";
        LOG_INFO_CAT("Image", "Supported formats: {}", formatsList);
    }

    LOG_SUCCESS_CAT("Image", "SDL_image initialized — all formats ready");
}

void cleanupImage() {
    LOG_INFO_CAT("Image", "SDL_image cleanup complete");
}

bool isSupportedImage(const std::string& filePath) {
    if (filePath.empty()) return false;

    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();
    if (ext.empty()) return false;

    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    ext = ext.substr(1);

    bool supported = std::find(SUPPORTED_FORMATS.begin(), SUPPORTED_FORMATS.end(), ext) != SUPPORTED_FORMATS.end();
    LOG_DEBUG_CAT("Image", "Format check '{}' → {} (ext: {})", filePath, supported ? "SUPPORTED" : "unsupported", ext);
    return supported;
}

bool detectFormat(SDL_IOStream* src, std::string& format) {
    if (!src) { format = "unknown"; return false; }

    auto tryDetect = [&](auto fn, const char* name) {
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

SDL_Surface* loadSurface(const std::string& file) {
    LOG_DEBUG_CAT("Image", "Loading surface: {}", file);

    if (!isSupportedImage(file)) {
        LOG_WARNING_CAT("Image", "Potentially unsupported format: {}", file);
    }

    SDL_Surface* surface = IMG_Load(file.c_str());
    if (!surface) {
        LOG_ERROR_CAT("Image", "IMG_Load failed: {} → {}", file, SDL_GetError());
        throw std::runtime_error(std::string("IMG_Load failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Surface loaded: {} ({}x{})", file, surface->w, surface->h);
    return surface;
}

SDL_Surface* loadSurfaceIO(SDL_IOStream* src, bool closeIO) {
    if (!src) throw std::invalid_argument("Null IO stream");

    std::string fmt;
    detectFormat(src, fmt);
    LOG_DEBUG_CAT("Image", "IO stream format detected: {}", fmt);

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    SDL_Surface* surface = IMG_Load_IO(src, closeIO);
    if (!surface) {
        LOG_ERROR_CAT("Image", "IMG_Load_IO failed: {}", SDL_GetError());
        throw std::runtime_error(std::string("IMG_Load_IO failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Surface loaded from IO: {}x{}", surface->w, surface->h);
    return surface;
}

bool saveSurface(const SDL_Surface* surface, const std::string& file, const std::string& type) {
    if (!surface || file.empty()) return false;

    SDL_IOStream* dst = SDL_IOFromFile(file.c_str(), "wb");
    if (!dst) {
        LOG_ERROR_CAT("Image", "Cannot open file for writing: {}", file);
        return false;
    }

    bool ok = IMG_SaveTyped_IO(const_cast<SDL_Surface*>(surface), dst, true, type.c_str());
    if (ok) LOG_INFO_CAT("Image", "Surface saved: {}", file);
    else LOG_ERROR_CAT("Image", "Save failed: {}", SDL_GetError());
    return ok;
}

bool saveSurfaceIO(const SDL_Surface* surface, SDL_IOStream* dst, bool closeIO, const std::string& type) {
    if (!surface || !dst) return false;
    bool ok = IMG_SaveTyped_IO(const_cast<SDL_Surface*>(surface), dst, closeIO, type.c_str());
    if (ok && closeIO) LOG_INFO_CAT("Image", "Surface saved to IO stream");
    return ok;
}

SDL_Texture* loadTextureRaw(SDL_Renderer* renderer, const std::string& file) {
    if (!renderer) throw std::invalid_argument("Null renderer");

    LOG_DEBUG_CAT("Image", "Loading texture: {}", file);

    SDL_Texture* tex = IMG_LoadTexture(renderer, file.c_str());
    if (!tex) {
        LOG_ERROR_CAT("Image", "IMG_LoadTexture failed: {} → {}", file, SDL_GetError());
        throw std::runtime_error(std::string("LoadTexture failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Texture loaded: {}", file);
    return tex;
}

SDL_Texture* loadTextureRawIO(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO) {
    if (!renderer || !src) throw std::invalid_argument("Null renderer/IO");

    std::string fmt; detectFormat(src, fmt);
    LOG_DEBUG_CAT("Image", "Loading texture from IO (format: {})", fmt);

    SDL_SeekIO(src, 0, SDL_IO_SEEK_SET);
    SDL_Texture* tex = IMG_LoadTexture_IO(renderer, src, closeIO);
    if (!tex) {
        LOG_ERROR_CAT("Image", "IMG_LoadTexture_IO failed: {}", SDL_GetError());
        throw std::runtime_error(std::string("LoadTexture_IO failed: ") + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Texture loaded from IO");
    return tex;
}

void freeTextureRaw(SDL_Texture* texture) {
    if (texture) {
        LOG_DEBUG_CAT("Image", "Destroying texture: {:p}", static_cast<void*>(texture));
        SDL_DestroyTexture(texture);
    }
}

SDL_Surface* textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer) {
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

    if (surf) LOG_DEBUG_CAT("Image", "Texture → Surface: {}x{}", w, h);
    else LOG_ERROR_CAT("Image", "textureToSurface failed: {}", SDL_GetError());

    return surf;
}

// Texture RAII
void Texture::queryInfo() {
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

void Texture::applyDefaultMods() {
    if (m_info.format == SDL_PIXELFORMAT_RGBA8888) {
        setBlendMode(SDL_BLENDMODE_BLEND);
    }
}

Texture::Texture(SDL_Renderer* renderer, const std::string& file)
    : m_handle(loadTextureRaw(renderer, file)), m_sourcePath(file) {
    if (m_handle) { queryInfo(); applyDefaultMods(); }
}

Texture::Texture(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO)
    : m_handle(loadTextureRawIO(renderer, src, closeIO)), m_sourcePath("IO_stream") {
    if (m_handle) { queryInfo(); applyDefaultMods(); }
}

Texture::Texture(Texture&& o) noexcept
    : m_handle(o.m_handle), m_info(o.m_info), m_sourcePath(std::move(o.m_sourcePath)) {
    o.m_handle = nullptr;
}

Texture& Texture::operator=(Texture&& o) noexcept {
    if (this != &o) {
        freeTextureRaw(m_handle);
        m_handle = o.m_handle;
        m_info = o.m_info;
        m_sourcePath = std::move(o.m_sourcePath);
        o.m_handle = nullptr;
    }
    return *this;
}

Texture::~Texture() { freeTextureRaw(m_handle); }

void Texture::setColorMod(Uint8 r, Uint8 g, Uint8 b) {
    if (SDL_SetTextureColorMod(m_handle, r, g, b)) {
        LOG_WARNING_CAT("Image", "setColorMod failed");
    }
}

void Texture::getColorMod(Uint8& r, Uint8& g, Uint8& b) const {
    SDL_GetTextureColorMod(m_handle, &r, &g, &b);
}

void Texture::setAlphaMod(Uint8 alpha) {
    if (SDL_SetTextureAlphaMod(m_handle, alpha)) {
        LOG_WARNING_CAT("Image", "setAlphaMod failed");
    }
}

void Texture::getAlphaMod(Uint8& alpha) const {
    SDL_GetTextureAlphaMod(m_handle, &alpha);
}

void Texture::setBlendMode(SDL_BlendMode mode) {
    if (SDL_SetTextureBlendMode(m_handle, mode) == 0) {
        m_info.blendMode = mode;
    } else {
        LOG_WARNING_CAT("Image", "setBlendMode failed");
    }
}

void Texture::getBlendMode(SDL_BlendMode& mode) const {
    SDL_GetTextureBlendMode(m_handle, &mode);
}

bool Texture::saveToFile(const std::string& file, const std::string& type, SDL_Renderer* renderer) const {
    if (!m_handle || !renderer) return false;
    SDL_Surface* surf = textureToSurface(m_handle, renderer);
    if (!surf) return false;
    bool ok = saveSurface(surf, file, type);
    SDL_DestroySurface(surf);
    return ok;
}

// Cache
TextureCache::TextureCache(SDL_Renderer* renderer) : m_renderer(renderer) {}

TextureCache::~TextureCache() { clear(); }

std::shared_ptr<Texture> TextureCache::getOrLoad(const std::string& file) {
    auto it = m_cache.find(file);
    if (it != m_cache.end()) {
        LOG_DEBUG_CAT("Image", "Cache HIT: {}", file);
        return it->second;
    }

    auto tex = std::make_shared<Texture>(m_renderer, file);
    m_cache[file] = tex;
    LOG_INFO_CAT("Image", "Cache MISS → loaded: {}", file);
    return tex;
}

void TextureCache::clear() {
    m_cache.clear();
    LOG_INFO_CAT("Image", "Texture cache cleared");
}

} // namespace AmouranthRTX::Graphics