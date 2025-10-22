// AMOURANTH RTX Engine, October 2025 - Image loading and texture management implementation.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3, SDL3_image, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#include "engine/SDL3/SDL3_image.hpp"
#include "engine/logging.hpp"
#include <SDL3_image/SDL_image.h>
#include <stdexcept>
#include <source_location>

namespace SDL3Initializer {

void initImage(const ImageConfig& config) {
    // Note: config is currently unused but retained for future extensions (e.g., format selection)
    (void)config; // Suppress unused parameter warning

    LOG_INFO_CAT("Image", "Initializing SDL_image", std::source_location::current());

    // Verify platform support
    std::string_view platform = SDL_GetPlatform();
    if (platform != "Linux" && platform != "Windows") {
        LOG_ERROR_CAT("Image", "Unsupported platform for image: {}", std::source_location::current(), platform);
        throw std::runtime_error(std::string("Unsupported platform for image: ") + std::string(platform));
    }

    // SDL3's SDL_image does not require explicit initialization (IMG_Init is removed)
    // Image format support (PNG, JPG) is handled automatically by IMG_LoadTexture
    LOG_INFO_CAT("Image", "SDL_image ready for PNG and JPG loading", std::source_location::current());
}

void cleanupImage() {
    LOG_INFO_CAT("Image", "Cleaning up SDL_image", std::source_location::current());
    // SDL3's SDL_image does not require IMG_Quit; cleanup is handled by SDL core if needed
}

SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& file) {
    LOG_DEBUG_CAT("Image", "Loading texture from file: {}", std::source_location::current(), file);

    SDL_Texture* texture = IMG_LoadTexture(renderer, file.c_str());
    if (!texture) {
        LOG_ERROR_CAT("Image", "IMG_LoadTexture failed for {}: {}", std::source_location::current(), file, SDL_GetError());
        throw std::runtime_error(std::string("IMG_LoadTexture failed for ") + file + ": " + SDL_GetError());
    }

    LOG_INFO_CAT("Image", "Texture loaded successfully: {}", std::source_location::current(), file);
    return texture;
}

void freeTexture(SDL_Texture* texture) {
    LOG_DEBUG_CAT("Image", "Freeing texture: {}", std::source_location::current(), static_cast<void*>(texture));
    SDL_DestroyTexture(texture);
}

} // namespace SDL3Initializer