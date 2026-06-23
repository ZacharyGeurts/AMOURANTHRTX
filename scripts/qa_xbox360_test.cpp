#include "CHIPS/Xbox360/FieldXbox360.hpp"

#include <cstdio>
#include <memory>
#include <vector>

int main() {
    auto st = std::make_unique<FieldChips::Xbox360::State>();
    std::vector<std::uint8_t> img(4096, 0x5A);
    if (!FieldChips::Xbox360::loadImage(*st, img.data(), img.size())) {
        std::fprintf(stderr, "FAIL xbox360 loadImage\n");
        return 1;
    }
    for (int f = 0; f < 30; ++f)
        FieldChips::Xbox360::runFrame(*st);
    int nz = 0;
    for (auto px : st->gpu.fb)
        if (px != 0) ++nz;
    std::printf("METRIC xbox360_gpu_wave=%d\n", st->gpu.waveActive ? 1 : 0);
    std::printf("METRIC xbox360_die_cycles=%u\n", st->gpu.dieCycles);
    std::printf("METRIC xbox360_fb_nz=%d\n", nz);
    if (!st->gpu.waveActive || nz < 10000) {
        std::fprintf(stderr, "FAIL xbox360 Xenos die wave\n");
        return 1;
    }
    std::printf("OK qa_xbox360_test\n");
    return 0;
}