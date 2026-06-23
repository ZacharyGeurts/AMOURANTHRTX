#include "FieldDos.hpp"
#include "FieldStorage.hpp"
#include "FieldStorageHyper.hpp"
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
    std::printf("METRIC sdf_logical_gb=%.2f\n",
        static_cast<double>(FieldStorage::sdf.logicalBytes) / (1024.0 * 1024.0 * 1024.0));
    std::printf("METRIC wave_phase=%.6f\n", FieldStorage::sdf.phase);
    std::printf("METRIC sdf_folded_blocks=%llu\n",
        static_cast<unsigned long long>(FieldStorage::sdf.foldedBlocks));

    const char* vfsProbe = "VFS:\\probe.sdf";
    if (!FieldStorage::vfsBridgeWrite(vfsProbe,
            reinterpret_cast<const std::uint8_t*>("sdf_wave"), 8u)) {
        std::fprintf(stderr, "FAIL vfsBridgeWrite\n");
        return 1;
    }
    std::vector<std::uint8_t> vfsRead;
    if (!FieldStorage::vfsBridgeRead(vfsProbe, vfsRead) || vfsRead.empty()) {
        std::fprintf(stderr, "FAIL vfsBridgeRead\n");
        return 1;
    }
    std::printf("METRIC vfs_bridge_ok=1\n");

    FieldStorage::enableAllBreakthroughs(true);
    FieldStorage::sdfFoldBlock(42u);
    std::printf("METRIC hyper_breakthroughs=%d\n", FieldStorage::hyperEnabled() ? 1 : 0);
    std::printf("METRIC hyper_lead_in=%.4f\n", FieldStorage::hyperLeadInPeak());
    std::printf("METRIC hyper_lead_out=%.4f\n", FieldStorage::hyperLeadOutPeak());
    std::printf("METRIC hyper_entropy_fold=%.4f\n", FieldStorage::hyperEntropyFold());
    std::printf("METRIC hyper_fabric_scale=%.4f\n", FieldStorage::chipsFabricScale());
    std::printf("METRIC hyper_phase_in=%.4f\n", FieldStorage::hyper.phaseVelocityIn);
    std::printf("METRIC hyper_phase_out=%.4f\n", FieldStorage::hyper.phaseVelocityOut);
    std::printf("METRIC hyper_resonance=%.4f\n", FieldStorage::hyper.resonanceCoupling);
    std::printf("METRIC hyper_phi_super=%.4f\n", FieldStorage::hyperPhiSuperposition());
    std::printf("METRIC end_game_mode=%d\n", FieldStorage::endGameMode() ? 1 : 0);
    if (FieldStorage::hyperPhiSuperposition() <= 0.0) {
        std::fprintf(stderr, "FAIL phi superposition not active\n");
        return 1;
    }
    if (!FieldStorage::hyperEnabled() || FieldStorage::chipsFabricScale() < 1.0) {
        std::fprintf(stderr, "FAIL hyper breakthroughs not active\n");
        return 1;
    }

    if (FieldStorage::sdf.logicalBytes < 2ull * 1024u * 1024u * 1024u) {
        std::fprintf(stderr, "FAIL sdf logical below 2GB anchor\n");
        return 1;
    }

    if (wBps < 1024u || rBps < 1024u) {
        std::fprintf(stderr, "FAIL bench throughput\n");
        return 1;
    }
    std::printf("OK fieldstorage v2 multi-fs dual-host\n");
    return 0;
}