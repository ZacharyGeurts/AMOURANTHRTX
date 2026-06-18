#pragma once

// RTX QBASIC core interpreter — used by Golden Era IDE (FieldRtxBasicIde.hpp).

#include "FieldAmmoVfs.hpp"
#include "FieldRtxHelp.hpp"
#include "FieldVga.hpp"

#include <cctype>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace FieldRtxBasic {

inline bool active = false;
inline std::vector<std::string> program;
inline std::map<std::string, double> vars;

using EchoFn = std::function<void(std::uint8_t*, char)>;
using NewlineFn = std::function<void(std::uint8_t*)>;

inline bool istreq(const char* a, const char* b) noexcept {
    while (*a && *b) {
        if (std::tolower(static_cast<unsigned char>(*a))
            != std::tolower(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline void shellPrint(std::uint8_t* ram, EchoFn echo, NewlineFn nl, const char* text) noexcept {
    for (const char* p = text; *p; ++p) {
        if (*p == '\r') continue;
        if (*p == '\n') nl(ram);
        else echo(ram, *p);
    }
}

inline void leaveBasic(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept;

inline std::string upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

inline void trim(std::string& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
}

inline double evalExpr(const std::string& expr) {
    std::string e = expr;
    for (const auto& kv : vars) {
        std::size_t pos = 0;
        const std::string& key = kv.first;
        while ((pos = e.find(key, pos)) != std::string::npos) {
            const char before = pos > 0 ? e[pos - 1] : ' ';
            const char after = pos + key.size() < e.size() ? e[pos + key.size()] : ' ';
            if (std::isalpha(static_cast<unsigned char>(before))
                || std::isalnum(static_cast<unsigned char>(after))) {
                ++pos;
                continue;
            }
            e.replace(pos, key.size(), std::to_string(kv.second));
            pos += 8;
        }
    }
    double v = 0.0;
    if (std::sscanf(e.c_str(), "%lf", &v) == 1) return v;
    return 0.0;
}

inline bool loadBas(const char* path, std::vector<std::string>& out) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(path, data) || data.empty()) return false;
    out.clear();
    std::string line;
    for (std::uint8_t b : data) {
        if (b == '\r') continue;
        if (b == '\n') {
            trim(line);
            if (!line.empty()) out.push_back(line);
            line.clear();
        } else if (b >= 32 && b < 127)
            line += static_cast<char>(b);
    }
    trim(line);
    if (!line.empty()) out.push_back(line);
    return true;
}

inline bool saveBas(const char* path, const std::vector<std::string>& lines) {
    std::string body;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        body += lines[i];
        if (i + 1 < lines.size()) body += "\r\n";
    }
    return FieldAmmoVfs::writePath(path,
        reinterpret_cast<const std::uint8_t*>(body.data()), body.size());
}

inline void immPrint(std::uint8_t* ram, EchoFn echo, NewlineFn nl, const std::string& text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == ';') { ++i; continue; }
        if (text[i] == '"') {
            ++i;
            while (i < text.size() && text[i] != '"') {
                echo(ram, text[i]);
                ++i;
            }
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(text[i]))) {
            std::size_t j = i;
            while (j < text.size() && (std::isalnum(static_cast<unsigned char>(text[j]))
                   || text[j] == '_')) ++j;
            const std::string id = upper(text.substr(i, j - i));
            auto it = vars.find(id);
            if (it != vars.end()) {
                char buf[32];
                std::snprintf(buf, sizeof buf, "%g", it->second);
                for (char* p = buf; *p; ++p) echo(ram, *p);
            }
            i = j - 1;
            continue;
        }
        if (text[i] != ' ') echo(ram, text[i]);
    }
    nl(ram);
}

inline bool runLine(std::uint8_t* ram, EchoFn echo, NewlineFn nl, const std::string& raw) {
    std::string line = upper(raw);
    trim(line);
    if (line.empty() || line[0] == '\'') return true;
    if (line.rfind("PRINT ", 0) == 0) {
        immPrint(ram, echo, nl, raw.substr(6));
        return true;
    }
    if (line == "CLS") return true;
    if (line.rfind("LET ", 0) == 0) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) return true;
        std::string name = line.substr(4, eq - 4);
        trim(name);
        vars[upper(name)] = evalExpr(raw.substr(eq + 1));
        return true;
    }
    if (line == "END" || line == "SYSTEM") {
        leaveBasic(ram, echo, nl);
        return false;
    }
    if (line.rfind("REM", 0) == 0) return true;
    return true;
}

inline int compileCheck(std::vector<std::string>& errors) {
    errors.clear();
    int nextDepth = 0;
    for (std::size_t i = 0; i < program.size(); ++i) {
        const std::string u = upper(program[i]);
        int quotes = 0;
        for (char c : program[i]) {
            if (c == '"') ++quotes;
        }
        if (quotes % 2 != 0)
            errors.push_back("Line " + std::to_string(i + 1) + ": unmatched quote");
        if (u.find("FOR ") == 0) ++nextDepth;
        if (u.find("NEXT") == 0) {
            if (nextDepth > 0) --nextDepth;
            else errors.push_back("Line " + std::to_string(i + 1) + ": NEXT without FOR");
        }
        if (u.find("PRINT") == 0 && u.size() > 5 && u[5] != ' ' && u != "PRINT")
            errors.push_back("Line " + std::to_string(i + 1) + ": PRINT syntax");
    }
    if (nextDepth > 0) errors.push_back("Missing NEXT for FOR loop");
    return static_cast<int>(errors.size());
}

inline void listProgram(std::uint8_t* ram, EchoFn echo, NewlineFn nl) {
    int n = 1;
    for (const auto& ln : program) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "%4d %s\r\n", n * 10, ln.c_str());
        shellPrint(ram, echo, nl, buf);
    }
}

inline bool runProgram(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       int startPc, bool oneStep, int& pc) {
    for (std::size_t i = static_cast<std::size_t>(startPc); i < program.size(); ++i) {
        pc = static_cast<int>(i);
        if (!runLine(ram, echo, nl, program[i])) return false;
        if (oneStep) {
            pc = static_cast<int>(i) + 1;
            return true;
        }
    }
    pc = static_cast<int>(program.size());
    return true;
}

} // namespace FieldRtxBasic