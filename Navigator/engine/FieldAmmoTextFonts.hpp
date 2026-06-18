#pragma once

// AmmoText font presets — TTF families mapped to GPU scale + spacing.

#include <cstdint>
#include <cstring>

namespace FieldAmmoTextFonts {

struct Preset {
    const char* name;
    const char* ttfHint;
    float       scale;
    float       spacing;
};

inline int activeIndex = 0;

inline const Preset kPresets[] = {
    { "JetBrains Mono",  "JetBrainsMono-Regular.ttf", 1.25f, 1.00f },
    { "Consolas",        "Consolas",                  1.22f, 1.00f },
    { "Courier New",     "Courier",                   1.18f, 1.02f },
    { "Lucida Console",  "Lucida Console",            1.20f, 0.98f },
    { "Segoe UI",        "Segoe UI",                  1.30f, 1.05f },
    { "Arial",           "Arial",                     1.28f, 1.06f },
    { "Times New Roman", "Times New Roman",           1.26f, 1.08f },
    { "Georgia",         "Georgia",                   1.24f, 1.08f },
    { "Calibri",         "Calibri",                   1.32f, 1.04f },
    { "Cambria",         "Cambria",                   1.27f, 1.06f },
    { "Brass Mono",      "BrassMonoRegular",          1.20f, 1.00f },
    { "SF Plasmatica",   "sf-plasmatica-open.ttf",    1.15f, 0.96f },
    { "Small",           "JetBrainsMono-Regular.ttf", 1.00f, 1.00f },
    { "Large",           "JetBrainsMono-Regular.ttf", 1.55f, 1.00f },
    { "Extra Large",     "JetBrainsMono-Regular.ttf", 1.85f, 1.00f },
};

constexpr int kCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

inline void applyIndex(int idx) noexcept {
    if (idx < 0 || idx >= kCount) idx = 0;
    activeIndex = idx;
}

inline const char* activeName() noexcept {
    return kPresets[static_cast<std::size_t>(activeIndex)].name;
}

inline float activeScale() noexcept {
    return kPresets[static_cast<std::size_t>(activeIndex)].scale;
}

inline void next() noexcept { applyIndex((activeIndex + 1) % kCount); }
inline void prev() noexcept { applyIndex((activeIndex + kCount - 1) % kCount); }

} // namespace FieldAmmoTextFonts