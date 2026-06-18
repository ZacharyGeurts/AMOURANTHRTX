#include "FieldDos.hpp"
#include <cstdio>
int main() {
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    FieldDos::loadFloppyIntoGuest(nullptr, 0, FieldDos::defaultImagePath(".")); // won't work
    std::vector<std::uint8_t> buf(8*1024*1024,0);
    FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4, FieldDos::defaultImagePath("."));
    std::vector<std::uint8_t> out;
    bool ok = FieldDos::readHostFile("C:\\MINISHL.COM", out);
    std::printf("C MINISHL ok=%d size=%zu\n", ok?1:0, out.size());
    ok = FieldDos::readHostFile("C:\\DOOM.EXE", out);
    std::printf("C DOOM ok=%d size=%zu\n", ok?1:0, out.size());
    return 0;
}