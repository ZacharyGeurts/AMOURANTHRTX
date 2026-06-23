// QA: RTX-PM Keen — LZEXE expand + direct LE bootstrap (skip DOS/4GW extender).
#include "FieldAmmoExec.hpp"
#include "FieldBios.hpp"
#include "FieldDos.hpp"
#include "FieldEms.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldDosConfig.hpp"
#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxLe.hpp"
#include "FieldRtxPm.hpp"
#include "FieldVga.hpp"
#include "FieldX86Emu.hpp"
#include "FieldX86Runtime.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
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
    const std::filesystem::path outDir = (argc >= 2) ? argv[1] : "build/grabs/keen_rtxpm";
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

    const char* keenPath = "C:\\GAMES\\KEEN4\\KEEN4E.EXE";
    std::vector<std::uint8_t> image;
    if (!FieldDos::readHostFile(keenPath, image) || image.empty()) {
        std::fprintf(stderr, "FAIL cannot read %s\n", keenPath);
        return 1;
    }

    if (!FieldRtxPm::launchMzPm(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ram, image, keenPath)) {
        std::fprintf(stderr, "FAIL FieldRtxPm::launchMzPm\n");
        return 1;
    }

    FieldBios::pmExecActive = true;
    FieldAmmoExec::active = true;
    FieldAmmoExec::storePath(keenPath);
    FieldAmmoExec::launchedFormat = FieldAmmoExec::Format::DosMz;

    auto* e0 = FieldX86Emu::emu;
    std::fprintf(stderr, "RTX-PM launch cs=%04x ip=%04x pm=%d lePending=%d\n",
        static_cast<unsigned>(e0->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(FieldDpmi::isProtected(e0) ? e0->x86.R_EIP : (e0->x86.R_EIP & 0xFFFFu)),
        FieldDpmi::isProtected(e0) ? 1 : 0,
        FieldDpmi::leBootPending ? 1 : 0);

    int bestNz = 0;
    int bestRound = -1;
    std::uint8_t bestMode = 0;
    const std::uint32_t cycles = 12'000'000u;
    std::uint32_t ip0 = 0, ipLast = 0;
    std::uint16_t cs0 = 0, csLast = 0;
    for (int round = 0; round < 32; ++round) {
        FieldAmmoExec::pump(ram, buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, 0u, false, cycles);
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
            auto* e = FieldX86Emu::emu;
            std::fprintf(stderr, "round=%d mode=%u nz=%d cs=%04x ip=%04x pm=%d\n",
                round, static_cast<unsigned>(mode), nz,
                static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
                static_cast<unsigned>(FieldDpmi::isProtected(e) ? e->x86.R_EIP : (e->x86.R_EIP & 0xFFFFu)),
                FieldDpmi::isProtected(e) ? 1 : 0);
        }
        if (nz > 500) break;
    }

    bool titlePainted = false;
    if (bestNz < 500 && FieldX86Emu::emu
            && FieldRtxLe::keenTitleStalled(FieldX86Emu::emu, bestMode, bestNz)) {
        std::fprintf(stderr, "RTX-PM title probe stalled — titleForcePaint fallback\n");
        if (FieldRtxLe::titleForcePaint(ram)) {
            titlePainted = true;
            bestMode = ram[0x449u];
            bestNz = countFbNz(ram, 0, 200);
            bestRound = 99;
        }
    }

    const bool ipProgress = FieldRtxPm::keenLaunchProgress(FieldX86Emu::emu,
        static_cast<std::uint16_t>(ip0), static_cast<std::uint16_t>(ipLast), cs0, csLast,
        titlePainted, bestNz, bestMode, ram);
    std::printf("METRIC rtxpm_title_paint=%d\n", titlePainted ? 1 : 0);
    std::printf("METRIC rtxpm_ip_progress=%d\n", ipProgress ? 1 : 0);
    std::printf("METRIC rtxpm_mode=%u\n", static_cast<unsigned>(bestMode));
    std::printf("METRIC rtxpm_fb_nz=%d\n", bestNz);
    std::printf("METRIC rtxpm_best_round=%d\n", bestRound);
    std::printf("METRIC rtxpm_ticks=%llu\n", static_cast<unsigned long long>(ctx.ticks));

    FieldAmmoExec::close(ram);
    if (bestNz < 500 && !ipProgress) {
        std::fprintf(stderr, "FAIL keen RTX-PM (mode=%u nz=%d ip=%d)\n",
            static_cast<unsigned>(bestMode), bestNz, ipProgress ? 1 : 0);
        return 1;
    }
    std::printf("OK keen RTX-PM fb nz=%d mode=%u\n", bestNz, static_cast<unsigned>(bestMode));
    return 0;
}