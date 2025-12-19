// src/engine/stb_image_impl.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: Full printf logging for stb_image — FILE:LINE — hex dump — pure C++11 + ANSI

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <cstdio>
#include <vector>
#include <iomanip>
#include <sstream>

// ANSI color codes — SHE DEMANDS VISUAL GLORY
#define RESET           "\033[0m"
#define ARCTIC_CYAN     "\033[96m"
#define EMERALD_GREEN   "\033[92m"
#define CRIMSON_MAGENTA "\033[95m"
#define OCEAN_TEAL      "\033[38;5;45m"

// ---------------------------------------------------------------------------
//  Helper: hex dump first N bytes (max 16 shown)
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
//  Custom stb_image load wrapper with printf logging
// ---------------------------------------------------------------------------
extern "C" {

unsigned char* stbi_load_logged(const char* filename, int* x, int* y, int* channels_in_file, int desired_channels)
{
    printf(ARCTIC_CYAN "[stb_image] Loading image: %s" RESET "\n", filename);

    unsigned char* data = stbi_load(filename, x, y, channels_in_file, desired_channels);

    if (!data) {
        const char* fail = stbi_failure_reason();
        printf(CRIMSON_MAGENTA "[stb_image] [%s:%d] FAILED to load image: %s | Reason: %s" RESET "\n",
               __FILE__, __LINE__, filename, fail ? fail : "unknown");
        return nullptr;
    }

    int channels = desired_channels ? desired_channels : *channels_in_file;
    long long totalBytes = (long long)(*x) * (*y) * channels;

    printf(EMERALD_GREEN "[stb_image] Image loaded: %dx%d | %d channel(s) | %lld bytes | First pixels: %s" RESET "\n",
           *x, *y, channels, totalBytes, hexDump(data, (int)totalBytes).c_str());

    return data;
}

void stbi_image_free_logged(void* retval_from_stbi_load)
{
    if (retval_from_stbi_load) {
        printf(OCEAN_TEAL "[stb_image] Freeing image data @ 0x%llx" RESET "\n",
               (unsigned long long)reinterpret_cast<uintptr_t>(retval_from_stbi_load));
    }
    stbi_image_free(retval_from_stbi_load);
}

} // extern "C"

// ---------------------------------------------------------------------------
//  Override default stb_image functions to use our logged versions
// ---------------------------------------------------------------------------
#undef stbi_load
#undef stbi_image_free

#define stbi_load       stbi_load_logged
#define stbi_image_free stbi_image_free_logged