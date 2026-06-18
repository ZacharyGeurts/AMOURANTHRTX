#pragma once

// RTX-AMMOS field layer shell state — IDs match FieldLayer::LayerId / driver table.

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace FieldLayer {

enum class LayerId : std::uint8_t {
    Ram = 0,
    Vga,
    Fat,
    Drives,
    Viewport,
    Audio,
    Mscdex,
    Input,
    Io,
    Bios,
    Count
};

struct ShellEntry {
    LayerId     id;
    const char* name;
};

inline constexpr ShellEntry kShellRegistry[] = {
    {LayerId::Ram,      "ram"},
    {LayerId::Vga,      "vga"},
    {LayerId::Fat,      "fat"},
    {LayerId::Drives,   "drives"},
    {LayerId::Viewport, "viewport"},
    {LayerId::Audio,    "audio"},
    {LayerId::Mscdex,   "mscdex"},
    {LayerId::Input,    "input"},
    {LayerId::Io,       "io"},
    {LayerId::Bios,     "bios"},
};

inline bool shellEnabled[static_cast<std::size_t>(LayerId::Count)] = {
    true, true, true, true, true, true, true, true, true, true,
};

inline bool istreq_alias(const char* a, const char* b) noexcept {
    while (*a && *b) {
        if (std::tolower(static_cast<unsigned char>(*a))
            != std::tolower(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline const ShellEntry* findShellByName(const char* name) noexcept {
    if (!name || !name[0]) return nullptr;
    for (const auto& entry : kShellRegistry) {
        if (istreq_alias(name, entry.name)) return &entry;
        if (entry.id == LayerId::Fat && istreq_alias(name, "AMMOFAT")) return &entry;
        if (entry.id == LayerId::Drives && istreq_alias(name, "DRIVES")) return &entry;
        if (entry.id == LayerId::Mscdex && istreq_alias(name, "MSCDEX")) return &entry;
        if (entry.id == LayerId::Viewport && istreq_alias(name, "VIEWPORT")) return &entry;
        if (entry.id == LayerId::Bios && istreq_alias(name, "DOS")) return &entry;
    }
    return nullptr;
}

inline bool isShellActive(LayerId id) noexcept {
    const auto i = static_cast<std::size_t>(id);
    return i < static_cast<std::size_t>(LayerId::Count) && shellEnabled[i];
}

inline bool setShellActive(LayerId id, bool on) noexcept {
    const auto i = static_cast<std::size_t>(id);
    if (i >= static_cast<std::size_t>(LayerId::Count)) return false;
    shellEnabled[i] = on;
    return on;
}

inline bool toggleShell(LayerId id) noexcept {
    return setShellActive(id, !isShellActive(id));
}

inline int shellActiveCount() noexcept {
    int n = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(LayerId::Count); ++i)
        if (shellEnabled[i]) ++n;
    return n;
}

inline void formatShellSummary(char* buf, std::size_t len) noexcept {
    std::snprintf(buf, len, "%d/%u field layers active",
        shellActiveCount(), static_cast<unsigned>(LayerId::Count));
}

} // namespace FieldLayer