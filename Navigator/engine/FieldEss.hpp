#pragma once

// ESS ES688 — SB Pro-compatible ports delegated to SB16 stack.

#include "FieldDosConfig.hpp"
#include "FieldSb16.hpp"

#include <cstdint>

namespace FieldEss {

inline bool ready = false;
inline std::uint8_t indexReg = 0;

inline void shutdown() noexcept { ready = false; indexReg = 0; }

inline bool init() noexcept {
    shutdown();
    if (!FieldDosConfig::cfg.essEnabled) return false;
    ready = true;
    return true;
}

inline std::uint16_t basePort() noexcept {
    return FieldDosConfig::cfg.essBasePort;
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (!FieldDosConfig::cfg.essEnabled) return 0xFFu;
    const std::uint16_t base = basePort();
    if (port < base || port >= base + 0x10u) return 0xFFu;
    if (port == static_cast<std::uint16_t>(base + 0x0Au))
        return FieldSb16::portIn(static_cast<std::uint16_t>(FieldDosConfig::cfg.sbBasePort + 0x0Au));
    if (port == static_cast<std::uint16_t>(base + 0x0Cu))
        return FieldSb16::portIn(static_cast<std::uint16_t>(FieldDosConfig::cfg.sbBasePort + 0x0Cu));
    if (port == static_cast<std::uint16_t>(base + 0x0Eu)) return 0x68u;
    return 0xFFu;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (!FieldDosConfig::cfg.essEnabled) return;
    const std::uint16_t base = basePort();
    if (port == base) {
        indexReg = val;
        return;
    }
    if (port == static_cast<std::uint16_t>(base + 1u)) {
        (void)indexReg;
        return;
    }
    if (port >= base + 0x08u && port <= base + 0x0Fu) {
        const std::uint8_t off = static_cast<std::uint8_t>(port - (base + 0x08u));
        FieldSb16::portOut(static_cast<std::uint16_t>(FieldDosConfig::cfg.sbBasePort + off), val);
    }
}

} // namespace FieldEss