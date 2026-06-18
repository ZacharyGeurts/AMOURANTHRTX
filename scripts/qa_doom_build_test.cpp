// QA: BUILD DOOMBOOT with AMMOASM/AMMOCC + field absolutes; verify DOOM.EXE on C:.
#include "FieldAmmoCc.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoLink.hpp"
#include "FieldAmmoRun.hpp"
#include "FieldAmmoTools.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxFieldAbs.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static void echoChar(std::uint8_t* ram, char ch) {
    (void)ram;
    std::fputc(ch, stderr);
}

static void echoNl(std::uint8_t* ram) {
    (void)ram;
    std::fputc('\n', stderr);
}

static bool screenHas(const std::uint8_t* ram, const char* needle) {
    const std::size_t nlen = std::strlen(needle);
    if (nlen == 0) return true;
    char flat[80 * 25 + 1]{};
    int bi = 0;
    for (int i = 0; i < 80 * 25 && bi < 80 * 25; ++i) {
        const char c = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
        if (c) flat[bi++] = c;
    }
    flat[bi] = '\0';
    return std::strstr(flat, needle) != nullptr;
}

static const char* kDoomBootAsm =
    "INCLUDE FIELD.INC\n"
    ".MODEL TINY\n.CODE\nORG 100h\nstart:\n"
    "    mov ah,9\n    mov dx,offset msg\n    int 21h\n"
    "    mov ax,TESLA_R_FWD_MILLI\n    cmp ax,180\n    je ok\n"
    "    mov ah,9\n    mov dx,offset bad\n    int 21h\n"
    "    mov ax,4C01h\n    int 21h\n"
    "ok:\n"
    "    mov ah,9\n    mov dx,offset go\n    int 21h\n"
    "    mov ax,4C00h\n    int 21h\n"
    ".DATA\n"
    "msg db 'DOOMBOOT RTX-DOS 7.0 Field Die',13,10,'$'\n"
    "bad db 'Tesla valve mismatch',13,10,'$'\n"
    "go  db 'Launch GAMES/DOOM/DOOM.EXE',13,10,'$'\n"
    "END start\n";

static const char* kDoomBootC =
    "#include \"FIELD.H\"\n"
    "#include \"RTX.H\"\n"
    "void main() {\n"
    "    int fwd;\n"
    "    int rev;\n"
    "    fwd = TESLA_R_FWD_MILLI;\n"
    "    rev = TESLA_R_REV_MILLI;\n"
    "    rtx_puts(\"DOOMBOOT AMMOCC Field Die\\r\\n\");\n"
    "    if (fwd < rev) rtx_puts(\"Tesla valve OK\\r\\n\");\n"
    "    rtx_puts(\"Run GAMES/DOOM/DOOM.EXE\\r\\n\");\n"
    "    return 0;\n"
    "}\n";

int main() {
    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();

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

    std::vector<std::uint8_t> doomExe;
    if (!FieldAmmoFat::readFile("C:\\GAMES\\DOOM\\DOOM.EXE", doomExe) || doomExe.size() < 64u
        || doomExe[0] != 'M' || doomExe[1] != 'Z') {
        std::fprintf(stderr, "FAIL DOOM.EXE not on C:\\GAMES\\DOOM (run stage_dos_games.py)\n");
        return 1;
    }
    std::printf("OK DOOM.EXE on FAT (%zu bytes)\n", doomExe.size());

    if (FieldRtxFieldAbs::TESLA_R_FWD_MILLI != 180
        || FieldRtxFieldAbs::TESLA_R_REV_MILLI != 3200
        || FieldRtxFieldAbs::BUS_TESLA != 31) {
        std::fprintf(stderr, "FAIL engine field absolutes drift\n");
        return 1;
    }
    std::printf("OK field absolutes Tesla fwd=%d rev=%d bus=%u\n",
        FieldRtxFieldAbs::TESLA_R_FWD_MILLI,
        FieldRtxFieldAbs::TESLA_R_REV_MILLI,
        FieldRtxFieldAbs::BUS_TESLA);

    FieldAmmoFat::writeRootFile("C:\\DOOMBOOT.ASM",
        reinterpret_cast<const std::uint8_t*>(kDoomBootAsm), std::strlen(kDoomBootAsm));
    FieldAmmoFat::writeRootFile("C:\\DOOMBOOT.C",
        reinterpret_cast<const std::uint8_t*>(kDoomBootC), std::strlen(kDoomBootC));

    auto buildAsm = FieldAmmoTools::buildAll({"BUILD", "DOOMBOOT"});
    if (!buildAsm.ok) {
        std::fprintf(stderr, "FAIL BUILD DOOMBOOT.ASM: %s\n", buildAsm.message.c_str());
        return 1;
    }
    std::printf("OK %s\n", buildAsm.message.c_str());

    std::vector<std::uint8_t> com;
    if (!FieldAmmoFat::readFile("C:\\DOOMBOOT.COM", com) || com.empty()) {
        std::fprintf(stderr, "FAIL DOOMBOOT.COM missing\n");
        return 1;
    }
    std::memset(ram + 0xB8000u, 0, 80u * 25u * 2u);
    auto runR = FieldAmmoRun::runCom(ram, com, echoChar, echoNl);
    if (!runR.ok || !screenHas(ram, "DOOMBOOT") || !screenHas(ram, "DOOM")) {
        std::fprintf(stderr, "FAIL DOOMBOOT.ASM run: %s\n", runR.error.c_str());
        return 1;
    }
    std::printf("OK DOOMBOOT.ASM run steps=%u\n", runR.steps);

    auto ccR = FieldAmmoCc::compilePath("C:\\DOOMBOOT.C");
    if (!ccR.ok) {
        std::fprintf(stderr, "FAIL AMMOCC DOOMBOOT.C: %s\n", ccR.error.c_str());
        return 1;
    }
    auto ccLink = FieldAmmoLink::linkObject(ccR.asmResult.object);
    if (!ccLink.ok || ccLink.com.empty()) {
        std::fprintf(stderr, "FAIL link DOOMBOOT.C: %s\n", ccLink.error.c_str());
        return 1;
    }
    FieldAmmoFat::writeRootFile("C:\\DOOMBOOT_C.COM",
        ccLink.com.data(), ccLink.com.size());
    const std::vector<std::uint8_t>& comC = ccLink.com;
    std::memset(ram + 0xB8000u, 0, 80u * 25u * 2u);
    runR = FieldAmmoRun::runCom(ram, comC, echoChar, echoNl);
    if (!runR.ok || !screenHas(ram, "DOOMBOOT") || !screenHas(ram, "Tesla valve OK")) {
        std::fprintf(stderr, "FAIL DOOMBOOT.C run: %s\n", runR.error.c_str());
        return 1;
    }
    std::printf("OK DOOMBOOT.C AMMOCC compile+run steps=%u\n", runR.steps);

    std::printf("DOOMBOOT build QA passed (shareware DOOM.EXE ready on C:)\n");
    return 0;
}