#pragma once

// Intel 8237 DMA controller — channels 1 (8-bit) and 5 (16-bit) for SB16.

#include <cstdint>
#include <cstring>

namespace FieldDma {

struct Channel {
    std::uint32_t page  = 0;
    std::uint16_t addr  = 0;
    std::uint16_t count = 0;
    std::uint8_t  mode  = 0;
};

inline Channel ch[8]{};
inline std::uint8_t flipFlop = 0;

inline void reset() noexcept {
    for (auto& c : ch) c = Channel{};
    flipFlop = 0;
}

inline std::uint32_t physAddr8(int channel) noexcept {
    if (channel < 0 || channel >= 8) return 0;
    return (static_cast<std::uint32_t>(ch[channel].page) << 16)
         | static_cast<std::uint32_t>(ch[channel].addr);
}

inline std::uint16_t transferBytes8(int channel) noexcept {
    if (channel < 0 || channel >= 8) return 0;
    return static_cast<std::uint16_t>(ch[channel].count + 1u);
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == 0x0Cu) { flipFlop = 0; return; }
    if (port <= 0x0Fu) {
        const int chNum = (port >> 1) & 3;
        if (port & 1u) {
            if (!flipFlop) ch[chNum].count = static_cast<std::uint16_t>((ch[chNum].count & 0xFF00u) | val);
            else ch[chNum].count = static_cast<std::uint16_t>((ch[chNum].count & 0x00FFu) | (static_cast<std::uint16_t>(val) << 8));
            flipFlop ^= 1u;
        } else {
            if (!flipFlop) ch[chNum].addr = static_cast<std::uint16_t>((ch[chNum].addr & 0xFF00u) | val);
            else ch[chNum].addr = static_cast<std::uint16_t>((ch[chNum].addr & 0x00FFu) | (static_cast<std::uint16_t>(val) << 8));
            flipFlop ^= 1u;
        }
        return;
    }
    if (port == 0x0Bu) { ch[0].mode = val; return; }
    if (port >= 0x80u && port <= 0x8Fu)
        ch[(port - 0x80u) >> 1].page = val;
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port <= 0x0Fu) {
        const int chNum = (port >> 1) & 3;
        if (port & 1u)
            return static_cast<std::uint8_t>((flipFlop ? ch[chNum].count >> 8 : ch[chNum].count) & 0xFFu);
        return static_cast<std::uint8_t>((flipFlop ? ch[chNum].addr >> 8 : ch[chNum].addr) & 0xFFu);
    }
    return 0xFFu;
}

} // namespace FieldDma