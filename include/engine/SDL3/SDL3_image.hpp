// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// SDL3_image wrapper — FULL C++23 — NOVEMBER 08 2025
// NO source_location in logging → compiles with -std=c++23

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL.h>
#include <stdexcept>
#include <source_location>
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

// Forward declarations
class Texture;
class TextureCache;

// Free functions
void initImage(const ImageConfig& config = {});
void cleanupImage();

bool isSupportedImage(const std::string& filePath);
bool detectFormat(SDL_IOStream* src, std::string& format);

SDL_Surface* loadSurface(const std::string& file);
SDL_Surface* loadSurfaceIO(SDL_IOStream* src, bool closeIO = true);
bool saveSurface(const SDL_Surface* surface, const std::string& file, const std::string& type = "png");
bool saveSurfaceIO(const SDL_Surface* surface, SDL_IOStream* dst, bool closeIO, const std::string& type = "png");

SDL_Texture* loadTextureRaw(SDL_Renderer* renderer, const std::string& file);
SDL_Texture* loadTextureRawIO(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO = true);
void freeTextureRaw(SDL_Texture* texture);

SDL_Surface* textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer);

// RAII Texture
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

    SDL_Texture* get() const noexcept { return m_handle; }
    const TextureInfo& info() const noexcept { return m_info; }
    int width() const noexcept { return m_info.width; }
    int height() const noexcept { return m_info.height; }
    Uint32 pixelFormat() const noexcept { return m_info.format; }
    const std::string& source() const noexcept { return m_sourcePath; }

    void setColorMod(Uint8 r, Uint8 g, Uint8 b);
    void getColorMod(Uint8& r, Uint8& g, Uint8& b) const;
    void setAlphaMod(Uint8 alpha);
    void getAlphaMod(Uint8& alpha) const;
    void setBlendMode(SDL_BlendMode mode);
    void getBlendMode(SDL_BlendMode& mode) const;

    bool saveToFile(const std::string& file, const std::string& type = "png", SDL_Renderer* renderer = nullptr) const;
};

// Simple cache
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

} // namespace AmouranthRTX::Graphics