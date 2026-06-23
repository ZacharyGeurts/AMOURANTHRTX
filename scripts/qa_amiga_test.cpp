#include "CHIPS/Amiga/FieldAmiga.hpp"

#include <cstdio>
#include <memory>
#include <vector>

int main() {
    auto st = std::make_unique<FieldChips::Amiga::State>();
    std::vector<std::uint8_t> kick(8192, 0x4E);
    if (!FieldChips::Amiga::loadKickstart(*st, kick.data(), kick.size())) {
        std::fprintf(stderr, "FAIL amiga loadKickstart\n");
        return 1;
    }
    for (int f = 0; f < 40; ++f)
        FieldChips::Amiga::runFrame(*st);
    int nz = 0;
    for (auto px : st->denise.fb)
        if (px != 0) ++nz;
    std::printf("METRIC amiga_love_score=%d\n", st->loveScore);
    std::printf("METRIC amiga_paula_level=%.3f\n", st->paula.level);
    std::printf("METRIC amiga_fb_nz=%d\n", nz);
    std::printf("METRIC amiga_aga=%d\n", st->denise.aga ? 1 : 0);
    if (st->loveScore < 10 || nz < 5000) {
        std::fprintf(stderr, "FAIL amiga love canvas\n");
        return 1;
    }
    std::printf("OK qa_amiga_test Love of EVERYTHING\n");
    return 0;
}