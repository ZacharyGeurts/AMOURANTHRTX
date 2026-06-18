// QA: embedded RTX-DOS 32 MiB slice boots without external rtx_dos_hd.img.
#include "FieldDos.hpp"
#include "FieldDosEmbed.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxFieldAbs.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

int main() {
#ifdef FIELD_DOS_EMBED_HD
    if (!FieldDosEmbed::slice() || FieldDosEmbed::sliceSize() < 512u) {
        std::fprintf(stderr, "FAIL embed slice missing\n");
        return 1;
    }
    std::printf("OK embed slice %zu bytes (full HD %zu)\n",
        FieldDosEmbed::sliceSize(), FieldDosEmbed::fullHdSize());

    FieldDos::hdReady = false;
    FieldDos::hdImage.clear();
    if (!FieldDos::loadHdFromEmbedded()) {
        std::fprintf(stderr, "FAIL loadHdFromEmbedded\n");
        return 1;
    }
    if (!FieldDos::hdReady || FieldDos::hdImage.size() != FieldDosEmbed::fullHdSize()) {
        std::fprintf(stderr, "FAIL hd image size\n");
        return 1;
    }
    if (FieldDos::hdImage[510] != 0x55 || FieldDos::hdImage[511] != 0xAA) {
        std::fprintf(stderr, "FAIL missing MBR signature at LBA0\n");
        return 1;
    }
    const std::size_t partOff = FieldPlatform::HD_PART_LBA * 512u;
    if (FieldDos::hdImage.size() < partOff + 2u
        || FieldDos::hdImage[partOff] != 0xEB) {
        std::fprintf(stderr, "FAIL missing FAT boot sector at partition LBA\n");
        return 1;
    }

    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    if (!FieldDos::loadHdIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4)) {
        std::fprintf(stderr, "FAIL loadHdIntoGuest\n");
        return 1;
    }
    const auto* ram = buf.data() + FieldPlatform::DIE_HEADER_UINTS * 4;
    if (ram[FieldRtxFieldAbs::BOOT_VECTOR] != 0xEB) {
        std::fprintf(stderr, "FAIL guest boot sector at 0x7C00\n");
        return 1;
    }
    std::printf("OK embedded RTX-DOS HD mirror in guest RAM\n");
    return 0;
#else
    std::fprintf(stderr, "SKIP FIELD_DOS_EMBED_HD not defined\n");
    return 0;
#endif
}