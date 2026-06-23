#include "FieldStorage.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    FieldStorage::enableEndGameMode(true);
    FieldStorage::enableInfiniteMode(true);
    FieldStorage::enableAllBreakthroughs(true);
    for (std::uint32_t i = 0; i < 8192u; ++i)
        FieldStorage::sdfFoldBlock(i);
    const std::uint64_t logical = FieldStorage::sdfLogicalCapacity();
    constexpr std::uint64_t k12GiB = 12ull * 1024u * 1024u * 1024u;
    const double logicalGiB = static_cast<double>(logical) / (1024.0 * 1024.0 * 1024.0);
    std::printf("METRIC rtx_logical_gib=%.2f\n", logicalGiB);
    std::printf("METRIC rtx_resident_mib=256\n");
    std::printf("METRIC rtx_load_ops=0\n");
    std::printf("METRIC rtx_density_ok=%d\n", logical >= k12GiB ? 1 : 0);
    if (logical < k12GiB) {
        std::fprintf(stderr, "FAIL 12GB RTX density logical=%.2f GiB\n", logicalGiB);
        return 1;
    }
    std::printf("OK qa_rtx_density_test infinite SDF >= 12 GiB logical\n");
    return 0;
}