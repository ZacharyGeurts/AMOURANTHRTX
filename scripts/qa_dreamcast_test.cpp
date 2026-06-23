#include "CHIPS/Dreamcast/FieldDreamcast.hpp"

#include <cstdio>
#include <memory>
#include <vector>

int main() {
    auto st = std::make_unique<FieldChips::Dreamcast::State>();
    std::vector<std::uint8_t> rom(32768, 0x5A);
    if (!FieldChips::Dreamcast::loadCart(*st, rom.data(), rom.size())) {
        std::fprintf(stderr, "FAIL dreamcast loadCart\n");
        return 1;
    }
    for (int f = 0; f < 48; ++f)
        FieldChips::Dreamcast::runFrame(*st);
    int nz = 0;
    for (auto px : st->pvr.fb)
        if (px != 0) ++nz;
    std::printf("METRIC dreamcast_gpu_wave=%d\n", st->pvr.waveActive ? 1 : 0);
    std::printf("METRIC dreamcast_die_cycles=%u\n", st->pvr.dieCycles);
    std::printf("METRIC dreamcast_fb_nz=%d\n", nz);
    if (!st->pvr.waveActive || st->pvr.dieCycles < 8192u || nz < 10000) {
        std::fprintf(stderr, "FAIL dreamcast PVR wave\n");
        return 1;
    }
    std::printf("OK qa_dreamcast_test FieldDie PowerVR2 wave\n");
    return 0;
}