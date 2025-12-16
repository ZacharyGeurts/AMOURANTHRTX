// src/engine/stb_image_impl.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: Full printf logging for stb_image — FILE:LINE — hex dump — pure C++11 + ANSI

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
