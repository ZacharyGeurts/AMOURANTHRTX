#pragma once

// IBM PC/AT CMOS RTC + NVRAM — ports 70h/71h, INT 1Ah companion.

#include "FieldPlatform.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>

namespace FieldCmos {

constexpr std::uint16_t PORT_ADDR = 0x0070u;
constexpr std::uint16_t PORT_DATA = 0x0071u;

constexpr std::uint8_t REG_SEC       = 0x00u;
constexpr std::uint8_t REG_SEC_ALRM  = 0x01u;
constexpr std::uint8_t REG_MIN       = 0x02u;
constexpr std::uint8_t REG_MIN_ALRM  = 0x03u;
constexpr std::uint8_t REG_HOUR      = 0x04u;
constexpr std::uint8_t REG_HOUR_ALRM = 0x05u;
constexpr std::uint8_t REG_WEEKDAY   = 0x06u;
constexpr std::uint8_t REG_DAY      = 0x07u;
constexpr std::uint8_t REG_MONTH    = 0x08u;
constexpr std::uint8_t REG_YEAR     = 0x09u;
constexpr std::uint8_t REG_STAT_A   = 0x0Au;
constexpr std::uint8_t REG_STAT_B   = 0x0Bu;
constexpr std::uint8_t REG_STAT_C   = 0x0Cu;
constexpr std::uint8_t REG_STAT_D   = 0x0Du;
constexpr std::uint8_t REG_FLOPPY   = 0x10u;
constexpr std::uint8_t REG_EQUIP    = 0x14u;
constexpr std::uint8_t REG_EXT_LO   = 0x15u;
constexpr std::uint8_t REG_EXT_HI   = 0x16u;
constexpr std::uint8_t REG_CENTURY  = 0x32u;
constexpr std::uint8_t REG_BOOT     = 0x3Fu;

inline std::uint8_t regs[128]{};
inline std::uint8_t addrLatch = 0;
inline bool nmiMasked = false;

inline std::uint8_t toBcd(int v) noexcept {
    return static_cast<std::uint8_t>(((v / 10) << 4) | (v % 10));
}

inline int fromBcd(std::uint8_t b) noexcept {
    return static_cast<int>((b >> 4) & 0x0Fu) * 10 + static_cast<int>(b & 0x0Fu);
}

inline void syncClockFromHost() noexcept {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    regs[REG_SEC]    = toBcd(tm.tm_sec);
    regs[REG_MIN]    = toBcd(tm.tm_min);
    regs[REG_HOUR]   = toBcd(tm.tm_hour);
    regs[REG_WEEKDAY]= toBcd((tm.tm_wday == 0) ? 7 : tm.tm_wday);
    regs[REG_DAY]    = toBcd(tm.tm_mday);
    regs[REG_MONTH]  = toBcd(tm.tm_mon + 1);
    const int year = tm.tm_year + 1900;
    regs[REG_YEAR]   = toBcd(year % 100);
    regs[REG_CENTURY]= toBcd(year / 100);
}

inline void init(bool hdPresent) noexcept {
    for (auto& b : regs) b = 0;

    syncClockFromHost();

    regs[REG_STAT_A] = 0x26u; /* 32.768 kHz divider, update in progress clear */
    regs[REG_STAT_B] = 0x02u; /* 24-hour mode, BCD */
    regs[REG_STAT_C] = 0x00u;
    regs[REG_STAT_D] = 0x80u; /* CMOS battery good + RTC valid */

    regs[REG_FLOPPY] = hdPresent ? 0x48u : 0x40u; /* A:1.44M + C:type47 */
    regs[REG_EQUIP]  = 0x47u; /* FPU + VGA + 2 drives */
    regs[0x12u]      = hdPresent ? 0xF8u : 0x00u; /* HD type C: */

    const std::uint32_t extKb = FieldPlatform::EXTENDED_KB;
    const std::uint32_t ext256k = (extKb + 255u) / 256u;
    regs[REG_EXT_LO] = static_cast<std::uint8_t>(ext256k & 0xFFu);
    regs[REG_EXT_HI] = static_cast<std::uint8_t>((ext256k >> 8) & 0xFFu);

    regs[REG_BOOT] = hdPresent ? 0x02u : 0x01u; /* 2=C: first, 1=A: */
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == PORT_ADDR) {
        addrLatch = static_cast<std::uint8_t>(val & 0x7Fu);
        nmiMasked = (val & 0x80u) != 0u;
        return;
    }
    if (port == PORT_DATA) {
        if (addrLatch < sizeof regs)
            regs[addrLatch] = val;
        if (addrLatch >= REG_SEC && addrLatch <= REG_YEAR)
            syncClockFromHost();
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port == PORT_DATA) {
        if (addrLatch == REG_STAT_C) {
            const std::uint8_t v = regs[REG_STAT_C];
            regs[REG_STAT_C] = 0;
            return v;
        }
        if (addrLatch == REG_STAT_D)
            return regs[REG_STAT_D];
        if (addrLatch < sizeof regs) {
            if (addrLatch >= REG_SEC && addrLatch <= REG_YEAR)
                syncClockFromHost();
            return regs[addrLatch];
        }
    }
    return 0xFFu;
}

} // namespace FieldCmos