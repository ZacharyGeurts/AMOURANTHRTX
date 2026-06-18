#pragma once

// AMMODECOMP v4 — COM/OBJ → MASM-style listing for round-trip inspection.

#include "FieldAmmoDisasm.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoObj.hpp"

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace FieldAmmoDecomp {

struct Result {
    bool ok = false;
    std::string error;
    std::string listing;
};

inline std::string inferDataSection(const std::vector<std::uint8_t>& com, std::uint32_t codeEnd) {
    if (codeEnd >= com.size()) return {};
    std::string out = "\r\n.DATA\r\n";
    std::uint32_t off = codeEnd;
    int label = 0;
    while (off < com.size()) {
        bool allZero = true;
        for (std::uint32_t i = off; i < com.size() && i < off + 16u; ++i)
            if (com[i]) { allZero = false; break; }
        if (allZero) { ++off; continue; }

        std::uint32_t end = off;
        while (end < com.size() && com[end]) ++end;
        char hdr[48];
        std::snprintf(hdr, sizeof hdr, "d_%04X db ", off);
        out += hdr;
        bool first = true;
        for (std::uint32_t i = off; i < end; ++i) {
            const unsigned b = com[i];
            if (b >= 32u && b < 127u) {
                if (!first) out += ',';
                first = false;
                char ch[8];
                std::snprintf(ch, sizeof ch, "'%c'", static_cast<char>(b));
                out += ch;
            } else {
                if (!first) out += ',';
                first = false;
                out += std::to_string(b);
            }
        }
        out += "\r\n";
        (void)label;
        off = end;
    }
    return out;
}

inline Result decompileComBytes(const std::vector<std::uint8_t>& com, std::uint32_t org = 0x100u) {
    Result r;
    if (com.empty()) { r.error = "empty COM"; return r; }
    std::string out =
        "; AMMODECOMP v4 listing\r\n"
        ".MODEL TINY\r\n.CODE\r\nORG " + std::to_string(org) + "h\r\nstart:\r\n";

    std::uint32_t ip = org;
    std::uint32_t codeEnd = 0;
    while (ip - org < com.size()) {
        const auto ins = FieldAmmoDisasm::decodeAt(com.data(), ip - org, com.size());
        if (ins.len == 0) break;
        const std::string t = ins.text;
        if (t.rfind("db ", 0) == 0 && ins.len == 1) break;
        if (t == "int 21h" && ip - org + 2u <= com.size()) {
            const std::uint16_t ax = (com[ip - org + 2u] == 0x4Cu)
                ? 0x4C00u : 0u;
            (void)ax;
        }
        char line[160];
        std::snprintf(line, sizeof line, "    %s\r\n", ins.text);
        out += line;
        codeEnd = (ip - org) + ins.len;
        ip += ins.len;
        if (t == "ret" || (t.rfind("int 4Ch", 0) == 0)) break;
    }

    out += inferDataSection(com, codeEnd);
    out += "END start\r\n";
    r.listing = out;
    r.ok = true;
    return r;
}

inline Result decompileObjBytes(const std::vector<std::uint8_t>& blob) {
    Result r;
    FieldAmmoObj::Module m;
    if (!FieldAmmoObj::unpack(blob, m)) {
        r.error = "not AMMO object";
        return r;
    }
    std::string out =
        "; AMMODECOMP v4 — AMMO object\r\n"
        ".MODEL TINY\r\n.CODE\r\nORG " + std::to_string(m.org) + "h\r\n";
    if (!m.exports.empty()) {
        for (const auto& e : m.exports) {
            char name[13]{};
            std::memcpy(name, e.name, 12);
            out += "PUBLIC " + std::string(name) + "\r\n";
        }
    }
    out += "start:\r\n";

    std::uint32_t ip = m.org;
    while (ip - m.org < m.code.size()) {
        const auto ins = FieldAmmoDisasm::decodeAt(m.code.data(), ip - m.org, m.code.size());
        if (ins.len == 0) break;
        char line[160];
        std::snprintf(line, sizeof line, "    %s\r\n", ins.text);
        out += line;
        ip += ins.len;
    }

    if (!m.data.empty()) {
        out += "\r\n.DATA\r\n";
        std::uint32_t off = 0;
        while (off < m.data.size()) {
            char line[96];
            std::snprintf(line, sizeof line, "    db %u\r\n", m.data[off++]);
            out += line;
        }
    }
    out += "END start\r\n";
    r.listing = out;
    r.ok = true;
    return r;
}

inline Result decompilePath(const char* dosPath) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile(dosPath, data) || data.empty())
        return Result{false, "cannot read file", {}};

    const bool isObj = data.size() >= 4u
        && data[0] == 'A' && data[1] == 'M' && data[2] == 'M' && data[3] == 'O';
    if (isObj) return decompileObjBytes(data);
    return decompileComBytes(data);
}

} // namespace FieldAmmoDecomp