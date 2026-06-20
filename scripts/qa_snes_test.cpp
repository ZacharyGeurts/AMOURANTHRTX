// QA: AmmoSNES Super FX detection, thermo-governed GSU batch, AmouranthOS boot chrome.
#include "FieldAmmoSnes.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAosAppJournal.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static std::vector<std::uint8_t> makeSuperFxStub() {
    std::vector<std::uint8_t> rom(0x10000, 0xFF);
    std::memcpy(rom.data() + 0x7FD0, "STAR FOX        ", 16);
    std::memcpy(rom.data() + 0x7FC0 + 0x10, "Super FX", 8);
    rom[0x7FFC] = 0x00;
    rom[0x7FFD] = 0x80;
    rom[0x8000] = 0x55; // NOP
    rom[0x8001] = 0x54; // STOP
    return rom;
}

int main() {
    auto rom = makeSuperFxStub();
    if (!AmmoSnes::detectSuperFxRom(rom.data(), rom.size())) {
        std::fprintf(stderr, "FAIL Super FX ROM header not detected\n");
        return 1;
    }
    std::printf("OK Super FX ROM header detected\n");

    AmmoSnes::State st{};
    if (!AmmoSnes::loadRom(st, rom.data(), rom.size())) {
        std::fprintf(stderr, "FAIL loadRom\n");
        return 1;
    }
    if (!st.gsu.present) {
        std::fprintf(stderr, "FAIL GSU not marked present\n");
        return 1;
    }
    std::printf("OK loadRom GSU present title=%.21s\n", st.cart.title);

    st.thermoGovernor = true;
    st.thermoSteps = 3;
    st.gsu.sfr = 0x80;
    const int plots0 = st.gsu.plotOps;
    AmmoSnes::runGsuFrame(st, st.thermoSteps);
    std::printf("OK runGsuFrame steps thermo=%d plotOps=%d pc=0x%04x\n",
        st.thermoSteps, st.gsu.plotOps - plots0, st.gsu.pc);

    FieldAosAppJournal::clearSessionJournal();
    FieldAmouranthOs::boot();
    if (FieldAmouranthOs::infoPanelVisible) {
        std::fprintf(stderr, "FAIL info panel visible at boot\n");
        return 1;
    }
    if (FieldAmouranthOs::panelVisible) {
        std::fprintf(stderr, "FAIL DOS panel visible at boot\n");
        return 1;
    }
    std::printf("OK AmouranthOS boot — no thermo/info panel at startup\n");
    return 0;
}