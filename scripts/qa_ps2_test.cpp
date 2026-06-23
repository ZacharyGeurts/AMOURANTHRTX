#include "CHIPS/PS2/FieldPs2.hpp"

#include <cstdio>
#include <memory>
#include <vector>

int main() {
    auto st = std::make_unique<FieldChips::Ps2::State>();
    std::vector<std::uint8_t> rom(65536, 0xA5);
    if (!FieldChips::Ps2::loadCart(*st, rom.data(), rom.size())) {
        std::fprintf(stderr, "FAIL ps2 loadCart\n");
        return 1;
    }
    for (int f = 0; f < 48; ++f)
        FieldChips::Ps2::runFrame(*st);
    int nz = 0;
    for (auto px : st->gs.fb)
        if (px != 0) ++nz;
    std::printf("METRIC ps2_gpu_wave=%d\n", st->gs.waveActive ? 1 : 0);
    std::printf("METRIC ps2_die_cycles=%u\n", st->gs.dieCycles);
    std::printf("METRIC ps2_fb_nz=%d\n", nz);
    if (!st->gs.waveActive || st->gs.dieCycles < 8192u || nz < 10000) {
        std::fprintf(stderr, "FAIL ps2 GS wave\n");
        return 1;
    }
    std::printf("OK qa_ps2_test FieldDie GS wave\n");
    return 0;
}