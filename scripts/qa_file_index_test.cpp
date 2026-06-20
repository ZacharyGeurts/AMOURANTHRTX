// QA: GPU-backed drive indexer — sorted flat index + Start-menu search flyout.
#include "FieldAmouranthDnD.hpp"
#include "FieldAmouranthFileIndex.hpp"
#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthSearchFlyout.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static bool pathsSorted(const std::vector<const FieldAmouranthFileIndex::Entry*>& hits) {
    for (std::size_t i = 1; i < hits.size(); ++i) {
        if (hits[i - 1]->sortKey > hits[i]->sortKey)
            return false;
    }
    return true;
}

static void searchAndCheck(const char* query, int maxExpected) {
    FieldAmouranthSearchFlyout::clearQuery();
    for (const char* p = query; *p; ++p)
        FieldAmouranthSearchFlyout::appendChar(*p);
    if (FieldAmouranthSearchFlyout::resultCount <= 0) {
        std::fprintf(stderr, "FAIL search '%s' — no results\n", query);
        std::exit(1);
    }
    if (FieldAmouranthSearchFlyout::resultCount > FieldAmouranthFileIndex::SEARCH_MAX) {
        std::fprintf(stderr, "FAIL search '%s' — too many results (%d)\n",
            query, FieldAmouranthSearchFlyout::resultCount);
        std::exit(1);
    }
    if (!pathsSorted(FieldAmouranthSearchFlyout::hits)) {
        std::fprintf(stderr, "FAIL search '%s' — results not alphabetical\n", query);
        std::exit(1);
    }
    if (maxExpected > 0 && FieldAmouranthSearchFlyout::resultCount > maxExpected) {
        std::fprintf(stderr, "FAIL search '%s' — expected <= %d got %d\n",
            query, maxExpected, FieldAmouranthSearchFlyout::resultCount);
        std::exit(1);
    }
    std::printf("OK search '%s' — %d results (alphabetical)\n",
        query, FieldAmouranthSearchFlyout::resultCount);
}

int main() {
    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))) {
        std::fprintf(stderr, "FAIL hd load\n");
        return 1;
    }
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL fat mount\n");
        return 1;
    }

    FieldAmouranthFileIndex::build();
    if (!FieldAmouranthFileIndex::built || FieldAmouranthFileIndex::entries.empty()) {
        std::fprintf(stderr, "FAIL index empty after build\n");
        return 1;
    }
    std::printf("OK index built — %zu files\n", FieldAmouranthFileIndex::entries.size());

    searchAndCheck("doom", 10);
    searchAndCheck("wolf", 10);
    searchAndCheck("exe", 10);

    std::vector<std::uint8_t> ram(FieldPlatform::GUEST_RAM_BYTES, 0);
    FieldAmouranthSearchFlyout::clearQuery();
    FieldAmouranthSearchFlyout::appendChar('d');
    FieldAmouranthSearchFlyout::appendChar('o');
    FieldAmouranthSearchFlyout::appendChar('o');
    FieldAmouranthSearchFlyout::appendChar('m');
    FieldAmouranthSearchFlyout::hover = 0;
    FieldAmouranthSearchFlyout::packRam(ram.data());
    if (ram[FieldAmouranthHudRam::FILE_SEARCH_RAM + 3u] == 0u) {
        std::fprintf(stderr, "FAIL FILE_SEARCH_RAM active flag not set\n");
        return 1;
    }
    if (ram[FieldAmouranthHudRam::FILE_SEARCH_RAM] == 0u) {
        std::fprintf(stderr, "FAIL FILE_SEARCH_RAM result count zero\n");
        return 1;
    }
    std::printf("OK FILE_SEARCH_RAM packed for GPU flyout\n");

    FieldAmouranthDnD::begin("C:\\TEST\\FILE.EXE", 4u, 1, 100.f, 200.f);
    FieldAmouranthDnD::packRam(ram.data());
    if (ram[FieldAmouranthHudRam::DND_RAM] != 1u) {
        std::fprintf(stderr, "FAIL DND_RAM phase not Dragging\n");
        return 1;
    }
    FieldAmouranthDnD::cancel();
    std::printf("OK DND_RAM scaffold packs ghost state\n");

    return 0;
}