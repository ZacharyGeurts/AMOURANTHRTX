#pragma once

// Variable DOS machine profile — SB16, mouse, joystick, cycle budgets.

#include <cstdint>
#include <cstring>

namespace FieldDosConfig {

enum class Preset : std::uint8_t {
    Minimal,    // PC speaker + keyboard only
    Classic,    // SB Pro + mouse
    Full,       // SB16 + OPL + mouse + joystick + gamepad
    Ammos,      // Full RTX-AMMOS rack: all sound cards + CD-ROM
};

struct Config {
    Preset preset = Preset::Full;

    /* Sound Blaster 16 */
    bool     sb16Enabled     = true;
    std::uint16_t sbBasePort = 0x220u;
    std::uint8_t  sbIrq      = 5u;    // IRQ5 → INT 0x0D
    std::uint8_t  sbDma8     = 1u;    // channel 1
    std::uint8_t  sbDma16    = 5u;    // channel 5 (16-bit)
    bool     oplEnabled      = true;
    std::uint16_t oplPort    = 0x388u;
    bool     mpu401Enabled   = true;
    std::uint16_t mpuPort    = 0x330u;
    bool     lapc1Enabled    = true;   /* Roland LAPC-1 / MT-32 on MPU-401 */
    bool     disneyEnabled   = true;
    std::uint16_t disneyPort = 0x378u; /* Disney Sound Source (LPT DAC) */

    /* RTX-AMMOS audio rack */
    bool     gusEnabled      = true;
    std::uint16_t gusBasePort = 0x240u;
    bool     essEnabled      = true;
    std::uint16_t essBasePort = 0x280u;
    bool     covoxEnabled    = true;
    std::uint16_t covoxPort  = 0x378u;

    /* CD-ROM */
    bool     cdRomEnabled    = true;

    /* Legacy audio */
    bool pcSpeakerEnabled = true;

    /* Input */
    bool keyboardEnabled   = true;
    bool mouseEnabled      = true;
    bool joystickEnabled   = true;
    bool gamepadAsJoystick = true;
    std::uint16_t mouseMinX = 0u;
    std::uint16_t mouseMaxX = 319u;
    std::uint16_t mouseMinY = 0u;
    std::uint16_t mouseMaxY = 199u;
    bool mouseCaptureRelative = false;

    /* CPU budget */
    std::uint32_t cyclesBoot = 16'777'216u;
    std::uint32_t cyclesRun  = 131'072u;
    std::uint32_t cyclesMax  = 33'554'432u;

    /* Feature flags for DataBus / shader */
    bool s1e1Playthrough = false;
};

inline Config cfg{};

inline void applyPreset(Preset p) noexcept {
    cfg.preset = p;
    switch (p) {
    case Preset::Minimal:
        cfg.sb16Enabled = false;
        cfg.oplEnabled = false;
        cfg.mpu401Enabled = false;
        cfg.mouseEnabled = false;
        cfg.joystickEnabled = false;
        cfg.gamepadAsJoystick = false;
        cfg.pcSpeakerEnabled = true;
        break;
    case Preset::Classic:
        cfg.sb16Enabled = true;
        cfg.sbBasePort = 0x220u;
        cfg.sbIrq = 7u;
        cfg.oplEnabled = true;
        cfg.mpu401Enabled = false;
        cfg.mouseEnabled = true;
        cfg.joystickEnabled = false;
        cfg.gamepadAsJoystick = false;
        cfg.pcSpeakerEnabled = true;
        break;
    case Preset::Full:
        cfg.sb16Enabled = true;
        cfg.sbBasePort = 0x220u;
        cfg.sbIrq = 5u;
        cfg.sbDma8 = 1u;
        cfg.sbDma16 = 5u;
        cfg.oplEnabled = true;
        cfg.mpu401Enabled = false;
        cfg.lapc1Enabled = false;
        cfg.disneyEnabled = false;
        cfg.gusEnabled = false;
        cfg.essEnabled = false;
        cfg.covoxEnabled = false;
        cfg.cdRomEnabled = true;
        cfg.mouseEnabled = true;
        cfg.joystickEnabled = true;
        cfg.gamepadAsJoystick = true;
        cfg.pcSpeakerEnabled = true;
        break;
    case Preset::Ammos:
    default:
        cfg.sb16Enabled = true;
        cfg.sbBasePort = 0x220u;
        cfg.sbIrq = 5u;
        cfg.sbDma8 = 1u;
        cfg.sbDma16 = 5u;
        cfg.oplEnabled = true;
        cfg.mpu401Enabled = true;
        cfg.lapc1Enabled = true;
        cfg.disneyEnabled = true;
        cfg.gusEnabled = true;
        cfg.essEnabled = true;
        cfg.covoxEnabled = true;
        cfg.cdRomEnabled = true;
        cfg.mouseEnabled = true;
        cfg.joystickEnabled = true;
        cfg.gamepadAsJoystick = true;
        cfg.pcSpeakerEnabled = true;
        break;
    }
}

inline std::uint8_t sbIrqVector() noexcept {
    return static_cast<std::uint8_t>(8u + cfg.sbIrq);
}

inline bool portInSbRange(std::uint16_t port) noexcept {
    if (!cfg.sb16Enabled) return false;
    const std::uint16_t b = cfg.sbBasePort;
    return port >= b && port < b + 0x10u;
}

inline bool portInOplRange(std::uint16_t port) noexcept {
    if (!cfg.oplEnabled) return false;
    return port == cfg.oplPort || port == static_cast<std::uint16_t>(cfg.oplPort + 1u)
        || (cfg.sb16Enabled && (port == static_cast<std::uint16_t>(cfg.sbBasePort + 8u)
                             || port == static_cast<std::uint16_t>(cfg.sbBasePort + 9u)));
}

} // namespace FieldDosConfig