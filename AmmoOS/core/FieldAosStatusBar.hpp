#pragma once

// AmouranthOS top status strip — host metrics written to guest RAM for GPU HUD.

#include "FieldAmouranthHudRam.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmouranthInfo.hpp"
#include "FieldDrives.hpp"
#include "FieldRtxTerm.hpp"
#include "FieldRtxVfs.hpp"
#include "FieldRuntimeInfo.hpp"

#include <cstdio>
#include <cstring>

namespace FieldAosStatusBar {

constexpr std::uint32_t STATUS_RAM = FieldAmouranthHudRam::STATUS_RAM;
constexpr int STATUS_LEN = 128;
constexpr float BAR_H = 28.f;

inline float height() noexcept { return BAR_H; }

inline void writeStatus(std::uint8_t* ram) noexcept {
    if (!ram) return;
    static int s_tick = 0;
    FieldAmouranthInfo::refreshClock();
    if ((++s_tick & 15) == 0) {
        FieldDrives::refresh();
        FieldRuntimeInfo::refresh();
        FieldAmouranthInfo::refreshQuality();
    }
    const std::uint32_t convFreeK = FieldRtxVfs::getFreeConvMem() / 1024u;
    const std::uint32_t umbFreeK = FieldRtxVfs::getFreeUmbMem() / 1024u;
    char buf[STATUS_LEN + 1]{};
    std::snprintf(buf, sizeof buf,
        " RTX-AMMOS %.48s | %c: %s | %uK conv %uK UMB | %02d:%02d",
        FieldRuntimeInfo::masterStatusLine(),
        FieldDrives::currentLetter,
        FieldDrives::currentLetter == 'C' ? "C:\\" : "A:\\",
        convFreeK,
        umbFreeK,
        FieldAmouranthInfo::clockHour,
        FieldAmouranthInfo::clockMin);
    for (int i = 0; i < STATUS_LEN; ++i) {
        const char ch = (i < static_cast<int>(sizeof buf) && buf[i]) ? buf[i] : ' ';
        ram[STATUS_RAM + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(ch);
    }
}

} // namespace FieldAosStatusBar