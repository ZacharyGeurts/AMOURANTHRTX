#pragma once

// RTX-AMMOS audio rack — SB16, OPL, LAPC-1/MT-32, Disney Sound Source, GUS, ESS, Covox, PC speaker.

#include "FieldCovox.hpp"
#include "FieldDisneySound.hpp"
#include "FieldDosConfig.hpp"
#include "FieldEss.hpp"
#include "FieldGus.hpp"
#include "FieldMpu401.hpp"
#include "FieldSb16.hpp"
#include "FieldSpeaker.hpp"


#include <SDL3_mixer/SDL_mixer.h>

#include <cstdint>

namespace FieldAudioRack {

inline void reset() noexcept {
    FieldGus::shutdown();
    FieldEss::shutdown();
    FieldCovox::shutdown();
}

inline bool init(MIX_Mixer* mix) noexcept {
    reset();
    bool ok = false;
    if (FieldDosConfig::cfg.pcSpeakerEnabled && mix)
        ok = FieldSpeaker::init(mix) || ok;
    if (FieldDosConfig::cfg.sb16Enabled && mix)
        ok = FieldSb16::init(mix) || ok;
    if (FieldDosConfig::cfg.mpu401Enabled && FieldDosConfig::cfg.lapc1Enabled && mix)
        ok = FieldMpu401::init(mix) || ok;
    if (FieldDosConfig::cfg.disneyEnabled && mix)
        ok = FieldDisneySound::init(mix) || ok;
    if (FieldDosConfig::cfg.gusEnabled && mix)
        ok = FieldGus::init(mix) || ok;
    if (FieldDosConfig::cfg.essEnabled)
        ok = FieldEss::init() || ok;
    if (FieldDosConfig::cfg.covoxEnabled && mix)
        ok = FieldCovox::init(mix) || ok;
    return ok;
}

inline void shutdown() noexcept {
    FieldSb16::shutdown();
    FieldMpu401::shutdown();
    FieldDisneySound::shutdown();
    FieldSpeaker::shutdown();
    reset();
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    std::uint8_t v = 0xFFu;
    if (FieldDosConfig::portInSbRange(port) || FieldDosConfig::portInOplRange(port)) {
        v = FieldSb16::portIn(port);
        if (v != 0xFFu) return v;
    }
    if (FieldDosConfig::cfg.pcSpeakerEnabled) {
        const std::uint8_t sp = FieldSpeaker::portIn(port);
        if (port == 0x61u || port == 0x40u || port == 0x42u) return sp;
    }
    v = FieldGus::portIn(port);
    if (v != 0xFFu) return v;
    v = FieldEss::portIn(port);
    if (v != 0xFFu) return v;
    v = FieldDisneySound::portIn(port);
    if (v != 0xFFu) return v;
    v = FieldMpu401::portIn(port);
    if (v != 0xFFu) return v;
    return 0xFFu;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (FieldDosConfig::cfg.pcSpeakerEnabled)
        FieldSpeaker::portOut(port, val);
    FieldSb16::portOut(port, val);
    FieldGus::portOut(port, val);
    FieldEss::portOut(port, val);
    FieldCovox::portOut(port, val);
    FieldDisneySound::portOut(port, val);
    FieldMpu401::portOut(port, val);
}

inline void pump() noexcept {
    if (FieldDosConfig::cfg.pcSpeakerEnabled)
        FieldSpeaker::pump();
    if (FieldDosConfig::cfg.sb16Enabled)
        FieldSb16::pump();
    if (FieldDosConfig::cfg.mpu401Enabled && FieldDosConfig::cfg.lapc1Enabled)
        FieldMpu401::pump();
    if (FieldDosConfig::cfg.disneyEnabled)
        FieldDisneySound::pump();
    if (FieldDosConfig::cfg.gusEnabled)
        FieldGus::pump();
    if (FieldDosConfig::cfg.covoxEnabled)
        FieldCovox::pump();

}

inline void playTestTone() noexcept {
    if (FieldDosConfig::cfg.sb16Enabled)
        FieldSb16::buildOplTone();
}

} // namespace FieldAudioRack