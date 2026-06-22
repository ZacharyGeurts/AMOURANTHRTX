#include "FieldDos.hpp"
#include "FieldStorage.hpp"
#include "FieldPlatform.hpp"

#include <cstdio>
#include <vector>

int main() {
    const auto root = FieldDos::assetRoot().string();
    if (!FieldStorage::mountMultiFS(root.c_str())) {
        std::fprintf(stderr, "FAIL mountMultiFS\n");
        return 1;
    }
    std::printf("METRIC fs_mounts=%zu\n", FieldStorage::mounts.size());

    const char* blob = "fieldstorage_v2_bo_probe";
    if (!FieldStorage::writePath("probe.bin", reinterpret_cast<const std::uint8_t*>(blob),
            std::strlen(blob))) {
        std::fprintf(stderr, "FAIL writePath\n");
        return 1;
    }
    std::vector<std::uint8_t> readback;
    if (!FieldStorage::readPath("probe.bin", readback) || readback.empty()) {
        std::fprintf(stderr, "FAIL readPath\n");
        return 1;
    }
    std::printf("METRIC fs_read_bytes=%zu\n", readback.size());

    const std::size_t ramBytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> ram(ramBytes, 0);
    if (!FieldStorage::activateLinux(ram.data(), ram.size())) {
        std::fprintf(stderr, "FAIL activateLinux\n");
        return 1;
    }
    if (!FieldStorage::activateWindows(ram.data(), ram.size())) {
        std::fprintf(stderr, "FAIL activateWindows\n");
        return 1;
    }
    if (!FieldStorage::dualHostReady()) {
        std::fprintf(stderr, "FAIL dualHostReady\n");
        return 1;
    }
    std::printf("METRIC dual_host=1\n");

    const std::uint64_t wBps = FieldStorage::benchWrite("bench.bin", 65536u, 8u);
    const std::uint64_t rBps = FieldStorage::benchRead("bench.bin", 65536u, 8u);
    std::printf("METRIC bench_write_bps=%llu\n", static_cast<unsigned long long>(wBps));
    std::printf("METRIC bench_read_bps=%llu\n", static_cast<unsigned long long>(rBps));
    std::printf("METRIC bo_gain=%.3f\n", FieldStorage::boGain());

    if (wBps < 1024u || rBps < 1024u) {
        std::fprintf(stderr, "FAIL bench throughput\n");
        return 1;
    }
    std::printf("OK fieldstorage v2 multi-fs dual-host\n");
    return 0;
}