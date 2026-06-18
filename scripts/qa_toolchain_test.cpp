// QA: RTX-AMMOS DevKit v4 — asm/link/run/decomp + AMMOCC parser.
#include "FieldAmmoAsm.hpp"
#include "FieldAmmoDbg.hpp"
#include "FieldAmmoDecomp.hpp"
#include "FieldAmmoDisasm.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoLink.hpp"
#include "FieldAmmoRun.hpp"
#include "FieldAmmoTools.hpp"
#include "FieldAmmoCc.hpp"
#include "FieldFieldCc.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
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

static const char* kHelloAsm =
    ".MODEL TINY\n.CODE\nORG 100h\nstart:\n"
    "    mov ah,9\n    mov dx,offset msg\n    int 21h\n"
    "    mov ax,4C00h\n    int 21h\n.DATA\n"
    "msg db 'Hello RTX-AMMOS 2026+',13,10,'$'\nEND start\n";

static const char* kUtilAsm =
    ".MODEL TINY\n.CODE\nPUBLIC RTXPUT\nRTXPUT:\n"
    "    mov ah,2\n    mov dl,'*'\n    int 21h\n    ret\nEND\n";

static const char* kMainAsm =
    ".MODEL TINY\n.CODE\nORG 100h\nEXTRN RTXPUT\nstart:\n"
    "    call RTXPUT\n    mov ah,9\n    mov dx,offset msg\n    int 21h\n"
    "    mov ax,4C00h\n    int 21h\n.DATA\n"
    "msg db ' MULTI v3',13,10,'$'\nEND start\n";

int main() {
    std::vector<std::uint8_t> buf(FieldPlatform::GUEST_RAM_BYTES, 0);
    std::uint8_t* ram = buf.data();

    if (!FieldDos::loadHdImage(FieldDos::defaultHdPath("."))) {
        std::fprintf(stderr, "FAIL hd load\n");
        return 1;
    }
    if (!FieldAmmoFat::mount()) {
        std::fprintf(stderr, "FAIL mount\n");
        return 1;
    }

    FieldAmmoFat::writeRootFile("C:\\HELLO.ASM",
        reinterpret_cast<const std::uint8_t*>(kHelloAsm), std::strlen(kHelloAsm));

    auto asmR = FieldAmmoAsm::assembleSource(kHelloAsm, std::strlen(kHelloAsm));
    if (!asmR.ok) {
        std::fprintf(stderr, "FAIL assemble: %s\n", asmR.error.c_str());
        return 1;
    }
    std::printf("OK AMMOASM v4 object %zu bytes\n", asmR.object.size());

    auto linkR = FieldAmmoLink::linkObject(asmR.object);
    if (!linkR.ok || linkR.com.size() < 16u) {
        std::fprintf(stderr, "FAIL link: %s\n", linkR.error.c_str());
        return 1;
    }
    std::printf("OK AMMOLINK COM %zu bytes\n", linkR.com.size());

    FieldAmmoFat::writeRootFile("C:\\HELLO.COM", linkR.com.data(), linkR.com.size());

    std::memset(ram + 0xB8000u, 0, 80u * 25u * 2u);
    auto runR = FieldAmmoRun::runPath(ram, "C:\\HELLO.COM", echoChar, echoNl);
    if (!runR.ok || !screenHas(ram, "Hello RTX-AMMOS")) {
        std::fprintf(stderr, "FAIL run: %s\n", runR.error.c_str());
        return 1;
    }
    std::printf("OK AMMORUN steps=%u\n", runR.steps);

    auto utilR = FieldAmmoAsm::assembleSource(kUtilAsm, std::strlen(kUtilAsm));
    auto mainR = FieldAmmoAsm::assembleSource(kMainAsm, std::strlen(kMainAsm));
    if (!utilR.ok || !mainR.ok) {
        std::fprintf(stderr, "FAIL multi asm util=%s main=%s\n",
            utilR.error.c_str(), mainR.error.c_str());
        return 1;
    }
    auto multi = FieldAmmoLink::linkObjects({utilR.object, mainR.object});
    if (!multi.ok) {
        std::fprintf(stderr, "FAIL multi link: %s\n", multi.error.c_str());
        return 1;
    }
    FieldAmmoFat::writeRootFile("C:\\MULTI.COM", multi.com.data(), multi.com.size());
    std::memset(ram + 0xB8000u, 0, 80u * 25u * 2u);
    runR = FieldAmmoRun::runCom(ram, multi.com, echoChar, echoNl, multi.entryOffset);
    if (!runR.ok || !screenHas(ram, "*") || !screenHas(ram, "MULTI v3")) {
        std::fprintf(stderr, "FAIL multi run\n");
        return 1;
    }
    std::printf("OK multi-module link+run\n");

    const char* kCc =
        "void main(){\n"
        "  int n;\n"
        "  n = 3;\n"
        "  rtx_puts(\"CCv4\\r\\n\");\n"
        "  if (n < 5) rtx_puts(\"ok\\r\\n\");\n"
        "  return 2;\n"
        "}\n";
    FieldAmmoFat::writeRootFile("C:\\TINY.C",
        reinterpret_cast<const std::uint8_t*>(kCc), std::strlen(kCc));
    auto ccR = FieldAmmoCc::compilePath("C:\\TINY.C");
    if (!ccR.ok) {
        std::fprintf(stderr, "FAIL AMMOCC: %s\n", ccR.error.c_str());
        return 1;
    }
    std::printf("OK AMMOCC v4 object %zu bytes\n", ccR.asmResult.object.size());

    auto ccLink = FieldAmmoLink::linkObject(ccR.asmResult.object);
    if (!ccLink.ok) {
        std::fprintf(stderr, "FAIL AMMOCC link: %s\n", ccLink.error.c_str());
        return 1;
    }
    FieldAmmoFat::writeRootFile("C:\\TINY.COM", ccLink.com.data(), ccLink.com.size());
    std::memset(ram + 0xB8000u, 0, 80u * 25u * 2u);
    runR = FieldAmmoRun::runCom(ram, ccLink.com, echoChar, echoNl);
    if (!runR.ok || !screenHas(ram, "CCv4") || !screenHas(ram, "ok")) {
        std::fprintf(stderr, "FAIL AMMOCC run: %s\n", runR.error.c_str());
        return 1;
    }
    std::printf("OK AMMOCC v4 compile+run\n");

    const char* kFieldCc =
        "#include \"FIELD.H\"\n"
        "void main(){\n"
        "  int t;\n"
        "  t = TESLA_R_FWD_MILLI;\n"
        "  rtx_puts(\"FIELD\\r\\n\");\n"
        "  if (t == 180) rtx_puts(\"tesla\\r\\n\");\n"
        "  return 0;\n"
        "}\n";
    auto fieldCc = FieldAmmoCc::compileSource(kFieldCc, std::strlen(kFieldCc));
    if (!fieldCc.ok) {
        std::fprintf(stderr, "FAIL FIELD.H AMMOCC: %s\n", fieldCc.error.c_str());
        return 1;
    }
    auto fieldLink = FieldAmmoLink::linkObject(fieldCc.asmResult.object);
    if (!fieldLink.ok) {
        std::fprintf(stderr, "FAIL FIELD link: %s\n", fieldLink.error.c_str());
        return 1;
    }
    std::memset(ram + 0xB8000u, 0, 80u * 25u * 2u);
    runR = FieldAmmoRun::runCom(ram, fieldLink.com, echoChar, echoNl);
    if (!runR.ok || !screenHas(ram, "FIELD") || !screenHas(ram, "tesla")) {
        std::fprintf(stderr, "FAIL FIELD.H run\n");
        return 1;
    }
    std::printf("OK FIELD.H Tesla absolutes in AMMOCC\n");

    auto decomp = FieldAmmoDecomp::decompileComBytes(linkR.com);
    if (!decomp.ok || decomp.listing.find("int 21h") == std::string::npos) {
        std::fprintf(stderr, "FAIL AMMODECOMP\n");
        return 1;
    }
    FieldAmmoFat::writeRootFile("C:\\HELLO.LST",
        reinterpret_cast<const std::uint8_t*>(decomp.listing.data()), decomp.listing.size());
    std::printf("OK AMMODECOMP listing %zu bytes\n", decomp.listing.size());

    if (!FieldAmmoDbg::loadSession(ram, "C:\\HELLO.COM")) {
        std::fprintf(stderr, "FAIL AMMODBG load\n");
        return 1;
    }
    auto dbg = FieldAmmoDbg::stepSession(ram, echoChar, echoNl, 4u);
    if (!dbg.ok) {
        std::fprintf(stderr, "FAIL AMMODBG step: %s\n", dbg.error.c_str());
        return 1;
    }
    std::printf("OK AMMODBG v4 session step=%u\n", dbg.steps);

    const char* kFld =
        "field PADTEST\r\nprint \"Field pad QA\"\r\nreturn 0\r\n";
    FieldAmmoFat::writeRootFile("C:\\PADTEST.FLD",
        reinterpret_cast<const std::uint8_t*>(kFld), std::strlen(kFld));
    auto fcR = FieldFieldCc::compilePath("C:\\PADTEST.FLD");
    if (!fcR.ok) {
        std::fprintf(stderr, "FAIL FIELDC: %s\n", fcR.error.c_str());
        return 1;
    }
    std::printf("OK FIELDC v4 object %zu bytes\n", fcR.asmResult.object.size());

    auto build = FieldAmmoTools::buildAll({"BUILD", "HELLO"});
    if (!build.ok) {
        std::fprintf(stderr, "FAIL BUILD: %s\n", build.message.c_str());
        return 1;
    }
    std::printf("OK BUILD %s\n", build.message.c_str());

    std::printf("RTX-AMMOS DevKit v4 toolchain QA passed\n");
    return 0;
}