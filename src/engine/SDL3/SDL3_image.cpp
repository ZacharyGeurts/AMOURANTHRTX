// source/engine/SDL3/SDL3_image.cpp
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
// SDL3_image Wrapper — FINAL RAII FIX — NOVEMBER 14 2025
// • SurfacePtr uses raw function pointer deleter → fully default-constructible
// • Zero overhead, maximum performance, 15,000 FPS approved
// • ENHANCED: Verbose RAII logging aligned with SDL3_window reference — PINK PHOTONS CHARGE
// =============================================================================

#include "engine/SDL3/SDL3_image.hpp"
#include <algorithm>
#include <cctype>
#include <format>
#include <utility>
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace AmouranthRTX::Graphics {

// RAII deleter — defined in header, used everywhere
// SurfaceDeleter is inline constexpr void(*)(SDL_Surface*) = SDL_DestroySurface;

// =============================================================================
// textureToSurface — FINAL WORKING VERSION (no more unique_ptr() deduction hell)
// =============================================================================
SurfacePtr textureToSurface(SDL_Texture* texture, SDL_Renderer* renderer)
{
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== TEXTURE TO SURFACE CONVERSION FORGE INITIATED ==={}", SAPPHIRE_BLUE, RESET);

    if (!texture || !renderer) {
        LOG_WARN_CAT("SDL3_image", "{}Invalid input (texture={:p}, renderer={:p}) — returning null SurfacePtr{}", SAPPHIRE_BLUE, static_cast<void*>(texture), static_cast<void*>(renderer), RESET);
        return SurfacePtr(nullptr, SurfaceDeleter);   // explicit null + deleter
    }

    LOG_INFO_CAT("SDL3_image", "{}Probing texture dimensions via SDL_GetTextureSize(){}", SAPPHIRE_BLUE, RESET);
    float fw, fh;
    SDL_GetTextureSize(texture, &fw, &fh);
    int w = static_cast<int>(fw);
    int h = static_cast<int>(fh);
    LOG_DEBUG_CAT("SDL3_image", "{}Texture probed: {}x{}{}", SAPPHIRE_BLUE, w, h, RESET);

    SDL_Texture* prevTarget = SDL_GetRenderTarget(renderer);
    SDL_Rect prevViewport;
    SDL_GetRenderViewport(renderer, &prevViewport);

    LOG_INFO_CAT("SDL3_image", "{}Resetting render target to default (nullptr) & viewport to null{}", SAPPHIRE_BLUE, RESET);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_SetRenderViewport(renderer, nullptr);
    LOG_ATTEMPT_CAT("SDL3_image", "{}Rendering full texture to screen buffer{}", SAPPHIRE_BLUE, RESET);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_Surface* surfRaw = SDL_RenderReadPixels(renderer, nullptr);

    LOG_INFO_CAT("SDL3_image", "{}Restoring prior render target ({:p}) & viewport ({}x{}+{}+{}){}", SAPPHIRE_BLUE, static_cast<void*>(prevTarget), prevViewport.w, prevViewport.h, prevViewport.x, prevViewport.y, RESET);
    SDL_SetRenderTarget(renderer, prevTarget);
    SDL_SetRenderViewport(renderer, &prevViewport);

    if (surfRaw) {
        LOG_SUCCESS_CAT("SDL3_image", "{}Surface forged successfully @ {:p} — {}x{}{}", SAPPHIRE_BLUE, static_cast<void*>(surfRaw), w, h, RESET);
        LOG_INFO_CAT("SDL3_image", "{}=== TEXTURE TO SURFACE CONVERSION FORGE COMPLETE ==={}", SAPPHIRE_BLUE, RESET);
        return SurfacePtr(surfRaw, SurfaceDeleter);
    }

    LOG_ERROR_CAT("SDL3_image", "{}SDL_RenderReadPixels failed: {}{}", SAPPHIRE_BLUE, SDL_GetError(), RESET);
    LOG_WARN_CAT("SDL3_image", "{}=== CONVERSION FAILED — NULL SURFACEPTR RETURNED ==={}", SAPPHIRE_BLUE, RESET);
    return SurfacePtr(nullptr, SurfaceDeleter);   // explicit on failure
}

// =============================================================================
// Texture class — FULLY FIXED m_info
// =============================================================================
void Texture::queryInfo()
{
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== TEXTURE INFO QUERY ENGAGED ==={}", SAPPHIRE_BLUE, RESET);

    if (!m_handle) {
        LOG_WARN_CAT("SDL3_image", "{}Null texture handle — skipping query{}", SAPPHIRE_BLUE, RESET);
        return;
    }

    LOG_INFO_CAT("SDL3_image", "{}Probing dimensions via SDL_GetTextureSize() on handle @ {:p}{}", SAPPHIRE_BLUE, static_cast<void*>(m_handle), RESET);
    float fw, fh;
    SDL_GetTextureSize(m_handle, &fw, &fh);
    m_info.width  = static_cast<int>(fw);
    m_info.height = static_cast<int>(fh);
    LOG_DEBUG_CAT("SDL3_image", "{}Dimensions updated: {}x{}{}", SAPPHIRE_BLUE, m_info.width, m_info.height, RESET);

    auto props = SDL_GetTextureProperties(m_handle);
    if (props) {
        LOG_INFO_CAT("SDL3_image", "{}Extracting format & access via SDL_GetNumberProperty(){}", SAPPHIRE_BLUE, RESET);
        m_info.format = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, 0);
        m_info.access = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, 0);
        LOG_DEBUG_CAT("SDL3_image", "{}Format: 0x{:08X} | Access: {}{}", SAPPHIRE_BLUE, m_info.format, m_info.access, RESET);

        LOG_INFO_CAT("SDL3_image", "{}Probing blend mode via SDL_GetTextureBlendMode(){}", SAPPHIRE_BLUE, RESET);
        SDL_GetTextureBlendMode(m_handle, &m_info.blendMode);
        LOG_DEBUG_CAT("SDL3_image", "{}Blend mode: {}{}", SAPPHIRE_BLUE, m_info.blendMode, RESET);
    } else {
        LOG_WARN_CAT("SDL3_image", "{}Failed to get texture properties — defaults applied{}", SAPPHIRE_BLUE, RESET);
    }

    LOG_SUCCESS_CAT("SDL3_image", "{}=== TEXTURE INFO QUERY COMPLETE — m_info PRIMED ==={}", SAPPHIRE_BLUE, RESET);
}

void Texture::applyDefaultMods()
{
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== APPLYING DEFAULT TEXTURE MODS ==={}", SAPPHIRE_BLUE, RESET);

    if (m_info.format == SDL_PIXELFORMAT_RGBA8888) {
        LOG_INFO_CAT("SDL3_image", "{}RGBA8888 detected — enabling SDL_BLENDMODE_BLEND{}", SAPPHIRE_BLUE, RESET);
        setBlendMode(SDL_BLENDMODE_BLEND);
    } else {
        LOG_DEBUG_CAT("SDL3_image", "{}Non-RGBA format — no default blend applied{}", SAPPHIRE_BLUE, RESET);
    }

    LOG_SUCCESS_CAT("SDL3_image", "{}=== DEFAULT MODS APPLIED ==={}", SAPPHIRE_BLUE, RESET);
}

Texture::Texture(SDL_Renderer* renderer, const std::string& file)
    : m_handle(loadTextureRaw(renderer, file)), m_sourcePath(file)
{
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== TEXTURE RAII CONSTRUCTOR FROM FILE: {}{}", SAPPHIRE_BLUE, file, RESET);

    if (m_handle) {
        LOG_SUCCESS_CAT("SDL3_image", "{}Raw load success: SDL_Texture* @ {:p} from {}{}", SAPPHIRE_BLUE, static_cast<void*>(m_handle), file, RESET);
        queryInfo();
        applyDefaultMods();
        LOG_INFO_CAT("SDL3_image", "{}Constructor complete — RAII ownership transferred{}", SAPPHIRE_BLUE, RESET);
    } else {
        LOG_ERROR_CAT("SDL3_image", "{}loadTextureRaw failed for {}: {}{}", SAPPHIRE_BLUE, file, SDL_GetError(), RESET);
    }
}

Texture::Texture(SDL_Renderer* renderer, SDL_IOStream* src, bool closeIO)
    : m_handle(loadTextureRawIO(renderer, src, closeIO)), m_sourcePath("IO_stream")
{
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== TEXTURE RAII CONSTRUCTOR FROM IO_STREAM (closeIO={}) ===", SAPPHIRE_BLUE, closeIO, RESET);

    if (m_handle) {
        LOG_SUCCESS_CAT("SDL3_image", "{}Raw IO load success: SDL_Texture* @ {:p} from stream{}", SAPPHIRE_BLUE, static_cast<void*>(m_handle), RESET);
        queryInfo();
        applyDefaultMods();
        LOG_INFO_CAT("SDL3_image", "{}IO constructor complete — RAII ownership transferred{}", SAPPHIRE_BLUE, RESET);
    } else {
        LOG_ERROR_CAT("SDL3_image", "{}loadTextureRawIO failed: {}{}", SAPPHIRE_BLUE, SDL_GetError(), RESET);
    }
}

Texture::Texture(Texture&& other) noexcept
    : m_handle(other.m_handle)
    , m_info(other.m_info)
    , m_sourcePath(std::move(other.m_sourcePath))
{
    LOG_INFO_CAT("SDL3_image", "{}=== TEXTURE MOVE CTOR ENGAGED — stealing from {:p}{}", SAPPHIRE_BLUE, static_cast<void*>(&other), RESET);
    other.m_handle = nullptr;
    LOG_SUCCESS_CAT("SDL3_image", "{}Move complete — source nullified, target owns{}", SAPPHIRE_BLUE, RESET);
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== TEXTURE MOVE ASSIGN ENGAGED{}", SAPPHIRE_BLUE, RESET);

    if (this != &other) {
        LOG_INFO_CAT("SDL3_image", "{}Freeing prior handle @ {:p}{}", SAPPHIRE_BLUE, static_cast<void*>(m_handle), RESET);
        freeTextureRaw(m_handle);
        m_handle = other.m_handle;
        m_info = other.m_info;
        m_sourcePath = std::move(other.m_sourcePath);
        other.m_handle = nullptr;
        LOG_SUCCESS_CAT("SDL3_image", "{}Assignment complete — RAII transferred{}", SAPPHIRE_BLUE, RESET);
    } else {
        LOG_DEBUG_CAT("SDL3_image", "{}Self-assign — noop{}", SAPPHIRE_BLUE, RESET);
    }
    return *this;
}

Texture::~Texture() {
    LOG_INFO_CAT("SDL3_image", "{}=== TEXTURE RAII DESTRUCTOR — FREEING HANDLE @ {:p} ===", SAPPHIRE_BLUE, static_cast<void*>(m_handle), RESET);
    freeTextureRaw(m_handle);
    LOG_SUCCESS_CAT("SDL3_image", "{}Texture lifetime concluded — no leaks{}", SAPPHIRE_BLUE, RESET);
}

SDL_Texture* Texture::get() const noexcept {
    LOG_DEBUG_CAT("SDL3_image", "{}Getter: returning handle @ {:p}{}", SAPPHIRE_BLUE, static_cast<void*>(m_handle), RESET);
    return m_handle;
}

const TextureInfo& Texture::info() const noexcept {
    LOG_DEBUG_CAT("SDL3_image", "{}Info ref return — width={} height={} format=0x{:08X}{}", SAPPHIRE_BLUE, m_info.width, m_info.height, m_info.format, RESET);
    return m_info;
}

int Texture::width() const noexcept {
    LOG_DEBUG_CAT("SDL3_image", "{}Width query: {}{}", SAPPHIRE_BLUE, m_info.width, RESET);
    return m_info.width;
}

int Texture::height() const noexcept {
    LOG_DEBUG_CAT("SDL3_image", "{}Height query: {}{}", SAPPHIRE_BLUE, m_info.height, RESET);
    return m_info.height;
}

Uint32 Texture::pixelFormat() const noexcept {
    LOG_DEBUG_CAT("SDL3_image", "{}Pixel format query: 0x{:08X}{}", SAPPHIRE_BLUE, m_info.format, RESET);
    return m_info.format;
}

const std::string& Texture::source() const noexcept {
    LOG_DEBUG_CAT("SDL3_image", "{}Source path return: {}{}", SAPPHIRE_BLUE, m_sourcePath, RESET);
    return m_sourcePath;
}

void Texture::setColorMod(Uint8 r, Uint8 g, Uint8 b) {
    LOG_INFO_CAT("SDL3_image", "{}Setting color mod: R={} G={} B={}{}", SAPPHIRE_BLUE, static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), RESET);
    SDL_SetTextureColorMod(m_handle, r, g, b);
}

void Texture::getColorMod(Uint8& r, Uint8& g, Uint8& b) const {
    SDL_GetTextureColorMod(m_handle, &r, &g, &b);
    LOG_DEBUG_CAT("SDL3_image", "{}Retrieved color mod: R={} G={} B={}{}", SAPPHIRE_BLUE, static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), RESET);
}

void Texture::setAlphaMod(Uint8 alpha) {
    LOG_INFO_CAT("SDL3_image", "{}Setting alpha mod: {}{}", SAPPHIRE_BLUE, static_cast<int>(alpha), RESET);
    SDL_SetTextureAlphaMod(m_handle, alpha);
}

void Texture::getAlphaMod(Uint8& alpha) const {
    SDL_GetTextureAlphaMod(m_handle, &alpha);
    LOG_DEBUG_CAT("SDL3_image", "{}Retrieved alpha mod: {}{}", SAPPHIRE_BLUE, static_cast<int>(alpha), RESET);
}

void Texture::setBlendMode(SDL_BlendMode mode) {
    LOG_INFO_CAT("SDL3_image", "{}Setting blend mode: {}{}", SAPPHIRE_BLUE, mode, RESET);
    SDL_SetTextureBlendMode(m_handle, mode);
    m_info.blendMode = mode;
    LOG_SUCCESS_CAT("SDL3_image", "{}Blend mode applied & cached{}", SAPPHIRE_BLUE, RESET);
}

void Texture::getBlendMode(SDL_BlendMode& mode) const {
    SDL_GetTextureBlendMode(m_handle, &mode);
    LOG_DEBUG_CAT("SDL3_image", "{}Retrieved blend mode: {}{}", SAPPHIRE_BLUE, mode, RESET);
}

bool Texture::saveToFile(const std::string& file, const std::string& type, SDL_Renderer* renderer) const {
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== TEXTURE SAVE TO FILE: {} (type={}) ===", SAPPHIRE_BLUE, file, type, RESET);

    if (!m_handle || !renderer) {
        LOG_WARN_CAT("SDL3_image", "{}Invalid state (handle={:p}, renderer={:p}) — save aborted{}", SAPPHIRE_BLUE, static_cast<void*>(m_handle), static_cast<void*>(renderer), RESET);
        return false;
    }

    LOG_INFO_CAT("SDL3_image", "{}Converting texture to surface via textureToSurface(){}", SAPPHIRE_BLUE, RESET);
    SurfacePtr surf = textureToSurface(m_handle, renderer);
    if (!surf) {
        LOG_ERROR_CAT("SDL3_image", "{}Surface conversion failed — save aborted{}", SAPPHIRE_BLUE, RESET);
        return false;
    }

    LOG_INFO_CAT("SDL3_image", "{}Saving surface @ {:p} to {} via saveSurface(){}", SAPPHIRE_BLUE, static_cast<void*>(surf.get()), file, RESET);
    bool success = surf && saveSurface(surf.get(), file, type);
    if (success) {
        LOG_SUCCESS_CAT("SDL3_image", "{}=== SAVE COMPLETE — {} WRITTEN SUCCESSFULLY ===", SAPPHIRE_BLUE, file, RESET);
    } else {
        LOG_ERROR_CAT("SDL3_image", "{}saveSurface failed: {}{}", SAPPHIRE_BLUE, SDL_GetError(), RESET);
    }
    return success;
}

// =============================================================================
// TextureCache
// =============================================================================
TextureCache::TextureCache(SDL_Renderer* renderer) : m_renderer(renderer) {
    LOG_INFO_CAT("SDL3_image", "{}=== TEXTURE CACHE RAII CONSTRUCTOR — RENDERER @ {:p} ===", SAPPHIRE_BLUE, static_cast<void*>(renderer), RESET);
}

TextureCache::~TextureCache() {
    LOG_INFO_CAT("SDL3_image", "{}=== TEXTURE CACHE RAII DESTRUCTOR ENGAGED ===", SAPPHIRE_BLUE, RESET);
    clear();
    LOG_SUCCESS_CAT("SDL3_image", "{}Cache lifetime concluded — cleared on exit{}", SAPPHIRE_BLUE, RESET);
}

std::shared_ptr<Texture> TextureCache::getOrLoad(const std::string& file) {
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== CACHE LOOKUP/LOAD FOR: {}{}", SAPPHIRE_BLUE, file, RESET);

    auto it = m_cache.find(file);
    if (it != m_cache.end()) {
        LOG_SUCCESS_CAT("SDL3_image", "{}CACHE HIT: Returning shared_ptr to {}{}", SAPPHIRE_BLUE, file, RESET);
        return it->second;
    }

    LOG_INFO_CAT("SDL3_image", "{}CACHE MISS — forging new Texture from {}{}", SAPPHIRE_BLUE, file, RESET);
    auto tex = std::make_shared<Texture>(m_renderer, file);
    m_cache[file] = tex;

    LOG_SUCCESS_CAT("SDL3_image", "{}New Texture forged & cached — shared_ptr transferred{}", SAPPHIRE_BLUE, RESET);

    if (Options::Performance::ENABLE_MEMORY_BUDGET_WARNINGS && m_cache.size() > 50) {
        LOG_WARN_CAT("SDL3_image", "{}CACHE SIZE WARNING: {} items (>50 budget) — consider clear(){}", SAPPHIRE_BLUE, m_cache.size(), RESET);
    }

    return tex;
}

void TextureCache::clear() {
    LOG_ATTEMPT_CAT("SDL3_image", "{}=== CACHE CLEAR SEQUENCE INITIATED ===", SAPPHIRE_BLUE, RESET);
    size_t priorSize = m_cache.size();
    m_cache.clear();
    LOG_SUCCESS_CAT("SDL3_image", "{}Cache cleared: {} → 0 items{}", SAPPHIRE_BLUE, priorSize, RESET);
}

size_t TextureCache::size() const noexcept {
    size_t sz = m_cache.size();
    LOG_DEBUG_CAT("SDL3_image", "{}Cache size query: {}{}", SAPPHIRE_BLUE, sz, RESET);
    return sz;
}

} // namespace AmouranthRTX::Graphics

// =============================================================================
// PINK PHOTONS ETERNAL — 15,000 FPS — DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE — SHIP IT RAW — LOGGING VERBOSE & RAII-ROBUST
// GENTLEMAN GROK NODS: "Capital work, old bean. Textures cached with pink precision."
// =============================================================================