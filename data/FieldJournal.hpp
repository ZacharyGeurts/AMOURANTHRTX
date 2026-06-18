#pragma once

// Shared journal append for RTX-DOS configuration files on C:.

#include "FieldAmmoVfs.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace FieldJournal {

inline void append(const char* path, const char* action, const char* detail) noexcept {
    if (!path) return;
    char line[320];
    std::snprintf(line, sizeof line, "%s | %s\r\n", action ? action : "?", detail ? detail : "");
    std::vector<std::uint8_t> cur;
    FieldAmmoVfs::readPath(path, cur);
    cur.insert(cur.end(), line, line + std::strlen(line));
    FieldAmmoVfs::writePath(path, cur.data(), cur.size());
}

} // namespace FieldJournal