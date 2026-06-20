// QA: Exit confirm Yes → graceful shutdown closes all guest apps (PadTest + emulators).
#include <cstdio>

#include "FieldAmouranthExec.hpp"
#include "FieldAmouranthDeactivate.hpp"
#include "FieldAmouranthExitConfirm.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmouranthShutdown.hpp"
#include "FieldPadTest.hpp"
#include "FieldAmmoNes.hpp"
#include "FieldDos.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxBoot.hpp"
#include "FieldX86Emu.hpp"
#include "OptionsMenu.hpp"

#include <cstdio>
#include <vector>

int main() {
    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))) {
        std::fprintf(stderr, "FAIL hd load\n");
        return 1;
    }
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL fat mount\n");
        return 1;
    }

    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();
    FieldRtxBoot::paintWelcome(ram);

    FieldAmouranthOs::boot();
    FieldX86Emu::ramHost = ram;
    FieldPadTest::open(ram);
    if (!FieldPadTest::active) {
        std::fprintf(stderr, "FAIL PadTest not active\n");
        return 1;
    }

    FieldAmouranthExitConfirm::show();
    if (!FieldAmouranthExitConfirm::open) {
        std::fprintf(stderr, "FAIL exit confirm not open\n");
        return 1;
    }

    FieldAmouranthExitConfirm::confirmYes(ram);  // calls completeShutdown
    if (FieldAmouranthExitConfirm::open) {
        std::fprintf(stderr, "FAIL exit confirm still open after Yes\n");
        return 1;
    }
    if (!Options::SDL3::RequestQuit) {
        std::fprintf(stderr, "FAIL RequestQuit not set\n");
        return 1;
    }
    if (FieldPadTest::active) {
        std::fprintf(stderr, "FAIL PadTest still active after shutdown\n");
        return 1;
    }
    if (!FieldAmouranthOs::programs.empty()) {
        std::fprintf(stderr, "FAIL programs not cleared\n");
        return 1;
    }

    FieldAmouranthOs::deactivate();
    if (FieldAmouranthOs::active) {
        std::fprintf(stderr, "FAIL deactivate after shutdown\n");
        return 1;
    }

    std::printf("OK exit confirm Yes shuts down PadTest and clears programs\n");
    std::printf("Exit shutdown QA passed\n");
    return 0;
}