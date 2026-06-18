#pragma once

// AMMO object v3 — modules, exports, imports for multi-file link.

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace FieldAmmoObj {

constexpr char MAGIC[4] = {'A', 'M', 'M', 'O'};
constexpr std::uint8_t VERSION = 3u;

struct ExportSym {
    char name[12]{};
    std::uint32_t codeOff = 0;
};

struct ImportFix {
    std::uint32_t codeOff = 0;
    char name[12]{};
    std::uint8_t kind = 0; /* 0=CALL rel16 */
};

struct Module {
    std::vector<std::uint8_t> code;
    std::vector<std::uint8_t> data;
    std::uint32_t entryIp = 0;
    std::uint32_t org = 0x100u;
    std::vector<ExportSym> exports;
    std::vector<ImportFix> imports;
    std::vector<std::uint32_t> dataFixups; /* code offset of OFFSET imm16 */
};

inline void put32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

inline std::uint32_t get32(const std::vector<std::uint8_t>& b, std::size_t off) {
    return static_cast<std::uint32_t>(b[off])
        | (static_cast<std::uint32_t>(b[off + 1]) << 8)
        | (static_cast<std::uint32_t>(b[off + 2]) << 16)
        | (static_cast<std::uint32_t>(b[off + 3]) << 24);
}

inline void packName(char out[12], const std::string& name) {
    std::memset(out, 0, 12);
    const std::string u = name.size() > 12 ? name.substr(0, 12) : name;
    std::memcpy(out, u.c_str(), u.size());
}

inline std::vector<std::uint8_t> pack(const Module& m) {
    std::vector<std::uint8_t> b;
    b.insert(b.end(), MAGIC, MAGIC + 4);
    b.push_back(VERSION);
    put32(b, static_cast<std::uint32_t>(m.code.size()));
    put32(b, static_cast<std::uint32_t>(m.data.size()));
    put32(b, m.entryIp);
    put32(b, m.org);
    b.insert(b.end(), m.code.begin(), m.code.end());
    b.insert(b.end(), m.data.begin(), m.data.end());
    put32(b, static_cast<std::uint32_t>(m.exports.size()));
    for (const auto& e : m.exports) {
        b.insert(b.end(), e.name, e.name + 12);
        put32(b, e.codeOff);
    }
    put32(b, static_cast<std::uint32_t>(m.imports.size()));
    for (const auto& im : m.imports) {
        put32(b, im.codeOff);
        b.insert(b.end(), im.name, im.name + 12);
        b.push_back(im.kind);
    }
    put32(b, static_cast<std::uint32_t>(m.dataFixups.size()));
    for (const auto& d : m.dataFixups)
        put32(b, d);
    return b;
}

inline bool unpack(const std::vector<std::uint8_t>& blob, Module& m) {
    if (blob.size() < 21u || std::memcmp(blob.data(), MAGIC, 4) != 0) return false;
    const std::uint8_t ver = blob[4];
    if (ver != 2u && ver != 3u) return false;
    const std::uint32_t codeLen = get32(blob, 5);
    const std::uint32_t dataLen = get32(blob, 9);
    m.entryIp = get32(blob, 13);
    m.org = get32(blob, 17);
    const std::size_t body = 21u + codeLen + dataLen;
    if (blob.size() < body) return false;
    m.code.assign(blob.begin() + 21, blob.begin() + 21 + codeLen);
    m.data.assign(blob.begin() + 21 + codeLen, blob.begin() + body);
    m.exports.clear();
    m.imports.clear();
    if (ver < 3u || blob.size() <= body) return true;
    std::size_t off = body;
    if (off + 4 > blob.size()) return true;
    const std::uint32_t ec = get32(blob, off);
    off += 4;
    for (std::uint32_t i = 0; i < ec; ++i) {
        if (off + 16 > blob.size()) return false;
        ExportSym e{};
        std::memcpy(e.name, blob.data() + off, 12);
        e.codeOff = get32(blob, off + 12);
        off += 16;
        m.exports.push_back(e);
    }
    if (off + 4 > blob.size()) return true;
    const std::uint32_t ic = get32(blob, off);
    off += 4;
    for (std::uint32_t i = 0; i < ic; ++i) {
        if (off + 17 > blob.size()) return false;
        ImportFix im{};
        im.codeOff = get32(blob, off);
        std::memcpy(im.name, blob.data() + off + 4, 12);
        im.kind = blob[off + 16];
        off += 17;
        m.imports.push_back(im);
    }
    if (off + 4 <= blob.size()) {
        const std::uint32_t dc = get32(blob, off);
        off += 4;
        for (std::uint32_t i = 0; i < dc; ++i) {
            if (off + 4 > blob.size()) return false;
            m.dataFixups.push_back(get32(blob, off));
            off += 4;
        }
    }
    return true;
}

} // namespace FieldAmmoObj