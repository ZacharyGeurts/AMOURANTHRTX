#pragma once

// RTX-AMMOS I/O hub — VGA, audio rack, DMA, gameport.

#include "FieldAudioRack.hpp"
#include "FieldCmos.hpp"
#include "FieldDma.hpp"
#include "FieldFdc765.hpp"
#include "FieldIde.hpp"
#include "FieldPic8259.hpp"
#include "FieldPit8254.hpp"
#include "FieldVgaCrtc.hpp"
#include "FieldVgaPlanes.hpp"
#include "FieldVesa.hpp"
#include "FieldDosConfig.hpp"
#include "FieldInput.hpp"
#include "FieldSb16.hpp"
#include "FieldPorts.hpp"
#include "FieldSpeaker.hpp"
#include "FieldVga.hpp"

#include <cstdint>

namespace FieldDevices {

inline const std::uint8_t* guestPtr = nullptr;

inline void bindGuest(const std::uint8_t* ram) noexcept {
    guestPtr = ram;
    FieldFdc765::bindGuest(ram);
    FieldVgaCrtc::bindGuest(const_cast<std::uint8_t*>(ram));
    FieldVgaPlanes::bindGuest(const_cast<std::uint8_t*>(ram));
    FieldGus::bindGuest(ram);
    FieldSb16::setGuestRam(ram);
}

inline void reset() noexcept {
    FieldPic8259::reset();
    FieldPit8254::reset();
    FieldDma::reset();
    FieldFdc765::reset();
    FieldIde::reset();
    FieldVgaCrtc::reset();
    FieldVgaPlanes::reset();
    FieldVesa::reset();
    FieldAudioRack::shutdown();
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == FieldPic8259::MASTER_CMD || port == FieldPic8259::MASTER_DATA
        || port == FieldPic8259::SLAVE_CMD || port == FieldPic8259::SLAVE_DATA) {
        FieldPic8259::portOut(port, val);
        return;
    }
    if (port == 0x40u || port == 0x41u || port == 0x42u || port == 0x43u) {
        FieldPit8254::portOut(port, val);
        return;
    }
    if (port == FieldCmos::PORT_ADDR || port == FieldCmos::PORT_DATA) {
        FieldCmos::portOut(port, val);
        return;
    }
    if (FieldFdc765::handlesPort(port)) {
        FieldFdc765::portOut(port, val);
        return;
    }
    if (FieldIde::handlesPort(port)) {
        FieldIde::portOut(port, val);
        return;
    }
    if (FieldVgaCrtc::handlesPort(port)) {
        FieldVgaCrtc::portOut(port, val);
        return;
    }
    if (FieldVgaPlanes::handlesPort(port)) {
        FieldVgaPlanes::portOut(port, val);
        return;
    }
    FieldVga::portOut(port, val);
    FieldDma::portOut(port, val);
    FieldAudioRack::portOut(port, val);
    FieldPorts::portOut(port, val);
    if (port == 0x201u) { /* gameport trigger */ (void)val; }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port == FieldPic8259::MASTER_CMD || port == FieldPic8259::MASTER_DATA
        || port == FieldPic8259::SLAVE_CMD || port == FieldPic8259::SLAVE_DATA) {
        const std::uint8_t v = FieldPic8259::portIn(port);
        if (v != 0xFFu) return v;
    }
    if (port >= 0x40u && port <= 0x43u) {
        const std::uint8_t v = FieldPit8254::portIn(port);
        if (v != 0xFFu) return v;
    }
    if (port == FieldCmos::PORT_ADDR || port == FieldCmos::PORT_DATA) {
        const std::uint8_t v = FieldCmos::portIn(port);
        if (v != 0xFFu) return v;
    }
    if (FieldFdc765::handlesPort(port)) {
        const std::uint8_t v = FieldFdc765::portIn(port);
        if (v != 0xFFu) return v;
    }
    if (FieldIde::handlesPort(port)) {
        const std::uint8_t v = FieldIde::portIn(port);
        if (v != 0xFFu) return v;
    }
    if (FieldVgaCrtc::handlesPort(port)) {
        const std::uint8_t v = FieldVgaCrtc::portIn(port);
        if (v != 0xFFu) return v;
    }
    if (FieldVgaPlanes::handlesPort(port)) {
        const std::uint8_t v = FieldVgaPlanes::portIn(port);
        if (v != 0xFFu) return v;
    }
    if (port <= 0x0Fu)
        return FieldDma::portIn(port);
    const std::uint8_t prt = FieldPorts::portIn(port);
    if (prt != 0xFFu) return prt;
    const std::uint8_t rack = FieldAudioRack::portIn(port);
    if (rack != 0xFFu) return rack;
    if (port == 0x201u && FieldDosConfig::cfg.joystickEnabled) {
        const auto& j = FieldInput::state.joystick;
        std::uint8_t v = 0xF0u;
        if (j.buttons1 & 1u) v &= ~0x10u;
        if (j.buttons1 & 2u) v &= ~0x20u;
        if (j.buttons2 & 1u) v &= ~0x01u;
        if (j.buttons2 & 2u) v &= ~0x02u;
        const int ax = static_cast<int>(j.x1) + 128;
        const int ay = static_cast<int>(j.y1) + 128;
        if (ax < 100) v &= ~0x01u;
        if (ax > 156) v &= ~0x02u;
        if (ay < 100) v &= ~0x04u;
        if (ay > 156) v &= ~0x08u;
        return v;
    }
    return 0xFFu;
}

inline bool initAudio(MIX_Mixer* mix) noexcept {
    return FieldAudioRack::init(mix);
}

inline void pumpAudio() noexcept {
    FieldAudioRack::pump();
}

} // namespace FieldDevices