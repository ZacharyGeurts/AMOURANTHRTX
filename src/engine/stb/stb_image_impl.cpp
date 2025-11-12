// src/engine/stb_image_impl.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: Full logging for stb_image, FILE:LINE, hex dump, C++11

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "engine/GLOBAL/logging.hpp"
#include <cstdio>
#include <vector>
#include <iomanip>
#include <sstream>

using namespace Logging::Color;

// ---------------------------------------------------------------------------
//  Helper: hex dump first N bytes
// ---------------------------------------------------------------------------
static std::string hexDump(const unsigned char* data, int size, int maxBytes = 16)
{
    if (!data || size <= 0) return "<empty>";
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < std::min(size, maxBytes); ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]) << ' ';
    }
    if (size > maxBytes) oss << "...";
    return oss.str();
}

// ---------------------------------------------------------------------------
//  Custom stb_image load wrapper with logging
// ---------------------------------------------------------------------------
extern "C" {

unsigned char* stbi_load_logged(const char* filename, int* x, int* y, int* channels_in_file, int desired_channels)
{
    LOG_INFO_CAT("stb_image", "{}Loading image: {}{}", ARCTIC_CYAN, filename, RESET);

    unsigned char* data = stbi_load(filename, x, y, channels_in_file, desired_channels);

    if (!data) {
        const char* fail = stbi_failure_reason();
        LOG_ERROR_CAT("stb_image", "{}[{}:{}] FAILED to load image: {} | Reason: {}{}",
                      CRIMSON_MAGENTA, __FILE__, __LINE__, filename, fail ? fail : "unknown", RESET);
        return nullptr;
    }

    int channels = desired_channels ? desired_channels : *channels_in_file;
    LOG_INFO_CAT("stb_image", "{}Image loaded: {}x{} | {} channel(s) | {} bytes | First pixels: {}{}",
                 EMERALD_GREEN, *x, *y, channels, (*x) * (*y) * channels,
                 hexDump(data, (*x) * (*y) * channels), RESET);

    return data;
}

void stbi_image_free_logged(void* retval_from_stbi_load)
{
    if (retval_from_stbi_load) {
        LOG_INFO_CAT("stb_image", "{}Freeing image data @ 0x{:x}{}", OCEAN_TEAL,
                     reinterpret_cast<uintptr_t>(retval_from_stbi_load), RESET);
    }
    stbi_image_free(retval_from_stbi_load);
}

} // extern "C"

// ---------------------------------------------------------------------------
//  Override default stb_image functions
// ---------------------------------------------------------------------------
#undef stbi_load
#undef stbi_image_free

#define stbi_load       stbi_load_logged
#define stbi_image_free stbi_image_free_logged