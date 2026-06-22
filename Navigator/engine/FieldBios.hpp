#pragma once

// IBM PC/AT BIOS data area + INT 10/11/12/13/15/16/1A/19 stubs for MS-DOS 6.30 boot.

#include "FieldRtxShell.hpp"
#include "FieldAmmoShell.hpp"
#include "FieldCmos.hpp"
#include "FieldRtxBoot.hpp"
#include "FieldRtxDrivers.hpp"
#include "FieldDevices.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldMscdex.hpp"
#include "FieldInput.hpp"
#include "FieldSb16.hpp"
#include "FieldX86Emu.hpp"
#include "FieldX86Native.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>
#include "FieldDpmi.hpp"
#include "FieldDosInt.hpp"
#include "FieldEms.hpp"
#include "FieldInt21.hpp"
#include "FieldPc.hpp"
#include "FieldXms.hpp"
#include "FieldPlatform.hpp"
#include "FieldVga.hpp"

#include <x86emu.h>

#include <cstdint>

namespace FieldBios {

inline std::uint16_t syntheticKey = 0;
// guestBootSettled / rtxShellActive / pmExecActive — defined in FieldX86Emu.hpp (shared w/ runtime)
inline std::uint32_t lastHostKey = 0;
inline bool lastKeyDown = false;
inline std::uint16_t consumedHostKey = 0;
inline std::uint16_t keyQueue[16]{};
inline int keyQueueHead = 0;
inline int keyQueueTail = 0;

struct DosLineInput {
    std::uint32_t bufAddr = 0;
    std::uint8_t maxLen = 0;
    std::uint8_t count = 0;
    bool active = false;
};
inline DosLineInput dosLineInput{};
inline char hostShellLine[128]{};
inline int hostShellLen = 0;

constexpr std::uint32_t BDA_BASE     = 0x00000400u;
constexpr std::uint32_t VGA_BASE     = 0x000B8000u;
constexpr std::uint32_t DPT_ADDR      = 0x0000EFC0u;
constexpr std::uint16_t FLOPPY_MAX_CYL = 79u;   /* 720K: 80 cylinders 0-79 */
constexpr std::uint8_t  FLOPPY_SPT     = 9u;    /* sectors per track */
constexpr std::uint8_t  FLOPPY_HEADS   = 2u;

inline bool isFixedDisk(std::uint8_t dl) noexcept {
    return dl == 0x80u && FieldDos::hdReady;
}

inline std::uint32_t floppyOffset(std::uint16_t cyl, std::uint16_t head, std::uint16_t sector) {
    if (cyl > FLOPPY_MAX_CYL || head >= FLOPPY_HEADS) return 0xFFFFFFFFu;
    if (sector < 1 || sector > FLOPPY_SPT) return 0xFFFFFFFFu;
    return FieldDos::DISK_IMAGE_BASE
         + (cyl * (FLOPPY_SPT * FLOPPY_HEADS) + head * FLOPPY_SPT + (sector - 1u)) * 512u;
}

inline std::uint32_t diskOffset(std::uint8_t dl, std::uint16_t cyl, std::uint16_t head,
                                std::uint16_t sector) {
    if (isFixedDisk(dl))
        return FieldDos::hdLbaOffset(cyl, head, sector);
    return floppyOffset(cyl, head, sector);
}

inline void vgaPut(x86emu_t* e, std::uint16_t col, std::uint8_t ch, std::uint8_t attr) {
    const std::uint32_t off = VGA_BASE + col * 2u;
    x86emu_write_byte(e, off, ch);
    x86emu_write_byte(e, off + 1, attr);
}

inline std::uint16_t cursorPos(x86emu_t* e) {
    const std::uint8_t col = static_cast<std::uint8_t>(x86emu_read_byte(e, BDA_BASE + 0x50));
    const std::uint8_t row = static_cast<std::uint8_t>(x86emu_read_byte(e, BDA_BASE + 0x51));
    return static_cast<std::uint16_t>(row * 80u + col);
}

inline void setCursor(x86emu_t* e, std::uint16_t pos) {
    x86emu_write_byte(e, BDA_BASE + 0x50, static_cast<unsigned>(pos % 80u));
    x86emu_write_byte(e, BDA_BASE + 0x51, static_cast<unsigned>(pos / 80u));
}

inline void alignPromptCursor(x86emu_t* e) noexcept {
    setCursor(e, static_cast<std::uint16_t>(23u * 80u + 4u));
}

inline char biosKeyToChar(std::uint16_t key) noexcept {
    if (key == 0x1C0Du || key == 0x0Du || key == '\r') return '\r';
    if (key == 0x0E08u) return '\b';
    return static_cast<char>(key & 0xFFu);
}

inline void scrollVgaUp(x86emu_t* e) {
    for (int row = 0; row < 24; ++row) {
        for (int col = 0; col < 80; ++col) {
            const std::uint16_t dst = static_cast<std::uint16_t>(row * 80 + col);
            const std::uint16_t src = static_cast<std::uint16_t>((row + 1) * 80 + col);
            const std::uint32_t doff = VGA_BASE + static_cast<std::uint32_t>(dst * 2u);
            const std::uint32_t soff = VGA_BASE + static_cast<std::uint32_t>(src * 2u);
            x86emu_write_byte(e, doff, static_cast<u8>(x86emu_read_byte(e, soff)));
            x86emu_write_byte(e, doff + 1u, static_cast<u8>(x86emu_read_byte(e, soff + 1u)));
        }
    }
    for (int col = 0; col < 80; ++col)
        vgaPut(e, static_cast<std::uint16_t>(24 * 80 + col), ' ', 0x07);
}

inline void echoDosChar(x86emu_t* e, char ch) {
    if (ch == '\r') return;
    if (ch == '\n') {
        std::uint16_t pos = cursorPos(e);
        const std::uint16_t row = static_cast<std::uint16_t>(pos / 80u + 1u);
        if (row >= 25u) {
            scrollVgaUp(e);
            setCursor(e, static_cast<std::uint16_t>(24 * 80u));
        } else {
            setCursor(e, static_cast<std::uint16_t>(row * 80u));
        }
        return;
    }
    if (ch == '\b') {
        std::uint16_t pos = cursorPos(e);
        if (pos > 0) {
            --pos;
            vgaPut(e, pos, ' ', 0x07);
            setCursor(e, pos);
        }
        return;
    }
    std::uint16_t pos = cursorPos(e);
    if (pos / 80u >= 25u) {
        scrollVgaUp(e);
        pos = static_cast<std::uint16_t>(24 * 80u);
    }
    vgaPut(e, pos, static_cast<std::uint8_t>(ch), 0x07);
    ++pos;
    if ((pos % 80u) == 0u) {
        if (pos / 80u >= 25u) {
            scrollVgaUp(e);
            pos = static_cast<std::uint16_t>(24 * 80u);
        }
    }
    setCursor(e, pos);
}

inline void enqueueHostKey(std::uint16_t key) noexcept {
    if (!key) return;
    keyQueue[keyQueueTail % 16] = key;
    keyQueueTail = (keyQueueTail + 1) % 16;
    if (keyQueueTail == keyQueueHead)
        keyQueueHead = (keyQueueHead + 1) % 16;
}

inline std::uint16_t pullHostKey() noexcept {
    if (keyQueueHead != keyQueueTail) {
        const std::uint16_t k = keyQueue[keyQueueHead];
        keyQueueHead = (keyQueueHead + 1) % 16;
        return k;
    }
    if (syntheticKey) {
        const std::uint16_t k = syntheticKey;
        syntheticKey = 0;
        return k;
    }
    if (FieldInput::state.keyboard.biosKey)
        return FieldInput::state.keyboard.biosKey;
    const std::uint16_t lo = static_cast<std::uint16_t>(lastHostKey & 0xFFFFu);
    if (lo && lo != consumedHostKey) {
        consumedHostKey = lo;
        return lo;
    }
    const std::uint16_t hi = static_cast<std::uint16_t>((lastHostKey >> 16) & 0xFFFFu);
    if (hi && hi != consumedHostKey) {
        consumedHostKey = hi;
        return hi;
    }
    return 0;
}

inline std::uint16_t peekHostKey() noexcept {
    if (keyQueueHead != keyQueueTail)
        return keyQueue[keyQueueHead];
    if (syntheticKey) return syntheticKey;
    if (FieldInput::state.keyboard.biosKey) return FieldInput::state.keyboard.biosKey;
    const std::uint16_t lo = static_cast<std::uint16_t>(lastHostKey & 0xFFFFu);
    if (lo && lo != consumedHostKey) return lo;
    const std::uint16_t hi = static_cast<std::uint16_t>((lastHostKey >> 16) & 0xFFFFu);
    if (hi && hi != consumedHostKey) return hi;
    return 0;
}

inline void hostShellNewline(x86emu_t* e) {
    echoDosChar(e, '\n');
}

inline void hostShellPrompt(x86emu_t* e) {
    hostShellNewline(e);
    for (int i = 0; i < 4; ++i)
        echoDosChar(e, "C:\\>"[i]);
    hostShellLen = 0;
    hostShellLine[0] = '\0';
}

inline void hostShellExec(x86emu_t* e, std::uint8_t* guestRam) {
    if (hostShellLen <= 0) {
        hostShellPrompt(e);
        return;
    }
    hostShellLine[hostShellLen] = '\0';
    std::memcpy(FieldRtxShell::shellLine, hostShellLine, sizeof FieldRtxShell::shellLine);
    FieldRtxShell::shellLen = hostShellLen;
    auto echo = [e](std::uint8_t*, char c) { echoDosChar(e, c); };
    auto nl = [e](std::uint8_t*) { hostShellNewline(e); };
    auto prompt = [e](std::uint8_t*) { hostShellPrompt(e); };
    FieldRtxShell::execLine(FieldRtxShell::shellLine, guestRam, echo, nl, prompt);
    hostShellLen = 0;
    hostShellLine[0] = '\0';
}

inline void pumpRtxShellGpu(std::uint8_t* guestRam, std::uint32_t hostKey, bool keyDown) noexcept {
    if (!rtxShellActive || !guestRam) return;
    FieldRtxShell::pumpInteractive(guestRam, hostKey, keyDown);
}

inline void pumpHostShell(x86emu_t* e, std::uint32_t hostKey, bool keyDown,
                          std::uint8_t* guestRam) {
    static std::uint32_t prevShellKey = 0;
    if (!guestBootSettled) return;
    if (FieldRtxShell::graphicsActive && keyDown && hostKey != 0u) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (key == 0x011Bu || (key & 0xFFu) == 27u) {
            FieldRtxShell::setTextMode(guestRam);
            FieldRtxBoot::paintWelcome(guestRam);
            hostShellLen = 0;
            hostShellLine[0] = '\0';
            prevShellKey = hostKey;
            return;
        }
    }
    if (!keyDown || hostKey == 0u) {
        prevShellKey = 0;
        return;
    }
    if (hostKey == prevShellKey) return;
    prevShellKey = hostKey;
    if (FieldRtxShell::graphicsActive) return;
    const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
    const char ch = biosKeyToChar(key);
    if (ch == '\r') {
        hostShellExec(e, guestRam);
        return;
    }
    if (ch == '\b') {
        if (hostShellLen > 0) {
            --hostShellLen;
            hostShellLine[hostShellLen] = '\0';
            echoDosChar(e, '\b');
        }
        return;
    }
    if (ch >= 32 && hostShellLen + 1 < static_cast<int>(sizeof hostShellLine)) {
        hostShellLine[hostShellLen++] = ch;
        hostShellLine[hostShellLen] = '\0';
        echoDosChar(e, ch);
    }
}

inline void seedKernelDecompressorStack(x86emu_t* e) {
    /* KERNEL.SYS stub does RETF with SS:SP = 12CE:38CF after decompress. */
    x86emu_write_word(e, 0x12CE0u + 0x38CFu, 0x0100u);
    x86emu_write_word(e, 0x12CE0u + 0x38CFu + 2u, 0x0268u);
}

inline void patchConventionalKb(x86emu_t* e, std::uint8_t* ram) noexcept {
    if (e) {
        x86emu_write_word(e, BDA_BASE + 0x13, FieldRtxMemory::conventionalKb);
        x86emu_write_word(e, 0x0413u, static_cast<u16>(FieldRtxMemory::conventionalKb));
    }
    if (!ram) ram = FieldX86Emu::ramHost;
    FieldRtxMemory::patchGuest(ram);
}

inline void init(x86emu_t* e) {
    if (!e) return;

    FieldCmos::init(FieldDos::hdReady);

    x86emu_write_word(e, BDA_BASE + 0x10, 0x54F3u); /* VGA + coproc + 2 floppies + 101-key */
    FieldRegistry::applyMemoryConfig();
    patchConventionalKb(e, nullptr);
    x86emu_write_word(e, BDA_BASE + 0x15, 0x0200u); /* user wait / lpt timeout */
    x86emu_write_byte(e, BDA_BASE + 0x17, FieldDos::hdReady ? 0x80u : 0x00u);
    x86emu_write_byte(e, BDA_BASE + 0x18, 0x70u);   /* fixed disk count */
    x86emu_write_byte(e, BDA_BASE + 0x19, 0x02u);   /* ctrl disk count */
    x86emu_write_byte(e, BDA_BASE + 0x49, 3);       /* video mode: text 80x25 color */
    x86emu_write_byte(e, BDA_BASE + 0x4A, 80);      /* columns */
    x86emu_write_byte(e, 0x44Au, 80);
    x86emu_write_byte(e, 0x484u, 24);              /* rows - 1 for 25 line display */
    x86emu_write_byte(e, BDA_BASE + 0x50, 0);
    x86emu_write_byte(e, BDA_BASE + 0x51, 0);
    x86emu_write_byte(e, BDA_BASE + 0x63, 0x03u);   /* floppy motor status */
    x86emu_write_byte(e, BDA_BASE + 0x75, FieldDos::hdReady ? 2 : 1);
    x86emu_write_word(e, BDA_BASE + 0x7C, static_cast<u16>(FieldPlatform::EXTENDED_KB));
    x86emu_write_byte(e, 0xFFFFEu, 0x00u);
    x86emu_write_byte(e, 0xFFFFFu, 0xFCu);         /* BIOS model: 80386 */

    static const std::uint8_t dpt[11] = {
        0xDF, 0x02, 0x25, 0x02, 0x09, 0x2A, 0xFF, 0x50, 0xF6, 0x0F, 0x08
    };
    for (int i = 0; i < 11; ++i)
        x86emu_write_byte(e, DPT_ADDR + static_cast<std::uint32_t>(i), dpt[i]);

    x86emu_write_word(e, BDA_BASE + 0x78, static_cast<u16>(DPT_ADDR));
    x86emu_write_word(e, BDA_BASE + 0x7A, 0);
    x86emu_write_word(e, 0x1Eu * 4u, static_cast<u16>(DPT_ADDR));
    x86emu_write_word(e, 0x1Eu * 4u + 2u, 0);

    for (int i = 0; i < 80 * 25; ++i)
        vgaPut(e, static_cast<std::uint16_t>(i), ' ', 0x07);

    seedKernelDecompressorStack(e);
    FieldVga::init(nullptr);
    FieldPc::init(e);
}

inline bool screenHas(x86emu_t* e, const char* needle) {
    char buf[80 * 25 + 1]{};
    for (int i = 0; i < 80 * 25; ++i)
        buf[i] = static_cast<char>(x86emu_read_byte(e, VGA_BASE + static_cast<std::uint32_t>(i * 2)));
    return std::strstr(buf, needle) != nullptr;
}

inline void patchPattern(x86emu_t* e, std::uint32_t base, std::uint32_t loadSize,
                         const std::uint8_t* probe, std::size_t probeLen,
                         const std::uint8_t* patch, std::size_t patchLen,
                         const char* tag) noexcept {
    for (std::uint32_t i = 0; i + probeLen <= loadSize; ++i) {
        bool hit = true;
        for (std::size_t j = 0; j < probeLen; ++j) {
            if (x86emu_read_byte(e, base + i + static_cast<std::uint32_t>(j)) != probe[j]) {
                hit = false;
                break;
            }
        }
        if (!hit) continue;
        for (std::size_t j = 0; j < patchLen; ++j)
            x86emu_write_byte(e, base + i + static_cast<std::uint32_t>(j), patch[j]);
        std::fprintf(stderr, "[DOS4GW] patched %s @%08x\n", tag, base + i);
    }
}

inline void patchDos4gwCpuProbe(x86emu_t* e, std::uint32_t base, std::uint32_t loadSize) noexcept {
    /* smsw ax; or ax,[0040h]; lmsw ax */
    static const std::uint8_t kProbeOr40[] = {
        0x0Fu, 0x01u, 0xE0u, 0x0Bu, 0x06u, 0x40u, 0x00u, 0x0Fu, 0x01u, 0xF0u,
    };
    static const std::uint8_t kPatchOr40[] = {
        0xB8u, 0x10u, 0x00u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u,
    };
    patchPattern(e, base, loadSize, kProbeOr40, sizeof kProbeOr40, kPatchOr40, sizeof kPatchOr40,
        "386 SMSW-or-40");

    /* smsw ax; or al,1; lmsw ax */
    static const std::uint8_t kProbeOrAl1[] = {
        0x0Fu, 0x01u, 0xE0u, 0x0Cu, 0x01u, 0x0Fu, 0x01u, 0xF0u,
    };
    static const std::uint8_t kPatchOrAl1[] = {
        0xB8u, 0x11u, 0x00u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u,
    };
    patchPattern(e, base, loadSize, kProbeOrAl1, sizeof kProbeOrAl1, kPatchOrAl1, sizeof kPatchOrAl1,
        "386 SMSW-or-al1");

    /* smsw ax; test al,4 (NE/Machine-check style 386 gate) */
    static const std::uint8_t kProbeTest4[] = { 0x0Fu, 0x01u, 0xE0u, 0xA8u, 0x04u };
    static const std::uint8_t kPatchTest4[] = { 0xB8u, 0x05u, 0x00u, 0x90u, 0x90u };
    patchPattern(e, base, loadSize, kProbeTest4, sizeof kProbeTest4, kPatchTest4, sizeof kPatchTest4,
        "386 SMSW-test4");

    /* CPUID leaf-1 family probe (DOS/4GW stub ~0x83fb): force EAX=0x400 (family 4 in AH). */
    static const std::uint8_t kProbeCpuid1[] = {
        0x66u, 0xB8u, 0x01u, 0x00u, 0x00u, 0x00u, 0x0Fu, 0xA2u,
    };
    static const std::uint8_t kPatchCpuid1[] = {
        0x66u, 0xB8u, 0x00u, 0x04u, 0x00u, 0x00u, 0x90u, 0x90u,
    };
    patchPattern(e, base, loadSize, kProbeCpuid1, sizeof kProbeCpuid1, kPatchCpuid1, sizeof kPatchCpuid1,
        "386 CPUID-leaf1");

    /* F000:FFFE BIOS model gate — force AX=0xE1 (386+) regardless of ROM byte. */
    static const std::uint8_t kProbeBiosModel[] = {
        0xB8u, 0x00u, 0x00u, 0x26u, 0x8Au, 0x0Eu, 0xFEu, 0xFFu, 0x07u, 0x80u, 0xF9u, 0xE1u,
    };
    static const std::uint8_t kPatchBiosModel[] = {
        0xB8u, 0x00u, 0x00u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u, 0x90u,
    };
    patchPattern(e, base, loadSize, kProbeBiosModel, sizeof kProbeBiosModel, kPatchBiosModel,
        sizeof kPatchBiosModel, "386 BIOS-FFFE");

    /* CPU tier gate wants SI==11 (486-class) after BIOS probe sets SI on AX!=0 path. */
    static const std::uint8_t kProbeSi16[] = { 0xBEu, 0x10u, 0x00u, 0x83u, 0xFEu, 0x02u };
    static const std::uint8_t kPatchSi11[] = { 0xBEu, 0x0Bu, 0x00u, 0x90u, 0x90u, 0x90u };
    patchPattern(e, base, loadSize, kProbeSi16, sizeof kProbeSi16, kPatchSi11, sizeof kPatchSi11,
        "386 CPU-SI11");

    /* Early tier gate: cmp si,11 / jz -> unconditional jz path. */
    static const std::uint8_t kProbeSi11[] = { 0x83u, 0xFEu, 0x0Bu, 0x74u, 0x03u };
    static const std::uint8_t kPatchSiJmp[] = { 0x83u, 0xFEu, 0x0Bu, 0xEBu, 0x03u };
    patchPattern(e, base, loadSize, kProbeSi11, sizeof kProbeSi11, kPatchSiJmp, sizeof kPatchSiJmp,
        "386 CPU-SI11-jz");

    static const std::uint8_t kProbeSi11nz[] = { 0x83u, 0xFEu, 0x0Bu, 0x75u, 0x0Au };
    static const std::uint8_t kPatchSi11nz[] = { 0x83u, 0xFEu, 0x0Bu, 0x90u, 0x90u };
    patchPattern(e, base, loadSize, kProbeSi11nz, sizeof kProbeSi11nz, kPatchSi11nz, sizeof kPatchSi11nz,
        "386 CPU-SI11-nz");

    for (std::uint32_t i = 0; i + 3u <= loadSize; ++i) {
        if (x86emu_read_byte(e, base + i) != 0x0Fu
            || x86emu_read_byte(e, base + i + 1u) != 0x01u
            || x86emu_read_byte(e, base + i + 2u) != 0xE0u)
            continue;
        x86emu_write_byte(e, base + i, 0xB8u);
        x86emu_write_byte(e, base + i + 1u, 0x10u);
        x86emu_write_byte(e, base + i + 2u, 0x00u);
        std::fprintf(stderr, "[DOS4GW] patched 386 SMSW ax @%08x\n", base + i);
    }

}

inline bool launchMzImage(x86emu_t* e, const std::vector<std::uint8_t>& img, std::uint16_t loadSeg = 0x1000u) {
    if (img.size() < 32u || img[0] != 'M' || img[1] != 'Z') return false;
    const std::uint16_t hdrPar = static_cast<std::uint16_t>(img[8] | (img[9] << 8));
    const std::uint32_t hdrBytes = static_cast<std::uint32_t>(hdrPar) * 16u;
    if (hdrBytes >= img.size()) return false;
    const std::uint16_t e_ip = static_cast<std::uint16_t>(img[0x14] | (img[0x15] << 8));
    const std::uint16_t e_cs = static_cast<std::uint16_t>(img[0x16] | (img[0x17] << 8));
    const std::uint16_t e_ss = static_cast<std::uint16_t>(img[0x0E] | (img[0x0F] << 8));
    const std::uint16_t e_sp = static_cast<std::uint16_t>(img[0x10] | (img[0x11] << 8));
    const std::uint32_t base = static_cast<std::uint32_t>(loadSeg) << 4;
    const std::uint32_t loadSize = static_cast<std::uint32_t>(img.size()) - hdrBytes;
    FieldDpmi::dos4gwMzHdrBytes = hdrBytes;
    FieldDpmi::dos4gwLeOff = 0u;
    for (std::uint32_t off = hdrBytes; off + 0x84u < img.size(); off += 4u) {
        if (img[off] != 'L' || img[off + 1u] != 'E' || img[off + 2u] != 0u || img[off + 3u] != 0u)
            continue;
        const std::uint32_t nobj = static_cast<std::uint32_t>(img[off + 0x44u])
            | (static_cast<std::uint32_t>(img[off + 0x45u]) << 8)
            | (static_cast<std::uint32_t>(img[off + 0x46u]) << 16)
            | (static_cast<std::uint32_t>(img[off + 0x47u]) << 24);
        const std::uint32_t psz  = static_cast<std::uint32_t>(img[off + 0x28u])
            | (static_cast<std::uint32_t>(img[off + 0x29u]) << 8)
            | (static_cast<std::uint32_t>(img[off + 0x2Au]) << 16)
            | (static_cast<std::uint32_t>(img[off + 0x2Bu]) << 24);
        if (nobj > 0u && nobj <= 64u && psz >= 512u && psz <= 65536u) {
            FieldDpmi::dos4gwLeOff = off - hdrBytes;
            break;
        }
    }
    const std::uint16_t pspSeg = static_cast<std::uint16_t>(loadSeg - 0x10u);
    const std::uint32_t pspBase = static_cast<std::uint32_t>(pspSeg) << 4;
    for (std::uint32_t i = 0; i < 0x100u; ++i)
        x86emu_write_byte(e, pspBase + i, 0);
    x86emu_write_byte(e, pspBase, 0xCDu);
    x86emu_write_byte(e, pspBase + 1u, 0x20u);
    x86emu_write_word(e, pspBase + 0x16u, 0x0000u);
    for (int fh = 0; fh < 20; ++fh)
        x86emu_write_byte(e, pspBase + 0x20u + static_cast<std::uint32_t>(fh), 0xFFu);
    for (std::uint32_t i = 0; i < loadSize; ++i)
        x86emu_write_byte(e, base + i, img[hdrBytes + i]);
    patchDos4gwCpuProbe(e, base, loadSize);
    const std::uint16_t relCount = static_cast<std::uint16_t>(img[6] | (img[7] << 8));
    const std::uint16_t relOff  = static_cast<std::uint16_t>(img[0x18] | (img[0x19] << 8));
    for (std::uint16_t ri = 0; ri < relCount; ++ri) {
        const std::uint32_t ro = static_cast<std::uint32_t>(relOff) + ri * 4u;
        if (ro + 4u > img.size()) break;
        const std::uint16_t roff = static_cast<std::uint16_t>(img[ro] | (img[ro + 1] << 8));
        const std::uint16_t rseg = static_cast<std::uint16_t>(img[ro + 2] | (img[ro + 3] << 8));
        const std::uint32_t addr = base + (static_cast<std::uint32_t>(rseg) << 4) + roff;
        const std::uint16_t word = static_cast<std::uint16_t>(
            x86emu_read_byte(e, addr) | (static_cast<std::uint16_t>(x86emu_read_byte(e, addr + 1)) << 8));
        const std::uint16_t fixed = static_cast<std::uint16_t>(word + loadSeg);
        x86emu_write_byte(e, addr, static_cast<unsigned>(fixed & 0xFFu));
        x86emu_write_byte(e, addr + 1, static_cast<unsigned>(fixed >> 8));
    }
    FieldDpmi::leaveProtected16(e);
    e->x86.crx[0] &= ~1u;
    e->x86.mode &= ~(_MODE_CODE32 | _MODE_DATA32 | _MODE_STACK32);
    x86emu_set_seg_register(e, e->x86.R_CS_SEL, static_cast<u16>(loadSeg + e_cs));
    x86emu_set_seg_register(e, e->x86.R_DS_SEL, loadSeg);
    x86emu_set_seg_register(e, e->x86.R_ES_SEL, loadSeg);
    x86emu_set_seg_register(e, e->x86.R_SS_SEL, static_cast<u16>(loadSeg + e_ss));
    e->x86.R_CS_ACC = e->x86.R_DS_ACC = e->x86.R_ES_ACC = e->x86.R_SS_ACC = 0x9Bu;
    e->x86.R_CS_LIMIT = e->x86.R_DS_LIMIT = e->x86.R_ES_LIMIT = e->x86.R_SS_LIMIT = 0xFFFFu;
    e->x86.R_EIP = e_ip;
    e->x86.R_ESP = e_sp;
    e->x86.R_EAX &= 0xFFFF0000u;
    e->x86.mode &= ~_MODE_HALTED;
    e->x86.R_FLG |= F_IF;
    return true;
}

inline bool launchComImage(x86emu_t* e, const std::vector<std::uint8_t>& com, std::uint16_t pspSeg = 0x0800u) {
    if (com.empty() || com.size() > 65280u) return false;
    const std::uint32_t base = static_cast<std::uint32_t>(pspSeg) << 4;
    for (std::uint32_t i = 0; i < 0x100u; ++i)
        x86emu_write_byte(e, base + i, 0);
    for (std::size_t i = 0; i < com.size(); ++i)
        x86emu_write_byte(e, base + 0x100u + static_cast<std::uint32_t>(i), com[i]);
    x86emu_set_seg_register(e, e->x86.R_CS_SEL, pspSeg);
    x86emu_set_seg_register(e, e->x86.R_DS_SEL, pspSeg);
    x86emu_set_seg_register(e, e->x86.R_ES_SEL, pspSeg);
    x86emu_set_seg_register(e, e->x86.R_SS_SEL, pspSeg);
    e->x86.R_EIP = 0x0100u;
    e->x86.R_ESP = 0xFFFEu;
    e->x86.R_EAX &= 0xFFFF0000u;
    e->x86.mode &= ~_MODE_HALTED;
    e->x86.R_FLG |= F_IF;
    return true;
}

inline void writePspFcb(x86emu_t* e, std::uint32_t pspBase, const char* path) {
    char drive = 0;
    char stem[9]{}, ext[4]{};
    if (path && FieldDos::parseDosName(path, drive, stem, ext)) {
        const std::uint8_t dosDrive = (drive >= 'C') ? static_cast<std::uint8_t>(drive - 'A' + 1) : 3u;
        x86emu_write_byte(e, pspBase + 0x5Cu, dosDrive);
        for (int i = 0; i < 8; ++i)
            x86emu_write_byte(e, pspBase + 0x5Du + static_cast<std::uint32_t>(i),
                static_cast<unsigned>(stem[i] == '\0' ? ' ' : stem[i]));
        for (int i = 0; i < 3; ++i)
            x86emu_write_byte(e, pspBase + 0x65u + static_cast<std::uint32_t>(i),
                static_cast<unsigned>(ext[i] == '\0' ? ' ' : ext[i]));
        FieldInt21::hostDrive = drive;
    }
}

inline bool launchMzExec(x86emu_t* e, const std::vector<std::uint8_t>& img, const char* dosPath,
                         std::uint16_t loadSeg = 0x1000u) {
    if (!launchMzImage(e, img, loadSeg)) return false;
    const std::uint16_t pspSeg = static_cast<std::uint16_t>(loadSeg - 0x10u);
    const std::uint32_t pspBase = static_cast<std::uint32_t>(pspSeg) << 4;
    const std::uint16_t envSeg = static_cast<std::uint16_t>(pspSeg - 0x10u);
    const std::uint32_t envBase = static_cast<std::uint32_t>(envSeg) << 4;
    x86emu_write_word(e, pspBase + 0x2Cu, envSeg);
    x86emu_write_word(e, pspBase + 0x16u, 0x1000u);
    FieldDpmi::writeCstr(e, envBase, "PATH=C:\\;A:\\");
    FieldDpmi::writeCstr(e, envBase + 16u, "COMSPEC=C:\\COMMAND.COM");
    FieldDpmi::writeCstr(e, envBase + 40u, "PROMPT=$P$G");
    writePspFcb(e, pspBase, dosPath);
    if (dosPath) {
        const char* base = dosPath;
        while (*base) ++base;
        while (base > dosPath && base[-1] != '\\' && base[-1] != '/') --base;
        std::uint8_t len = 0;
        for (const char* p = base; *p && len < 126u; ++p)
            x86emu_write_byte(e, pspBase + 0x81u + len++, static_cast<unsigned>(*p));
        x86emu_write_byte(e, pspBase + 0x80u, len);
        x86emu_write_byte(e, pspBase + 0x81u + len, 0x0Du);
    }
    x86emu_write_byte(e, BDA_BASE + 0x42u, 0x02u);
    FieldDpmi::inProtected = false;
    FieldDpmi::installed = false;
    FieldDpmi::install(e);
    /* BIOS model ID — F000:FFFE!=E1 lets DOS/4GW BIOS probe return AX=0 (success path). */
    x86emu_write_byte(e, 0xFFFFEu, 0x00u);
    x86emu_write_byte(e, 0xFFFFFu, 0xFCu);
    x86emu_write_word(e, BDA_BASE + 0x10u, 0x54F3u); /* equipment: VGA + coproc + 386-class */
    /* DOS/16M SMSW path ORs AX with [0000:0040] — must not look like 8088. */
    /* DOS/16M: SMSW ax; or ax,[0040h] — word must combine to 386 CR0 signature. */
    x86emu_write_word(e, 0x0040u, 0x0FF0u);
    x86emu_write_byte(e, 0x002Fu, 0x03u);
    e->x86.crx[0] = 0x0010u;
    /* CPUID pre-check reads FLAGS.ID (bit 21) via PUSHF/POPFD before executing CPUID. */
    e->x86.R_FLG |= 0x200000u;
    pmExecActive = true;
    if (dosPath) {
        const char* base = dosPath;
        while (*base) ++base;
        while (base > dosPath && base[-1] != '\\' && base[-1] != '/') --base;
        std::snprintf(FieldDpmi::pmExecCmdLine, sizeof FieldDpmi::pmExecCmdLine, "%s", base);
    } else {
        FieldDpmi::pmExecCmdLine[0] = '\0';
    }
    const std::uint32_t mzLinear = static_cast<std::uint32_t>(loadSeg) << 4;
    if (FieldDpmi::dos4gwLeOff != 0u
        && FieldDpmi::tryBootstrapDos4gw(e, mzLinear, static_cast<std::uint32_t>(img.size()))) {
        std::fprintf(stderr, "[DOS4GW] direct LE bootstrap from launch\n");
    }
    return true;
}

inline std::uint32_t biosLinear(x86emu_t* e, std::uint32_t segVal, std::uint32_t off) noexcept {
    return FieldDpmi::linearAddr(e,
        static_cast<std::uint16_t>(segVal & 0xFFFFu), FieldDpmi::pmOffset(e, off));
}

inline std::uint32_t int21Addr(x86emu_t* e, std::uint32_t off) noexcept {
    return FieldInt21::linearAddr(e, off);
}

inline int tryHostFileInt21(x86emu_t* e) {
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    if (ah == 0x3D) {
        char path[128]{};
        FieldInt21::readPathFromGuest(e, int21Addr(e, e->x86.R_EDX), path, 128);
        if (FieldDosInt::traceEnabled() || pmExecActive)
            std::fprintf(stderr, "[DOS] INT21 open %s\n", path);
    }
    const int rc = FieldInt21::tryHostFileOps(e, pmExecActive, echoDosChar);
    if (rc == 1 && ah == 0x3D && (e->x86.R_FLG & F_CF) && pmExecActive) {
        char path[128]{};
        FieldInt21::readPathFromGuest(e, int21Addr(e, e->x86.R_EDX), path, 128);
        std::fprintf(stderr, "[DOS] INT21 open FAIL %s\n", path);
    }
    return rc;
}

inline int tryHostComExec(x86emu_t* e) {
    if (static_cast<std::uint8_t>(e->x86.R_EAX >> 8) != 0x4Bu) return 0;
    if (!FieldInt21::hostDosEligible(e, pmExecActive)) return 0;
    char path[128]{};
    const std::uint32_t addr = int21Addr(e, e->x86.R_EDX);
    for (int i = 0; i < 127; ++i) {
        path[i] = static_cast<char>(x86emu_read_byte(e, addr + static_cast<std::uint32_t>(i)));
        if (!path[i]) break;
    }
    std::vector<std::uint8_t> image;
    if (!FieldDos::readHostFile(path, image)) return 0;
    if (image.size() >= 2u && image[0] == 'M' && image[1] == 'Z')
        return launchMzExec(e, image, path) ? 1 : 0;
    return launchComImage(e, image) ? 1 : 0;
}

inline bool screenHasPrompt(x86emu_t* e) noexcept {
    static const char kPrompt[] = "C:\\>";
    for (int i = 0; i < 4; ++i) {
        const std::uint8_t ch = static_cast<std::uint8_t>(
            x86emu_read_byte(e, VGA_BASE + static_cast<std::uint32_t>((23 * 80 + i) * 2)));
        if (ch != static_cast<std::uint8_t>(kPrompt[i]))
            return false;
    }
    return true;
}

inline void maintainBoot(x86emu_t* e, std::uint64_t tsc, std::uint8_t* /*guestRam*/ = nullptr) {
    if (guestBootSettled)
        return;
    if (tsc >= 12800u && tsc <= 13100u)
        seedKernelDecompressorStack(e);
    /* BIOS data area timer (~18.2 Hz) — MS-DOS kernel waits on this during init. */
    const std::uint32_t biosTicks = static_cast<std::uint32_t>(tsc / 30'000u);
    x86emu_write_word(e, BDA_BASE + 0x6C, static_cast<u16>(biosTicks & 0xFFFFu));
    x86emu_write_word(e, BDA_BASE + 0x6E, static_cast<u16>((biosTicks >> 16) & 0xFFFFu));
    x86emu_write_byte(e, 0x0829u, static_cast<unsigned>((biosTicks & 1u) ? 1u : 2u));
    if ((e->x86.R_CS & 0xFFFFu) == 0x0070u) {
        if (x86emu_read_byte(e, 0x898u) == 0xEBu)
            x86emu_write_byte(e, 0x898u, 0x90u);
        if (x86emu_read_byte(e, 0x899u) == 0xE1u)
            x86emu_write_byte(e, 0x899u, 0x90u);
    }
    if (screenHas(e, ": HD1,") || screenHas(e, "Press F8") || screenHas(e, "F5 to")
        || screenHas(e, "Starting MS-DOS") || screenHas(e, "MENUDEFAULT"))
        syntheticKey = 0x1C0Du;
    if (screenHas(e, "Bad or missing") || screenHas(e, "can't use LBA")) {
        static bool injected = false;
        if (!injected) {
            std::vector<std::uint8_t> com;
            if (FieldDos::readHostFile("C:\\COMMAND.COM", com)
                || FieldDos::readHostFile("A:\\COMMAND.COM", com)
                || FieldDos::readHostFile("C:\\IO.SYS", com)
                || FieldDos::readHostFile("A:\\MINISHL.COM", com))
                injected = launchComImage(e, com);
        }
    }
    /* MS-DOS MIT: A: IO.SYS boot → C:\COMMAND.COM — settle at C:\> */
    if (!guestBootSettled && tsc > 600'000u) {
        static bool handed = false;
        if (!handed) {
            std::vector<std::uint8_t> cmd;
            if (FieldDos::readHostFile("C:\\COMMAND.COM", cmd) && launchComImage(e, cmd)) {
                handed = true;
                for (int i = 0; i < 4; ++i)
                    vgaPut(e, static_cast<std::uint16_t>(24 * 80 + i), "C:\\>"[i], 0x02);
                alignPromptCursor(e);
                hostShellLen = 0;
                hostShellLine[0] = '\0';
            }
        }
    }
    if (!guestBootSettled && FieldInt21::hostDosEligible(e, pmExecActive) && screenHas(e, "C:\\>") && tsc > 120'000u) {
        FieldAmmoShell::paintWelcome(e, vgaPut, setCursor);
        hostShellLen = 0;
        hostShellLine[0] = '\0';
        guestBootSettled = true;
        e->x86.mode |= _MODE_HALTED;
    }
}

inline int ivtEmpty(x86emu_t* e, u8 num) {
    const u32 iv  = static_cast<u32>(num) * 4u;
    const u16 off = static_cast<u16>(x86emu_read_word(e, iv));
    const u16 seg = static_cast<u16>(x86emu_read_word(e, iv + 2));
    return off == 0 && seg == 0;
}

inline int handleInt21Early(x86emu_t* e, std::uint8_t ah) {
    if (ah == 0x25) {
        const u8 vint = static_cast<u8>(e->x86.R_EAX);
        const u16 off = static_cast<u16>(e->x86.R_EDX);
        const u16 seg = static_cast<u16>(e->x86.R_DS);
        x86emu_write_word(e, static_cast<u32>(vint) * 4u, off);
        x86emu_write_word(e, static_cast<u32>(vint) * 4u + 2u, seg);
        return 1;
    }
    if (ah == 0x35) {
        const u8 vint = static_cast<u8>(e->x86.R_EAX);
        const u16 off = static_cast<u16>(x86emu_read_word(e, static_cast<u32>(vint) * 4u));
        const u16 seg = static_cast<u16>(x86emu_read_word(e, static_cast<u32>(vint) * 4u + 2u));
        x86emu_set_seg_register(e, e->x86.R_ES_SEL, seg);
        e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | off;
        return 1;
    }
    if (ah == 0x30) {
        /* Watcom _cstart_ 'RAPH' probe expects high word DX (DOS/4GW) else Rational path. */
        if (pmExecActive && e->x86.R_EBX == 0x50484152u)
            e->x86.R_EAX = 0x44580700u;
        else
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x0700u;
        return 1;
    }
    if (ah == 0xFFu && pmExecActive
        && static_cast<std::uint16_t>(e->x86.R_EDX & 0xFFFFu) == 0x0078u) {
        /* Watcom _cstart_: Rational DOS/4GW probe (INT 21 AX=FF00 DX=78). */
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | 0x01u;
        return 1;
    }
    if (ah == 0x40) {
        const std::uint16_t handle = static_cast<std::uint16_t>(e->x86.R_EBX & 0xFFFFu);
        const std::uint16_t count = static_cast<std::uint16_t>(e->x86.R_ECX & 0xFFFFu);
        const std::uint32_t addr = int21Addr(e, e->x86.R_EDX);
        if (pmExecActive && FieldDosInt::traceEnabled() && count > 0u && count < 256u) {
            char msg[256]{};
            for (std::uint16_t i = 0; i < count; ++i)
                msg[i] = static_cast<char>(x86emu_read_byte(e, addr + static_cast<std::uint32_t>(i)));
            std::fprintf(stderr, "[DOS] write h=%u: %s\n",
                static_cast<unsigned>(handle), msg);
        }
        if (handle == 1u || handle == 2u) {
            for (std::uint16_t i = 0; i < count; ++i) {
                const char ch = static_cast<char>(
                    x86emu_read_byte(e, addr + static_cast<std::uint32_t>(i)));
                if (ch == '\r') continue;
                echoDosChar(e, ch);
            }
        }
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | count;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x44) {
        const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
        if (al == 0x00u || al == 0x01u)
            e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | 0x00C0u;
        e->x86.R_EAX &= 0xFFFF0000u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x4a) {
        const u16 want = static_cast<u16>(e->x86.R_BX & 0xFFFFu);
        e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u) | want;
        e->x86.R_EAX &= 0xFFFF0000u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x4c && pmExecActive) {
        const std::uint8_t code = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
        if (code == 0u && FieldDpmi::isProtected(e)) {
            if (FieldDpmi::tryCstartExit(e))
                return 1;
            const std::uint32_t target = FieldDpmi::lePostCstartEip
                ? FieldDpmi::lePostCstartEip : FieldDpmi::leBootEip;
            std::fprintf(stderr, "[DOS4GW] INT21 4C al=00 -> entry %08x\n", target);
            e->x86.R_EIP = target;
            e->x86.saved_eip = target;
            if (FieldDpmi::leBootEsp)
                e->x86.R_ESP = FieldDpmi::leBootEsp;
            FieldDpmi::cstartComplete = true;
            e->x86.mode &= ~_MODE_HALTED;
            e->x86.intr_type = 0;
            return 1;
        }
        if (code != 0u) {
            std::fprintf(stderr,
                "[DOS4GW] INT21 4C al=%02x cs=%04x ip=%04x eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x ebp=%08x\n",
                static_cast<unsigned>(code),
                static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
                static_cast<unsigned>(FieldDpmi::isProtected(e) ? e->x86.R_EIP : (e->x86.R_EIP & 0xFFFFu)),
                static_cast<unsigned>(e->x86.R_EAX),
                static_cast<unsigned>(e->x86.R_EBX),
                static_cast<unsigned>(e->x86.R_ECX),
                static_cast<unsigned>(e->x86.R_EDX),
                static_cast<unsigned>(e->x86.R_ESI),
                static_cast<unsigned>(e->x86.R_EDI),
                static_cast<unsigned>(e->x86.R_EBP));
            if (FieldDpmi::tryBootstrapDos4gw(e, 0x10000u, 720u * 1024u)) {
                if (FieldDpmi::leBootPending) {
                    FieldDpmi::leBootPending = false;
                    FieldDpmi::inProtected = true;
                    e->x86.crx[0] |= 1u;
                    e->x86.mode |= _MODE_CODE32 | _MODE_DATA32 | _MODE_STACK32;
                    e->x86.mode &= ~_MODE_HALTED;
                    e->x86.R_EIP = FieldDpmi::leBootEip;
                    e->x86.R_ESP = FieldDpmi::leBootEsp;
                    e->x86.R_EAX = FieldDpmi::leBootEax;
                    e->x86.R_EDX = FieldDpmi::leBootEdx;
                    x86emu_set_seg_register(e, e->x86.R_CS_SEL, FieldDpmi::leBootCs);
                    x86emu_set_seg_register(e, e->x86.R_SS_SEL, FieldDpmi::leBootSs);
                    x86emu_set_seg_register(e, e->x86.R_DS_SEL, FieldDpmi::leBootDs);
                    x86emu_set_seg_register(e, e->x86.R_ES_SEL, FieldDpmi::leBootDs);
                    x86emu_set_seg_register(e, e->x86.R_GS_SEL, FieldDpmi::leBootGs);
                    for (int idx : {R_CS_INDEX, R_SS_INDEX, R_DS_INDEX, R_ES_INDEX, R_FS_INDEX, R_GS_INDEX})
                        e->x86.seg[idx].limit = 0xFFFFFFFFu;
                    e->x86.R_FLG |= F_IF;
                }
                e->x86.intr_type = 0;
                return 1;
            }
        }
        e->x86.mode |= _MODE_HALTED;
        return 1;
    }
    if (ah == 0x1A) {
        e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | 0x0080u;
        return 1;
    }
    if (ah == 0x19) {
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | 0x02u;
        return 1;
    }
    if (ah == 0x01) {
        const std::uint16_t key = peekHostKey();
        if (key) {
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | static_cast<std::uint8_t>(biosKeyToChar(key));
            e->x86.R_FLG &= ~F_ZF;
        } else {
            e->x86.R_FLG |= F_ZF;
        }
        return 1;
    }
    if (ah == 0x02) {
        echoDosChar(e, static_cast<char>(e->x86.R_EDX & 0xFFu));
        return 1;
    }
    if (ah == 0x06) {
        const std::uint8_t dl = static_cast<std::uint8_t>(e->x86.R_EDX & 0xFFu);
        if (dl == 0xFFu) {
            const std::uint16_t key = pullHostKey();
            if (!key) return 1;
            const char ch = biosKeyToChar(key);
            echoDosChar(e, ch);
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | static_cast<std::uint8_t>(ch);
        } else {
            echoDosChar(e, static_cast<char>(dl));
        }
        return 1;
    }
    if (ah == 0x07) {
        const std::uint16_t key = pullHostKey();
        if (!key) return 1;
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u)
                     | static_cast<std::uint8_t>(biosKeyToChar(key));
        return 1;
    }
    if (ah == 0x08) {
        const std::uint16_t key = pullHostKey();
        if (!key) return 1;
        const char ch = biosKeyToChar(key);
        echoDosChar(e, ch);
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | static_cast<std::uint8_t>(ch);
        return 1;
    }
    if (ah == 0x0B) {
        if (peekHostKey())
            e->x86.R_AL = 0xFFu;
        else
            e->x86.R_AL = 0u;
        return 1;
    }
    if (ah == 0x09) {
        u32 addr = int21Addr(e, e->x86.R_EDX);
        for (;;) {
            const u8 ch = static_cast<u8>(x86emu_read_byte(e, addr++));
            if (ch == '$') break;
            if (ch == '\r' || ch == '\n') continue;
            echoDosChar(e, static_cast<char>(ch));
        }
        return 1;
    }
    if (ah == 0x0A) {
        const u32 addr = int21Addr(e, e->x86.R_EDX);
        if (!dosLineInput.active) {
            dosLineInput.bufAddr = addr;
            dosLineInput.maxLen = static_cast<std::uint8_t>(x86emu_read_byte(e, addr));
            dosLineInput.count = 0;
            dosLineInput.active = true;
        }
        const std::uint16_t key = pullHostKey();
        if (!key) {
            e->x86.R_EAX &= 0xFFFF00FFu;
            return 1;
        }
        const char ch = biosKeyToChar(key);
        if (ch == '\r') {
            x86emu_write_byte(e, dosLineInput.bufAddr + 1u, dosLineInput.count);
            dosLineInput.active = false;
            e->x86.R_EAX &= 0xFFFF00FFu;
            return 1;
        }
        if (ch == '\b') {
            if (dosLineInput.count > 0) {
                --dosLineInput.count;
                echoDosChar(e, '\b');
            }
            e->x86.R_EAX &= 0xFFFF00FFu;
            return 1;
        }
        if (ch >= 32 && dosLineInput.count + 1u < dosLineInput.maxLen) {
            x86emu_write_byte(e, dosLineInput.bufAddr + 2u + dosLineInput.count,
                static_cast<unsigned>(ch));
            echoDosChar(e, ch);
            ++dosLineInput.count;
        }
        e->x86.R_EAX &= 0xFFFF00FFu;
        return 1;
    }
    if (ah == 0x4C) {
        if (!pmExecActive)
            x86emu_stop(e);
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    if (ah == 0x48) {
        const u16 paras = static_cast<u16>(e->x86.R_BX & 0xFFFFu);
        static u16 heapSeg = 0x5000u;
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | heapSeg;
        heapSeg = static_cast<u16>(heapSeg + paras + 0x10u);
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x4A || ah == 0x49) {
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x2A) {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        int year = tm.tm_year + 1900;
        year = std::clamp(year, 1980, 2107);
        const std::uint16_t date = static_cast<std::uint16_t>(
            ((year - 1980) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday);
        e->x86.R_AL = static_cast<std::uint8_t>((tm.tm_wday + 1) % 7);
        e->x86.R_CX = (e->x86.R_CX & ~0xFFFFu) | static_cast<std::uint32_t>(date);
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x2C) {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        e->x86.R_CH = static_cast<std::uint8_t>(tm.tm_hour);
        e->x86.R_CL = static_cast<std::uint8_t>(tm.tm_min);
        e->x86.R_DH = static_cast<std::uint8_t>(tm.tm_sec);
        e->x86.R_DL = 0;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x2D) {
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    e->x86.R_EAX &= 0xFFFF0000u;
    return 1;
}

inline int int13ReadLba(x86emu_t* e, std::uint8_t dl, std::uint32_t lba, std::uint8_t count,
                        std::uint32_t dst) {
    if (!isFixedDisk(dl) || !FieldDos::hdReady) return 0;
    for (std::uint8_t blk = 0; blk < count; ++blk) {
        const std::uint32_t src = (lba + blk) * FieldDos::HD_SECTOR_BYTES;
        if (src + FieldDos::HD_SECTOR_BYTES > FieldDos::hdImage.size()) return 0;
        for (std::uint32_t b = 0; b < FieldDos::HD_SECTOR_BYTES; ++b)
            x86emu_write_byte(e, dst + blk * FieldDos::HD_SECTOR_BYTES + b,
                FieldDos::readHdByte(src + b, FieldDos::hdGuestRamPtr));
    }
    return 1;
}

inline int handleInt(x86emu_t* e, u8 num, std::uint32_t hostKey, bool keyDown, std::uint64_t ticks) {
    if (FieldDosInt::traceEnabled() && pmExecActive
        && num != 0x08u && num != 0x1Cu && num != 0x28u && num != 0x2Bu)
        FieldDosInt::traceInt(e, num);
    lastHostKey = hostKey;
    lastKeyDown = keyDown;
    if (!hostKey) consumedHostKey = 0;
    FieldDpmi::normalizeRealModeSegs(e);



    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);

    if (num == 0x06 && pmExecActive && !FieldDpmi::isProtected(e)) {
        const std::uint32_t ip = e->x86.R_EIP & 0xFFFFu;
        const std::uint32_t addr = (static_cast<std::uint32_t>(e->x86.R_CS & 0xFFFFu) << 4) + ip;
        std::uint32_t adv = 2u;
        const std::uint8_t b0 = static_cast<std::uint8_t>(x86emu_read_byte(e, addr));
        const std::uint8_t b1 = static_cast<std::uint8_t>(x86emu_read_byte(e, addr + 1u));
        if (b0 == 0x0Fu) adv = 3u;
        if (b0 == 0x66u && b1 == 0x0Fu) adv = 4u;
        e->x86.R_EIP = ip + adv;
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x0010u;
        return 1;
    }
    if (num == 0x06 && pmExecActive && FieldDpmi::isProtected(e)) {
        if (FieldDosInt::traceEnabled()) {
            const std::uint32_t pc = FieldDpmi::pmFaultPc(e);
            std::fprintf(stderr, "[DOS] #UD pm eip=%08x acc=%04x len=%u bytes=",
                static_cast<unsigned>(FieldDpmi::pmFaultEip(e)),
                static_cast<unsigned>(e->x86.R_CS_ACC),
                static_cast<unsigned>(FieldDpmi::pmInsnLen(e)));
            for (int i = 0; i < 8; ++i)
                std::fprintf(stderr, " %02x",
                    static_cast<unsigned>(x86emu_read_byte(e, pc + static_cast<std::uint32_t>(i))));
            std::fputc('\n', stderr);
        }
        if (FieldDpmi::tryPmLoadSegGpInsn(e)) return 1;
        if (FieldDpmi::tryPmStackSegInsn(e)) return 1;
        if (FieldDpmi::tryEmulatePmGpInsn(e)) return 1;
        if (FieldX86Native::tryEmulateFfCallJmp(e, FieldDpmi::pmFaultEip(e))) return 1;
        if (FieldX86Native::tryEmulateNearRet(e, FieldDpmi::pmFaultEip(e))) return 1;
        if (FieldX86Native::tryEmulateNearCallRel(e, FieldDpmi::pmFaultEip(e))) return 1;
        if (FieldDpmi::pmStepActive)
            FieldDpmi::advancePmFaultEip(e);
        else
            FieldDpmi::pmNativeStep(e);
        return 1;
    }

    if (num == 0x05)
        return FieldDosInt::handleInt05(e);

    if (num == 0x08 || num == 0x1C) {
        const u32 t = static_cast<u32>(ticks & 0xFFFFFFFFu);
        x86emu_write_word(e, BDA_BASE + 0x6C, static_cast<u16>(t & 0xFFFFu));
        x86emu_write_word(e, BDA_BASE + 0x6E, static_cast<u16>((t >> 16) & 0xFFFFu));
        return 1;
    }

    if (((num >= 0x09u && num <= 0x0Fu) || (num >= 0x70u && num <= 0x77u))
        && !(num == 0x0Du && pmExecActive && FieldDpmi::isProtected(e)))
        return FieldDosInt::handleIrq(e, num);

    if (num == 0x18)
        return FieldDosInt::handleInt18(e);

    if (num == 0x20) {
        if (pmExecActive && FieldDpmi::isProtected(e)) {
            if (FieldDpmi::tryCstartExit(e))
                return 1;
            return 1;
        }
        if (FieldDosInt::handleInt20(e) == 2)
            return handleInt(e, 0x21, hostKey, keyDown, ticks);
        return 1;
    }

    if (num == 0x29) {
        echoDosChar(e, static_cast<char>(e->x86.R_EDX & 0xFFu));
        return 1;
    }

    if (num == 0x2A || num == 0x2C || num == 0x2D)
        return FieldDosInt::handleIntNet(e, num);

    if (num == 0x2E)
        return FieldDosInt::handleInt2E(e);

    if (num == 0x30)
        return FieldDosInt::handleInt30(e);

    if (num == 0x34)
        return FieldEms::handleInt34(e);

    if (num == 0x35)
        return FieldDosInt::handleInt35(e);

    if (num == 0x36)
        return FieldDosInt::handleInt36(e);

    if (num == 0x38)
        return FieldDosInt::handleInt38(e);

    if (num == 0x3F)
        return FieldDosInt::handleInt3F(e);

    if (num == 0x68)
        return FieldDosInt::handleInt68(e);

    if (num == 0x6B || num == 0x6C || num == 0x6D)
        return FieldDosInt::handleIntForeign(e);

    if (num == 0x10) {
        if (ah == 0x00) {
            const std::uint8_t mode = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
            if (FieldDosInt::traceEnabled())
                std::fprintf(stderr, "[DOS] INT10 set mode %02Xh\n", static_cast<unsigned>(mode));
            std::uint8_t* ram = FieldX86Emu::ramHost;
            FieldVesa::active = false;
            FieldVga::setMode(mode, ram);
            if (FieldVga::isPlanarMode(mode))
                FieldVgaPlanes::applyModeDefaults(mode);
            x86emu_write_byte(e, BDA_BASE + 0x49, mode);
            x86emu_write_byte(e, 0x449u, mode);
            if (mode == 3u) {
                for (int i = 0; i < 80 * 25; ++i)
                    vgaPut(e, static_cast<std::uint16_t>(i), ' ', 0x07);
                setCursor(e, 0);
            }
            if (mode == 0x13u || mode == 0x0Du || mode == 0xA0u) {
                if (ram)
                    std::memset(ram + 0xA0000u, 0, 320u * 200u);
                FieldVga::resetPalette();
                if (ram)
                    FieldVga::syncPaletteToGuest(ram);
            }
            if (mode == 0x12u && ram)
                std::memset(ram + 0xA0000u, 0, 640u * 480u);
            return 1;
        }
        if (ah == 0x4F) {
            const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
            const u32 info = biosLinear(e, e->x86.R_ES, e->x86.R_EDI);
            if (al == 0x00) {
                x86emu_write_byte(e, info, 'V');
                x86emu_write_byte(e, info + 1u, 'E');
                x86emu_write_byte(e, info + 2u, 'S');
                x86emu_write_byte(e, info + 3u, 'A');
                e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | (0x4F00u);
                e->x86.R_EDI = (e->x86.R_EDI & 0xFFFF0000u) | 0x0200u;
                return 1;
            }
            if (al == 0x01) {
                const std::uint16_t mode = static_cast<std::uint16_t>(e->x86.R_ECX & 0xFFFFu);
                for (u32 i = 0; i < 256u; ++i)
                    x86emu_write_byte(e, info + i, 0);
                x86emu_write_word(e, info + 0u, mode);
                if (mode == 0x100u) {
                    x86emu_write_word(e, info + 18u, 640u);
                    x86emu_write_word(e, info + 20u, 400u);
                    x86emu_write_byte(e, info + 25u, 8u);
                    x86emu_write_byte(e, info + 39u, 1u);
                } else if (mode == 0x101u || mode == 0x111u || mode == 0x112u) {
                    x86emu_write_word(e, info + 18u, 640u);
                    x86emu_write_word(e, info + 20u, 480u);
                    x86emu_write_byte(e, info + 25u, (mode == 0x111u) ? 4u : 8u);
                    x86emu_write_byte(e, info + 39u, 1u);
                } else if (mode == 0x102u || mode == 0x103u) {
                    x86emu_write_word(e, info + 18u, 800u);
                    x86emu_write_word(e, info + 20u, 600u);
                    x86emu_write_byte(e, info + 25u, (mode == 0x102u) ? 4u : 8u);
                    x86emu_write_byte(e, info + 39u, 1u);
                }
                e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x4F00u;
                return 1;
            }
            if (al == 0x02) {
                const std::uint16_t vesaMode = static_cast<std::uint16_t>(e->x86.R_EBX & 0xFFFFu);
                std::uint8_t* ram = FieldX86Emu::ramHost;
                if (FieldVesa::setVesaMode(vesaMode, ram)) {
                    const std::uint8_t vgaMode = FieldVga::mode;
                    x86emu_write_byte(e, BDA_BASE + 0x49, vgaMode);
                    x86emu_write_byte(e, 0x449u, vgaMode);
                    e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x4F00u;
                    return 1;
                }
                if (vesaMode == 0x12u || vesaMode == 0x13u) {
                    const std::uint8_t vgaMode = static_cast<std::uint8_t>(vesaMode & 0xFFu);
                    FieldVesa::active = false;
                    FieldVga::setMode(vgaMode, ram);
                    if (FieldVga::isPlanarMode(vgaMode))
                        FieldVgaPlanes::applyModeDefaults(vgaMode);
                    x86emu_write_byte(e, BDA_BASE + 0x49, vgaMode);
                    x86emu_write_byte(e, 0x449u, vgaMode);
                    if (vgaMode == 0x13u && ram)
                        std::memset(ram + 0xA0000u, 0, 320u * 200u);
                    e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x4F00u;
                    return 1;
                }
            }
            if (al == 0x05) {
                const std::uint16_t b = static_cast<std::uint16_t>(e->x86.R_EBX & 0xFFFFu);
                if (FieldVesa::setBank(b, FieldX86Emu::ramHost)) {
                    e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x4F00u;
                    return 1;
                }
            }
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x014Fu;
            return 1;
        }
        if (ah == 0x01 || ah == 0x02 || ah == 0x03 || ah == 0x0F || ah == 0x1A)
            return 1;
        if (ah == 0x02) {
            x86emu_write_byte(e, BDA_BASE + 0x51, static_cast<unsigned>((e->x86.R_EDX >> 8) & 0xFF));
            x86emu_write_byte(e, BDA_BASE + 0x50, static_cast<unsigned>(e->x86.R_EDX & 0xFF));
            return 1;
        }
        if (ah == 0x03) {
            e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF00FFu)
                         | (static_cast<std::uint32_t>(x86emu_read_byte(e, BDA_BASE + 0x51)) << 8);
            e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF00FFu)
                         | static_cast<std::uint32_t>(x86emu_read_byte(e, BDA_BASE + 0x50));
            return 1;
        }
        if (ah == 0x06) {
            const std::uint8_t ch = static_cast<std::uint8_t>(e->x86.R_ECX >> 8);
            const std::uint8_t cl = static_cast<std::uint8_t>(e->x86.R_ECX);
            const std::uint8_t dh = static_cast<std::uint8_t>(e->x86.R_EDX >> 8);
            const std::uint8_t dl = static_cast<std::uint8_t>(e->x86.R_EDX);
            const std::uint8_t attr = static_cast<std::uint8_t>(e->x86.R_EBX >> 8);
            for (int row = ch; row <= dh; ++row)
                for (int col = cl; col <= dl; ++col)
                    vgaPut(e, static_cast<std::uint16_t>(row * 80 + col), ' ',
                        attr ? attr : 0x07);
            return 1;
        }
        if (ah == 0x0E) {
            std::uint16_t pos = cursorPos(e);
            vgaPut(e, pos, static_cast<std::uint8_t>(e->x86.R_EAX), 0x07);
            ++pos;
            if ((pos % 80u) == 0u) pos += 80u;
            setCursor(e, pos);
            return 1;
        }
        if (ah == 0x0C) {
            const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
            const std::uint16_t col = static_cast<std::uint16_t>(e->x86.R_CX & 0xFFFFu);
            const std::uint16_t row = static_cast<std::uint16_t>(e->x86.R_DX & 0xFFFFu);
            const std::uint8_t mode = static_cast<std::uint8_t>(x86emu_read_byte(e, 0x449u));
            if (mode == 0x13u) {
                const std::uint32_t off = 0xA0000u + static_cast<std::uint32_t>(row) * 320u + col;
                x86emu_write_byte(e, off, al);
                if (FieldX86Emu::ramHost)
                    FieldX86Emu::ramHost[off] = al;
            } else {
                vgaPut(e, static_cast<std::uint16_t>(row * 80u + col), al, 0x07);
            }
            return 1;
        }
        if (ah == 0x10) {
            const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
            if (al == 0x10u) {
                const std::uint8_t idx = static_cast<std::uint8_t>(e->x86.R_BX & 0xFFu);
                FieldVga::palette[static_cast<std::size_t>(idx) * 3u]
                    = static_cast<std::uint8_t>((e->x86.R_CX >> 8) & 0xFFu);
                FieldVga::palette[static_cast<std::size_t>(idx) * 3u + 1u]
                    = static_cast<std::uint8_t>(e->x86.R_CX & 0xFFu);
                FieldVga::palette[static_cast<std::size_t>(idx) * 3u + 2u]
                    = static_cast<std::uint8_t>((e->x86.R_DX >> 8) & 0xFFu);
                if (FieldX86Emu::ramHost)
                    FieldVga::syncPaletteToGuest(FieldX86Emu::ramHost);
                return 1;
            }
            if (al == 0x12u) {
                const std::uint16_t count = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
                const std::uint32_t ptr = biosLinear(e, e->x86.R_ES, e->x86.R_EDX);
                for (std::uint16_t i = 0; i < count; ++i) {
                    const std::uint8_t idx = static_cast<std::uint8_t>(x86emu_read_byte(e, ptr + i * 4u));
                    FieldVga::palette[static_cast<std::size_t>(idx) * 3u]
                        = static_cast<std::uint8_t>(x86emu_read_byte(e, ptr + i * 4u + 1u));
                    FieldVga::palette[static_cast<std::size_t>(idx) * 3u + 1u]
                        = static_cast<std::uint8_t>(x86emu_read_byte(e, ptr + i * 4u + 2u));
                    FieldVga::palette[static_cast<std::size_t>(idx) * 3u + 2u]
                        = static_cast<std::uint8_t>(x86emu_read_byte(e, ptr + i * 4u + 3u));
                }
                if (FieldX86Emu::ramHost)
                    FieldVga::syncPaletteToGuest(FieldX86Emu::ramHost);
                return 1;
            }
            if (al == 0x15u) {
                const std::uint8_t idx = static_cast<std::uint8_t>(e->x86.R_BX & 0xFFu);
                e->x86.R_DH = FieldVga::palette[static_cast<std::size_t>(idx) * 3u];
                e->x86.R_CH = FieldVga::palette[static_cast<std::size_t>(idx) * 3u + 1u];
                e->x86.R_CL = FieldVga::palette[static_cast<std::size_t>(idx) * 3u + 2u];
                return 1;
            }
            return 1;
        }
        return 1;
    }

    if (num == 0x11) {
        const std::uint16_t equip = static_cast<std::uint16_t>(
            x86emu_read_word(e, BDA_BASE + 0x10u));
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u)
                     | (equip ? equip : static_cast<std::uint16_t>(0x54F3u));
        return 1;
    }
    if (num == 0x12) {
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | FieldRtxMemory::conventionalKb;
        return 1;
    }

    if (num == 0x13) {
        const std::uint8_t dl = static_cast<std::uint8_t>(e->x86.R_EDX);
        if (pmExecActive && !isFixedDisk(dl)) {
            if (ah == 0x41 || ah == 0x48 || ah == 0x4Bu) {
                e->x86.R_FLG &= ~F_CF;
                e->x86.R_AH = 0;
                return 1;
            }
            if (ah == 0x02 || ah == 0x03 || ah == 0x04 || ah == 0x08 || ah == 0x0C
                || ah == 0x11 || ah == 0x15 || ah == 0x16) {
                if (ah == 0x02) {
                    const std::uint8_t n = static_cast<std::uint8_t>(e->x86.R_EAX);
                    const u32 dst = (e->x86.R_ES << 4) + (e->x86.R_EBX & 0xFFFFu);
                    for (std::uint8_t i = 0; i < n; ++i)
                        for (int b = 0; b < 512; ++b)
                            x86emu_write_byte(e, dst + i * 512u + static_cast<u32>(b), 0);
                    e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | n;
                } else if (ah == 0x08) {
                    e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | 0x4F09u;
                    e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF00FFu) | (1u << 8);
                    e->x86.R_EDX = (e->x86.R_EDX & 0xFFFFFF00u) | 0x01u;
                    e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | 0x0000u;
                }
                e->x86.R_FLG &= ~F_CF;
                return 1;
            }
        }
        if (ah == 0x00) {
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x02 || ah == 0x03) {
            const std::uint8_t n = static_cast<std::uint8_t>(e->x86.R_EAX);
            const std::uint16_t head = static_cast<std::uint16_t>((e->x86.R_EDX >> 8) & 0xFFu);
            const std::uint16_t cyl = static_cast<std::uint16_t>(
                ((e->x86.R_ECX >> 8) & 0xFF) | ((e->x86.R_ECX & 0xC0u) << 2));
            const std::uint16_t sector = static_cast<std::uint16_t>(e->x86.R_ECX & 0x3Fu);
            if (!n || !sector || (!isFixedDisk(dl) && sector > FLOPPY_SPT)) {
                e->x86.R_EAX |= 1u << 8;
                e->x86.R_FLG |= F_CF;
                return 1;
            }
            const u32 dst = (e->x86.R_ES << 4) + (e->x86.R_EBX & 0xFFFFu);
            if (ah == 0x03) {
                for (std::uint8_t i = 0; i < n; ++i) {
                    const std::uint32_t trg = diskOffset(dl, cyl, head,
                        static_cast<std::uint16_t>(sector + i));
                    if (trg == 0xFFFFFFFFu) {
                        e->x86.R_EAX |= 1u << 8;
                        e->x86.R_FLG |= F_CF;
                        return 1;
                    }
                    for (int b = 0; b < 512; ++b) {
                        const std::uint8_t byte = static_cast<std::uint8_t>(
                            x86emu_read_byte(e, dst + i * 512u + static_cast<u32>(b)));
                        x86emu_write_byte(e, trg + static_cast<u32>(b), byte);
                    }
                }
                e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | n;
                e->x86.R_FLG &= ~F_CF;
                return 1;
            }
            for (std::uint8_t i = 0; i < n; ++i) {
                const std::uint32_t src = diskOffset(dl, cyl, head,
                    static_cast<std::uint16_t>(sector + i));
                if (src == 0xFFFFFFFFu) {
                    e->x86.R_EAX |= 1u << 8;
                    e->x86.R_FLG |= F_CF;
                    return 1;
                }
                for (int b = 0; b < 512; ++b) {
                    std::uint8_t byte = 0;
                    if (isFixedDisk(dl) && FieldDos::hdReady)
                        byte = FieldDos::readHdByte(src + static_cast<std::uint32_t>(b),
                            FieldDos::hdGuestRamPtr);
                    else
                        byte = static_cast<std::uint8_t>(x86emu_read_byte(e, src + static_cast<u32>(b)));
                    x86emu_write_byte(e, dst + i * 512u + static_cast<u32>(b), byte);
                }
            }
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | n;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x41) {
            if ((e->x86.R_EBX & 0xFFFFu) == 0x55AAu) {
                e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | 0xAA55u;
                e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | 0x0003u;
                e->x86.R_FLG &= ~F_CF;
            } else {
                e->x86.R_FLG |= F_CF;
            }
            return 1;
        }
        if (ah == 0x42) {
            const std::uint8_t dl = static_cast<std::uint8_t>(e->x86.R_EDX);
            const u32 dap = (e->x86.R_DS << 4) + (e->x86.R_ESI & 0xFFFFu);
            const u16 count = static_cast<u16>(x86emu_read_word(e, dap + 2u));
            const u16 bufOff = static_cast<u16>(x86emu_read_word(e, dap + 4u));
            const u16 bufSeg = static_cast<u16>(x86emu_read_word(e, dap + 6u));
            const u32 lba = static_cast<u32>(x86emu_read_word(e, dap + 8u))
                          | (static_cast<u32>(x86emu_read_word(e, dap + 10u)) << 16);
            const u32 dst = (static_cast<u32>(bufSeg) << 4) + bufOff;
            if (!int13ReadLba(e, dl, lba, static_cast<std::uint8_t>(count), dst)) {
                e->x86.R_EAX |= 1u << 8;
                e->x86.R_FLG |= F_CF;
            } else {
                e->x86.R_FLG &= ~F_CF;
            }
            return 1;
        }
        if (ah == 0x08) {
            const std::uint8_t dl = static_cast<std::uint8_t>(e->x86.R_EDX);
            if (isFixedDisk(dl) && FieldDos::hdReady) {
                constexpr std::uint8_t hdSpt = 63;
                constexpr std::uint8_t hdHeads = 16;
                const std::uint32_t sectors = static_cast<std::uint32_t>(
                    FieldDos::hdImage.size() / FieldDos::HD_SECTOR_BYTES);
                const std::uint32_t cylCount = sectors / (static_cast<std::uint32_t>(hdHeads) * hdSpt);
                const std::uint32_t maxCyl = cylCount > 0u ? cylCount - 1u : 0u;
                const std::uint8_t cl = static_cast<std::uint8_t>(
                    hdSpt | (((maxCyl >> 6) & 3u) << 6));
                const std::uint8_t ch = static_cast<std::uint8_t>(maxCyl & 0xFFu);
                e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u)
                             | (static_cast<std::uint32_t>(cl) << 8) | ch;
                e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF00FFu) | (15u << 8);
                e->x86.R_EDX = (e->x86.R_EDX & 0xFFFFFF00u) | 0x80u;
            } else {
                const std::uint8_t cl = static_cast<std::uint8_t>(
                    FLOPPY_SPT | (((FLOPPY_MAX_CYL >> 6) & 3u) << 6));
                const std::uint8_t ch = static_cast<std::uint8_t>(FLOPPY_MAX_CYL & 0xFFu);
                e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u)
                             | (static_cast<std::uint32_t>(cl) << 8) | ch;
                e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF00FFu)
                             | (static_cast<std::uint32_t>(FLOPPY_HEADS - 1) << 8);
                e->x86.R_EDX = (e->x86.R_EDX & 0xFFFFFF00u) | 0x01u;
            }
            e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | 0x0000u;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x15) {
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | (1u << 8);
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x16) {
            e->x86.R_EAX &= 0xFFFF00FFu;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x48) {
            const std::uint8_t dl = static_cast<std::uint8_t>(e->x86.R_EDX);
            if (!isFixedDisk(dl) || !FieldDos::hdReady) {
                e->x86.R_FLG |= F_CF;
                return 1;
            }
            constexpr std::uint8_t hdSpt = 63;
            constexpr std::uint8_t hdHeads = 16;
            const std::uint32_t sectors = static_cast<std::uint32_t>(
                FieldDos::hdImage.size() / FieldDos::HD_SECTOR_BYTES);
            const std::uint32_t cylinders = sectors / (static_cast<std::uint32_t>(hdHeads) * hdSpt);
            const u32 buf = (e->x86.R_DS << 4) + (e->x86.R_ESI & 0xFFFFu);
            const u16 bufSize = static_cast<u16>(x86emu_read_word(e, buf));
            if (bufSize < 30u) {
                e->x86.R_FLG |= F_CF;
                return 1;
            }
            /* MS-DOS _bios_LBA_disk_parameterS (30 bytes). */
            x86emu_write_word(e, buf + 0u, 0x001Eu);
            x86emu_write_word(e, buf + 2u, 0x003Fu);
            x86emu_write_dword(e, buf + 4u, cylinders);
            x86emu_write_dword(e, buf + 8u, hdHeads);
            x86emu_write_dword(e, buf + 12u, hdSpt);
            x86emu_write_dword(e, buf + 16u, sectors);
            x86emu_write_dword(e, buf + 20u, 0u);
            x86emu_write_word(e, buf + 24u, 0x0200u);
            if (bufSize >= 30u)
                x86emu_write_dword(e, buf + 26u, 0u);
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        e->x86.R_EAX |= 1u << 8;
        e->x86.R_FLG |= F_CF;
        return 1;
    }

    if (num == 0x15) {
        if (FieldXms::handleInt15Xms(e)) return 1;
        if ((e->x86.R_EAX & 0xFFFFu) == 0xE801u) {
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u)
                         | (static_cast<std::uint32_t>(FieldPlatform::EXTENDED_KB) & 0xFFFFu);
            e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u)
                         | (static_cast<std::uint32_t>(FieldRtxMemory::conventionalKb) & 0xFFFFu);
            e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u)
                         | ((FieldPlatform::EXTENDED_KB >> 16) & 0xFFFFu);
            e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u)
                         | ((FieldRtxMemory::conventionalKb >> 8) & 0xFFFFu);
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x24) {
            const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
            if (al == 0x01u || al == 0x02u) {
                e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | 0x02u;
                e->x86.R_FLG &= ~F_CF;
                return 1;
            }
            e->x86.R_EAX |= 1u << 8;
            e->x86.R_FLG |= F_CF;
            return 1;
        }
        if (ah == 0xE8) {
            const u32 buf = (e->x86.R_ES << 4) + (e->x86.R_EDI & 0xFFFFu);
            const u32 sig = static_cast<u32>(x86emu_read_dword(e, buf));
            if (sig != 0x534D4150u) {
                e->x86.R_EAX |= 1u << 8;
                e->x86.R_FLG |= F_CF;
                return 1;
            }
            const u32 cont = e->x86.R_EBX & 0xFFFFu;
            struct E820Ent { u32 base; u32 len; u32 type; u32 acpi; };
            static const E820Ent map[] = {
                {0x00000000u, 0x0009FC00u, 1u, 0u},
                {0x000F0000u, 0x00010000u, 2u, 0u},
                {0x00100000u, FieldPlatform::GUEST_RAM_BYTES - 0x00100000u, 1u, 0u},
                {FieldPlatform::GUEST_RAM_BYTES,
                 static_cast<u32>(FieldPlatform::REPORTED_RAM_BYTES - FieldPlatform::GUEST_RAM_BYTES),
                 1u, 0u},
            };
            if (cont < 4u) {
                const auto& ent = map[cont];
                x86emu_write_dword(e, buf + 0u, ent.base);
                x86emu_write_dword(e, buf + 4u, ent.len);
                x86emu_write_dword(e, buf + 8u, ent.type);
                x86emu_write_dword(e, buf + 12u, ent.acpi);
                e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | (cont + 1u);
                e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | 20u;
                e->x86.R_EAX &= 0xFFFF00FFu;
                e->x86.R_FLG &= ~F_CF;
                return 1;
            }
            e->x86.R_EBX &= 0xFFFF0000u;
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x8600u;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0xC0) {
            constexpr std::uint32_t sdt = 0x00000500u;
            x86emu_write_word(e, sdt + 0u, 16u);
            x86emu_write_byte(e, sdt + 2u, 0xFCu); /* 80386 */
            x86emu_write_byte(e, sdt + 3u, 0x00u);
            x86emu_write_byte(e, sdt + 4u, 0x01u);
            x86emu_write_byte(e, sdt + 5u, 0x01u); /* FPU */
            x86emu_write_byte(e, sdt + 6u, 0x00u);
            for (std::uint32_t i = 7u; i < 16u; ++i)
                x86emu_write_byte(e, sdt + i, 0u);
            e->x86.R_ES  = (e->x86.R_ES & 0xFFFF0000u) | static_cast<u16>(sdt >> 4);
            e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | static_cast<u16>(sdt & 0xFu);
            e->x86.R_EAX &= 0xFFFF00FFu;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        e->x86.R_EAX |= 1u << 8;
        e->x86.R_FLG |= F_CF;
        return 1;
    }

    if (num == 0x16) {
        /* Always synthesize keyboard input — guest IVT hooks would spin forever. */
        constexpr std::uint16_t kEnter = 0x1C0Du;
        std::uint16_t key = guestBootSettled ? pullHostKey()
                                             : (syntheticKey ? syntheticKey
                                                             : FieldInput::state.keyboard.biosKey);
        if (!guestBootSettled) {
            if (!key) key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
            if (key == 0) key = static_cast<std::uint16_t>((hostKey >> 16) & 0xFFFFu);
        }
        if (key == 0x0Du || key == '\r') key = kEnter;
        if (!guestBootSettled && (screenHas(e, "Press F8") || screenHas(e, "F5 to")))
            key = kEnter;
        else if (!guestBootSettled && key == 0 && ticks > 200'000u && (e->x86.R_CS & 0xFFFFu) < 0x0060u)
            key = kEnter;
        if (ah == 0x00 || ah == 0x10) {
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | key;
            if (!guestBootSettled && key == syntheticKey)
                syntheticKey = 0;
            e->x86.R_FLG &= ~F_ZF;
            return 1;
        }
        if (ah == 0x01) {
            if (key != 0 || keyDown) {
                e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | key;
                e->x86.R_FLG &= ~F_ZF;
            } else {
                e->x86.R_EAX &= 0xFFFF00FFu;
                e->x86.R_FLG |= F_ZF;
            }
            return 1;
        }
        return 1;
    }

    if (num == 0x1A) {
        FieldCmos::syncClockFromHost();
        if (ah == 0x00) {
            e->x86.R_CH = FieldCmos::regs[FieldCmos::REG_HOUR];
            e->x86.R_CL = FieldCmos::regs[FieldCmos::REG_MIN];
            e->x86.R_DH = FieldCmos::regs[FieldCmos::REG_SEC];
            e->x86.R_DL = 0;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x01) {
            FieldCmos::regs[FieldCmos::REG_HOUR] = static_cast<std::uint8_t>(e->x86.R_CH);
            FieldCmos::regs[FieldCmos::REG_MIN]  = static_cast<std::uint8_t>(e->x86.R_CL);
            FieldCmos::regs[FieldCmos::REG_SEC]  = static_cast<std::uint8_t>(e->x86.R_DH);
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x02) {
            e->x86.R_CH = FieldCmos::regs[FieldCmos::REG_HOUR_ALRM];
            e->x86.R_CL = FieldCmos::regs[FieldCmos::REG_MIN_ALRM];
            e->x86.R_DH = FieldCmos::regs[FieldCmos::REG_SEC_ALRM];
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x03) {
            FieldCmos::regs[FieldCmos::REG_HOUR_ALRM] = static_cast<std::uint8_t>(e->x86.R_CH);
            FieldCmos::regs[FieldCmos::REG_MIN_ALRM]  = static_cast<std::uint8_t>(e->x86.R_CL);
            FieldCmos::regs[FieldCmos::REG_SEC_ALRM]  = static_cast<std::uint8_t>(e->x86.R_DH);
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x04) {
            const int year = FieldCmos::fromBcd(FieldCmos::regs[FieldCmos::REG_YEAR])
                           + FieldCmos::fromBcd(FieldCmos::regs[FieldCmos::REG_CENTURY]) * 100;
            e->x86.R_CX = (e->x86.R_CX & 0xFFFF0000u) | static_cast<std::uint16_t>(year);
            e->x86.R_DH = FieldCmos::regs[FieldCmos::REG_MONTH];
            e->x86.R_DL = FieldCmos::regs[FieldCmos::REG_DAY];
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x05) {
            const std::uint16_t year = static_cast<std::uint16_t>(e->x86.R_CX & 0xFFFFu);
            FieldCmos::regs[FieldCmos::REG_YEAR]    = FieldCmos::toBcd(year % 100);
            FieldCmos::regs[FieldCmos::REG_CENTURY] = FieldCmos::toBcd(year / 100);
            FieldCmos::regs[FieldCmos::REG_MONTH]   = static_cast<std::uint8_t>(e->x86.R_DH);
            FieldCmos::regs[FieldCmos::REG_DAY]     = static_cast<std::uint8_t>(e->x86.R_DL);
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x07) {
            FieldCmos::regs[FieldCmos::REG_STAT_B] |= 0x01u;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x08) {
            e->x86.R_CX = (e->x86.R_CX & 0xFFFF0000u)
                        | static_cast<std::uint16_t>(
                              FieldCmos::fromBcd(FieldCmos::regs[FieldCmos::REG_CENTURY]) * 100u
                            + FieldCmos::fromBcd(FieldCmos::regs[FieldCmos::REG_YEAR]));
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        e->x86.R_EAX |= 1u << 8;
        e->x86.R_FLG |= F_CF;
        return 1;
    }

    if (num == 0x14)
        return FieldDosInt::handleInt14(e);

    if (num == 0x17)
        return FieldDosInt::handleInt17(e);

    if (num == 0x1B)
        return FieldDosInt::handleInt1B(e);

    if (num == 0x1D || num == 0x1E || num == 0x1F)
        return FieldDosInt::handleInt1D1E1F(e, num);

    if (num == 0x19) {
        if (pmExecActive)
            return 1;
        e->x86.R_EIP = FieldDos::BOOT_VECTOR;
        x86emu_set_seg_register(e, e->x86.R_CS_SEL, 0);
        return 1;
    }

    if (num == 0x33) {
        if (!FieldDosConfig::cfg.mouseEnabled) {
            e->x86.R_EAX &= 0xFFFF0000u;
            return 1;
        }
        const u16 ax = static_cast<u16>(e->x86.R_EAX & 0xFFFFu);
        auto& m = FieldInput::state.mouse;
        switch (ax) {
        case 0x0000:
            m.installed = true;
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0xFFFFu;
            e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | 0x0002u;
            break;
        case 0x0001: m.visible = true; break;
        case 0x0002: m.visible = false; break;
        case 0x0003:
            e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | static_cast<u16>(m.x & 0xFFFF);
            e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | static_cast<u16>(m.y & 0xFFFF);
            e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF00FFu) | (static_cast<u32>(m.buttons) << 8);
            break;
        case 0x0004: {
            const u16 nx = static_cast<u16>(e->x86.R_ECX & 0xFFFFu);
            const u16 ny = static_cast<u16>(e->x86.R_EDX & 0xFFFFu);
            m.x = nx; m.y = ny;
            break;
        }
        case 0x0005: {
            const u16 btn = static_cast<u16>(e->x86.R_CX & 0xFFFFu);
            e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u)
                        | static_cast<u16>(FieldInput::state.mouse.pressCount[btn & 3u] & 0xFFFFu);
            FieldInput::state.mouse.pressCount[btn & 3u] = 0;
            break;
        }
        case 0x0006: {
            const u16 btn = static_cast<u16>(e->x86.R_CX & 0xFFFFu);
            e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u)
                        | static_cast<u16>(FieldInput::state.mouse.releaseCount[btn & 3u] & 0xFFFFu);
            FieldInput::state.mouse.releaseCount[btn & 3u] = 0;
            break;
        }
        case 0x0007:
            FieldInput::state.mouse.mickeysX = static_cast<u16>(e->x86.R_CX & 0xFFFFu);
            FieldInput::state.mouse.mickeysY = static_cast<u16>(e->x86.R_EDX & 0xFFFFu);
            break;
        case 0x0008:
            FieldInput::state.mouse.mickeysX = static_cast<u16>(e->x86.R_CX & 0xFFFFu);
            FieldInput::state.mouse.mickeysY = static_cast<u16>(e->x86.R_EDX & 0xFFFFu);
            break;
        case 0x000B: {
            const auto& c = FieldDosConfig::cfg;
            const std::int32_t mx = (c.mouseMaxX > c.mouseMinX)
                ? (m.relX * static_cast<std::int32_t>(FieldInput::state.mouse.mickeysX))
                  / static_cast<std::int32_t>(c.mouseMaxX - c.mouseMinX + 1)
                : m.relX;
            const std::int32_t my = (c.mouseMaxY > c.mouseMinY)
                ? (m.relY * static_cast<std::int32_t>(FieldInput::state.mouse.mickeysY))
                  / static_cast<std::int32_t>(c.mouseMaxY - c.mouseMinY + 1)
                : m.relY;
            e->x86.R_CX = (e->x86.R_CX & 0xFFFF0000u) | static_cast<u16>(mx & 0xFFFFu);
            e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | static_cast<u16>(my & 0xFFFFu);
            m.relX = 0;
            m.relY = 0;
            break;
        }
        default:
            break;
        }
        return 1;
    }

    if (num == 0x15 && ah == 0x84) {
        if (!FieldDosConfig::cfg.joystickEnabled) {
            e->x86.R_EAX |= 1u << 8;
            e->x86.R_FLG |= F_CF;
            return 1;
        }
        const auto& j = FieldInput::state.joystick;
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu)
                     | (static_cast<u32>(j.buttons1 & 3u) << 8);
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }

    if (num == 0x0D && pmExecActive && FieldDpmi::isProtected(e)) {
        const std::uint32_t pc = FieldDpmi::pmFaultPc(e);
        const std::uint8_t b0 = static_cast<std::uint8_t>(x86emu_read_byte(e, pc));
        const std::uint8_t b1 = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 1u));
        const std::uint16_t dsSel = static_cast<std::uint16_t>(e->x86.R_DS & 0xFFFFu);
        const std::uint16_t esSel = static_cast<std::uint16_t>(e->x86.R_ES & 0xFFFFu);
        const bool badData = !FieldDpmi::pmSelValid(dsSel) || e->x86.R_DS_BASE == 0u;
        const bool badEs   = !FieldDpmi::pmSelValid(esSel) || e->x86.R_ES_BASE == 0u;
        if ((badData || badEs)
            && (b0 == 0x26u || b0 == 0x36u || b0 == 0x3Eu || b0 == 0x65u || b0 == 0x8Eu
                || (b0 == 0x66u && (b1 == 0x26u || b1 == 0x3Eu)))) {
            FieldDpmi::pmEnsureFlatDataSegs(e);
            return 1;
        }
        if (b0 == 0x90u || b0 == 0xEEu || b0 == 0xECu || b0 == 0xE4u || b0 == 0xE6u) {
            FieldDpmi::advancePmFaultEip(e);
            return 1;
        }
        if (b0 == 0xB4u && b1 == 0x4Au
            && static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 2u)) == 0xCDu
            && static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 3u)) == 0x21u) {
            e->x86.R_EAX = (e->x86.R_EAX & 0x00FFFFFFu) | 0x4A0000u;
            e->x86.R_EIP = FieldDpmi::pmFaultEip(e) + 4u;
            return 1;
        }
        if (b0 == 0xCDu) {
            const std::uint8_t vint = b1;
            e->x86.R_EIP = FieldDpmi::pmFaultEip(e) + 2u;
            e->x86.intr_type = 0;
            if (vint == 0x21u && pc >= e->x86.R_CS_BASE + 2u
                && x86emu_read_byte(e, pc - 2u) == 0xB4u
                && x86emu_read_byte(e, pc - 1u) == 0x4Au)
                e->x86.R_EAX = (e->x86.R_EAX & 0x00FFFFFFu) | 0x4A0000u;
            if (vint == 0x31u) {
                FieldDpmi::handleInt31(e);
                return 1;
            }
            return handleInt(e, vint, 0u, false, 0u);
        }
        if (b0 == 0x83u && b1 == 0xE4u) {
            e->x86.R_EIP = FieldDpmi::pmFaultEip(e) + 3u;
            return 1;
        }
        if (FieldDpmi::tryPmLoadSegGpInsn(e)) return 1;
        if (FieldDpmi::tryPmStackSegInsn(e)) return 1;
        if (FieldDpmi::tryEmulatePmGpInsn(e)) return 1;
        if (FieldX86Native::tryEmulateFfCallJmp(e, FieldDpmi::pmFaultEip(e))) return 1;
        if (FieldX86Native::tryEmulateNearRet(e, FieldDpmi::pmFaultEip(e))) return 1;
        if (FieldX86Native::tryEmulateNearCallRel(e, FieldDpmi::pmFaultEip(e))) return 1;
        if (FieldDosInt::traceEnabled())
            FieldDosInt::traceGpFault(e, b0);
        FieldDpmi::pmEnsureFlatSegLimits(e);
        FieldDpmi::pmEnsureFlatDataSegs(e);
        if (FieldDpmi::pmStepActive)
            FieldDpmi::advancePmFaultEip(e);
        else
            FieldDpmi::pmNativeStep(e);
        return 1;
    }

    if (num >= 0x22u && num <= 0x27u)
        return FieldDosInt::chainOrIret(e, num);

    if (num == 0x2F) {
        if (FieldXms::handleInt2fXms(e)) return 1;
        if (FieldDpmi::handleInt2f(e)) return 1;
        const std::uint8_t ah = static_cast<std::uint8_t>((e->x86.R_EAX >> 8) & 0xFFu);
        const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
        if (ah == 0x15u) {
            std::uint16_t outBx = 0, outCx = 0, outDx = 0;
            std::uint32_t outEax = 0;
            const std::uint16_t ax = static_cast<std::uint16_t>(e->x86.R_EAX & 0xFFFFu);
            const std::uint16_t bx = static_cast<std::uint16_t>(e->x86.R_EBX & 0xFFFFu);
            const std::uint16_t cx = static_cast<std::uint16_t>(e->x86.R_ECX & 0xFFFFu);
            if (FieldMscdex::handle(ax, bx, cx, outBx, outCx, outDx, outEax)) {
                e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | outBx;
                e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | outCx;
                e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | outDx;
                e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | (outEax & 0xFFFFu);
                return 1;
            }
            e->x86.R_AL = 0u;
            return 1;
        }
        if (ah == 0x52u) {
            if (al == 0x00u) {
                e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u) | 0x0700u;
                e->x86.R_CX = (e->x86.R_CX & 0xFFFF0000u) | 0x0100u;
                e->x86.R_DX = (e->x86.R_DX & 0xFFFF0000u) | 10u;
                e->x86.R_AL = 0xFFu;
                return 1;
            }
            if (al == 0x01u) {
                std::uint32_t outEax = e->x86.R_EAX;
                std::uint16_t ebx = static_cast<std::uint16_t>(e->x86.R_EBX & 0xFFFFu);
                std::uint16_t ecx = static_cast<std::uint16_t>(e->x86.R_ECX & 0xFFFFu);
                std::uint16_t edx = static_cast<std::uint16_t>(e->x86.R_EDX & 0xFFFFu);
                const std::uint16_t code = ecx;
                if (FieldRtxDrivers::ioctl(code, outEax, ebx, ecx, edx)) {
                    e->x86.R_EAX = (e->x86.R_EAX & 0xFFFFFF00u) | (outEax & 0xFFu);
                    e->x86.R_AL = 0xFFu;
                } else {
                    e->x86.R_AL = 0u;
                }
                return 1;
            }
            if (al == 0x02u) {
                FieldRtxDrivers::syncAllLayers();
                e->x86.R_AL = 0xFFu;
                return 1;
            }
            e->x86.R_AL = 0u;
            return 1;
        }
        if (pmExecActive) {
            e->x86.R_AL = 0u;
            return 1;
        }
        return ivtEmpty(e, num) ? 1 : 0;
    }

    if (num == 0x31) {
        if (FieldDpmi::handleInt31(e)) return 1;
        return 1;
    }

    if (num == 0x21) {
        if (tryHostFileInt21(e)) return 1;
        if (tryHostComExec(e)) return 1;
        if (FieldInt21::hostDosEligible(e, pmExecActive))
            return handleInt21Early(e, ah);
        if (pmExecActive)
            return handleInt21Early(e, ah);
        if (!ivtEmpty(e, num)) return 0;
        return handleInt21Early(e, ah);
    }

    if (num == 0x28 || num == 0x2B) {
        const int idle = FieldDosInt::handleDosIdle(e, num);
        if (idle == 0) return 0;
        FieldDevices::pumpAudio();
        return 1;
    }

    if (num == 0x67) {
        if (FieldRtxMemory::emmLive() && FieldEms::handleInt34(e))
            return 1;
        const std::uint8_t ah67 = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
        if (pmExecActive && ah67 != 0x00u && ah67 != 0x05u && ah67 != 0x08u && ah67 != 0x10u) {
            e->x86.R_EAX &= 0xFFFF00FFu;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah67 == 0x00u) {
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x0300u;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah67 == 0x05u) {
            e->x86.R_DX = (e->x86.R_DX & 0xFFFF0000u) | 0x0001u;
            e->x86.R_EAX &= 0xFFFF00FFu;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah67 == 0x08u) {
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | FieldPlatform::EXTENDED_KB;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah67 == 0x10u) {
            e->x86.R_BL = 0u;
            e->x86.R_EAX &= 0xFFFF00FFu;
            e->x86.R_FLG &= ~F_CF;
            return 1;
        }
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x0080u;
        e->x86.R_FLG |= F_CF;
        return 1;
    }

    if (pmExecActive)
        return FieldDosInt::chainOrIret(e, num);

    return ivtEmpty(e, num) ? 1 : 0;
}

inline void recoverFromHalt(x86emu_t* e) {
    if (pmExecActive && FieldDpmi::isProtected(e) && (e->x86.mode & _MODE_HALTED))
        return;
    if (pmExecActive && FieldDpmi::isProtected(e))
        FieldDpmi::pmEnsureFlatSegLimits(e);
    /* MS-DOS idles at 0000:0010 between INT 16 polls — must unhalt to reach C:\> */
    e->x86.mode &= ~_MODE_HALTED;
    e->x86.R_FLG |= F_IF;
}

} // namespace FieldBios