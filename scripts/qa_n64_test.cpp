#include "CHIPS/N64/FieldN64.hpp"

#include <cstdio>
#include <memory>
#include <vector>

int main() {
    auto st = std::make_unique<FieldChips::N64::State>();
    std::vector<std::uint8_t> rom(16384, 0x4E);
    if (!FieldChips::N64::loadCart(*st, rom.data(), rom.size())) {
        std::fprintf(stderr, "FAIL n64 loadCart\n");
        return 1;
    }
    for (int f = 0; f < 48; ++f)
        FieldChips::N64::runFrame(*st);
    int nz = 0;
    for (auto px : st->rsp.fb)
        if (px != 0) ++nz;
    std::printf("METRIC n64_gpu_wave=%d\n", st->rsp.waveActive ? 1 : 0);
    std::printf("METRIC n64_die_cycles=%u\n", st->rsp.dieCycles);
    std::printf("METRIC n64_fb_nz=%d\n", nz);
    if (!st->rsp.waveActive || st->rsp.dieCycles < 8192u || nz < 5000) {
        std::fprintf(stderr, "FAIL n64 FieldDie wave\n");
        return 1;
    }
    std::printf("OK qa_n64_test FieldDie RSP/RDP wave\n");
    return 0;
}