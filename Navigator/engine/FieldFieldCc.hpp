#pragma once

// Field Compiler (FIELDC) v4 — .fld → ASM → AMMO .OBJ (Golden Era Field Die language).

#include "FieldAmmoAsm.hpp"
#include "FieldAmmoFat.hpp"

#include <cctype>
#include <regex>
#include <string>
#include <vector>

namespace FieldFieldCc {

struct Result {
    bool ok = false;
    std::string error;
    FieldAmmoAsm::Result asmResult;
};

inline std::string extractStringLiteral(const std::string& src, std::size_t pos) {
    auto q = src.find('"', pos);
    if (q == std::string::npos) return {};
    std::string out;
    for (++q; q < src.size() && src[q] != '"'; ++q) {
        if (src[q] == '\\' && q + 1 < src.size()) {
            const char n = src[q + 1];
            if (n == 'n') { out += "\r\n"; ++q; continue; }
            if (n == 'r') { out += "\r"; ++q; continue; }
            if (n == 't') { out += "\t"; ++q; continue; }
        }
        out.push_back(src[q]);
    }
    return out;
}

inline std::vector<std::string> findPrints(const std::string& src) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos < src.size()) {
        auto p = src.find("print", pos);
        if (p == std::string::npos) break;
        if (p > 0 && std::isalnum(static_cast<unsigned char>(src[p - 1]))) {
            pos = p + 5;
            continue;
        }
        const std::string lit = extractStringLiteral(src, p + 5);
        if (!lit.empty()) out.push_back(lit);
        pos = p + 5;
    }
    return out;
}

inline int parseReturn(const std::string& src) {
    std::smatch m;
    if (std::regex_search(src, m, std::regex(R"(return\s+(\d+)\s*;?)")))
        return std::stoi(m[1].str());
    return 0;
}

inline std::string dbData(const std::string& msg) {
    std::string line;
    bool first = true;
    auto append = [&](const std::string& piece) {
        if (!first) line += ",";
        first = false;
        line += piece;
    };
    for (char c : msg) {
        if (c == '$') { append("36"); continue; }
        if (c >= 32 && c < 127) append(std::string("'") + c + "'");
        else append(std::to_string(static_cast<unsigned char>(c)));
    }
    if (msg.empty() || msg.back() != '$') append("36");
    return line;
}

inline Result compileSource(const char* src, std::size_t len) {
    Result r;
    const std::string text(src, len);
    auto msgs = findPrints(text);
    if (msgs.empty()) {
        if (text.find("pad") != std::string::npos || text.find("PAD") != std::string::npos)
            msgs.push_back("Field pad program — run PADTEST for Xbox360 live view\r\n");
        else
            msgs.push_back("Field Compiler v4 — Golden Era Man+Machine\r\n");
    }
    const int retVal = parseReturn(text);

    std::string asmSrc =
        ".MODEL TINY\n.CODE\nORG 100h\nstart:\n";
    for (std::size_t i = 0; i < msgs.size(); ++i)
        asmSrc += "    mov ah,9\n    mov dx,offset msg" + std::to_string(i) + "\n    int 21h\n";
    asmSrc += "    mov ax," + std::to_string(0x4C00 + (retVal & 0xFF)) + "h\n    int 21h\n.DATA\n";
    for (std::size_t i = 0; i < msgs.size(); ++i)
        asmSrc += "msg" + std::to_string(i) + " db " + dbData(msgs[i]) + "\n";
    asmSrc += "END start\n";

    r.asmResult = FieldAmmoAsm::assembleSource(asmSrc.c_str(), asmSrc.size());
    r.ok = r.asmResult.ok;
    if (!r.ok) r.error = r.asmResult.error;
    return r;
}

inline Result compilePath(const char* dosPath) {
    std::vector<std::uint8_t> src;
    if (!FieldAmmoFat::readFile(dosPath, src) || src.empty())
        return Result{false, "cannot read .fld source", {}};
    return compileSource(reinterpret_cast<const char*>(src.data()), src.size());
}

} // namespace FieldFieldCc