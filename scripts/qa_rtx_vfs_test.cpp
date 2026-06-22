// QA: RTX-AMMOS DOS 7 VFS + deep MZ/PE throttle (no SDL shell deps).
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldRtxThrottle.hpp"
#include "FieldRtxVfs.hpp"

#include <cstdio>
#include <vector>

int main() {
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL AMMOFAT mount\n");
        return 1;
    }

    FieldRtxVfs::vfsInit();
    FieldRtxVfs::FileMeta sanity;
    if (!FieldRtxVfs::lookupMeta("COMMAND.COM", sanity) || sanity.longDesc.empty()) {
        std::fprintf(stderr, "FAIL builtin metadata COMMAND.COM\n");
        return 1;
    }
    std::printf("OK VFS metadata COMMAND.COM\n");

    FieldRtxVfs::loadDescriptIon();
    FieldRtxVfs::FileMeta golden;
    if (!FieldRtxVfs::lookupMeta("GOLDEN.TXT", golden) || golden.longDesc.empty()) {
        std::fprintf(stderr, "FAIL DESCRIPT.ION GOLDEN.TXT (rebuild HD: ./linux.sh dos --force)\n");
        return 1;
    }
    std::printf("OK DESCRIPT.ION GOLDEN.TXT\n");

    static const std::uint8_t kOldMz[] = {
        0x4D, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    const auto legacy = FieldRtxThrottle::analyzeBytes(kOldMz, sizeof kOldMz);
    if (legacy.profile != FieldRtxThrottle::Profile::LegacyMz
        || legacy.cycles != FieldRtxThrottle::kLegacyCycles) {
        std::fprintf(stderr, "FAIL legacy MZ throttle cycles=%u profile=%u\n",
            legacy.cycles, static_cast<unsigned>(legacy.profile));
        return 1;
    }
    std::printf("OK throttle legacy MZ %u cycles\n", legacy.cycles);

    FieldRtxThrottle::apply(legacy);
    if (FieldRtxThrottle::activeCycles() != FieldRtxThrottle::kLegacyCycles) {
        std::fprintf(stderr, "FAIL throttle apply\n");
        return 1;
    }
    std::printf("OK throttle apply\n");

    std::vector<FieldRtxVfs::RichEntry> entries;
    if (!FieldRtxVfs::listPathRich("C:\\", false, true, entries) || entries.empty()) {
        std::fprintf(stderr, "FAIL rich DIR C:\\\n");
        return 1;
    }
    bool sawColor = false;
    bool sawMeta = false;
    for (const auto& e : entries) {
        if (e.vgaAttr != 0x07u) sawColor = true;
        if (e.desc && e.desc[0]) sawMeta = true;
    }
    if (!sawColor) {
        std::fprintf(stderr, "FAIL colorful DIR attrs\n");
        return 1;
    }
    if (!sawMeta) {
        std::fprintf(stderr, "FAIL DIR metadata descriptions\n");
        return 1;
    }
    std::printf("OK colorful DIR %zu entries with metadata\n", entries.size());

    const std::uint32_t freeK = FieldRtxVfs::getFreeConvMem() / 1024u;
    const std::uint32_t bootK = FieldRtxMemory::bootConventionalKb;
    const std::uint32_t minFree = bootK > 28u ? bootK - 28u : bootK / 2u;
    if (freeK < minFree || freeK > FieldRtxMemory::maxConventionalKb) {
        std::fprintf(stderr, "FAIL conv mem freeK=%u (bootK=%u min=%u)\n",
            freeK, bootK, minFree);
        return 1;
    }
    std::printf("OK conv mem %uK free\n", freeK);

    std::vector<FieldRtxVfs::RichEntry> txts;
    FieldRtxVfs::listPathRich("C:\\", false, true, txts);
    FieldRtxVfs::filterByWildcard(txts, "*.TXT");
    if (txts.empty()) {
        std::fprintf(stderr, "FAIL wildcard *.TXT\n");
        return 1;
    }
    std::printf("OK wildcard DIR *.TXT (%zu hits)\n", txts.size());

    FieldRtxVfs::vfsReload();
    if (FieldRtxVfs::metadata.size() < 10u) {
        std::fprintf(stderr, "FAIL vfsReload metadata count\n");
        return 1;
    }
    std::printf("OK vfsReload %zu metadata entries\n", FieldRtxVfs::metadata.size());

    std::printf("RTX-AMMOS VFS + throttle QA passed\n");
    return 0;
}