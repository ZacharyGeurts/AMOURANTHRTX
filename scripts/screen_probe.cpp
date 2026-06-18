#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

static void dumpSearch(const std::uint8_t* ram, const char* needle) {
    char buf[80 * 25 + 1]{};
    for (int i = 0; i < 80 * 25; ++i)
        buf[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
    const char* hit = std::strstr(buf, needle);
    std::printf("needle '%s' %s\n", needle, hit ? "FOUND" : "missing");
    if (hit) std::printf("  context: %.40s\n", hit);
}

int main() {
    const std::size_t bytes = FieldPlatform::FIELD_X86_DIE_UINTS * sizeof(std::uint32_t);
    std::vector<std::uint8_t> buf(bytes, 0);
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldDos::defaultImagePath("."));
    auto* ram = FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4);
    FieldX86Emu::Ctx ctx{};
    for (int round = 0; round < 6; ++round) {
        FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
            FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 8'000'000u);
    }
    dumpSearch(ram, ": HD1,");
    dumpSearch(ram, "Press F8");
    dumpSearch(ram, "C:\\>");
    auto* e = FieldX86Emu::emu;
    std::printf("before launch cs=%04x ip=%04x\n",
        static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu));
    std::vector<std::uint8_t> com;
    const bool got = FieldDos::readHostFile("C:\\MINISHL.COM", com);
    std::printf("readHostFile=%d size=%zu\n", got ? 1 : 0, com.size());
    const bool launched = got && FieldBios::launchComImage(e, com);
    std::printf("launchComImage=%d cs=%04x ip=%04x\n", launched ? 1 : 0,
        static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu));
    FieldX86Emu::syncToDie(buf.data());
    FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 16'000'000u);
    dumpSearch(ram, "C:\\>");
    dumpSearch(ram, "C:");
    int nz = 0;
    for (int y = 40; y < 120; ++y)
        for (int x = 0; x < 320; ++x)
            if (ram[0xA0000u + y * 320 + x]) ++nz;
    std::printf("after run cs=%04x ip=%04x mode=%u vgaNz=%d titlePx sample=%u\n",
        static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_EIP & 0xFFFFu),
        static_cast<unsigned>(ram[0x449]),
        static_cast<unsigned>(ram[0xA0000u + 100 * 320 + 100]));
    return 0;
}