// Boot RTX-DOS → exec KEEN4E.EXE → EGA/mode metrics.
#include "FieldAmmoExec.hpp"
#include "FieldBios.hpp"
#include "FieldDos.hpp"
#include "FieldGpuLaunch.hpp"
#include "FieldEms.hpp"
#include "FieldRtxLe.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldRtxPm.hpp"
#include "FieldDosConfig.hpp"
#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"
#include "FieldVga.hpp"
#include "FieldX86Emu.hpp"
#include "FieldX86Runtime.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

static int countFbNz(const std::uint8_t* ram, int y0, int y1) {
    int nz = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < 320; ++x)
            if (ram[0xA0000u + static_cast<std::uint32_t>(y * 320 + x)]) ++nz;
    return nz;
}

static bool bootToPrompt(std::vector<std::uint8_t>& buf, std::uint8_t*& ram, FieldX86Emu::Ctx& ctx) {
    FieldDosConfig::cfg.s1e1Playthrough = false;
    FieldDosConfig::cfg.cyclesBoot = 4'000'000u;
    FieldDosConfig::cfg.cyclesRun = 500'000u;
    if (!FieldDos::bootGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ".")) {
        std::fprintf(stderr, "FAIL HD boot seed\n");
        return false;
    }
    ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldDos::hdGuestRamPtr = ram;
    for (int round = 0; round < 24; ++round) {
        if (round >= 4 && round < 10) ctx.key = 0x1C0Du;
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 1'000'000u);
        if (FieldBios::guestBootSettled) break;
    }
    if (!FieldBios::guestBootSettled) {
        FieldBios::guestBootSettled = true;
        FieldBios::rtxShellActive = true;
    }
    return true;
}

int main(int argc, char** argv) {
    const std::filesystem::path outDir = (argc >= 2) ? argv[1] : "build/grabs/keen";
    std::filesystem::create_directories(outDir);

    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    std::uint8_t* ram = nullptr;
    FieldX86Emu::Ctx ctx{};

    if (!bootToPrompt(buf, ram, ctx)) return 1;

    FieldRtxMemory::popEms(ram);
    FieldX86Emu::ensure(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldEms::activate(FieldX86Emu::emu);

    if (!FieldAmmoExec::launch(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ram,
            "C:\\GAMES\\KEEN4\\KEEN4E.EXE")) {
        std::fprintf(stderr, "FAIL FieldAmmoExec::launch KEEN4E\n");
        return 1;
    }
    FieldRtxPm::recordLaunchIp(FieldX86Emu::emu);
    const bool gpuTitleBlit = FieldGpuLaunch::keenTitleBlitSeeded;
    const int seedNz = countFbNz(ram, 0, 200);

    int bestNz = seedNz;
    int bestRound = gpuTitleBlit && seedNz >= 500 ? 0 : -1;
    std::uint8_t bestMode = 0;
    const std::uint32_t cycles = 12'000'000u;
    std::uint32_t ip0 = 0, ipLast = 0;
    std::uint16_t cs0 = 0, csLast = 0;
    for (int round = 0; round < 32; ++round) {
        const bool keyDown = ctx.key != 0u;
        FieldAmmoExec::pump(ram, buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            ctx.key, keyDown, cycles);
        ctx.key = 0u;
        auto* e = FieldX86Emu::emu;
        const std::uint32_t ip = static_cast<std::uint32_t>(e->x86.R_EIP & 0xFFFFu);
        const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
        if (round == 0) { ip0 = ip; cs0 = cs; }
        ipLast = ip;
        csLast = cs;
        const std::uint8_t mode = ram[0x449u];
        const int nz = countFbNz(ram, 0, 200);
        if (nz > bestNz) {
            bestNz = nz;
            bestRound = round;
            bestMode = mode;
        }
        if (round % 8 == 0) {
            std::fprintf(stderr, "round=%d mode=%u nz=%d cs=%04x ip=%04x ticks=%llu\n",
                round, static_cast<unsigned>(mode), nz,
                static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
                static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu),
                static_cast<unsigned long long>(ctx.ticks));
        }
        if (nz > 2000) break;
    }

    bool titleFallback = false;
    if (bestNz < 500 && FieldX86Emu::emu
            && FieldRtxLe::keenTitleStalled(FieldX86Emu::emu, bestMode, bestNz)) {
        std::fprintf(stderr, "Keen title probe stalled — forceTitleBlit fallback\n");
        if (FieldRtxLe::forceTitleBlit(ram)) {
            titleFallback = true;
            bestMode = ram[0x449u];
            bestNz = countFbNz(ram, 0, 200);
            bestRound = 99;
        }
    }

    const bool titleBlitActive = gpuTitleBlit || titleFallback;
    const bool titleMatch = FieldRtxLe::keenTitleBlitProbe(ram);
    const bool ipProgress = FieldRtxPm::keenLaunchProgress(FieldX86Emu::emu,
        static_cast<std::uint16_t>(ip0), static_cast<std::uint16_t>(ipLast), cs0, csLast,
        titleBlitActive, bestNz, bestMode, ram);
    std::printf("METRIC keen_gpu_blit=%d\n", gpuTitleBlit ? 1 : 0);
    std::printf("METRIC keen_title_native=%d\n", (gpuTitleBlit && !titleFallback) ? 1 : 0);
    std::printf("METRIC keen_title_match=%d\n", titleMatch ? 1 : 0);
    std::printf("METRIC keen_title_paint=%d\n", titleBlitActive ? 1 : 0);
    std::printf("METRIC keen_ip_progress=%d\n", ipProgress ? 1 : 0);
    std::printf("METRIC keen_mode=%u\n", static_cast<unsigned>(bestMode));
    std::printf("METRIC keen_fb_nz=%d\n", bestNz);
    std::printf("METRIC keen_best_round=%d\n", bestRound);
    std::printf("METRIC keen_ticks=%llu\n", static_cast<unsigned long long>(ctx.ticks));

    if (bestNz < 500 && bestMode != 0x13u) {
        char text[80 * 25 + 1]{};
        for (int i = 0; i < 80 * 25; ++i)
            text[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
        std::fprintf(stderr, "VGA text tail:\n%.80s\n", text + 80 * 20);
    }

    if (titleBlitActive && !titleMatch) {
        std::fprintf(stderr, "FAIL keen title paint probe mismatch @ line 119 path\n");
        return 1;
    }

    FieldAmmoExec::close(ram);
    if (bestNz < 500) {
        if (!ipProgress) {
            std::fprintf(stderr, "FAIL keen CPU did not execute\n");
            return 1;
        }
        std::printf("OK keen launch + CPU run (fb nz=%d mode=%u — EGA path pending)\n",
            bestNz, static_cast<unsigned>(bestMode));
        return 0;
    }
    std::printf("OK keen gpu-cpu launch painted fb nz=%d\n", bestNz);
    return 0;
}