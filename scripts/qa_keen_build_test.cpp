// QA: Commander Keen 4 on C: FAT + field toolchain sanity.
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxFieldAbs.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool fileOk(const char* path, std::size_t minBytes, bool needMz) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile(path, data) || data.size() < minBytes)
        return false;
    if (needMz && (data[0] != 'M' || data[1] != 'Z'))
        return false;
    return true;
}

int main() {
    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))
#ifdef FIELD_DOS_EMBED_HD
        && !FieldDos::loadHdFromEmbedded()
#endif
    ) {
        std::fprintf(stderr, "FAIL hd load\n");
        return 1;
    }
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL mount\n");
        return 1;
    }

    static const char* kFiles[] = {
        "C:\\GAMES\\KEEN4\\KEEN4E.EXE",
        "C:\\GAMES\\KEEN4\\AUDIO.CK4",
        "C:\\GAMES\\KEEN4\\CONFIG.CK4",
        "C:\\GAMES\\KEEN4\\EGAGRAPH.CK4",
        "C:\\GAMES\\KEEN4\\GAMEMAPS.CK4",
    };
    for (const char* path : kFiles) {
        const bool mz = std::strstr(path, ".EXE") != nullptr;
        if (!fileOk(path, mz ? 64u : 32u, mz)) {
            std::fprintf(stderr, "FAIL missing %s (run stage_dos_games.py)\n", path);
            return 1;
        }
    }
    std::vector<std::uint8_t> exe;
    FieldAmmoFat::readFile("C:\\GAMES\\KEEN4\\KEEN4E.EXE", exe);
    std::printf("OK KEEN4E.EXE on FAT (%zu bytes)\n", exe.size());

    if (FieldRtxFieldAbs::TESLA_R_FWD_MILLI != 180) {
        std::fprintf(stderr, "FAIL field absolutes\n");
        return 1;
    }
    std::printf("Commander Keen 4 build QA passed\n");
    return 0;
}