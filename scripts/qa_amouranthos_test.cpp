// QA: AmouranthOS desktop mode, file commander, launch queue — no SDL blackout path.
#include "FieldAmouranthExec.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmouranthWm.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldDosViewport.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxBoot.hpp"
#include "FieldRtxShell.hpp"
#include "OptionsMenu.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool screenHas(const std::uint8_t* ram, const char* needle) {
    char buf[80 * 25 + 1]{};
    for (int i = 0; i < 80 * 25; ++i)
        buf[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
    return std::strstr(buf, needle) != nullptr;
}

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
    if (!FieldAmouranthOs::active) {
        std::fprintf(stderr, "FAIL AmouranthOS not active after boot\n");
        return 1;
    }
    if (FieldAmouranthOs::panelVisible) {
        std::fprintf(stderr, "FAIL floating DOS panel should stay hidden until a program opens\n");
        return 1;
    }
    if (!FieldAmouranthOs::infoPanelVisible) {
        std::fprintf(stderr, "FAIL DevKit info card should be visible at boot (desktop mode)\n");
        return 1;
    }
    std::printf("OK AmouranthOS boot desktop — info panel + taskbar, no popup panel\n");

    FieldAmouranthOs::onMouseDown(nullptr, 40.f, static_cast<float>(FieldAmouranthOs::winH) - 20.f, 1, 1);
    if (!FieldAmouranthOs::startOpen) {
        std::fprintf(stderr, "FAIL Start button should open programs popup\n");
        return 1;
    }

    std::uint32_t bus[64]{};
    FieldDosViewport::packDataBus(bus, 64, ram);
    FieldAmouranthOs::packDataBus(bus);
    if ((bus[42] & 4096u) == 0u) {
        std::fprintf(stderr, "FAIL data_bus aos flag not set\n");
        return 1;
    }
    if ((bus[42] & (1u << 24u)) == 0u) {
        std::fprintf(stderr, "FAIL info panel bus bit should be set at boot\n");
        return 1;
    }
    if ((bus[42] & (1u << 25u)) == 0u) {
        std::fprintf(stderr, "FAIL panel hide bus bit should be set when no popup panel\n");
        return 1;
    }
    if (bus[29] == 0u) {
        std::fprintf(stderr, "FAIL clock not packed to bus[29]\n");
        return 1;
    }
    if ((bus[42] & (1u << 20u)) == 0u) {
        std::fprintf(stderr, "FAIL Start menu bus bit not set\n");
        return 1;
    }
    FieldAmouranthMenu::rebuildVisible();
    const std::uint32_t menuRows = (bus[54] >> FieldAmouranthOs::BUS_CHROME_MENU_ROWS_SHIFT) & 0xFFu;
    const std::uint32_t menuHover = (bus[54] >> FieldAmouranthOs::BUS_CHROME_MENU_HOVER_SHIFT) & 0xFFu;
    const std::uint32_t focusTitle = (bus[54] >> FieldAmouranthOs::BUS_CHROME_FOCUS_TITLE_SHIFT) & 0xFFu;
    if (menuRows != static_cast<std::uint32_t>(FieldAmouranthMenu::rootCount)) {
        std::fprintf(stderr, "FAIL bus[54] menu rows got %u expected %d\n",
            menuRows, FieldAmouranthMenu::rootCount);
        return 1;
    }
    if (menuHover != FieldAmouranthOs::BUS_CHROME_NONE) {
        std::fprintf(stderr, "FAIL bus[54] menu hover should be unset (0xFF) got 0x%x\n", menuHover);
        return 1;
    }
    if (focusTitle != FieldAmouranthOs::BUS_CHROME_NONE) {
        std::fprintf(stderr, "FAIL bus[54] focus title index should be unset at boot (0xFF) got %u\n",
            focusTitle);
        return 1;
    }
    std::printf("OK Start popup panel open (bus42=0x%x bus54=0x%x rows=%u)\n",
        bus[42], bus[54], menuRows);
    std::printf("OK data_bus AmouranthOS 0x%x clock=0x%x quality=0x%x\n",
        bus[42], bus[29], bus[30]);

    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Shell);
    FieldAmouranthExec::execPending(ram);
    if (!FieldAmouranthExec::screenHasGuiMarker(ram, FieldAmouranthOs::AppId::Shell)) {
        std::fprintf(stderr, "FAIL RTX Shell GUI panel missing\n");
        return 1;
    }
    FieldAmouranthOs::packDataBus(bus, ram);
    const std::uint32_t shellFocus = (bus[54] >> FieldAmouranthOs::BUS_CHROME_FOCUS_TITLE_SHIFT) & 0xFFu;
    if (shellFocus != 0u) {
        std::fprintf(stderr, "FAIL bus[54] focus title index after Shell launch got %u expected 0\n",
            shellFocus);
        return 1;
    }
    std::printf("OK GUI launch RTX Shell (focusTitle=%u)\n", shellFocus);

    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::AmmoCode);
    FieldAmouranthExec::execPending(ram);
    if (!FieldAmouranthExec::screenHasGuiMarker(ram, FieldAmouranthOs::AppId::AmmoCode)) {
        std::fprintf(stderr, "FAIL AmmoCode GUI not active\n");
        return 1;
    }
    std::printf("OK GUI launch AmmoCode\n");

    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::PadTest);
    FieldAmouranthExec::execPending(ram);
    if (!FieldAmouranthExec::screenHasGuiMarker(ram, FieldAmouranthOs::AppId::PadTest)) {
        std::fprintf(stderr, "FAIL PADTEST GUI missing\n");
        return 1;
    }
    std::printf("OK GUI launch PADTEST\n");

    FieldAmouranthOs::showDosPanelCentered();
    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::FileCmd);
    FieldAmouranthExec::execPending(ram);
    if (!FieldAmouranthOs::panelVisible) {
        std::fprintf(stderr, "FAIL DOS panel should open for FILECMD\n");
        return 1;
    }
    if (!FieldAmouranthFileCmd::active) {
        std::fprintf(stderr, "FAIL FILECMD not active\n");
        return 1;
    }
    if (!screenHas(ram, "Field Commander")) {
        std::fprintf(stderr, "FAIL Field Commander UI missing\n");
        return 1;
    }
    const int n = static_cast<int>(FieldAmouranthFileCmd::entriesL.size());
    if (n < 2) {
        std::fprintf(stderr, "FAIL file list empty (got %d)\n", n);
        return 1;
    }
    std::printf("OK FILECMD %d entries C:\\ scrollTop=%d\n", n, FieldAmouranthFileCmd::scrollTop);

    for (int i = 0; i < 5; ++i)
        FieldAmouranthFileCmd::scroll(1);
    if (FieldAmouranthFileCmd::scrollTop == 0 && n > FieldAmouranthFileCmd::ROWS_VIS) {
        std::fprintf(stderr, "FAIL scroll did not move\n");
        return 1;
    }
    std::printf("OK FILECMD scrollbar scrollTop=%d\n", FieldAmouranthFileCmd::scrollTop);

    FieldAmouranthOs::tick(1920, 1080);
    FieldAmouranthWm::resetScale();
    FieldAmouranthWm::syncViewport();
    if (FieldDosViewport::chromeTitleH <= 0.f) {
        std::fprintf(stderr, "FAIL WM title bar height not set when panel open\n");
        return 1;
    }
    FieldAmouranthWm::hover = FieldAmouranthWm::ChromeHit::TitleBar;
    FieldAmouranthWm::packIntoBus(bus);
    if ((bus[52] & 0xFu) != static_cast<unsigned>(FieldAmouranthWm::ChromeHit::TitleBar)) {
        std::fprintf(stderr, "FAIL WM hover not packed to bus[52]\n");
        return 1;
    }
    if (bus[53] < 200u || bus[53] > 320u) {
        std::fprintf(stderr, "FAIL WM panel scale bus[53]=0x%x\n", bus[53]);
        return 1;
    }
    std::printf("OK RTX WM chrome titleH=%.1f bus52=0x%x bus53=0x%x\n",
        FieldDosViewport::chromeTitleH, bus[52], bus[53]);

    FieldAmouranthOs::deactivate();
    if (FieldAmouranthOs::active) {
        std::fprintf(stderr, "FAIL deactivate\n");
        return 1;
    }
    std::printf("OK AmouranthOS deactivate\n");
    std::printf("AmouranthOS QA passed\n");
    return 0;
}