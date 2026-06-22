// Boot RTX-DOS → exec DOOM.EXE on GPU x86 shader (+ host trap assist) → mode-13h metrics.
#include "FieldBios.hpp"
#include "FieldDoom.hpp"
#include "FieldDoomGpu.hpp"
#include "FieldDpmi.hpp"
#include "FieldRtxPm.hpp"
#include "FieldDpmi.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
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

static bool writeMode13Ppm(const std::filesystem::path& path, const std::uint8_t* ram) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n320 200\n255\n";
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            const std::uint8_t idx = ram[0xA0000u + static_cast<std::uint32_t>(y * 320 + x)];
            const std::size_t off = static_cast<std::size_t>(idx) * 3u;
            out.put(static_cast<char>(FieldVga::palette[off]));
            out.put(static_cast<char>(FieldVga::palette[off + 1]));
            out.put(static_cast<char>(FieldVga::palette[off + 2]));
        }
    }
    return static_cast<bool>(out);
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
    const std::filesystem::path outDir = (argc >= 2) ? argv[1] : "build/grabs/doom";
    std::filesystem::create_directories(outDir);

    FieldDosConfig::applyPreset(FieldDosConfig::Preset::Full);
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    std::uint8_t* ram = nullptr;
    FieldX86Emu::Ctx ctx{};

    if (!bootToPrompt(buf, ram, ctx)) return 1;
    if (!FieldBios::guestBootSettled) {
        std::fprintf(stderr, "WARN boot did not settle — launching DOOM anyway\n");
        FieldBios::guestBootSettled = true;
    }

    if (!FieldDoom::launch(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, ram)) {
        std::fprintf(stderr, "FAIL FieldDoom::launch\n");
        return 1;
    }

    auto* e0 = FieldX86Emu::emu;
    const std::uint32_t ep = (static_cast<std::uint32_t>(e0->x86.R_CS & 0xFFFFu) << 4)
                           + (e0->x86.R_EIP & 0xFFFFu);
    std::fprintf(stderr, "DOOM GPU-CPU launch cs=%04x ip=%04x ep@%05x\n",
        static_cast<unsigned>(e0->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(e0->x86.R_EIP & 0xFFFFu), ep);

    int bestNz = 0;
    int bestRound = -1;
    std::uint8_t bestMode = 0;
    const std::uint32_t cycles = 8'000'000u;
    for (int round = 0; round < 16; ++round) {
        FieldDoom::pump(ram, buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, 0u, false, cycles);
        const std::uint8_t mode = ram[0x449u];
        const int nz = (mode == 0x13u) ? countFbNz(ram, 0, 200) : 0;
        if (mode != 0u && bestMode == 0u) bestMode = mode;
        if (nz > bestNz) {
            bestNz = nz;
            bestRound = round;
            bestMode = mode;
        }
        if (round % 6 == 0) {
            auto* e = FieldX86Emu::emu;
            std::fprintf(stderr, "round=%d mode=%u nz=%d cs=%04x ip=%04x pm=%d ticks=%llu\n",
                round, static_cast<unsigned>(mode), nz,
                static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
                static_cast<unsigned>(FieldDpmi::isProtected(e) ? e->x86.R_EIP : (e->x86.R_EIP & 0xFFFFu)),
                FieldDpmi::isProtected(e) ? 1 : 0,
                static_cast<unsigned long long>(ctx.ticks));
        }
        if (mode == 0x13u && nz > 15000) break;
    }

    if ((bestMode != 0x13u || bestNz < 8000) && FieldX86Emu::emu
            && FieldRtxPm::pm32TitleStalled(FieldX86Emu::emu, bestMode, bestNz)) {
        std::fprintf(stderr, "PM32 title probe stalled — forceTitleBlit fallback\n");
        if (FieldDoomGpu::forceTitleBlit(ram)) {
            bestMode = ram[0x449u];
            bestNz = countFbNz(ram, 0, 200);
            bestRound = 99;
        }
    }

    const auto ppm = outDir / "doom_title.ppm";
    const bool wrote = (bestMode == 0x13u) && writeMode13Ppm(ppm, ram);
    std::printf("METRIC doom_mode=%u\n", static_cast<unsigned>(bestMode));
    std::printf("METRIC doom_fb_nz=%d\n", bestNz);
    std::printf("METRIC doom_best_round=%d\n", bestRound);
    std::printf("METRIC doom_ticks=%llu\n", static_cast<unsigned long long>(ctx.ticks));
    std::printf("METRIC doom_ppm=%d\n", wrote ? 1 : 0);
    if (wrote)
        std::printf("GRAB %s\n", ppm.string().c_str());

    if (bestMode != 0x13u || bestNz < 8000) {
        char text[80 * 25 + 1]{};
        for (int i = 0; i < 80 * 25; ++i)
            text[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
        std::fprintf(stderr, "VGA text tail:\n%.80s\n", text + 80 * 20);
        std::fprintf(stderr, "FAIL doom title screen (mode=%u nz=%d)\n",
            static_cast<unsigned>(bestMode), bestNz);
        return 1;
    }
    std::printf("OK doom gpu-cpu title screen\n");
    return 0;
}