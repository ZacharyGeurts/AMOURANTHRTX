#pragma once

// RTX Desktop I/O — serial, parallel, USB, Bluetooth (out-of-box desktop rack).

#include "FieldInput.hpp"

#include <cstdint>
#include <cstring>

namespace FieldPorts {

constexpr std::uint16_t COM1_BASE = 0x3F8u;
constexpr std::uint16_t COM2_BASE = 0x2F8u;
constexpr std::uint16_t COM3_BASE = 0x3E8u;
constexpr std::uint16_t COM4_BASE = 0x2E8u;
constexpr std::uint16_t LPT1_BASE = 0x378u;
constexpr std::uint16_t LPT2_BASE = 0x278u;
constexpr std::uint16_t LPT3_BASE = 0x3BCu;

struct Uart {
    std::uint8_t rbr = 0;
    std::uint8_t thr = 0;
    std::uint8_t ier = 0;
    std::uint8_t iir = 1;
    std::uint8_t lcr = 3;
    std::uint8_t mcr = 3;
    std::uint8_t lsr = 0x60; // THRE + TEMT
    std::uint8_t msr = 0x10;
    std::uint8_t scr = 0;
    bool enabled = true;
};

struct Parallel {
    std::uint8_t data = 0;
    std::uint8_t status = 0x90; // not busy, ack, online
    std::uint8_t control = 0x0C;
    bool enabled = true;
};

struct UsbHost {
    int deviceCount = 0;
    int hidCount = 0;
    int storageCount = 0;
    bool gamepad = false;
};

struct BtHost {
    bool adapterPresent = true;
    int paired = 0;
    int connected = 0;
};

inline Uart com[4]{};
inline Parallel lpt[3]{};
inline UsbHost usb{};
inline BtHost bt{};

inline int comIndex(std::uint16_t port) noexcept {
    if (port >= COM1_BASE && port < COM1_BASE + 8) return 0;
    if (port >= COM2_BASE && port < COM2_BASE + 8) return 1;
    if (port >= COM3_BASE && port < COM3_BASE + 8) return 2;
    if (port >= COM4_BASE && port < COM4_BASE + 8) return 3;
    return -1;
}

inline int lptIndex(std::uint16_t port) noexcept {
    if (port >= LPT1_BASE && port < LPT1_BASE + 3) return 0;
    if (port >= LPT2_BASE && port < LPT2_BASE + 3) return 1;
    if (port >= LPT3_BASE && port < LPT3_BASE + 3) return 2;
    return -1;
}

inline void refreshHost() noexcept {
    usb = {};
    bt.adapterPresent = true;
    if (FieldInput::state.gamepad.connected) {
        usb.deviceCount = 1;
        usb.hidCount = 1;
        usb.gamepad = true;
        bt.connected = 1;
        bt.paired = 1;
    }
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    const int ci = comIndex(port);
    if (ci >= 0 && com[static_cast<std::size_t>(ci)].enabled) {
        const std::uint16_t off = port & 7u;
        auto& u = com[static_cast<std::size_t>(ci)];
        switch (off) {
        case 0: u.thr = val; u.rbr = val; break;
        case 1: u.ier = val; break;
        case 3: u.lcr = val; break;
        case 4: u.mcr = val; break;
        case 7: u.scr = val; break;
        default: break;
        }
        return;
    }
    const int li = lptIndex(port);
    if (li >= 0 && lpt[static_cast<std::size_t>(li)].enabled) {
        const std::uint16_t off = port - (li == 0 ? LPT1_BASE : (li == 1 ? LPT2_BASE : LPT3_BASE));
        auto& p = lpt[static_cast<std::size_t>(li)];
        if (off == 0) p.data = val;
        else if (off == 2) p.control = val;
        return;
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    refreshHost();
    const int ci = comIndex(port);
    if (ci >= 0 && com[static_cast<std::size_t>(ci)].enabled) {
        const std::uint16_t off = port & 7u;
        const auto& u = com[static_cast<std::size_t>(ci)];
        switch (off) {
        case 0: return u.rbr;
        case 1: return u.ier;
        case 2: return u.iir;
        case 3: return u.lcr;
        case 4: return u.mcr;
        case 5: return u.lsr;
        case 6: return u.msr;
        case 7: return u.scr;
        default: break;
        }
        return 0;
    }
    const int li = lptIndex(port);
    if (li >= 0 && lpt[static_cast<std::size_t>(li)].enabled) {
        const std::uint16_t off = port - (li == 0 ? LPT1_BASE : (li == 1 ? LPT2_BASE : LPT3_BASE));
        const auto& p = lpt[static_cast<std::size_t>(li)];
        if (off == 0) return p.data;
        if (off == 1) return p.status;
        if (off == 2) return p.control;
        return 0;
    }
    return 0xFFu;
}

inline void formatPortsBlock(char* buf, std::size_t cap) noexcept {
    refreshHost();
    std::snprintf(buf, cap,
        "\r\nRTX Desktop I/O rack\r\n"
        "  Serial: COM1 %04X COM2 %04X COM3 %04X COM4 %04X [8250 UART ready]\r\n"
        "  Parallel: LPT1 %04X LPT2 %04X LPT3 %04X [IEEE1284 ready]\r\n"
        "  USB host: %d device(s) HID=%d storage=%d gamepad=%s\r\n"
        "  Bluetooth: adapter %s paired=%d connected=%d\r\n",
        COM1_BASE, COM2_BASE, COM3_BASE, COM4_BASE,
        LPT1_BASE, LPT2_BASE, LPT3_BASE,
        usb.deviceCount, usb.hidCount, usb.storageCount, usb.gamepad ? "yes" : "no",
        bt.adapterPresent ? "ON" : "off", bt.paired, bt.connected);
}

} // namespace FieldPorts