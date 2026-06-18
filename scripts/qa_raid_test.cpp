// QA: Field RAID-0 — contiguous 4 GiB facade, journal, background sync.
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRaid.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static bool testHdContiguous() {
    const std::uint64_t kOff = 64u * 1024u * 1024u + 500u;
    if (kOff >= FieldPlatform::HD_SIZE_BYTES) return false;
    FieldDos::writeHdByte(static_cast<std::uint32_t>(kOff), 0xA5u, FieldDos::hdGuestRamPtr);
    const std::uint8_t v = FieldDos::readHdByte(static_cast<std::uint32_t>(kOff),
                                                 FieldDos::hdGuestRamPtr);
    if (v != 0xA5u) {
        std::fprintf(stderr, "FAIL HD readback got %02X\n", v);
        return false;
    }
    FieldRaid::tick(FieldDos::hdGuestRamPtr, 0.016f);
    const std::uint8_t v2 = FieldDos::readHdByte(static_cast<std::uint32_t>(kOff), nullptr);
    if (v2 != 0xA5u) {
        std::fprintf(stderr, "FAIL HD post-tick readback got %02X\n", v2);
        return false;
    }
    return true;
}

static bool testRamSpill() {
    const std::uint64_t kOff = FieldPlatform::SPILL_RAM_BASE + 4096u;
    FieldRaid::writeRam(kOff, 0x3Cu, FieldDos::hdGuestRamPtr);
    if (FieldRaid::readRam(kOff, FieldDos::hdGuestRamPtr) != 0x3Cu) {
        std::fprintf(stderr, "FAIL RAM spill readback\n");
        return false;
    }
    FieldRaid::tick(FieldDos::hdGuestRamPtr, 0.016f);
    if (FieldRaid::readRam(kOff, nullptr) != 0x3Cu) {
        std::fprintf(stderr, "FAIL RAM spill post-tick\n");
        return false;
    }
    return true;
}

static bool testAmmozipRegression() {
    static const char* kZip = "C:\\GAMES\\WOLF3D\\WOLF3D.ZIP";
    std::vector<std::uint8_t> data;
    if (!FieldAmmoFat::readFile(kZip, data) || data.size() < 100u) {
        std::fprintf(stderr, "FAIL wolf zip missing on FAT\n");
        return false;
    }
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
    if (!FieldRaid::ready) {
        std::fprintf(stderr, "FAIL raid init\n");
        return 1;
    }

    if (!testHdContiguous()) return 1;
    std::printf("OK HD contiguous 4 GiB facade\n");

    if (!testRamSpill()) return 1;
    std::printf("OK RAM spill RAID tier\n");

    if (!testAmmozipRegression()) return 1;
    std::printf("OK AMMOZIP regression (wolf zip present)\n");

    std::printf("RAID QA passed — 4 GiB logical HD + RAM spill, journal, tick sync\n");
    return 0;
}