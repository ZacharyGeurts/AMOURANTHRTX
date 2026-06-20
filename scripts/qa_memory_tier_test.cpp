// QA: RTX memory octree — 512 KiB boot, in-place pop/dismiss, extenders on demand.
#include "FieldRegistry.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldRtxMemTree.hpp"

#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* name) {
    if (ok) std::printf("OK %s\n", name);
    else {
        std::fprintf(stderr, "FAIL %s\n", name);
        ++g_fail;
    }
}

int main() {
    FieldRegistry::loaded = false;
    FieldRegistry::hive.clear();
    FieldRegistry::seedDefaults();
    FieldRegistry::applyMemoryConfig();

    check(FieldRtxMemory::bootConventionalKb == 512u, "boot tier 512K");
    check(FieldRtxMemory::conventionalKb == 512u, "active tier 512K at boot");
    check(!FieldRtxMemTree::isLive(FieldRtxMemTree::Kind::Xms), "XMS idle at boot");
    check(FieldRtxMemory::bootClearBytes() == 0x00080000u, "boot touch 512K only");

    std::uint8_t ram[0x500]{};
    FieldRtxMemory::patchGuest(ram);
    check(ram[0x413] == 0x00u && ram[0x414] == 0x02u, "BDA 0x413 reports 512K");

    check(FieldRtxMemory::growConventional(640, nullptr), "grow to 640K");
    check(FieldRtxMemTree::isLive(FieldRtxMemTree::Kind::Conventional), "conv octree live");
    check(FieldRtxMemory::popXms(nullptr), "pop XMS in-place");
    check(FieldRtxMemory::popEms(nullptr), "pop EMM386 in-place");
    check(FieldRtxMemory::popMscdex(), "pop MSCDEX in-place");
    check(FieldRtxMemory::xmsLive() && FieldRtxMemory::emmLive()
            && FieldRtxMemory::mscdexLive(), "extenders live");

    FieldRtxMemory::dismissExtenders();
    FieldRtxMemory::dismissConventional();
    check(!FieldRtxMemory::xmsLive(), "XMS dismissed");
    check(FieldRtxMemory::conventionalKb == 512u, "back to 512K");

    const std::uint64_t resume = FieldRtxMemTree::resumeAt(FieldRtxMemTree::Kind::Xms);
    check(resume == FieldPlatform::XMS_POOL_BYTE, "XMS resume checkpoint");

    if (g_fail) {
        std::fprintf(stderr, "qa_memory_tier_test: %d failure(s)\n", g_fail);
        return 1;
    }
    std::printf("qa_memory_tier_test: all passed\n");
    return 0;
}