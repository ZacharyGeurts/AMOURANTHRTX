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
    for (int f = 0; f < 32; ++f)
        FieldEverything::tick(ram);
    FieldStorage::persistFieldState();
    FieldStorage::dismissAll();
    FieldStorage::mountMultiFS(root.c_str());
    const bool restored = FieldStorage::restoreFieldState();
    std::printf("METRIC everything_active=%d\n", FieldEverything::active ? 1 : 0);
    std::printf("METRIC everything_ticks=%u\n", FieldEverything::tickCount);
    std::printf("METRIC field_persist_restored=%d\n", restored ? 1 : 0);
    std::printf("METRIC ps1_live=%d\n", FieldPs1::active ? 1 : 0);
    std::printf("METRIC n64_live=%d\n", FieldN64::active ? 1 : 0);
    std::printf("METRIC amiga_live=%d\n", FieldAmiga::active ? 1 : 0);
    if (!FieldEverything::active || FieldEverything::tickCount < 32u) {
        std::fprintf(stderr, "FAIL everything everywhere tick\n");
        return 1;
    }
    if (!restored) {
        std::fprintf(stderr, "FAIL field persistence restore\n");
        return 1;
    }
    std::printf("OK qa_everything_test Everything Everywhere + resonance hold\n");
    return 0;
}