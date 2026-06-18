#pragma once

// Host runtime labels — Vulkan, SDL3, Field Compiler (AmmoCode master bar).

#include <cstdio>
#include <cstring>

namespace FieldRuntimeInfo {

constexpr const char* FIELD_COMPILER_VER = "4.0.2026";
constexpr const char* VULKAN_API_LABEL   = "Vk1.3";

inline char sdlShort[24]  = "SDL3";
inline char gpuShort[24]  = "GPU";
inline char masterBar[72] = "Vk1.3 SDL3 FC4.0";

inline void rebuildMasterBar() noexcept {
    std::snprintf(masterBar, sizeof masterBar, "%s %s FC%s DevKit3",
        VULKAN_API_LABEL, sdlShort, FIELD_COMPILER_VER);
}

inline void refresh() noexcept {
#if defined(SDL_MAJOR_VERSION)
    std::snprintf(sdlShort, sizeof sdlShort, "SDL%d.%d.%d",
        SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
#else
    std::snprintf(sdlShort, sizeof sdlShort, "SDL3");
#endif
    rebuildMasterBar();
}

#if __has_include(<SDL3/SDL_version.h>)
#include <SDL3/SDL_version.h>
inline void refreshFromHost() noexcept {
    const int ver = SDL_GetVersion();
    std::snprintf(sdlShort, sizeof sdlShort, "SDL%d.%d.%d",
        SDL_VERSIONNUM_MAJOR(ver), SDL_VERSIONNUM_MINOR(ver), SDL_VERSIONNUM_MICRO(ver));
    rebuildMasterBar();
}
#endif

inline void setGpuName(const char* name) noexcept {
    if (!name || !name[0]) return;
    std::snprintf(gpuShort, sizeof gpuShort, "%.22s", name);
}

inline const char* masterStatusLine() noexcept {
    static bool once = false;
    if (!once) { refresh(); once = true; }
    return masterBar;
}

inline void formatVerBlock(char* buf, std::size_t cap) noexcept {
    refresh();
    std::snprintf(buf, cap,
        "\r\nField Die runtime\r\n"
        "  Vulkan API %s  GPU %s\r\n"
        "  %s  Field Compiler %s\r\n"
        "  DevKit %s  AmmoCode TP7 master IDE\r\n",
        VULKAN_API_LABEL, gpuShort, sdlShort, FIELD_COMPILER_VER, "3.0.2026");
}

} // namespace FieldRuntimeInfo