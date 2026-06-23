#pragma once

// Headless NES/Contra QA driver — env AMOURANTHRTX_NES_TEST=1

#include "FieldAmmoNes.hpp"
#include "FieldVga.hpp"

#include <cstdlib>
#include <string>

namespace FieldAosNesTest {

inline std::uint64_t dispatchFrame = 0u;
inline bool launched = false;

inline bool enabled() noexcept {
    return std::getenv("AMOURANTHRTX_NES_TEST") != nullptr;
}

inline void tickDispatch(std::uint8_t* ram) noexcept {
    if (!enabled() || !ram) return;
    ++dispatchFrame;
    if (!launched && dispatchFrame >= 12u) {
        FieldNesImport::ensureImported();
        std::string path;
        if (FieldNesImport::findContra(path) || FieldNesImport::findAnyRom(path))
            FieldNes::open(ram, path.c_str());
        launched = FieldNes::active;
        if (launched)
            std::fprintf(stderr, "[NES_QA] launched %s frame=%llu pc=%04X\n",
                path.c_str(), static_cast<unsigned long long>(dispatchFrame), FieldNes::pc);
    }
    if (launched && FieldNes::active) {
        if (dispatchFrame == 90u || dispatchFrame == 120u)
            FieldNes::pad1 |= 0x08u;  // Start — advance Contra title
        else
            FieldNes::pad1 &= ~0x08u;
    }
    if (launched && dispatchFrame >= 240u && (dispatchFrame % 60u) == 0u) {
        int ntNonZero = 0;
        for (int i = 0; i < 0x700; ++i)
            if (FieldNes::chip.ppu.vram[static_cast<std::size_t>(i)] != 0) ++ntNonZero;
        int fbNonZero = 0;
        if (ram) {
            for (int i = 0; i < 320 * 200; i += 17)
                if (ram[FieldVga::VGA_FB + static_cast<std::uint32_t>(i)] != 0) ++fbNonZero;
        }
        std::fprintf(stderr,
            "[NES_QA] frame=%llu nesFrames=%u pc=%04X mask=%02X ntNZ=%d fbNZ=%d audioLevel=%.4f\n",
            static_cast<unsigned long long>(dispatchFrame), FieldNes::frames,
            FieldNes::pc, FieldNes::chip.ppu.mask, ntNonZero, fbNonZero, FieldNes::audioLevel);
    }
}

} // namespace FieldAosNesTest