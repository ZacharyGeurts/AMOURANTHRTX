#pragma once

// AMMOASM v4 — MASM subset + PUBLIC/EXTRN multi-module.

#include "FieldAmmoFat.hpp"
#include "FieldAmmoInc.hpp"
#include "FieldAmmoObj.hpp"
#include "FieldDos.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace FieldAmmoAsm {

constexpr std::uint8_t OBJ_VERSION = FieldAmmoObj::VERSION;

struct Result {
    bool ok = false;
    std::string error;
    std::vector<std::uint8_t> object;
};

inline std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || std::isspace(static_cast<unsigned char>(s.back()))))
        s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

inline std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

inline bool parseNumber(const char* tok, std::uint32_t& out) {
    if (!tok || !*tok) return false;
    std::string t(tok);
    while (!t.empty() && std::isspace(static_cast<unsigned char>(t.back()))) t.pop_back();
    int base = 10;
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        out = static_cast<std::uint32_t>(std::strtoul(t.c_str() + 2, nullptr, 16));
        return true;
    }
    if (!t.empty() && (t.back() == 'h' || t.back() == 'H')) {
        t.pop_back();
        base = 16;
    }
    char* end = nullptr;
    out = static_cast<std::uint32_t>(std::strtoul(t.c_str(), &end, base));
    return end && end > t.c_str();
}

enum class Reg : std::uint8_t {
    AL, CL, DL, BL, AH, CH, DH, BH,
    AX, CX, DX, BX, SP, BP, SI, DI, NONE
};

inline Reg parseReg(const char* t) {
    static const struct { const char* n; Reg r; } k[] = {
        {"AL", Reg::AL}, {"CL", Reg::CL}, {"DL", Reg::DL}, {"BL", Reg::BL},
        {"AH", Reg::AH}, {"CH", Reg::CH}, {"DH", Reg::DH}, {"BH", Reg::BH},
        {"AX", Reg::AX}, {"CX", Reg::CX}, {"DX", Reg::DX}, {"BX", Reg::BX},
        {"SP", Reg::SP}, {"BP", Reg::BP}, {"SI", Reg::SI}, {"DI", Reg::DI},
    };
    const std::string u = upper(t);
    for (const auto& e : k)
        if (u == e.n) return e.r;
    return Reg::NONE;
}

inline bool reg8(Reg r) {
    return r >= Reg::AL && r <= Reg::BH;
}

inline std::uint8_t reg8Code(Reg r) {
    switch (r) {
    case Reg::AL: return 0; case Reg::CL: return 1; case Reg::DL: return 2; case Reg::BL: return 3;
    case Reg::AH: return 4; case Reg::CH: return 5; case Reg::DH: return 6; case Reg::BH: return 7;
    default: return 0xFF;
    }
}

inline std::uint8_t reg16Code(Reg r) {
    switch (r) {
    case Reg::AX: return 0; case Reg::CX: return 1; case Reg::DX: return 2; case Reg::BX: return 3;
    case Reg::SP: return 4; case Reg::BP: return 5; case Reg::SI: return 6; case Reg::DI: return 7;
    default: return 0xFF;
    }
}

struct Emitter {
    std::vector<std::uint8_t> code;
    std::vector<std::uint8_t> data;
    std::unordered_map<std::string, std::uint32_t> labels;
    std::unordered_map<std::string, std::uint32_t> constants;
    std::unordered_map<std::string, std::uint32_t> dataLabels;
    std::vector<std::pair<std::size_t, std::string>> fixups16;
    std::vector<std::pair<std::size_t, std::string>> pendingDataFixups;
    std::vector<std::size_t> dataFixups;
    std::vector<std::pair<std::size_t, std::string>> fixups8;
    std::unordered_map<std::string, bool> extrn;
    std::unordered_map<std::string, bool> pub;
    std::vector<FieldAmmoObj::ExportSym> exports;
    std::vector<FieldAmmoObj::ImportFix> imports;
    std::uint32_t org = 0x100u;
    std::string entry = "start";
    std::string lastError;

    void emit8(std::uint8_t b) { code.push_back(b); }
    void emit16(std::uint16_t w) {
        emit8(static_cast<std::uint8_t>(w & 0xFFu));
        emit8(static_cast<std::uint8_t>((w >> 8) & 0xFFu));
    }

    std::uint32_t ip() const { return org + static_cast<std::uint32_t>(code.size()); }

    bool defineLabel(const std::string& name) {
        const auto it = labels.find(name);
        if (it != labels.end() && it->second != ip()) {
            lastError = "duplicate label: " + name;
            return false;
        }
        labels[name] = ip();
        if (pub.count(name)) {
            FieldAmmoObj::ExportSym e{};
            FieldAmmoObj::packName(e.name, name);
            e.codeOff = static_cast<std::uint32_t>(code.size());
            exports.push_back(e);
        }
        return true;
    }

    bool emitMovRegImm(Reg dst, std::uint32_t imm) {
        if (reg8(dst)) {
            emit8(static_cast<std::uint8_t>(0xB0u + reg8Code(dst)));
            emit8(static_cast<std::uint8_t>(imm & 0xFFu));
            return true;
        }
        if (reg16Code(dst) != 0xFF) {
            emit8(static_cast<std::uint8_t>(0xB8u + reg16Code(dst)));
            emit16(static_cast<std::uint16_t>(imm & 0xFFFFu));
            return true;
        }
        lastError = "mov reg, imm — bad register";
        return false;
    }

    bool emitMovRegReg(Reg dst, Reg src) {
        if (dst == Reg::AX && reg16Code(src) != 0xFF) {
            emit8(0x8Bu); emit8(static_cast<std::uint8_t>(0xC0u | (reg16Code(src) << 3)));
            return true;
        }
        if (src == Reg::AX && reg16Code(dst) != 0xFF) {
            emit8(0x89u); emit8(static_cast<std::uint8_t>(0xC0u | (reg16Code(dst) << 3)));
            return true;
        }
        lastError = "mov reg, reg — limited to AX pairs";
        return false;
    }

    bool emitInt(std::uint32_t n) {
        if (n > 0xFFu) { lastError = "int imm8 overflow"; return false; }
        emit8(0xCDu);
        emit8(static_cast<std::uint8_t>(n));
        return true;
    }

    bool emitJmp(const std::string& label) {
        fixups8.push_back({code.size() + 1u, label});
        emit8(0xEBu);
        emit8(0x00u);
        return true;
    }

    bool emitCall(const std::string& label) {
        if (extrn.count(label)) {
            FieldAmmoObj::ImportFix im{};
            im.codeOff = static_cast<std::uint32_t>(code.size() + 1u);
            FieldAmmoObj::packName(im.name, label);
            im.kind = 0;
            imports.push_back(im);
            emit8(0xE8u);
            emit16(0x0000u);
            return true;
        }
        fixups16.push_back({code.size() + 1u, label});
        emit8(0xE8u);
        emit16(0x0000u);
        return true;
    }

    bool emitAddRegImm(Reg dst, std::uint32_t imm) {
        if (dst == Reg::AX) { emit8(0x05u); emit16(static_cast<std::uint16_t>(imm & 0xFFFFu)); return true; }
        if (dst == Reg::AL) { emit8(0x04u); emit8(static_cast<std::uint8_t>(imm & 0xFFu)); return true; }
        lastError = "ADD reg, imm — AX/AL only";
        return false;
    }

    bool emitAddAxBx() { emit8(0x01u); emit8(0xD8u); return true; }

    bool emitCmpAxImm(std::uint32_t imm) {
        emit8(0x3Du);
        emit16(static_cast<std::uint16_t>(imm & 0xFFFFu));
        return true;
    }

    bool emitJe(const std::string& label) {
        fixups8.push_back({code.size() + 1u, label});
        emit8(0x74u);
        emit8(0x00u);
        return true;
    }

    bool emitJne(const std::string& label) {
        fixups8.push_back({code.size() + 1u, label});
        emit8(0x75u);
        emit8(0x00u);
        return true;
    }

    bool emitIncReg16(Reg r) {
        const std::uint8_t c = reg16Code(r);
        if (c == 0xFF) { lastError = "INC bad reg"; return false; }
        emit8(static_cast<std::uint8_t>(0x40u + c));
        return true;
    }

    bool emitDecReg16(Reg r) {
        const std::uint8_t c = reg16Code(r);
        if (c == 0xFF) { lastError = "DEC bad reg"; return false; }
        emit8(static_cast<std::uint8_t>(0x48u + c));
        return true;
    }

    bool emitPushReg16(Reg r) {
        const std::uint8_t c = reg16Code(r);
        if (c == 0xFF) { lastError = "PUSH bad reg"; return false; }
        emit8(static_cast<std::uint8_t>(0x50u + c));
        return true;
    }

    bool emitPopReg16(Reg r) {
        const std::uint8_t c = reg16Code(r);
        if (c == 0xFF) { lastError = "POP bad reg"; return false; }
        emit8(static_cast<std::uint8_t>(0x58u + c));
        return true;
    }

    bool emitSubAxBx() { emit8(0x2Bu); emit8(0xC3u); return true; }
    bool emitXorAxAx() { emit8(0x33u); emit8(0xC0u); return true; }

    bool emitCmpRegReg(Reg a, Reg b) {
        if (a == Reg::BX && b == Reg::AX) { emit8(0x39u); emit8(0xC3u); return true; }
        if (a == Reg::AX && b == Reg::BX) { emit8(0x3Bu); emit8(0xC3u); return true; }
        if (a == Reg::BX && b == Reg::NONE) return false;
        lastError = "CMP reg,reg — BX/AX only";
        return false;
    }

    bool emitCmpBxImm(std::uint32_t imm) {
        emit8(0x81u);
        emit8(0xFBu);
        emit16(static_cast<std::uint16_t>(imm & 0xFFFFu));
        return true;
    }

    bool emitJb(const std::string& label) {
        fixups8.push_back({code.size() + 1u, label});
        emit8(0x72u);
        emit8(0x00u);
        return true;
    }

    bool emitJa(const std::string& label) {
        fixups8.push_back({code.size() + 1u, label});
        emit8(0x77u);
        emit8(0x00u);
        return true;
    }

    bool emitJbe(const std::string& label) {
        fixups8.push_back({code.size() + 1u, label});
        emit8(0x76u);
        emit8(0x00u);
        return true;
    }

    bool emitJae(const std::string& label) {
        fixups8.push_back({code.size() + 1u, label});
        emit8(0x73u);
        emit8(0x00u);
        return true;
    }

    bool emitRet() { emit8(0xC3u); return true; }

    bool resolveImm(const std::string& tok, std::uint32_t& out) const {
        const std::string u = upper(tok);
        const auto c = constants.find(u);
        if (c != constants.end()) { out = c->second; return true; }
        return parseNumber(tok.c_str(), out);
    }

    std::uint32_t symValue(const std::string& sym) const {
        const auto k = constants.find(sym);
        if (k != constants.end()) return k->second;
        const auto c = labels.find(sym);
        if (c != labels.end()) return c->second;
        const auto d = dataLabels.find(sym);
        if (d != dataLabels.end())
            return org + static_cast<std::uint32_t>(code.size()) + d->second;
        return 0xFFFFFFFFu;
    }

    bool resolveFixups() {
        for (const auto& f : fixups8) {
            const std::uint32_t tgt = symValue(f.second);
            if (tgt == 0xFFFFFFFFu) { lastError = "undefined label: " + f.second; return false; }
            const int delta = static_cast<int>(tgt) - static_cast<int>(org + f.first + 1u);
            if (delta < -128 || delta > 127) { lastError = "short jmp out of range: " + f.second; return false; }
            code[f.first] = static_cast<std::uint8_t>(delta & 0xFF);
        }
        for (const auto& f : fixups16) {
            const std::uint32_t tgt = symValue(f.second);
            if (tgt == 0xFFFFFFFFu) { lastError = "undefined label: " + f.second; return false; }
            const std::uint16_t imm = static_cast<std::uint16_t>(tgt & 0xFFFFu);
            code[f.first] = static_cast<std::uint8_t>(imm & 0xFFu);
            code[f.first + 1u] = static_cast<std::uint8_t>((imm >> 8) & 0xFFu);
        }
        for (const auto& f : pendingDataFixups) {
            const std::uint32_t tgt = symValue(f.second);
            if (tgt == 0xFFFFFFFFu) { lastError = "undefined label: " + f.second; return false; }
            const std::uint16_t imm = static_cast<std::uint16_t>(tgt & 0xFFFFu);
            code[f.first] = static_cast<std::uint8_t>(imm & 0xFFu);
            code[f.first + 1u] = static_cast<std::uint8_t>((imm >> 8) & 0xFFu);
            dataFixups.push_back(f.first);
        }
        return true;
    }

    bool packObject(Result& out) {
        if (!resolveFixups()) return false;
        auto it = labels.find(entry);
        if (it == labels.end()) it = labels.find("START");
        if (it == labels.end() && !labels.empty()) it = labels.begin();
        FieldAmmoObj::Module m;
        m.code = code;
        m.data = data;
        m.entryIp = (it != labels.end()) ? it->second : org;
        m.org = org;
        m.exports = exports;
        m.imports = imports;
        for (std::size_t d : dataFixups)
            m.dataFixups.push_back(static_cast<std::uint32_t>(d));
        out.object = FieldAmmoObj::pack(m);
        out.ok = true;
        return true;
    }
};

inline bool tokenizeLine(const std::string& line, std::vector<std::string>& tok) {
    tok.clear();
    std::string cur;
    bool inQuote = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '\'') {
            cur.push_back(c);
            if (inQuote) {
                tok.push_back(cur);
                cur.clear();
                inQuote = false;
            } else {
                inQuote = true;
            }
            continue;
        }
        if (inQuote) {
            cur.push_back(c);
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
        } else if (c == ',') {
            if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
        } else
            cur.push_back(c);
    }
    if (!cur.empty()) tok.push_back(cur);
    return !tok.empty();
}

inline Result assembleSource(const char* src, std::size_t len) {
    const std::string expanded = FieldAmmoInc::preprocessAsm(std::string(src, len));
    src = expanded.c_str();
    len = expanded.size();
    Result out;
    Emitter em;
    bool inData = false;
    std::string dataLabel;
    [[maybe_unused]] std::uint32_t dataBase = 0;

    auto fail = [&](const std::string& msg) {
        out.ok = false;
        out.error = msg;
        return out;
    };

    std::string text(src, len);
    std::size_t pos = 0;
    int lineNo = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string raw = text.substr(pos, end - pos);
        pos = end + 1;
        ++lineNo;

        std::string cmt;
        const auto sc = raw.find(';');
        if (sc != std::string::npos) { cmt = raw.substr(sc); raw = raw.substr(0, sc); }
        std::string line = trim(raw);
        if (line.empty()) continue;

        std::string label;
        const auto colon = line.find(':');
        if (colon != std::string::npos) {
            label = trim(line.substr(0, colon));
            line = trim(line.substr(colon + 1));
            if (!label.empty()) {
                if (!inData) {
                    if (!em.defineLabel(upper(label))) return fail(em.lastError);
                } else {
                    em.dataLabels[upper(label)] = static_cast<std::uint32_t>(em.data.size());
                }
            }
        }
        if (line.empty()) continue;

        std::vector<std::string> tok;
        tokenizeLine(line, tok);
        if (tok.size() >= 2) {
            const std::string t1 = upper(tok[1]);
            if (t1 == "DB" || t1 == "DW") {
                if (inData)
                    em.dataLabels[upper(tok[0])] = static_cast<std::uint32_t>(em.data.size());
                else if (!em.defineLabel(upper(tok[0]))) return fail(em.lastError);
                tok.erase(tok.begin());
            }
        }
        const std::string op = upper(tok[0]);

        if (tok.size() >= 3 && upper(tok[1]) == "EQU") {
            std::uint32_t v = 0;
            if (!em.resolveImm(tok[2], v)) return fail("EQU bad value: " + tok[0]);
            em.constants[upper(tok[0])] = v;
            continue;
        }
        if (op == ".MODEL" || op == "MODEL" || op == "ASSUME" || op == ".CODE" || op == "CODE") continue;
        if (op == "PUBLIC" && tok.size() >= 2) { em.pub[upper(tok[1])] = true; continue; }
        if (op == "EXTRN" && tok.size() >= 2) { em.extrn[upper(tok[1])] = true; continue; }
        if (op == ".DATA" || op == "DATA") { inData = true; continue; }
        if (op == "ORG") {
            std::uint32_t v = 0;
            if (tok.size() < 2 || !em.resolveImm(tok[1], v)) return fail("ORG needs number");
            if (inData) dataBase = v; else em.org = v;
            continue;
        }
        if (op == "END") {
            if (tok.size() >= 2) em.entry = upper(tok[1]);
            break;
        }
        if (op == "DB") {
            auto& seg = inData ? em.data : em.code;
            for (std::size_t i = 1; i < tok.size(); ++i) {
                if (tok[i] == "'") continue;
                if (tok[i].size() >= 2 && tok[i].front() == '\'' && tok[i].back() == '\'') {
                    for (std::size_t c = 1; c + 1 < tok[i].size(); ++c)
                        seg.push_back(static_cast<std::uint8_t>(tok[i][c]));
                } else if (tok[i].size() > 1 && tok[i][0] == '\'' && tok[i].back() == '$') {
                    for (std::size_t c = 1; c < tok[i].size(); ++c)
                        seg.push_back(static_cast<std::uint8_t>(tok[i][c]));
                } else {
                    std::uint32_t v = 0;
                    if (!em.resolveImm(tok[i], v)) return fail("DB bad value");
                    seg.push_back(static_cast<std::uint8_t>(v & 0xFFu));
                }
            }
            continue;
        }
        if (op == "DW") {
            auto& seg = inData ? em.data : em.code;
            for (std::size_t i = 1; i < tok.size(); ++i) {
                std::uint32_t v = 0;
                if (!em.resolveImm(tok[i], v)) return fail("DW bad value");
                seg.push_back(static_cast<std::uint8_t>(v & 0xFFu));
                seg.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
            }
            continue;
        }

        if (inData) return fail("instruction in .DATA section");

        if (op == "MOV") {
            if (tok.size() < 3) return fail("MOV needs operands");
            const Reg dst = parseReg(tok[1].c_str());
            std::string srcs = upper(tok[2]);
            for (std::size_t i = 3; i < tok.size(); ++i) { srcs.push_back(' '); srcs += upper(tok[i]); }
            if (srcs.rfind("OFFSET ", 0) == 0) {
                const std::string sym = upper(trim(srcs.substr(7)));
                em.pendingDataFixups.push_back({em.code.size() + 1u, sym});
                if (!em.emitMovRegImm(dst, 0)) return fail(em.lastError);
                continue;
            }
            const Reg src = parseReg(tok[2].c_str());
            if (src != Reg::NONE) {
                if (!em.emitMovRegReg(dst, src)) return fail(em.lastError);
                continue;
            }
            std::uint32_t imm = 0;
            if (tok[2].size() >= 3 && tok[2].front() == '\'' && tok[2].back() == '\'')
                imm = static_cast<unsigned char>(tok[2][1]);
            else if (!em.resolveImm(tok[2], imm)) return fail("MOV bad immediate");
            if (!em.emitMovRegImm(dst, imm)) return fail(em.lastError);
            continue;
        }
        if (op == "INT") {
            std::uint32_t n = 0;
            if (tok.size() < 2 || !em.resolveImm(tok[1], n)) return fail("INT needs vector");
            if (!em.emitInt(n)) return fail(em.lastError);
            continue;
        }
        if (op == "JMP") {
            if (tok.size() < 2) return fail("JMP needs label");
            if (!em.emitJmp(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "CALL") {
            if (tok.size() < 2) return fail("CALL needs label");
            if (!em.emitCall(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "RET") { em.emitRet(); continue; }
        if (op == "NOP") { em.emit8(0x90u); continue; }
        if (op == "ADD") {
            if (tok.size() < 3) return fail("ADD needs operands");
            const Reg dst = parseReg(tok[1].c_str());
            const Reg src = parseReg(tok[2].c_str());
            if (dst == Reg::AX && src == Reg::BX) { em.emitAddAxBx(); continue; }
            std::uint32_t imm = 0;
            if (!em.resolveImm(tok[2], imm)) return fail("ADD bad imm");
            if (!em.emitAddRegImm(dst, imm)) return fail(em.lastError);
            continue;
        }
        if (op == "JE" || op == "JZ") {
            if (tok.size() < 2) return fail("JE needs label");
            if (!em.emitJe(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "JNE" || op == "JNZ") {
            if (tok.size() < 2) return fail("JNE needs label");
            if (!em.emitJne(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "INC") {
            if (tok.size() < 2) return fail("INC needs reg");
            if (!em.emitIncReg16(parseReg(tok[1].c_str()))) return fail(em.lastError);
            continue;
        }
        if (op == "DEC") {
            if (tok.size() < 2) return fail("DEC needs reg");
            if (!em.emitDecReg16(parseReg(tok[1].c_str()))) return fail(em.lastError);
            continue;
        }
        if (op == "PUSH") {
            if (tok.size() < 2) return fail("PUSH needs reg");
            if (!em.emitPushReg16(parseReg(tok[1].c_str()))) return fail(em.lastError);
            continue;
        }
        if (op == "POP") {
            if (tok.size() < 2) return fail("POP needs reg");
            if (!em.emitPopReg16(parseReg(tok[1].c_str()))) return fail(em.lastError);
            continue;
        }
        if (op == "XOR") {
            if (tok.size() >= 3 && upper(tok[1]) == "AX" && upper(tok[2]) == "AX") {
                em.emitXorAxAx();
                continue;
            }
            return fail("XOR AX,AX only");
        }
        if (op == "SUB") {
            if (tok.size() >= 3 && upper(tok[1]) == "AX" && upper(tok[2]) == "BX") {
                em.emitSubAxBx();
                continue;
            }
            return fail("SUB AX,BX only");
        }
        if (op == "CMP") {
            if (tok.size() < 3) return fail("CMP needs operands");
            const Reg a = parseReg(tok[1].c_str());
            const Reg b = parseReg(tok[2].c_str());
            if (a != Reg::NONE && b != Reg::NONE) {
                if (!em.emitCmpRegReg(a, b)) return fail(em.lastError);
                continue;
            }
            if (parseReg(tok[1].c_str()) != Reg::AX) return fail("CMP AX, imm only");
            std::uint32_t imm = 0;
            if (!em.resolveImm(tok[2], imm)) return fail("CMP bad imm");
            em.emitCmpAxImm(imm);
            continue;
        }
        if (op == "JB" || op == "JC") {
            if (tok.size() < 2) return fail("JB needs label");
            if (!em.emitJb(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "JA" || op == "JNBE") {
            if (tok.size() < 2) return fail("JA needs label");
            if (!em.emitJa(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "JBE" || op == "JNA") {
            if (tok.size() < 2) return fail("JBE needs label");
            if (!em.emitJbe(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "JAE" || op == "JNB" || op == "JNC") {
            if (tok.size() < 2) return fail("JAE needs label");
            if (!em.emitJae(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "JL") {
            if (tok.size() < 2) return fail("JL needs label");
            if (!em.emitJb(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "JG") {
            if (tok.size() < 2) return fail("JG needs label");
            if (!em.emitJa(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "JLE") {
            if (tok.size() < 2) return fail("JLE needs label");
            if (!em.emitJbe(upper(tok[1]))) return fail(em.lastError);
            continue;
        }
        if (op == "JGE") {
            if (tok.size() < 2) return fail("JGE needs label");
            if (!em.emitJae(upper(tok[1]))) return fail(em.lastError);
            continue;
        }

        return fail("unsupported opcode: " + op);
    }

    if (!em.packObject(out)) return fail(em.lastError);
    return out;
}

inline Result assemblePath(const char* dosPath) {
    std::vector<std::uint8_t> src;
    if (!FieldAmmoFat::readFile(dosPath, src) || src.empty())
        return Result{false, "cannot read source", {}};
    return assembleSource(reinterpret_cast<const char*>(src.data()), src.size());
}

} // namespace FieldAmmoAsm