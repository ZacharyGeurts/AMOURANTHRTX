// QA: AMMOZIP — PKZIP extract wolf/cosmo archives on C: FAT.
#include "FieldAmmoFat.hpp"
#include "FieldAmmoTools.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldAmmoZip.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool zipOnFat(const char* path) {
    std::vector<std::uint8_t> data;
    return FieldAmmoFat::readFile(path, data) && data.size() > 100u;
}

static bool fileOnFat(const char* path, std::size_t minBytes) {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(path, data) || data.size() < minBytes) return false;
    if (std::strstr(path, ".EXE") && (data[0] != 'M' || data[1] != 'Z')) return false;
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

    static const char* kZips[] = {
        "C:\\GAMES\\WOLF3D\\WOLF3D.ZIP",
        "C:\\GAMES\\COSMO\\COSMO.ZIP",
    };
    for (const char* z : kZips) {
        if (!zipOnFat(z)) {
            std::fprintf(stderr, "FAIL missing %s (run stage_dos_games.py)\n", z);
            return 1;
        }
        std::printf("OK %s on FAT\n", z);
    }

    auto wolf = FieldAmmoTools::zipFile({"AMMOZIP", "C:\\GAMES\\WOLF3D\\WOLF3D.ZIP"});
    if (!wolf.ok) {
        std::fprintf(stderr, "FAIL AMMOZIP wolf3d: %s\n", wolf.message.c_str());
        return 1;
    }
    std::printf("OK AMMOZIP wolf3d — %s\n", wolf.message.c_str());

    if (!fileOnFat("C:\\GAMES\\WOLF3D\\WOLF3D.EXE", 64u)) {
        std::fprintf(stderr, "FAIL WOLF3D.EXE not extracted\n");
        return 1;
    }
    std::printf("OK WOLF3D.EXE extracted\n");

    auto cosmo = FieldAmmoTools::zipFile({"AMMOZIP", "C:\\GAMES\\COSMO\\COSMO.ZIP"});
    if (!cosmo.ok) {
        std::fprintf(stderr, "FAIL AMMOZIP cosmo: %s\n", cosmo.message.c_str());
        return 1;
    }
    std::printf("OK AMMOZIP cosmo — %s\n", cosmo.message.c_str());

    if (!fileOnFat("C:\\GAMES\\COSMO\\COSMO1.EXE", 64u)) {
        std::fprintf(stderr, "FAIL COSMO1.EXE not extracted\n");
        return 1;
    }
    std::printf("OK COSMO1.EXE extracted\n");

    std::printf("AMMOZIP QA passed (wolf3d + cosmo, no doom/keen)\n");
    return 0;
}