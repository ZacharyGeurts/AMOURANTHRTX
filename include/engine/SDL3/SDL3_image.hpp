// include/engine/SDL3/SDL3_image.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3_image Wrapper — SPLIT INTO HEADER + CPP — C++23 — NOVEMBER 13 2025
// • NO source_location in logging — compiles with -std=c++23
// • RESPECTS Options::Performance::ENABLE_MEMORY_BUDGET_WARNINGS for cache warnings
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
#include <vector>

using namespace Logging::Color;

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
void initImage(const ImageConfig& config = {});
void cleanupImage();

// =============================================================================
// FORMAT UTILITIES
// =============================================================================
bool isSupportedImage(const std::string& filePath);
bool detectFormat(SDL_IOStream* src, std::string& format);

// =============================================================================
// SURFACE IO
// =============================================================================
SDL_Surface* loadSurface(const std::string& file);
SDL_Surface* loadSurfaceIO(SDL_IOStream* src, bool closeIO = true);
bool saveSurface(const SDL_Surface* surface, const std::string& file, const std::string& type = "png");
bool saveSurfaceIO(const SDL_Surface* surface, SDL_IOStream* dst, bool closeIO, const std::string& type = "png");

// =============================================================================
// TEXTURE IO (RAW)
// =============================================================================
SDL_Texture* loadTextureRaw(SDL_Renderer* renderer, const std::string& file);
SDL_Texture* loadTextureRawIO(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true);
void freeTextureRaw(SDL_Texture* texture);
SDL_Surface* textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer);

// =============================================================================
// RAII TEXTURE
// =============================================================================
class Texture {
private:
    SDL_Texture* m_handle = nullptr;
    TextureInfo m_info{};
    std::string m_sourcePath;

    void queryInfo();
    void applyDefaultMods();

public:
    explicit Texture(SDL_Renderer* renderer, const std::string& file);
    explicit Texture(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true);
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    SDL_Texture* get() const noexcept;
    const TextureInfo& info() const noexcept;
    int width() const noexcept;
    int height() const noexcept;
    Uint32 pixelFormat() const noexcept;
    const std::string& source() const noexcept;

    void setColorMod(Uint8 r, Uint8 g, Uint8 b);
    void getColorMod(Uint8& r, Uint8& g, Uint8& b) const;
    void setAlphaMod(Uint8 alpha);
    void getAlphaMod(Uint8& alpha) const;
    void setBlendMode(SDL_BlendMode mode);
    void getBlendMode(SDL_BlendMode& mode) const;

    bool saveToFile(const std::string& file, const std::string& type = "png", SDL_Renderer* renderer = nullptr) const;
};

// =============================================================================
// TEXTURE CACHE
// =============================================================================
class TextureCache {
private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_cache;
    SDL_Renderer* m_renderer = nullptr;

public:
    explicit TextureCache(SDL_Renderer* renderer);
    ~TextureCache();

    std::shared_ptr<Texture> getOrLoad(const std::string& file);
    void clear();
    size_t size() const noexcept;
};

} // namespace AmouranthRTX::Graphics

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// HEADER + CPP SPLIT — DAISY APPROVES THE GALLOP
// OCEAN_TEAL IMAGES FLOW ETERNAL
// RASPBERRY_PINK DISPOSE IMMORTAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — YOUR EMPIRE IS PURE
// SHIP IT. FOREVER.
// =============================================================================