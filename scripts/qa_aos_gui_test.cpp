// QA: AmouranthOS GUI polish — live clock, 4K layout, taskbar clicks at 3840×2160.
#include "FieldAmouranthExec.hpp"
#include "FieldAmouranthInfo.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldDosViewport.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxBoot.hpp"
#include "FieldTaskbarLayout.hpp"
#include "OptionsMenu.hpp"

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

static constexpr int k4kW = 3840;
static constexpr int k4kH = 2160;

static void pump(std::uint8_t* ram, int frames = 3) {
    for (int i = 0; i < frames; ++i)
        FieldAmouranthExec::execPending(ram);
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

    if (Options::SDL3::DefaultWidth != 3840 || Options::SDL3::DefaultHeight != 2160) {
        std::fprintf(stderr, "FAIL default resolution %dx%d expected 3840x2160\n",
            Options::SDL3::DefaultWidth, Options::SDL3::DefaultHeight);
        return 1;
    }
    std::printf("OK default resolution 3840x2160\n");

    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();
    FieldRtxBoot::paintWelcome(ram);

    FieldAmouranthOs::boot();
    FieldDosViewport::winW = static_cast<float>(k4kW);
    FieldDosViewport::winH = static_cast<float>(k4kH);
    FieldDosViewport::renderW = static_cast<float>(k4kW);
    FieldDosViewport::renderH = static_cast<float>(k4kH);
    FieldAmouranthOs::tick(k4kW, k4kH);

    const auto L = FieldAmouranthOs::taskbarLayout();
    if (static_cast<int>(L.vpW) != k4kW || static_cast<int>(L.vpH) != k4kH) {
        std::fprintf(stderr, "FAIL taskbar layout vp %.0fx%.0f expected %dx%d\n",
            L.vpW, L.vpH, k4kW, k4kH);
        return 1;
    }
    if (L.tabW < 50.f || L.tabW > 60.f) {
        std::fprintf(stderr, "FAIL 4K tab width %.1f expected ~54 (40px * uiScale)\n", L.tabW);
        return 1;
    }
    if (L.clockW < 250.f || L.clockW > 290.f) {
        std::fprintf(stderr, "FAIL 4K clock width %.1f expected ~270 (200px * uiScale)\n", L.clockW);
        return 1;
    }
    std::printf("OK 4K taskbar layout tabW=%.1f clockW=%.1f\n", L.tabW, L.clockW);

    std::uint32_t bus[64]{};
    FieldAmouranthOs::packDataBus(bus, ram);
    const std::uint32_t clk0 = bus[29];
    if (clk0 == 0u) {
        std::fprintf(stderr, "FAIL clock not packed to bus[29]\n");
        return 1;
    }
    const int sec0 = static_cast<int>(clk0 & 0xFFu);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    FieldAmouranthInfo::refreshClock();
    FieldAmouranthInfo::packDataBus(bus);
    const std::uint32_t clk1 = bus[29];
    const int sec1 = static_cast<int>(clk1 & 0xFFu);
    if (sec1 == sec0 && clk1 == clk0) {
        std::fprintf(stderr, "FAIL clock did not advance (bus[29]=%u)\n", clk1);
        return 1;
    }
    std::printf("OK live clock tick bus[29] %u -> %u\n", clk0, clk1);

    float btnY0 = 0.f, btnY1 = 0.f;
    FieldAmouranthOs::taskbarChromeButtonY(btnY0, btnY1);
    const float startX = FieldAmouranthOs::scaledStartW() * 0.5f;
    const float startY = (btnY0 + btnY1) * 0.5f;
    if (!FieldAmouranthOs::onTaskbarMouseDown(nullptr, startX, startY)
            || !FieldAmouranthOs::startOpen) {
        std::fprintf(stderr, "FAIL 4K Start click (%.1f,%.1f)\n", startX, startY);
        return 1;
    }
    std::printf("OK 4K Start button click\n");

    FieldAmouranthOs::startOpen = false;
    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Shell);
    pump(ram);

    const float tabX = FieldTaskbarLayout::tabX(L, 0) + L.tabW * 0.5f;
    const float tabY = L.tabY + L.tabH * 0.5f;
    FieldAmouranthOs::onMouseMotion(nullptr, tabX, tabY);
    if (FieldAmouranthOs::hover != FieldAmouranthOs::HitZone::TaskBtn) {
        std::fprintf(stderr, "FAIL 4K tab hover zone %d at (%.1f,%.1f)\n",
            static_cast<int>(FieldAmouranthOs::hover), tabX, tabY);
        return 1;
    }
    if (!FieldAmouranthOs::onTaskbarMouseDown(nullptr, tabX, tabY)) {
        std::fprintf(stderr, "FAIL 4K tab click not handled\n");
        return 1;
    }
    std::printf("OK 4K taskbar tab click\n");

    std::printf("AOS GUI QA passed (v2.0.4)\n");
    return 0;
}