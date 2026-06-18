#pragma once

// Virtual AMMOINC — FIELD.H / RTX.H / FIELD.INC baked from FieldRtxFieldAbs.

#include "FieldRtxFieldAbs.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace FieldAmmoInc {

inline std::string fieldH() {
    return std::string(
        "/* RTX Field theory absolutes - AMMOINC/FIELD.H */\n"
        "#define FIELD_THEORY_REV \"") + FieldRtxFieldAbs::THEORY_REV + "\"\n"
        "#define FIELD_LAYER_RAM 0\n"
        "#define FIELD_LAYER_VGA 1\n"
        "#define FIELD_LAYER_FAT 2\n"
        "#define FIELD_LAYER_COUNT 10\n"
        "#define FIELD_BUS_TESLA 31\n"
        "#define TESLA_R_FWD_MILLI 180\n"
        "#define TESLA_R_REV_MILLI 3200\n"
        "#define FIELD_PHI_MILLI 618\n"
        "#define FIELD_RAM_BYTES 0x04000000u\n"
        "#define FIELD_BOOT_VECTOR 0x7C00u\n";
}

inline const char* kRtxH =
    "/* RTX-AMMOS C intrinsics + field absolutes */\n"
    "void rtx_puts(const char* s);\n"
    "void rtx_out(int ch);\n"
    "#define RTX_VERSION \"4.0.2026\"\n"
    "#define TESLA_R_FWD_MILLI 180\n"
    "#define TESLA_R_REV_MILLI 3200\n"
    "#define FIELD_BUS_TESLA 31\n";

inline const char* kFieldInc =
    "; FIELD.INC - Tesla valve + layer absolutes (MASM)\n"
    "TESLA_R_FWD_MILLI EQU 180\n"
    "TESLA_R_REV_MILLI EQU 3200\n"
    "FIELD_BUS_TESLA   EQU 31\n"
    "LAYER_FAT         EQU 2\n"
    "RTX_MUX_AH        EQU 052h\n";

inline std::unordered_map<std::string, int> cDefines() {
    return {
        {"FIELD_LAYER_RAM", FieldRtxFieldAbs::LAYER_RAM},
        {"FIELD_LAYER_VGA", FieldRtxFieldAbs::LAYER_VGA},
        {"FIELD_LAYER_FAT", FieldRtxFieldAbs::LAYER_FAT},
        {"FIELD_LAYER_COUNT", FieldRtxFieldAbs::LAYER_COUNT},
        {"FIELD_BUS_TESLA", FieldRtxFieldAbs::BUS_TESLA},
        {"TESLA_R_FWD_MILLI", FieldRtxFieldAbs::TESLA_R_FWD_MILLI},
        {"TESLA_R_REV_MILLI", FieldRtxFieldAbs::TESLA_R_REV_MILLI},
        {"FIELD_PHI_MILLI", FieldRtxFieldAbs::FIELD_PHI_MILLI},
        {"FIELD_RAM_BYTES", static_cast<int>(FieldRtxFieldAbs::RAM_BYTES)},
        {"FIELD_BOOT_VECTOR", static_cast<int>(FieldRtxFieldAbs::BOOT_VECTOR)},
        {"RTX_VERSION", 0},
    };
}

inline bool resolveVirtual(const std::string& name, std::string& out) {
    std::string u = name;
    for (auto& c : u) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (u == "FIELD.H" || u == "FIELD\\H") { out = fieldH(); return true; }
    if (u == "RTX.H" || u == "RTX\\H") { out = kRtxH; return true; }
    if (u == "FIELD.INC" || u == "FIELD\\INC") { out = kFieldInc; return true; }
    if (u == "SUPERCORE.INC" || u == "SUPERCORE\\INC") {
        out = "; SUPERCORE.INC stub\nLAYER_FAT EQU 2\nRTX_MUX_AH EQU 052h\n";
        return true;
    }
    return false;
}

inline bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

inline std::string substituteDefines(std::string text,
                                     const std::unordered_map<std::string, int>& defs) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const char c = text[i];
        if (!std::isalpha(static_cast<unsigned char>(c)) && c != '_') {
            out.push_back(c);
            ++i;
            continue;
        }
        std::size_t j = i + 1;
        while (j < text.size() && isIdentChar(text[j])) ++j;
        const std::string id = text.substr(i, j - i);
        const auto it = defs.find(id);
        if (it != defs.end())
            out += std::to_string(it->second);
        else
            out.append(id);
        i = j;
    }
    return out;
}

inline void collectDefine(const std::string& line, std::unordered_map<std::string, int>& defs) {
    if (line.size() < 8 || line.substr(0, 7) != "#define") return;
    const auto sp = line.find_first_of(" \t", 7);
    if (sp == std::string::npos) return;
    std::string name = line.substr(7, sp - 7);
    std::string val = line.substr(sp + 1);
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    if (!val.empty() && val.front() == '"') return;
    while (!val.empty() && (val.back() == 'u' || val.back() == 'U' || val.back() == 'l' || val.back() == 'L'))
        val.pop_back();
    char* end = nullptr;
    const long n = std::strtol(val.c_str(), &end, 0);
    if (end && end > val.c_str()) defs[name] = static_cast<int>(n);
}

inline void processCLine(const std::string& raw, std::string& out,
                         std::unordered_map<std::string, int>& defs) {
    std::string line = raw;
    while (!line.empty() && line.front() == ' ') line.erase(0, 1);
    if (line.empty()) return;
    if (line.size() >= 8 && line.substr(0, 8) == "#include") {
        auto q1 = line.find('"');
        auto q2 = line.find('"', q1 + 1);
        if (q1 != std::string::npos && q2 != std::string::npos) {
            std::string inc;
                if (resolveVirtual(line.substr(q1 + 1, q2 - q1 - 1), inc)) {
                std::string incLine;
                for (char ch : inc) {
                    if (ch == '\r') continue;
                    if (ch == '\n') {
                        collectDefine(incLine, defs);
                        incLine.clear();
                        continue;
                    }
                    incLine += ch;
                }
                if (!incLine.empty()) collectDefine(incLine, defs);
            }
        }
        return;
    }
    collectDefine(line, defs);
    if (line[0] == '#') return;
    out += line;
    out += '\n';
}

inline std::string preprocessC(const char* src, std::size_t len) {
    std::string expanded;
    expanded.reserve(len + 1024);
    std::unordered_map<std::string, int> defs = cDefines();
    std::string line;
    for (std::size_t i = 0; i < len; ++i) {
        const char c = src[i];
        if (c == '\r') continue;
        if (c == '\n') {
            processCLine(line, expanded, defs);
            line.clear();
            continue;
        }
        line += c;
    }
    if (!line.empty()) processCLine(line, expanded, defs);
    return substituteDefines(expanded, defs);
}

inline std::string preprocessAsm(const std::string& src) {
    std::string out;
    std::string line;
    auto flush = [&]() {
        if (line.empty()) return;
        std::string trimmed = line;
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(0, 1);
        if (trimmed.size() >= 7) {
            std::string head = trimmed.substr(0, 7);
            for (auto& c : head) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (head == "INCLUDE") {
                auto sp = trimmed.find_last_of(" \t");
                std::string inc = (sp != std::string::npos) ? trimmed.substr(sp + 1) : "";
                std::string body;
                if (resolveVirtual(inc, body)) out += body;
                line.clear();
                return;
            }
        }
        out += line;
        out += '\n';
        line.clear();
    };
    for (char c : src) {
        if (c == '\r') continue;
        if (c == '\n') { flush(); continue; }
        line += c;
    }
    flush();
    return out;
}

} // namespace FieldAmmoInc