#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"

#include <cstdio>
#include <vector>

int main() {
    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))) {
        std::fprintf(stderr, "FAIL hd load\n");
        return 1;
    }
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL mount\n");
        return 1;
    }
    std::vector<FieldDos::FatRootEntry> ent;
    if (!FieldAmmoFat::listRoot(ent) || ent.empty()) {
        std::fprintf(stderr, "FAIL listRoot\n");
        return 1;
    }
    char info[256];
    if (!FieldAmmoFat::formatInfo(info, sizeof info)) {
        std::fprintf(stderr, "FAIL formatInfo\n");
        return 1;
    }
    std::printf("OK AMMOFAT %zu entries\n%s", ent.size(), info);

    static const char stamp[] = "RTX-AMMOS AMMOFAT write test 2026\r\n";
    if (!FieldAmmoFat::writeRootFile("C:\\AMMOTEST.TXT",
            reinterpret_cast<const std::uint8_t*>(stamp), sizeof stamp - 1u)) {
        std::fprintf(stderr, "FAIL writeRootFile\n");
        return 1;
    }
    std::vector<std::uint8_t> readBack;
    if (!FieldAmmoFat::readFile("C:\\AMMOTEST.TXT", readBack)) {
        std::fprintf(stderr, "FAIL read back\n");
        return 1;
    }
    readBack.push_back(0);
    if (std::strstr(reinterpret_cast<char*>(readBack.data()), "AMMOFAT write test") == nullptr) {
        std::fprintf(stderr, "FAIL read content\n");
        return 1;
    }
    std::printf("OK AMMOFAT write/read AMMOTEST.TXT\n");
    return 0;
}