#pragma once

// AmouranthOS asset paths — icon atlas + photo wallpapers for GPU sampling.

#include <cstdint>

namespace FieldAmouranthTextures {

inline constexpr const char* kIconAtlasPath     = "assets/data/icons/aos_icon_atlas.png";
inline constexpr const char* kStartIconPath     = "assets/data/icons/amouranthos.png";
inline constexpr const char* kStartPortraitPath = "assets/data/icons/amouranth_twitch_logo.png";
inline constexpr const char* kTwitchLogoPath    = "assets/data/icons/amouranth_twitch_logo.png";

// 64×64 cells in 1024×128 atlas (must match scripts/gen_amouranthos_icons.py)
enum class IconSlot : std::uint32_t {
    Start     = 0,
    Close     = 1,
    Minimize  = 2,
    Maximize  = 3,
    Shell     = 4,
    AmmoCode  = 5,
    FileCmd   = 6,
    Vscodium  = 7,
    QBasic    = 8,
    FieldC    = 9,
    PadTest   = 10,
    Nes       = 11,
    Settings  = 12,
    Power     = 13,
    Doom      = 14,
    Wallpaper = 15,
    Desktop   = 16,
    Display   = 17,
    AmmoText  = 18,
    Monitor   = 19,
    BrowserFirefox = 20,
    BrowserChrome  = 21,
    BrowserEdge    = 22,
};

inline constexpr std::uint32_t kMenuIconSlots[] = {
    static_cast<std::uint32_t>(IconSlot::Shell),
    static_cast<std::uint32_t>(IconSlot::AmmoCode),
    static_cast<std::uint32_t>(IconSlot::FileCmd),
    static_cast<std::uint32_t>(IconSlot::Vscodium),
    static_cast<std::uint32_t>(IconSlot::QBasic),
    static_cast<std::uint32_t>(IconSlot::FieldC),
    static_cast<std::uint32_t>(IconSlot::PadTest),
    static_cast<std::uint32_t>(IconSlot::Nes),
    static_cast<std::uint32_t>(IconSlot::Doom),
    static_cast<std::uint32_t>(IconSlot::Wallpaper),
    static_cast<std::uint32_t>(IconSlot::Settings),
};

inline constexpr const char* kWallpaperPaths[] = {
    "assets/wallpapers/amouranth_dreamhack_2024.jpg",
    "assets/wallpapers/amouranth_2022.jpg",
    "assets/wallpapers/amouranth_2021.png",
    "assets/wallpapers/amouranth_creator_clash_2023.png",
    "assets/wallpapers/amouranth_in_2022_01.png",
    "assets/wallpapers/amouranth_in_2022_09_cropped.png",
    "assets/wallpapers/amouranth_twitchcon_2022_01.png",
    "assets/wallpapers/amouranth_twitchcon_2022_02.png",
};

inline constexpr std::uint32_t kWallpaperCount =
    static_cast<std::uint32_t>(sizeof(kWallpaperPaths) / sizeof(kWallpaperPaths[0]));

// data_bus[42] bits 16-19: 0-7 = photo index, 15 = procedural American flag (default)
inline constexpr std::uint32_t kWallpaperAmericanFlag = 15u;

inline bool isPhotoWallpaper(std::uint32_t index) noexcept {
    return index < kWallpaperCount;
}

inline const char* wallpaperPath(std::uint32_t index) noexcept {
    if (!isPhotoWallpaper(index)) return nullptr;
    return kWallpaperPaths[index];
}

} // namespace FieldAmouranthTextures