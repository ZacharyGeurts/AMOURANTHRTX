// QA: CHIPS PS1 core — PS-X EXE detect, FieldDie GPU wave, GTE + N64/DC/PS2 full wave.
#include "CHIPS/PS1/FieldPS1.hpp"
#include "CHIPS/N64/FieldN64.hpp"
#include "CHIPS/Dreamcast/FieldDreamcast.hpp"
#include "CHIPS/PS2/FieldPs2.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

static std::vector<std::uint8_t> makePsxExeStub() {
    std::vector<std::uint8_t> exe(0x1000, 0);
    std::memcpy(exe.data(), "PS-X EXE", 8);
    const std::uint32_t load = 0x80010000u;
    const std::uint32_t entry = 0x80010100u;
    std::memcpy(exe.data() + 0x10, &load, 4);
    std::memcpy(exe.data() + 0x14, &entry, 4);
    std::memcpy(exe.data() + 0x4A, "AMOURANTH PS1 QA", 16);
    exe[0x800] = 0x00; // NOP-ish
    exe[0x801] = 0x00;
    exe[0x802] = 0x00;
    exe[0x803] = 0x00;
    return exe;
}

int main() {
    auto exe = makePsxExeStub();
    if (!FieldChips::Ps1::detectPsxExe(exe.data(), exe.size())) {
        std::fprintf(stderr, "FAIL PS-X EXE header not detected\n");
        return 1;
    }
    std::printf("OK PS-X EXE header detected\n");

    auto st = std::make_unique<FieldChips::Ps1::State>();
    if (!FieldChips::Ps1::loadCart(*st, exe.data(), exe.size())) {
        std::fprintf(stderr, "FAIL loadCart\n");
        return 1;
    }
    if (!st->cart.psxExe) {
        std::fprintf(stderr, "FAIL cart not marked psxExe\n");
        return 1;
    }
    std::printf("OK loadCart title=%.31s entry=0x%08x\n", st->cart.title, st->cart.entryPc);

    for (int f = 0; f < 60; ++f)
        FieldChips::Ps1::runFrame(*st);

    int nz = 0;
    for (int i = 0; i < st->gpu.fbW * st->gpu.fbH; ++i)
        if (st->gpu.fb[i] != 0) ++nz;

    std::printf("METRIC ps1_gpu_wave=%d\n", st->gpu.waveActive ? 1 : 0);
    std::printf("METRIC ps1_die_cycles=%u\n", st->gpu.dieCycles);
    std::printf("METRIC ps1_fb_nz=%d\n", nz);
    std::printf("METRIC ps1_gte_ops=%d\n", st->gte.ops);
    std::printf("METRIC ps1_cpu_cycles=%u\n", st->cpu.cycles);

    if (!st->gpu.waveActive || st->gpu.dieCycles < 8192u) {
        std::fprintf(stderr, "FAIL GPU die wave inactive\n");
        return 1;
    }
    if (nz < 1000) {
        std::fprintf(stderr, "FAIL PS1 framebuffer too sparse nz=%d\n", nz);
        return 1;
    }

    std::vector<std::uint8_t> blob(8192, 0xA5u);
    auto n64 = std::make_unique<FieldChips::N64::State>();
    auto dc = std::make_unique<FieldChips::Dreamcast::State>();
    auto ps2 = std::make_unique<FieldChips::Ps2::State>();
    if (!FieldChips::N64::loadCart(*n64, blob.data(), blob.size())
            || !FieldChips::Dreamcast::loadCart(*dc, blob.data(), blob.size())
            || !FieldChips::Ps2::loadCart(*ps2, blob.data(), blob.size())) {
        std::fprintf(stderr, "FAIL N64/DC/PS2 loadCart\n");
        return 1;
    }
    for (int f = 0; f < 24; ++f) {
        FieldChips::N64::runFrame(*n64);
        FieldChips::Dreamcast::runFrame(*dc);
        FieldChips::Ps2::runFrame(*ps2);
    }
    std::printf("METRIC chips_n64_wave=%d\n", n64->rsp.waveActive ? 1 : 0);
    std::printf("METRIC chips_dc_wave=%d\n", dc->pvr.waveActive ? 1 : 0);
    std::printf("METRIC chips_ps2_wave=%d\n", ps2->gs.waveActive ? 1 : 0);
    if (!n64->rsp.waveActive || !dc->pvr.waveActive || !ps2->gs.waveActive) {
        std::fprintf(stderr, "FAIL CHIPS wave inactive\n");
        return 1;
    }
    std::printf("OK qa_ps1_test CHIPS PS1 + N64 + DC + PS2 FieldDie waves\n");
    return 0;
}