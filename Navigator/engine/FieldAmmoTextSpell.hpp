#pragma once

// AmmoText spellcheck — embedded open word list (common English + RTX/DOS terms).

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoTextSpell {

inline bool ciEq(const char* a, const std::string& w) noexcept {
    for (std::size_t i = 0; i < w.size(); ++i) {
        if (!a[i]) return false;
        if (std::tolower(static_cast<unsigned char>(a[i]))
            != std::tolower(static_cast<unsigned char>(w[i])))
            return false;
    }
    return a[w.size()] == '\0';
}

static const char* kWords[] = {
    "a","about","above","add","after","all","also","am","an","and","any","are","as","at",
    "audio","auto","autoexec","bat","be","been","before","being","below","between","both",
    "buffer","but","by","call","can","cd","cfg","class","cmd","code","com","command","config",
    "could","cpu","data","device","did","disk","do","dos","down","drive","driver","each",
    "edit","else","end","enum","error","exe","file","find","for","format","from","function",
    "get","go","gpu","had","has","have","he","help","her","here","him","his","how","if","in",
    "inc","include","ini","int","into","is","it","its","jmp","journal","just","kernel","key",
    "layer","let","line","list","load","long","make","map","may","me","mem","memory","menu",
    "might","mov","must","my","name","new","no","not","now","of","off","on","one","only","or",
    "other","our","out","over","own","path","print","program","ram","reg","registry","return",
    "root","run","same","save","say","see","set","she","shell","should","so","some","such",
    "sys","system","than","that","the","their","them","then","there","these","they","this",
    "those","through","time","to","too","tools","under","up","use","used","using","value",
    "var","very","vga","void","was","we","were","what","when","where","which","while","who",
    "will","with","word","would","write","yes","you","your","zero",
    "ammofat","ammosys","ammotext","amouranth","basic","batch","bios","boot","byte","config",
    "device","drives","edit","field","floppy","golden","help","himem","host","ioctl","layer",
    "mscdex","notepad","opl","pascal","qbasic","registry","rtx","rtxsb","rtxvga","sb16","spell",
    "supercore","turbo","vesa","vscodium","word","wrap",
    nullptr
};

inline bool known(const std::string& word) noexcept {
    if (word.size() < 2) return true;
    bool allDigit = true;
    for (char c : word) {
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.' && c != '-')
            allDigit = false;
    }
    if (allDigit) return true;
    for (int i = 0; kWords[i]; ++i)
        if (ciEq(kWords[i], word)) return true;
    return false;
}

inline bool wordBadAt(const std::string& ln, int col) noexcept {
    if (col < 0 || col >= static_cast<int>(ln.size())) return false;
    int ws = col, we = col;
    while (ws > 0) {
        const char ch = ln[static_cast<std::size_t>(ws - 1)];
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') break;
        --ws;
    }
    while (we < static_cast<int>(ln.size())) {
        const char ch = ln[static_cast<std::size_t>(we)];
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') break;
        ++we;
    }
    if (we <= ws + 1) return false;
    return !known(ln.substr(static_cast<std::size_t>(ws), static_cast<std::size_t>(we - ws)));
}

inline int countBadInLine(const std::string& ln) noexcept {
    int n = 0;
    for (int c = 0; c < static_cast<int>(ln.size()); ++c) {
        if (wordBadAt(ln, c)) {
            ++n;
            int we = c;
            while (we < static_cast<int>(ln.size()) && std::isalnum(static_cast<unsigned char>(ln[static_cast<std::size_t>(we)])))
                ++we;
            c = we;
        }
    }
    return n;
}

} // namespace FieldAmmoTextSpell