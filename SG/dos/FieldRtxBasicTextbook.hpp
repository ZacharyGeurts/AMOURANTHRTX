#pragma once

// RTX QBASIC Textbook — Golden Era Man+Machine (in-GUI + TYPE C:\QBASIC\TEXTBOOK.TXT)

namespace FieldRtxBasicTextbook {

constexpr int kChapters = 8;

inline const char* const kChapterNames[kChapters] = {
    "1 Getting Started",
    "2 PRINT and Variables",
    "3 Control Flow",
    "4 Subroutines",
    "5 Graphics (RTX)",
    "6 Debug and Step",
    "7 Compile Pipeline",
    "8 Golden Era"
};

// Flat textbook — chapter boundaries in kChapterStart
inline const char* const kLines[] = {
    // Ch1 Getting Started
    " Welcome to RTX QBASIC — the Golden Era IDE on the Field Die.",
    " QBASIC is case-insensitive: print, Print, and PRINT all work.",
    " Menus: File Edit Run Debug Help — press F10 to open the menu bar.",
    " F1 Help  F2 Save  F5 Run  F8 Step  F9 Breakpoint  Shift+F5 Compile",
    " The editor shows line numbers. The cyan pane is the Immediate window.",
    " Type commands at the Ok prompt, or edit program lines above.",
    " SAVE HELLO.BAS writes to C:\\QBASIC\\. LOAD opens from disk.",
    " SYSTEM or File>Exit returns to RTX COMMAND.COM.",
    nullptr, // ch2
    " PRINT displays text and numbers. Strings use double quotes.",
    "   PRINT \"Hello, Golden Era!\"",
    "   PRINT 2 + 3",
    " Semicolon (;) keeps the cursor on the same line:",
    "   PRINT \"A=\"; 10",
    " LET assigns variables (A-Z, names). All math is real numbers.",
    "   LET score = 100",
    "   LET x = (5 + 3) * 2",
    " Variables you never LET default to 0 when printed.",
    nullptr,
    " Control flow — RTX QBASIC supports structured commands:",
    " IF ... THEN ... [ELSE ...]   (single line or block)",
    " FOR i = 1 TO 10 ... NEXT i",
    " WHILE condition ... WEND",
    " GOTO label   GOSUB subroutine   RETURN",
    " Example:",
    "   FOR i = 1 TO 5",
    "     PRINT i",
    "   NEXT i",
    " Line numbers are optional in the IDE; RUN uses source order.",
    nullptr,
    " Subroutines organize code. Use GOSUB/RETURN:",
    "   GOSUB greet",
    "   PRINT \"back\"",
    "   END",
    " greet:",
    "   PRINT \"Man+Machine\"",
    "   RETURN",
    " DEF FNname(x)=expr  — single-line functions (planned).",
    " SHARED vars pass between modules in full QBasic.",
    nullptr,
    " RTX adds GPU Field Die colors to classic QBASIC:",
    " COLOR foreground, background — VGA text attributes.",
    " CLS clears the blue IDE screen.",
    " SCREEN 0 = text (default). SCREEN 13 = 320x200 (ERA WIN31).",
    " LOCATE row, col — move text cursor in output pane.",
    " The viewport layer scales with crisp IBM VGA font + glass panel.",
    nullptr,
    " Debug menu — step through your program like MS QuickBASIC:",
    " F5 Run       — execute until breakpoint or end",
    " F8 Step      — execute one source line",
    " F9 Breakpoint— toggle break on current editor line",
    " Debug>Break All — stop on every line (trace mode)",
    " Watch window shows LET variables after each step.",
    " Red marker = breakpoint. Green highlight = current PC line.",
    " AMMODBG STEP file.com — machine-level step for .COM output.",
    nullptr,
    " Compile pipeline on C: (DevKit):",
    " 1) EDIT or QBASIC — write .BAS source",
    " 2) Shift+F5 Compile — syntax check + emit listing",
    " 3) For ASM: AMMOASM, AMMOLINK, AMMORUN on C:\\QBASIC\\OUT",
    " 4) BUILD — one-shot asm+link in C:\\SAMPLES",
    " COMPILE in IDE validates quotes, LET, PRINT, NEXT balance.",
    " Future: .BAS -> .COM transpile via RTX toolchain.",
    nullptr,
    " Golden Era Man+Machine — forever place.",
    " Human intent meets silicon on the RTX Field Die.",
    " Zachary Geurts MCSE+I — MCSE+I certified shell + IDE.",
    " TYPE GOLDEN.TXT — manifesto on C:",
    " EDIT file — full-screen editor with File/Help menus",
    " AMOURANTHRTX /help — host binary help",
    " Man writes. Machine executes. Together forever.",
    nullptr,
};

inline int chapterStart(int ch) noexcept {
    int idx = 0;
    for (int c = 0; c < ch && c < kChapters; ++c) {
        while (kLines[idx] != nullptr) ++idx;
        ++idx;
    }
    return idx;
}

inline int chapterLineCount(int ch) noexcept {
    const int start = chapterStart(ch);
    int n = 0;
    for (int i = start; kLines[i] != nullptr; ++i) ++n;
    return n;
}

inline int totalPages(int ch, int rowsPerPage) noexcept {
    const int n = chapterLineCount(ch);
    return (n + rowsPerPage - 1) / rowsPerPage;
}

} // namespace FieldRtxBasicTextbook