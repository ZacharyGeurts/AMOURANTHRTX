#pragma once

// Intel 8272A/NEC μPD765 floppy disk controller — ports 0x3F0-0x3F7, IRQ6.

#include "FieldDos.hpp"
#include "FieldPic8259.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldFdc765 {

constexpr std::uint16_t PORT_DOR  = 0x3F2u;
constexpr std::uint16_t PORT_MSR  = 0x3F4u;
constexpr std::uint16_t PORT_DATA = 0x3F5u;
constexpr std::uint16_t PORT_DIR  = 0x3F7u;

constexpr std::uint8_t FLOPPY_SPT   = 9u;
constexpr std::uint8_t FLOPPY_HEADS = 2u;
constexpr std::uint16_t FLOPPY_MAX_CYL = 79u;

enum class Phase : std::uint8_t {
    Idle,
    Command,
    Result,
    DataIn,
    DataOut,
};

inline std::uint8_t dor = 0x1Cu; /* motors off, drive 0, DMA+IRQ enabled */
inline std::uint8_t msr = 0x80u; /* RQM */
inline std::uint8_t dir = 0x00u; /* no disk change */
inline Phase phase = Phase::Idle;
inline std::uint8_t pendingCmd = 0;
inline std::uint8_t paramNeed = 0;
inline std::uint8_t paramGot = 0;
inline std::uint8_t params[16]{};
inline std::vector<std::uint8_t> fifo;
inline std::size_t fifoPos = 0;
inline std::uint8_t selectedDrive = 0;
inline std::uint8_t cylinder[4]{};
inline bool irqPending = false;
inline std::uint8_t* guestRamPtr = nullptr;

inline void bindGuest(const std::uint8_t* ram) noexcept {
    guestRamPtr = const_cast<std::uint8_t*>(ram);
}

inline std::uint8_t cmdParamBytes(std::uint8_t cmd) noexcept {
    switch (cmd & 0x1Fu) {
    case 0x03u: return 3u;  /* Specify */
    case 0x04u: return 1u;  /* Sense Drive Status */
    case 0x07u: return 1u;  /* Recalibrate */
    case 0x08u: return 0u;  /* Sense Interrupt Status */
    case 0x0Fu: return 2u;  /* Seek */
    case 0x05u: return 8u;  /* Write sector */
    case 0x06u: return 8u;  /* Read sector */
    default:
        if ((cmd & 0xF0u) == 0x40u || (cmd & 0xF0u) == 0x60u || (cmd & 0xF0u) == 0xC0u
            || (cmd & 0xF0u) == 0xA0u || (cmd & 0xF0u) == 0xE0u)
            return 8u;
        return 0u;
    }
}

inline bool isReadCmd(std::uint8_t cmd) noexcept {
    return (cmd & 0x1Fu) == 0x06u;
}

inline bool isWriteCmd(std::uint8_t cmd) noexcept {
    return (cmd & 0x1Fu) == 0x05u;
}

inline std::uint32_t floppyByteOffset(std::uint8_t cyl, std::uint8_t head,
                                      std::uint8_t sector) noexcept {
    if (cyl > FLOPPY_MAX_CYL || head >= FLOPPY_HEADS || sector < 1 || sector > FLOPPY_SPT)
        return 0xFFFFFFFFu;
    return FieldDos::DISK_IMAGE_BASE
         + (static_cast<std::uint32_t>(cyl) * (FLOPPY_SPT * FLOPPY_HEADS)
            + static_cast<std::uint32_t>(head) * FLOPPY_SPT
            + static_cast<std::uint32_t>(sector - 1u)) * 512u;
}

inline const std::uint8_t* sectorPtr(std::uint32_t byteOff) noexcept {
    if (byteOff == 0xFFFFFFFFu) return nullptr;
    if (guestRamPtr && byteOff + 512u <= FieldDos::GUEST_RAM_BYTES)
        return guestRamPtr + byteOff;
    const std::uint32_t imgOff = byteOff - FieldDos::DISK_IMAGE_BASE;
    if (!FieldDos::floppyImage.empty() && imgOff + 512u <= FieldDos::floppyImage.size())
        return FieldDos::floppyImage.data() + imgOff;
    return nullptr;
}

inline void updateMsr() noexcept {
    msr = 0x80u; /* RQM */
    msr |= static_cast<std::uint8_t>(selectedDrive & 3u);
    if (!fifo.empty() && fifoPos < fifo.size()) {
        msr |= 0x40u; /* DIO — data to CPU */
        if (phase == Phase::DataOut)
            msr &= static_cast<std::uint8_t>(~0x40u);
    }
    if (phase == Phase::Result || phase == Phase::DataIn)
        msr |= 0x40u;
    if (phase == Phase::DataOut)
        msr &= static_cast<std::uint8_t>(~0x40u);
    msr |= 0x10u; /* FDC busy / non-DMA */
}

inline void pushResult(std::uint8_t st0, std::uint8_t st1, std::uint8_t st2,
                       std::uint8_t cyl, std::uint8_t head, std::uint8_t sector) noexcept {
    fifo.clear();
    fifoPos = 0;
    fifo.push_back(st0);
    fifo.push_back(st1);
    fifo.push_back(st2);
    fifo.push_back(cyl);
    fifo.push_back(head);
    fifo.push_back(sector);
    fifo.push_back(2u); /* 512-byte sector size code */
    phase = Phase::Result;
    updateMsr();
}

inline void completeIrq() noexcept {
    irqPending = true;
    FieldPic8259::raiseIrq(6);
}

inline void execReadSector() noexcept {
    const std::uint8_t hdu = params[0];
    const std::uint8_t cyl = params[1];
    const std::uint8_t head = params[2];
    const std::uint8_t sector = params[3];
    const std::uint8_t drive = static_cast<std::uint8_t>(hdu & 3u);
    selectedDrive = drive;
    cylinder[drive] = cyl;

    const std::uint32_t off = floppyByteOffset(cyl, head, sector);
    const std::uint8_t* src = sectorPtr(off);
    fifo.clear();
    fifoPos = 0;
    if (!src) {
        pushResult(0x40u, 0x01u, 0x00u, cyl, head, sector);
        completeIrq();
        return;
    }
    fifo.push_back(0x00u); /* ST0 */
    fifo.push_back(0x00u); /* ST1 */
    fifo.push_back(0x00u); /* ST2 */
    fifo.push_back(cyl);
    fifo.push_back(head);
    fifo.push_back(sector);
    fifo.push_back(2u);
    for (int i = 0; i < 512; ++i)
        fifo.push_back(src[i]);
    fifo.push_back(0xF7u); /* CRC placeholder */
    fifo.push_back(0xF7u);
    phase = Phase::DataIn;
    updateMsr();
    completeIrq();
}

inline void execWriteSector() noexcept {
    const std::uint8_t hdu = params[0];
    const std::uint8_t cyl = params[1];
    const std::uint8_t drive = static_cast<std::uint8_t>(hdu & 3u);
    selectedDrive = drive;
    cylinder[drive] = cyl;
    fifo.clear();
    fifoPos = 0;
    phase = Phase::DataOut;
    updateMsr();
}

inline void finishWriteSector() noexcept {
    const std::uint8_t cyl = params[1];
    const std::uint8_t head = params[2];
    const std::uint8_t sector = params[3];
    const std::uint32_t off = floppyByteOffset(cyl, head, sector);
    if (off != 0xFFFFFFFFu && guestRamPtr
        && off + 512u <= FieldDos::GUEST_RAM_BYTES
        && fifo.size() >= 512u) {
        std::memcpy(guestRamPtr + off, fifo.data(), 512u);
        const std::uint32_t imgOff = off - FieldDos::DISK_IMAGE_BASE;
        if (imgOff + 512u <= FieldDos::floppyImage.size())
            std::memcpy(FieldDos::floppyImage.data() + imgOff, fifo.data(), 512u);
    }
    pushResult(0x00u, 0x00u, 0x00u, cyl, head, sector);
    completeIrq();
}

inline void execCommand(std::uint8_t cmd) noexcept {
    const std::uint8_t sub = cmd & 0x1Fu;
    if (sub == 0x03u) { /* Specify */
        phase = Phase::Idle;
        fifo.clear();
        fifoPos = 0;
        updateMsr();
        return;
    }
    if (sub == 0x07u) { /* Recalibrate */
        const std::uint8_t drive = static_cast<std::uint8_t>(params[0] & 3u);
        cylinder[drive] = 0;
        phase = Phase::Idle;
        completeIrq();
        return;
    }
    if (sub == 0x08u) { /* Sense Interrupt Status */
        fifo.clear();
        fifoPos = 0;
        fifo.push_back(irqPending ? 0x20u : 0x00u);
        fifo.push_back(cylinder[selectedDrive]);
        phase = Phase::Result;
        irqPending = false;
        updateMsr();
        return;
    }
    if (sub == 0x0Fu) { /* Seek */
        const std::uint8_t drive = static_cast<std::uint8_t>(params[0] & 3u);
        cylinder[drive] = params[1];
        selectedDrive = drive;
        phase = Phase::Idle;
        completeIrq();
        return;
    }
    if (sub == 0x04u) { /* Sense Drive Status */
        fifo.clear();
        fifoPos = 0;
        const std::uint8_t drive = static_cast<std::uint8_t>(params[0] & 3u);
        fifo.push_back(static_cast<std::uint8_t>(
            0x28u | (drive == selectedDrive ? 0x20u : 0x00u)));
        phase = Phase::Result;
        updateMsr();
        return;
    }
    if (isReadCmd(cmd)) {
        execReadSector();
        return;
    }
    if (isWriteCmd(cmd)) {
        execWriteSector();
        return;
    }
    phase = Phase::Idle;
    completeIrq();
}

inline void reset() noexcept {
    dor = 0x1Cu;
    msr = 0x80u;
    dir = 0x00u;
    phase = Phase::Idle;
    pendingCmd = 0;
    paramNeed = 0;
    paramGot = 0;
    fifo.clear();
    fifoPos = 0;
    irqPending = false;
    selectedDrive = 0;
    std::memset(cylinder, 0, sizeof cylinder);
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == PORT_DOR) {
        dor = val;
        selectedDrive = static_cast<std::uint8_t>(val & 3u);
        return;
    }
    if (port == PORT_MSR) return; /* DSR — ignored */
    if (port == PORT_DATA) {
        if (phase == Phase::DataOut) {
            fifo.push_back(val);
            if (fifo.size() >= 514u) { /* 512 data + 2 CRC */
                fifo.resize(512u);
                finishWriteSector();
            }
            return;
        }
        if (phase == Phase::Command || phase == Phase::Idle) {
            if (paramGot == 0) {
                pendingCmd = val;
                paramNeed = cmdParamBytes(val);
                paramGot = 0;
                phase = Phase::Command;
                if (paramNeed == 0) {
                    execCommand(pendingCmd);
                    paramGot = 0;
                    pendingCmd = 0;
                    phase = phase == Phase::Command ? Phase::Idle : phase;
                }
                return;
            }
        }
        if (phase == Phase::Command && paramGot < paramNeed) {
            params[paramGot++] = val;
            if (paramGot >= paramNeed) {
                execCommand(pendingCmd);
                paramGot = 0;
                paramNeed = 0;
                pendingCmd = 0;
            }
            return;
        }
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port == PORT_MSR) {
        updateMsr();
        return msr;
    }
    if (port == PORT_DIR)
        return static_cast<std::uint8_t>(dir | 0x80u); /* disk ready */
    if (port == PORT_DATA) {
        if (fifoPos < fifo.size()) {
            const std::uint8_t v = fifo[fifoPos++];
            if (fifoPos >= fifo.size() && phase == Phase::Result)
                phase = Phase::Idle;
            updateMsr();
            return v;
        }
        return 0xFFu;
    }
    if (port >= 0x3F0u && port <= 0x3F7u && port != 0x3F6u)
        return 0x00u;
    return 0xFFu;
}

inline bool handlesPort(std::uint16_t port) noexcept {
    return port >= 0x3F0u && port <= 0x3F7u && port != 0x3F6u;
}

} // namespace FieldFdc765