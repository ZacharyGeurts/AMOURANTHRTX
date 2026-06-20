#pragma once

// Shared journal append for RTX-DOS configuration files on C:.

#include "FieldAmmoVfs.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace FieldJournal {

inline bool writePath(const char* path, const std::vector<std::uint8_t>& data) noexcept {
    if (!path) return false;
    return FieldAmmoVfs::writePath(path, data.data(), data.size());
}

inline void append(const char* path, const char* action, const char* detail) noexcept {
    if (!path) return;
    char line[320];
    std::snprintf(line, sizeof line, "%s | %s\r\n", action ? action : "?", detail ? detail : "");
    std::vector<std::uint8_t> cur;
    FieldAmmoVfs::readPath(path, cur);
    cur.insert(cur.end(), line, line + std::strlen(line));
    writePath(path, cur);
}

inline std::size_t pathSize(const char* path) noexcept {
    std::vector<std::uint8_t> cur;
    if (!FieldAmmoVfs::readPath(path, cur)) return 0u;
    return cur.size();
}

} // namespace FieldJournal