#pragma once

// Intel 8254 PIT — channel 0 system tick (IRQ0/INT08), channel 2 speaker (delegates to FieldSpeaker).

#include "FieldPic8259.hpp"
#include "FieldSpeaker.hpp"

#include <cstdint>

namespace FieldPit8254 {

struct Channel {
    std::uint16_t latch = 0;
    std::uint16_t count = 0;
    std::uint8_t  mode  = 0;
    bool          bcd   = false;
    bool          hiLo  = false;
    std::uint64_t accum = 0;
};

inline Channel ch[3]{};
inline std::uint8_t ctrlLatch = 0;
inline std::uint8_t ctrlChannel = 0;
inline std::uint64_t hostTicks = 0;

constexpr std::uint32_t PIT_BASE_HZ = 1193182u;

inline void reset() noexcept {
    ch[0] = {};
    ch[1] = {};
    ch[2] = {};
    ch[0].count = 0xFFFFu;
    ctrlLatch = 0;
    ctrlChannel = 0;
    hostTicks = 0;
    FieldSpeaker::resetPit();
}

inline void writeCount(int c, std::uint16_t val) noexcept {
    if (c < 0 || c > 2) return;
    ch[c].count = val ? val : 0xFFFFu;
    ch[c].accum = 0;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == 0x43u) {
        ctrlChannel = static_cast<std::uint8_t>((val >> 6) & 3u);
        ch[ctrlChannel].mode = static_cast<std::uint8_t>((val >> 1) & 7u);
        ch[ctrlChannel].bcd = (val & 1u) != 0;
        ch[ctrlChannel].hiLo = true;
        if (ctrlChannel == 2u)
            FieldSpeaker::portOut(port, val);
        return;
    }
    if (port == 0x40u || port == 0x41u || port == 0x42u) {
        const int c = static_cast<int>(port - 0x40u);
        auto& channel = ch[c];
        if (channel.hiLo) {
            channel.latch = val;
            channel.hiLo = false;
        } else {
            writeCount(c, static_cast<std::uint16_t>(channel.latch | (static_cast<std::uint16_t>(val) << 8)));
            channel.hiLo = true;
        }
        if (c == 2)
            FieldSpeaker::portOut(port, val);
        return;
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port >= 0x40u && port <= 0x42u) {
        const int c = static_cast<int>(port - 0x40u);
        const std::uint16_t v = ch[c].count;
        return static_cast<std::uint8_t>(ch[c].hiLo ? (v & 0xFFu) : (v >> 8));
    }
    return FieldSpeaker::portIn(port);
}

inline void tick(std::uint64_t cycles) noexcept {
    hostTicks += cycles;
    if (ch[0].count == 0) return;
    const std::uint64_t period = static_cast<std::uint64_t>(ch[0].count);
    ch[0].accum += cycles;
    while (ch[0].accum >= period) {
        ch[0].accum -= period;
        FieldPic8259::raiseIrq(0);
    }
}

} // namespace FieldPit8254