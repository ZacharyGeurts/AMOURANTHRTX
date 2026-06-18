#pragma once

// AMMOCC v4 — tiny C compiler: int vars, +/-/ *, if/while, rtx_puts/rtx_out, return.

#include "FieldAmmoAsm.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoInc.hpp"
#include "FieldDos.hpp"

#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace FieldAmmoCc {

struct Result {
    bool ok = false;
    std::string error;
    FieldAmmoAsm::Result asmResult;
};

enum class TokKind : std::uint8_t {
    End, Ident, Number, String,
    KwInt, KwIf, KwElse, KwWhile, KwReturn, KwVoid,
    LParen, RParen, LBrace, RBrace, Semi, Comma,
    Assign, Plus, Minus, Star, Slash, Lt, Gt, Le, Ge, Eq, Ne,
};

struct Tok {
    TokKind kind = TokKind::End;
    std::string text;
    int line = 1;
    int value = 0;
};

struct Lexer {
    const char* src = nullptr;
    std::size_t len = 0;
    std::size_t pos = 0;
    int line = 1;
    std::string err;

    explicit Lexer(const char* s, std::size_t n) : src(s), len(n) {}

    char peek(std::size_t off = 0) const {
        const std::size_t i = pos + off;
        return i < len ? src[i] : '\0';
    }

    char get() {
        if (pos >= len) return '\0';
        char c = src[pos++];
        if (c == '\n') ++line;
        return c;
    }

    void skipSpace() {
        while (pos < len) {
            const char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { get(); continue; }
            if (c == '/' && peek(1) == '/') {
                while (get() && peek() != '\n') {}
                continue;
            }
            if (c == '/' && peek(1) == '*') {
                get(); get();
                while (get()) {
                    if (peek(-1) == '*' && peek() == '/') { get(); break; }
                }
                continue;
            }
            break;
        }
    }

    Tok next() {
        skipSpace();
        Tok t;
        t.line = line;
        if (pos >= len) { t.kind = TokKind::End; return t; }

        char c = peek();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string id;
            while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')
                id += get();
            t.text = id;
            if (id == "int") t.kind = TokKind::KwInt;
            else if (id == "if") t.kind = TokKind::KwIf;
            else if (id == "else") t.kind = TokKind::KwElse;
            else if (id == "while") t.kind = TokKind::KwWhile;
            else if (id == "return") t.kind = TokKind::KwReturn;
            else if (id == "void") t.kind = TokKind::KwVoid;
            else if (id == "rtx_puts" || id == "puts") t.kind = TokKind::Ident;
            else if (id == "rtx_out") t.kind = TokKind::Ident;
            else t.kind = TokKind::Ident;
            return t;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            int v = 0;
            while (std::isdigit(static_cast<unsigned char>(peek())))
                v = v * 10 + (get() - '0');
            t.kind = TokKind::Number;
            t.value = v;
            return t;
        }
        if (c == '"') {
            get();
            while (peek() && peek() != '"') {
                if (peek() == '\\' && peek(1)) {
                    get();
                    const char esc = get();
                    if (esc == 'n') t.text += "\r\n";
                    else if (esc == 'r') t.text += "\r";
                    else if (esc == 't') t.text += "\t";
                    else t.text += esc;
                } else
                    t.text += get();
            }
            if (peek() == '"') get();
            t.kind = TokKind::String;
            return t;
        }
        if (c == '\'') {
            get();
            char ch = get();
            if (peek() == '\'') get();
            t.kind = TokKind::Number;
            t.value = static_cast<unsigned char>(ch);
            return t;
        }

        auto two = [&](char a, char b, TokKind k) -> bool {
            if (peek() == a && peek(1) == b) { get(); get(); t.kind = k; return true; }
            return false;
        };
        if (two('<', '=', TokKind::Le)) return t;
        if (two('>', '=', TokKind::Ge)) return t;
        if (two('=', '=', TokKind::Eq)) return t;
        if (two('!', '=', TokKind::Ne)) return t;

        get();
        switch (c) {
        case '(': t.kind = TokKind::LParen; break;
        case ')': t.kind = TokKind::RParen; break;
        case '{': t.kind = TokKind::LBrace; break;
        case '}': t.kind = TokKind::RBrace; break;
        case ';': t.kind = TokKind::Semi; break;
        case ',': t.kind = TokKind::Comma; break;
        case '=': t.kind = TokKind::Assign; break;
        case '+': t.kind = TokKind::Plus; break;
        case '-': t.kind = TokKind::Minus; break;
        case '*': t.kind = TokKind::Star; break;
        case '/': t.kind = TokKind::Slash; break;
        case '<': t.kind = TokKind::Lt; break;
        case '>': t.kind = TokKind::Gt; break;
        default: err = "bad char"; t.kind = TokKind::End; break;
        }
        return t;
    }
};

struct Codegen {
    std::string asm_;
    std::unordered_map<std::string, std::string> varReg;
    int nextMsg = 0;
    int nextVar = 0;
    std::vector<std::string> dataLines;
    std::string lastError;

    static const char* kPool[4];

    std::string lbl() { return "L" + std::to_string(nextMsg++); }

    std::string allocVar(const std::string& name) {
        if (varReg.count(name)) return varReg[name];
        if (nextVar >= 4) { lastError = "too many int vars (max 4)"; return {}; }
        varReg[name] = kPool[nextVar++];
        return varReg[name];
    }

    void emitPuts(const std::string& msg) {
        const std::string sym = "msg" + std::to_string(nextMsg++);
        asm_ += "    mov ah,9\n    mov dx,offset " + sym + "\n    int 21h\n";
        std::string db = sym + " db ";
        bool first = true;
        for (char c : msg) {
            if (!first) db += ',';
            first = false;
            if (c == '$') db += "36";
            else if (c >= 32 && c < 127) db += std::string("'") + c + "'";
            else db += std::to_string(static_cast<unsigned char>(c));
        }
        if (msg.empty() || msg.back() != '$') { if (!first) db += ','; db += "36"; }
        dataLines.push_back(db);
    }

    void emitOut(int ch) {
        asm_ += "    mov ah,2\n    mov dl," + std::to_string(ch & 0xFF) + "\n    int 21h\n";
    }

    bool loadVar(const std::string& name) {
        const auto it = varReg.find(name);
        if (it == varReg.end()) { lastError = "undefined: " + name; return false; }
        asm_ += "    mov ax," + it->second + "\n";
        return true;
    }

    bool storeVar(const std::string& name) {
        const auto it = varReg.find(name);
        if (it == varReg.end()) { lastError = "undefined: " + name; return false; }
        asm_ += "    mov " + it->second + ",ax\n";
        return true;
    }

    bool emitTerm(Lexer& lx, Tok& cur) {
        if (cur.kind == TokKind::Number) {
            asm_ += "    mov ax," + std::to_string(cur.value) + "\n";
            cur = lx.next();
            return true;
        }
        if (cur.kind == TokKind::Ident) {
            if (!loadVar(cur.text)) return false;
            cur = lx.next();
            return true;
        }
        if (cur.kind == TokKind::LParen) {
            cur = lx.next();
            if (!emitExpr(lx, cur)) return false;
            if (cur.kind != TokKind::RParen) { lastError = "expected )"; return false; }
            cur = lx.next();
            return true;
        }
        lastError = "expected expression";
        return false;
    }

    bool emitExpr(Lexer& lx, Tok& cur) {
        if (!emitTerm(lx, cur)) return false;
        while (cur.kind == TokKind::Plus || cur.kind == TokKind::Minus
               || cur.kind == TokKind::Star || cur.kind == TokKind::Slash) {
            const TokKind op = cur.kind;
            cur = lx.next();
            asm_ += "    push ax\n";
            if (!emitTerm(lx, cur)) return false;
            if (op == TokKind::Plus) asm_ += "    pop bx\n    add ax,bx\n";
            else if (op == TokKind::Minus) asm_ += "    pop bx\n    sub ax,bx\n";
            else if (op == TokKind::Star) {
                const std::string n = std::to_string(nextMsg++);
                asm_ += "    pop bx\n    mov cx,ax\n    mov ax,0\n";
                asm_ += "Lmul" + n + ":\n    dec cx\n    je Lmul" + n + "d\n";
                asm_ += "    add ax,bx\n    jmp Lmul" + n + "\n";
                asm_ += "Lmul" + n + "d:\n";
            } else {
                const std::string n = std::to_string(nextMsg++);
                asm_ += "    pop bx\n    mov cx,0\n";
                asm_ += "Ldiv" + n + ":\n    cmp ax,bx\n    jb Ldiv" + n + "d\n";
                asm_ += "    sub ax,bx\n    inc cx\n    jmp Ldiv" + n + "\n";
                asm_ += "Ldiv" + n + "d:\n    mov ax,cx\n";
            }
        }
        return true;
    }

    bool emitCompare(Lexer& lx, Tok& cur, const std::string& trueLbl, const std::string& falseLbl) {
        if (!emitExpr(lx, cur)) return false;
        asm_ += "    push ax\n";
        if (cur.kind != TokKind::Lt && cur.kind != TokKind::Gt
            && cur.kind != TokKind::Le && cur.kind != TokKind::Ge
            && cur.kind != TokKind::Eq && cur.kind != TokKind::Ne) {
            lastError = "expected compare";
            return false;
        }
        const TokKind op = cur.kind;
        cur = lx.next();
        if (!emitExpr(lx, cur)) return false;
        asm_ += "    pop bx\n    cmp bx,ax\n";
        if (op == TokKind::Lt) asm_ += "    jb " + trueLbl + "\n    jmp " + falseLbl + "\n";
        else if (op == TokKind::Gt) asm_ += "    ja " + trueLbl + "\n    jmp " + falseLbl + "\n";
        else if (op == TokKind::Le) asm_ += "    jbe " + trueLbl + "\n    jmp " + falseLbl + "\n";
        else if (op == TokKind::Ge) asm_ += "    jae " + trueLbl + "\n    jmp " + falseLbl + "\n";
        else if (op == TokKind::Eq) asm_ += "    je " + trueLbl + "\n    jmp " + falseLbl + "\n";
        else asm_ += "    jne " + trueLbl + "\n    jmp " + falseLbl + "\n";
        return true;
    }

    bool emitBlock(Lexer& lx, Tok& cur) {
        if (cur.kind == TokKind::LBrace) cur = lx.next();
        while (cur.kind != TokKind::RBrace && cur.kind != TokKind::End) {
            if (!emitStmt(lx, cur)) return false;
        }
        if (cur.kind == TokKind::RBrace) cur = lx.next();
        return true;
    }

    bool emitStmt(Lexer& lx, Tok& cur) {
        if (cur.kind == TokKind::KwIf) {
            cur = lx.next();
            if (cur.kind != TokKind::LParen) { lastError = "if ("; return false; }
            cur = lx.next();
            const std::string t = lbl(), f = lbl(), e = lbl();
            if (!emitCompare(lx, cur, t, f)) return false;
            if (cur.kind != TokKind::RParen) { lastError = "if )"; return false; }
            cur = lx.next();
            asm_ += t + ":\n";
            if (!emitBlock(lx, cur)) return false;
            asm_ += "    jmp " + e + "\n" + f + ":\n";
            if (cur.kind == TokKind::KwElse) {
                cur = lx.next();
                if (!emitBlock(lx, cur)) return false;
            }
            asm_ += e + ":\n";
            return true;
        }
        if (cur.kind == TokKind::KwWhile) {
            cur = lx.next();
            if (cur.kind != TokKind::LParen) { lastError = "while ("; return false; }
            cur = lx.next();
            const std::string top = lbl(), body = lbl(), out = lbl();
            asm_ += top + ":\n";
            if (!emitCompare(lx, cur, body, out)) return false;
            if (cur.kind != TokKind::RParen) { lastError = "while )"; return false; }
            cur = lx.next();
            asm_ += body + ":\n";
            if (!emitBlock(lx, cur)) return false;
            asm_ += "    jmp " + top + "\n" + out + ":\n";
            return true;
        }
        if (cur.kind == TokKind::KwReturn) {
            cur = lx.next();
            if (cur.kind == TokKind::Number) {
                char buf[32];
                std::snprintf(buf, sizeof buf, "    mov ax,%04Xh\n    int 21h\n",
                    0x4C00 + (cur.value & 0xFF));
                asm_ += buf;
                cur = lx.next();
            } else {
                asm_ += "    mov ax,4C00h\n    int 21h\n";
            }
            if (cur.kind == TokKind::Semi) cur = lx.next();
            return true;
        }
        if (cur.kind == TokKind::Ident && cur.text == "rtx_puts") {
            cur = lx.next();
            if (cur.kind != TokKind::LParen) { lastError = "rtx_puts("; return false; }
            cur = lx.next();
            if (cur.kind != TokKind::String) { lastError = "rtx_puts string"; return false; }
            emitPuts(cur.text);
            cur = lx.next();
            if (cur.kind != TokKind::RParen) { lastError = "rtx_puts )"; return false; }
            cur = lx.next();
            if (cur.kind == TokKind::Semi) cur = lx.next();
            return true;
        }
        if (cur.kind == TokKind::Ident && cur.text == "rtx_out") {
            cur = lx.next();
            if (cur.kind != TokKind::LParen) { lastError = "rtx_out("; return false; }
            cur = lx.next();
            int ch = 0;
            if (cur.kind == TokKind::Number) ch = cur.value;
            else if (cur.kind == TokKind::Ident) {
                if (!loadVar(cur.text)) return false;
                asm_ += "    mov dl,al\n    mov ah,2\n    int 21h\n";
                cur = lx.next();
                if (cur.kind != TokKind::RParen) { lastError = "rtx_out )"; return false; }
                cur = lx.next();
                if (cur.kind == TokKind::Semi) cur = lx.next();
                return true;
            } else { lastError = "rtx_out arg"; return false; }
            emitOut(ch);
            cur = lx.next();
            if (cur.kind != TokKind::RParen) { lastError = "rtx_out )"; return false; }
            cur = lx.next();
            if (cur.kind == TokKind::Semi) cur = lx.next();
            return true;
        }
        if (cur.kind == TokKind::Ident) {
            const std::string name = cur.text;
            cur = lx.next();
            if (cur.kind == TokKind::Assign) {
                cur = lx.next();
                if (!emitExpr(lx, cur)) return false;
                if (!storeVar(name)) return false;
                if (cur.kind == TokKind::Semi) cur = lx.next();
                return true;
            }
            lastError = "bad statement for " + name;
            return false;
        }
        if (cur.kind == TokKind::Semi) { cur = lx.next(); return true; }
        lastError = "unexpected statement";
        return false;
    }

    bool compileBody(Lexer& lx, Tok& cur) {
        while (cur.kind != TokKind::End && cur.kind != TokKind::RBrace) {
            if (cur.kind == TokKind::KwInt) {
                cur = lx.next();
                while (cur.kind == TokKind::Ident) {
                    if (allocVar(cur.text).empty()) return false;
                    cur = lx.next();
                    if (cur.kind == TokKind::Comma) cur = lx.next();
                    else break;
                }
                if (cur.kind == TokKind::Semi) cur = lx.next();
                continue;
            }
            if (!emitStmt(lx, cur)) return false;
        }
        return true;
    }
};

inline Result compileSource(const char* src, std::size_t len) {
    Result r;
    const std::string expanded = FieldAmmoInc::preprocessC(src, len);
    Lexer lx(expanded.c_str(), expanded.size());
    Tok cur = lx.next();
    Codegen cg;

    if (cur.kind == TokKind::KwVoid || cur.kind == TokKind::KwInt) cur = lx.next();
    if (cur.kind == TokKind::Ident) cur = lx.next();
    if (cur.kind == TokKind::LParen) {
        cur = lx.next();
        while (cur.kind != TokKind::RParen && cur.kind != TokKind::End) cur = lx.next();
        if (cur.kind == TokKind::RParen) cur = lx.next();
    }
    if (cur.kind == TokKind::LBrace) cur = lx.next();

    if (!cg.compileBody(lx, cur)) {
        r.error = cg.lastError.empty() ? lx.err : cg.lastError;
        return r;
    }

    std::string asmSrc =
        ".MODEL TINY\n.CODE\nORG 100h\nstart:\n" + cg.asm_;
    if (cg.asm_.find("int 21h") == std::string::npos)
        asmSrc += "    mov ax,4C00h\n    int 21h\n";
    if (!cg.dataLines.empty()) {
        asmSrc += "\n.DATA\n";
        for (const auto& d : cg.dataLines) asmSrc += d + "\n";
    }
    asmSrc += "END start\n";

    r.asmResult = FieldAmmoAsm::assembleSource(asmSrc.c_str(), asmSrc.size());
    r.ok = r.asmResult.ok;
    if (!r.ok) r.error = r.asmResult.error;
    return r;
}

inline Result compilePath(const char* dosPath) {
    std::vector<std::uint8_t> src;
    if (!FieldAmmoFat::readFile(dosPath, src) || src.empty())
        return Result{false, "cannot read C source", {}};
    return compileSource(reinterpret_cast<const char*>(src.data()), src.size());
}

inline const char* Codegen::kPool[4] = {"bx", "cx", "dx", "di"};

} // namespace FieldAmmoCc