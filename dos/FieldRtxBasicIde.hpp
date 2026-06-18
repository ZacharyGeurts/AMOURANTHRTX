#pragma once

// QBASIC shell entry — forwards to AmmoCode master IDE (Turbo Pascal style).

#include "FieldAmmoCode.hpp"

namespace FieldRtxBasic {

inline void leaveBasic(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    if (FieldAmmoCode::active) {
        FieldAmmoCode::closeIde(ram, echo, nl);
        return;
    }
    active = false;
    FieldRtxGui::initTextMode(ram);
    shellPrint(ram, echo, nl, "\r\nReturned to COMMAND.COM\r\n");
}

inline void startIde(std::uint8_t* ram) noexcept {
    FieldAmmoCode::open(ram, FieldAmmoCode::Lang::Basic, "C:\\QBASIC\\UNTITLED.BAS");
}

inline void printBasicShellHelp(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldAmmoCode::cmdShell(ram, echo, nl, {"AMMOCODE", "/HELP"});
}

inline void cmdQbasic(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                      const std::vector<std::string>& args) {
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        printBasicShellHelp(ram, echo, nl);
        return;
    }
    if (args.size() >= 2
        && (istreq(args[1].c_str(), "/RUN") || istreq(args[1].c_str(), "RUN"))) {
        std::string p = args.size() >= 3 ? args[2] : "";
        if (p.empty()) {
            shellPrint(ram, echo, nl, "\r\nUsage: QBASIC /RUN file.bas\r\n");
            return;
        }
        if (p.find(':') == std::string::npos && p.find('\\') == std::string::npos)
            p = "C:\\QBASIC\\" + p;
        if (p.find('.') == std::string::npos) p += ".BAS";
        FieldAmmoCode::open(ram, FieldAmmoCode::Lang::Basic, upper(p).c_str());
        FieldAmmoCode::dispatch(FieldAmmoCode::A_RUN, ram, echo, nl);
        return;
    }
    if (args.size() >= 2 && istreq(args[1].c_str(), "LOAD")) {
        std::string p = args.size() >= 3 ? args[2] : "C:\\QBASIC\\HELLO.BAS";
        FieldAmmoCode::open(ram, FieldAmmoCode::Lang::Basic, upper(p).c_str());
        return;
    }
    FieldAmmoCode::open(ram, FieldAmmoCode::Lang::Basic, "C:\\QBASIC\\UNTITLED.BAS");
}

inline bool handleIdeKey(std::uint8_t* ram, std::uint16_t key,
                           EchoFn echo, NewlineFn nl) noexcept {
    if (!FieldAmmoCode::active) return false;
    FieldAmmoCode::pump(ram, key, true, echo, nl);
    return true;
}

inline bool handleBasicKey(std::uint8_t* /*ram*/, std::uint16_t) noexcept { return false; }

inline bool handleBasicLine(std::uint8_t* /*ram*/, const char*, EchoFn, NewlineFn) {
    return FieldAmmoCode::active;
}

} // namespace FieldRtxBasic