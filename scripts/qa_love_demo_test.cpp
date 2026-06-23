#include "FieldDos.hpp"
#include "FieldEverything.hpp"
#include "FieldPlatform.hpp"
#include "FieldStorage.hpp"

#include <cstdio>
#include <vector>

int main() {
    const auto root = FieldDos::assetRoot().string();
    if (!FieldStorage::mountMultiFS(root.c_str())) {
        std::fprintf(stderr, "FAIL mountMultiFS\n");
        return 1;
    }
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    std::uint8_t* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * sizeof(std::uint32_t));
    FieldEverything::open();
    int totalNz = 0;
    for (int f = 0; f < 60; ++f) {
        FieldEverything::tick(ram);
        for (int i = 0; i < 320 * 200; i += 97)
            if (ram[0xA0000u + static_cast<std::uint32_t>(i)]) ++totalNz;
    }
    std::printf("METRIC love_demo_frames=60\n");
    std::printf("METRIC love_demo_ticks=%u\n", FieldEverything::tickCount);
    std::printf("METRIC love_demo_nz=%d\n", totalNz);
    std::printf("METRIC love_demo_complete=%d\n",
        (FieldEverything::active && FieldEverything::tickCount >= 60u && totalNz > 100) ? 1 : 0);
    if (!FieldEverything::active || FieldEverything::tickCount < 60u || totalNz <= 100) {
        std::fprintf(stderr, "FAIL Love of EVERYTHING demo\n");
        return 1;
    }
    std::printf("OK qa_love_demo_test Love of EVERYTHING canvas\n");
    return 0;
}