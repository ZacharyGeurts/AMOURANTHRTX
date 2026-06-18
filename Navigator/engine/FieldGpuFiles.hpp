#pragma once

// Host-staged DOS files in guest RAM — GPU int21 AH=3D/3F/3E/42 reads this table.

#include "FieldDos.hpp"
#include "FieldPlatform.hpp"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldGpuFiles {

constexpr std::uint32_t TABLE_MAGIC   = 0x464C4141u; // 'AAFL'
constexpr std::uint32_t TABLE_BASE    = 0x03F80000u;
constexpr std::uint32_t DATA_BASE   = FieldPlatform::GPU_DATA_BYTE;
constexpr std::uint32_t TRAP_OPCODE = 0x03F7FFF8u;
constexpr std::uint32_t MAX_SLOTS   = 32u;
constexpr std::uint32_t SLOT_BYTES  = 64u;
constexpr std::uint32_t NAME_OFF    = 12u;
constexpr std::uint32_t NAME_LEN    = 12u;

inline void wr32(std::uint8_t* ram, std::uint32_t addr, std::uint32_t v) noexcept {
    ram[addr]     = static_cast<std::uint8_t>(v);
    ram[addr + 1] = static_cast<std::uint8_t>(v >> 8);
    ram[addr + 2] = static_cast<std::uint8_t>(v >> 16);
    ram[addr + 3] = static_cast<std::uint8_t>(v >> 24);
}

inline void wrName(std::uint8_t* ram, std::uint32_t addr, const char* path) noexcept {
    char stem[9]{}, ext[4]{};
    char drive = 0;
    if (path)
        FieldDos::parseDosName(path, drive, stem, ext);
    for (int i = 0; i < 8; ++i)
        ram[addr + static_cast<std::uint32_t>(i)] =
            static_cast<std::uint8_t>(stem[i] ? static_cast<unsigned char>(stem[i]) : ' ');
    ram[addr + 8u] = '.';
    for (int i = 0; i < 3; ++i)
        ram[addr + 9u + static_cast<std::uint32_t>(i)] =
            static_cast<std::uint8_t>(ext[i] ? static_cast<unsigned char>(ext[i]) : ' ');
}

inline bool addSlot(std::uint8_t* ram, std::uint32_t& dataOff, std::uint32_t slot,
                      const char* dosPath, std::vector<std::uint8_t>& fileData) noexcept {
    if (!ram || !dosPath || slot >= MAX_SLOTS) return false;
    if (!FieldDos::readHostFile(dosPath, fileData) || fileData.empty()) return false;
    if (dataOff + fileData.size() > TABLE_BASE) return false;

    for (std::size_t i = 0; i < fileData.size(); ++i)
        ram[dataOff + static_cast<std::uint32_t>(i)] = fileData[i];

    const std::uint32_t ent = TABLE_BASE + 8u + slot * SLOT_BYTES;
    wr32(ram, ent + 0u, dataOff);
    wr32(ram, ent + 4u, static_cast<std::uint32_t>(fileData.size()));
    wr32(ram, ent + 8u, 0u);
    wrName(ram, ent + NAME_OFF, dosPath);
    dataOff = (dataOff + static_cast<std::uint32_t>(fileData.size()) + 15u) & ~15u;
    return true;
}

inline void clear(std::uint8_t* ram) noexcept {
    if (!ram) return;
    for (std::uint32_t i = 0; i < 8u + MAX_SLOTS * SLOT_BYTES; ++i)
        ram[TABLE_BASE + i] = 0;
    wr32(ram, TRAP_OPCODE, 0u);
}

inline std::uint32_t stageForLaunch(std::uint8_t* ram, const char* dosPath) noexcept {
    if (!ram) return 0;
    clear(ram);
    wr32(ram, TABLE_BASE, TABLE_MAGIC);

    std::uint32_t dataOff = DATA_BASE;
    std::uint32_t count = 0;
    std::vector<std::uint8_t> buf;

    if (dosPath && addSlot(ram, dataOff, count, dosPath, buf))
        ++count;

    char upper[128]{};
    if (dosPath) {
        for (int i = 0; dosPath[i] && i < 127; ++i)
            upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(dosPath[i])));
    }
    if (std::strstr(upper, "DOOM")) {
        static const char* kWads[] = {
            "C:\\GAMES\\DOOM\\DOOM1.WAD", "C:\\DOOM1.WAD",
            "C:\\GAMES\\DOOM\\DOOM.WAD", "C:\\DOOM.WAD", "C:\\DOOM2.WAD",
        };
        for (const char* wad : kWads) {
            if (count >= MAX_SLOTS) break;
            if (addSlot(ram, dataOff, count, wad, buf))
                ++count;
        }
    }

    /* Stage sibling data files (WAD/VOL/CK/etc.) from the same C:\\GAMES\\... folder. */
    if (dosPath && std::strchr(dosPath, '\\')) {
        char dir[128]{};
        std::snprintf(dir, sizeof dir, "%s", dosPath);
        char* slash = std::strrchr(dir, '\\');
        if (slash) {
            *slash = '\0';
            auto wantDataExt = [](const char* ext) -> bool {
                static const char* kExact[] = {
                    "WAD", "WL6", "WL1", "WL3", "VOL", "MAP", "VGA", "VSW", "WM", "CM", "OVL",
                    "CK4", "CK3", "CK5", "CK6", "AUD", "CFG",
                };
                for (const char* wx : kExact)
                    if (std::strcmp(ext, wx) == 0) return true;
                return std::strncmp(ext, "CK", 2) == 0;
            };
            std::vector<FieldDos::FatRootEntry> ents;
            FieldDos::listFat16Dir(dir, ents);
            for (const auto& ent : ents) {
                if (ent.isDir || count >= MAX_SLOTS) continue;
                char path[160];
                std::snprintf(path, sizeof path, "%s\\%s", dir, ent.name);
                const char* dot = std::strrchr(ent.name, '.');
                if (!dot) continue;
                char eupper[8]{};
                std::snprintf(eupper, sizeof eupper, "%s", dot + 1);
                for (char* p = eupper; *p; ++p)
                    *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
                if (!wantDataExt(eupper)) continue;
                if (addSlot(ram, dataOff, count, path, buf))
                    ++count;
            }
        }
    }

    wr32(ram, TABLE_BASE + 4u, count);
    return count;
}

} // namespace FieldGpuFiles