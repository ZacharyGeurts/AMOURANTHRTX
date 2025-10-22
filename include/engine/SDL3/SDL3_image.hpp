// AMOURANTH RTX Engine, October 2025 - SDL3_image initialization and texture management.
// Dependencies: SDL3, SDL3_image, Vulkan 1.3+, C++20 standard library.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#pragma once
#include <SDL3/SDL.h>
#include <string>

extern "C" {
#include <SDL3_image/SDL_image.h>
}

namespace SDL3Initializer {

struct ImageConfig {
    // No specific config needed for SDL3_image as initialization is automatic
};

void initImage(const ImageConfig& config);
void cleanupImage();
SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& file);
void freeTexture(SDL_Texture* texture);

} // namespace SDL3Initializer