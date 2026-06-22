#pragma once

// Host DOS INT 21h file services — FAT-backed open/read/write/find for MZ/COM/DOS4GW launch.

#include "FieldDos.hpp"
#include "FieldDpmi.hpp"
#include "FieldRtxMemory.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <x86emu.h>

namespace FieldInt21 {

using EchoFn = void (*)(x86emu_t* e, char ch);

struct HostFileSlot {
    std::vector<std::uint8_t> data;
    std::uint32_t pos = 0;
    std::string path;
};

inline HostFileSlot hostFiles[8];
inline char hostDrive = 'C';

struct HostFindState {
    std::vector<FieldDos::FatRootEntry> matches;
    std::size_t index = 0;
    bool active = false;
};
inline HostFindState hostFind;

inline std::uint32_t linearAddr(x86emu_t* e, std::uint32_t off) noexcept {
    if (FieldDpmi::isProtected(e))
        return e->x86.R_DS_BASE + FieldDpmi::pmOffset(e, off);
    return (static_cast<std::uint32_t>(e->x86.R_DS & 0xFFFFu) << 4) + (off & 0xFFFFu);
}

inline std::uint32_t pspDtaAddr(x86emu_t* e) noexcept {
    const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
    const std::uint16_t psp = static_cast<std::uint16_t>(cs - 0x10u);
    return (static_cast<std::uint32_t>(psp) << 4) + 0x80u;
}

inline bool hostDosEligible(x86emu_t* e, bool pmExecActive) noexcept {
    if (pmExecActive) return true;
    return (e->x86.R_CS & 0xFFFFu) >= 0x0700u;
}

inline bool isEmsDevicePath(const char* path) noexcept {
    if (!path) return false;
    char upper[32]{};
    for (int i = 0; path[i] && i < 31; ++i)
        upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(path[i])));
    return std::strstr(upper, "EMMXXXX0") != nullptr;
}

inline bool readHostPath(const char* path, std::vector<std::uint8_t>& out) noexcept {
    if (!path || !path[0]) return false;
    if (isEmsDevicePath(path) && FieldRtxMemory::emmLive()) {
        static const std::uint8_t kEmsDevice[] = {
            'E', 'M', 'M', 'X', 'X', 'X', 'X', '0', 0x1Au, 0x00u, 0x4Cu, 0x49u, 0x4Du, 0x20u, 0x45u, 0x4Du, 0x53u
        };
        out.assign(std::begin(kEmsDevice), std::end(kEmsDevice));
        return true;
    }
    if (path[0] != 'C' && path[0] != 'c' && path[0] != 'A' && path[0] != 'a') {
        char full[128];
        std::snprintf(full, sizeof full, "%c:\\%s", hostDrive, path);
        return FieldDos::readHostFile(full, out);
    }
    return FieldDos::readHostFile(path, out);
}

inline bool dosPatChar(char a, char b) noexcept {
    if (b == '?') return true;
    if (b == '*') return true;
    return std::toupper(static_cast<unsigned char>(a))
        == std::toupper(static_cast<unsigned char>(b));
}

inline bool dosPatternMatch(const char* name, const char* pattern) noexcept {
    if (!name || !pattern) return false;
    if (pattern[0] == '*' && pattern[1] == '\0') return true;
    const char* np = name;
    const char* pp = pattern;
    const char* star = nullptr;
    const char* match = nullptr;
    while (*np) {
        if (*pp == '*') {
            star = pp++;
            match = np;
            continue;
        }
        if (*pp && dosPatChar(*np, *pp)) {
            ++np;
            ++pp;
            continue;
        }
        if (!star) return false;
        pp = star + 1;
        np = ++match;
    }
    while (*pp == '*') ++pp;
    return *pp == '\0';
}

inline void collectHostFiles(std::vector<FieldDos::FatRootEntry>& out) {
    out.clear();
    FieldDos::listFat16Root(out);

    std::vector<FieldDos::FatRootEntry> gameDirs;
    if (FieldDos::listFat16Dir("C:\\GAMES", gameDirs)) {
        for (const auto& gd : gameDirs) {
            if (!gd.isDir) continue;
            char subpath[96];
            std::snprintf(subpath, sizeof subpath, "C:\\GAMES\\%s", gd.name);
            std::vector<FieldDos::FatRootEntry> files;
            if (!FieldDos::listFat16Dir(subpath, files)) continue;
            for (const auto& f : files) {
                if (f.isDir) continue;
                bool dup = false;
                for (const auto& e : out)
                    if (std::strcmp(e.name, f.name) == 0) { dup = true; break; }
                if (!dup) out.push_back(f);
            }
        }
    }

    static const char* kLoose[] = {
        "DOOM.EXE", "DOOM1.WAD", "DOOM.WAD", "DOOM2.WAD",
    };
    for (const char* fn : kLoose) {
        std::vector<std::uint8_t> tmp;
        char path[32];
        std::snprintf(path, sizeof path, "C:\\%s", fn);
        if (!FieldDos::readHostFile(path, tmp) || tmp.empty()) continue;
        bool dup = false;
        for (const auto& ent : out)
            if (std::strcmp(ent.name, fn) == 0) { dup = true; break; }
        if (!dup) {
            FieldDos::FatRootEntry fe{};
            std::snprintf(fe.name, sizeof fe.name, "%s", fn);
            fe.size = static_cast<std::uint32_t>(tmp.size());
            fe.isDir = false;
            out.push_back(fe);
        }
    }
}

inline void fillFindDta(x86emu_t* e, std::uint32_t dta, const FieldDos::FatRootEntry& ent) {
    for (std::uint32_t i = 0; i < 0x30u; ++i)
        x86emu_write_byte(e, dta + i, 0);
    x86emu_write_byte(e, dta + 0x15u, ent.isDir ? 0x10u : 0x20u);
    x86emu_write_word(e, dta + 0x16u, 0);
    x86emu_write_word(e, dta + 0x18u, 0x21u);
    x86emu_write_dword(e, dta + 0x1Au, ent.size);
    char dosName[13]{};
    std::snprintf(dosName, sizeof dosName, "%s", ent.name);
    for (int i = 0; dosName[i]; ++i)
        x86emu_write_byte(e, dta + 0x1Eu + static_cast<std::uint32_t>(i),
            static_cast<unsigned>(dosName[i]));
}

inline void readPathFromGuest(x86emu_t* e, std::uint32_t addr, char* path, int maxLen) {
    for (int i = 0; i < maxLen - 1; ++i) {
        path[i] = static_cast<char>(x86emu_read_byte(e, addr + static_cast<std::uint32_t>(i)));
        if (!path[i]) break;
    }
    path[maxLen - 1] = '\0';
}

inline int tryHostFileOps(x86emu_t* e, bool pmExecActive, EchoFn echo) {
    if (!hostDosEligible(e, pmExecActive)) return 0;
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    if (ah == 0x3B) {
        char path[128]{};
        readPathFromGuest(e, linearAddr(e, e->x86.R_EDX), path, 128);
        if (path[0] == 'C' || path[0] == 'c') hostDrive = 'C';
        else if (path[0] == 'A' || path[0] == 'a') hostDrive = 'A';
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    if (ah == 0x3D) {
        char path[128]{};
        readPathFromGuest(e, linearAddr(e, e->x86.R_EDX), path, 128);
        std::vector<std::uint8_t> data;
        if (!readHostPath(path, data)) {
            e->x86.R_FLG |= F_CF;
            return 1;
        }
        for (int i = 0; i < 8; ++i) {
            if (hostFiles[i].data.empty()) {
                hostFiles[i].data = std::move(data);
                hostFiles[i].pos = 0;
                hostFiles[i].path = path;
                e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | static_cast<std::uint32_t>(i);
                e->x86.R_FLG &= ~F_CF;
                return 1;
            }
        }
        e->x86.R_FLG |= F_CF;
        return 1;
    }
    if (ah == 0x3F) {
        const int handle = static_cast<int>(e->x86.R_EBX & 0xFFFFu);
        const std::uint16_t want = static_cast<std::uint16_t>(e->x86.R_ECX & 0xFFFFu);
        if (handle < 0 || handle >= 8 || hostFiles[handle].data.empty()) {
            e->x86.R_FLG |= F_CF;
            return 1;
        }
        auto& slot = hostFiles[handle];
        const std::uint32_t left = static_cast<std::uint32_t>(slot.data.size()) - slot.pos;
        const std::uint16_t got = static_cast<std::uint16_t>(std::min<std::uint32_t>(want, left));
        const std::uint32_t dst = linearAddr(e, e->x86.R_EDX);
        for (std::uint16_t i = 0; i < got; ++i)
            x86emu_write_byte(e, dst + i, slot.data[slot.pos + i]);
        slot.pos += got;
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | got;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x3E) {
        const int handle = static_cast<int>(e->x86.R_EBX & 0xFFFFu);
        if (handle >= 0 && handle < 8) hostFiles[handle] = {};
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    if (ah == 0x42) {
        const int handle = static_cast<int>(e->x86.R_EBX & 0xFFFFu);
        const std::uint8_t whence = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
        const std::int32_t delta = static_cast<std::int32_t>(e->x86.R_EDX);
        if (handle < 0 || handle >= 8 || hostFiles[handle].data.empty()) {
            e->x86.R_FLG |= F_CF;
            return 1;
        }
        auto& slot = hostFiles[handle];
        std::int64_t np = slot.pos;
        if (whence == 0) np = delta;
        else if (whence == 1) np += delta;
        else if (whence == 2) np = static_cast<std::int64_t>(slot.data.size()) + delta;
        if (np < 0) np = 0;
        if (np > static_cast<std::int64_t>(slot.data.size()))
            np = static_cast<std::int64_t>(slot.data.size());
        slot.pos = static_cast<std::uint32_t>(np);
        e->x86.R_EAX = static_cast<std::uint32_t>(slot.pos);
        e->x86.R_EDX = static_cast<std::uint32_t>(slot.pos >> 16);
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x3C) {
        char path[128]{};
        readPathFromGuest(e, linearAddr(e, e->x86.R_EDX), path, 128);
        for (int i = 0; i < 8; ++i) {
            if (hostFiles[i].data.empty()) {
                hostFiles[i].data.clear();
                hostFiles[i].pos = 0;
                hostFiles[i].path = path;
                e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | static_cast<std::uint32_t>(i);
                e->x86.R_FLG &= ~F_CF;
                return 1;
            }
        }
        e->x86.R_FLG |= F_CF;
        return 1;
    }
    if (ah == 0x40) {
        const int handle = static_cast<int>(e->x86.R_EBX & 0xFFFFu);
        const std::uint16_t count = static_cast<std::uint16_t>(e->x86.R_ECX & 0xFFFFu);
        const std::uint32_t src = linearAddr(e, e->x86.R_EDX);
        if ((handle == 1 || handle == 2) && echo) {
            for (std::uint16_t i = 0; i < count; ++i) {
                const char ch = static_cast<char>(
                    x86emu_read_byte(e, src + static_cast<std::uint32_t>(i)));
                if (ch != '\r') echo(e, ch);
            }
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | count;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (handle >= 0 && handle < 8 && !hostFiles[handle].data.empty()) {
            auto& slot = hostFiles[handle];
            for (std::uint16_t i = 0; i < count; ++i) {
                const std::uint8_t b = static_cast<std::uint8_t>(
                    x86emu_read_byte(e, src + static_cast<std::uint32_t>(i)));
                if (slot.pos < slot.data.size())
                    slot.data[slot.pos++] = b;
                else
                    slot.data.push_back(b);
            }
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | count;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        e->x86.R_FLG |= F_CF;
        return 1;
    }
    if (ah == 0x43) {
        char path[128]{};
        readPathFromGuest(e, linearAddr(e, e->x86.R_EDX), path, 128);
        const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
        if (al == 0x00u) {
            std::vector<std::uint8_t> tmp;
            if (!readHostPath(path, tmp)) {
                e->x86.R_AX = 0x0002u;
                e->x86.R_FLG |= F_CF;
                return 1;
            }
            e->x86.R_CX = (e->x86.R_CX & 0xFFFF0000u) | 0x0020u;
            e->x86.R_AX = 0u;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        e->x86.R_AX = 0u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x47) {
        const std::uint8_t dl = static_cast<std::uint8_t>(e->x86.R_EDX & 0xFFu);
        const std::uint32_t dst = linearAddr(e, e->x86.R_ESI);
        const char* cwd = (dl == 0 || dl == 3) ? "C:\\" : "A:\\";
        for (int i = 0; cwd[i]; ++i)
            x86emu_write_byte(e, dst + static_cast<std::uint32_t>(i), static_cast<unsigned>(cwd[i]));
        x86emu_write_byte(e, dst + std::strlen(cwd), 0);
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x4E) {
        char pattern[128]{};
        readPathFromGuest(e, linearAddr(e, e->x86.R_EDX), pattern, 128);
        const char* pat = pattern;
        if (std::strchr(pattern, '\\') || std::strchr(pattern, '/')) {
            const char* slash = std::strrchr(pattern, '\\');
            if (!slash) slash = std::strrchr(pattern, '/');
            if (slash) pat = slash + 1;
        }
        hostFind.matches.clear();
        std::vector<FieldDos::FatRootEntry> all;
        collectHostFiles(all);
        const std::uint16_t attr = static_cast<std::uint16_t>(e->x86.R_CX & 0xFFFFu);
        for (const auto& ent : all) {
            if (ent.isDir && !(attr & 0x10u)) continue;
            if (!ent.isDir && (attr & 0x10u) && attr != 0xFFFFu) continue;
            if (dosPatternMatch(ent.name, pat))
                hostFind.matches.push_back(ent);
        }
        hostFind.index = 0;
        hostFind.active = !hostFind.matches.empty();
        if (!hostFind.active) {
            e->x86.R_AX = 0x0012u;
            e->x86.R_FLG |= F_CF;
            return 1;
        }
        fillFindDta(e, pspDtaAddr(e), hostFind.matches[0]);
        hostFind.index = 1;
        e->x86.R_AX = 0u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x4F) {
        if (!hostFind.active || hostFind.index >= hostFind.matches.size()) {
            e->x86.R_AX = 0x0012u;
            e->x86.R_FLG |= F_CF;
            return 1;
        }
        fillFindDta(e, pspDtaAddr(e), hostFind.matches[hostFind.index]);
        ++hostFind.index;
        e->x86.R_AX = 0u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x62) {
        const std::uint16_t psp = static_cast<std::uint16_t>((e->x86.R_CS & 0xFFFFu) - 0x10u);
        e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u) | psp;
        return 1;
    }
    return 0;
}

} // namespace FieldInt21