#pragma once

// Primary IDE controller — PIO ports 0x1F0-0x1F7, control 0x3F6, IRQ14, LBA28.

#include "FieldDos.hpp"
#include "FieldPic8259.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldIde {

constexpr std::uint16_t BASE_DATA   = 0x1F0u;
constexpr std::uint16_t BASE_ERROR  = 0x1F1u;
constexpr std::uint16_t BASE_SECCNT = 0x1F2u;
constexpr std::uint16_t BASE_SECNUM = 0x1F3u;
constexpr std::uint16_t BASE_CYL_LO = 0x1F4u;
constexpr std::uint16_t BASE_CYL_HI = 0x1F5u;
constexpr std::uint16_t BASE_DRVHD = 0x1F6u;
constexpr std::uint16_t BASE_STATUS = 0x1F7u;
constexpr std::uint16_t PORT_CTRL   = 0x3F6u;

constexpr std::uint8_t STATUS_BSY  = 0x80u;
constexpr std::uint8_t STATUS_RDY  = 0x40u;
constexpr std::uint8_t STATUS_DF   = 0x20u;
constexpr std::uint8_t STATUS_DRQ  = 0x08u;
constexpr std::uint8_t STATUS_ERR  = 0x01u;

constexpr std::uint8_t CMD_READ_SECTORS  = 0x20u;
constexpr std::uint8_t CMD_WRITE_SECTORS = 0x30u;
constexpr std::uint8_t CMD_IDENTIFY      = 0xECu;
constexpr std::uint8_t CMD_SET_FEATURES  = 0xEFu;

inline std::uint8_t errorReg = 0x01u;
inline std::uint8_t secCount = 1u;
inline std::uint8_t secNum   = 0u;
inline std::uint8_t cylLo    = 0u;
inline std::uint8_t cylHi    = 0u;
inline std::uint8_t driveHead = 0xE0u;
inline std::uint8_t status   = STATUS_RDY | STATUS_DF;
inline std::uint8_t control  = 0x02u; /* nIEN */
inline std::uint8_t dataHi   = 0u;
inline bool dataReadPhase = false;
inline std::vector<std::uint8_t> dataBuf;
inline std::size_t dataPos = 0;
inline bool writePhase = false;

inline std::uint32_t currentLba() noexcept {
    if ((driveHead & 0xC0u) != 0xE0u) {
        const std::uint16_t cyl = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(cylLo) | (static_cast<std::uint16_t>(cylHi) << 8));
        const std::uint8_t head = static_cast<std::uint8_t>(driveHead & 0x0Fu);
        return FieldDos::hdLbaOffset(cyl, head, secNum);
    }
    return FieldDos::hdAbsLbaOffset(
        static_cast<std::uint32_t>(secNum)
        | (static_cast<std::uint32_t>(cylLo) << 8)
        | (static_cast<std::uint32_t>(cylHi) << 16)
        | (static_cast<std::uint32_t>(driveHead & 0x0Fu) << 24));
}

inline void raiseIrqIfEnabled() noexcept {
    if ((control & 0x02u) == 0u)
        FieldPic8259::raiseIrq(14);
}

inline void beginReadSectors() noexcept {
    dataBuf.clear();
    dataPos = 0;
    writePhase = false;
    if (!FieldDos::hdReady) {
        status = STATUS_ERR | STATUS_RDY;
        errorReg = 0x04u;
        raiseIrqIfEnabled();
        return;
    }
    std::uint32_t lbaOff = currentLba();
    const std::uint8_t n = secCount ? secCount : 1u;
    for (std::uint8_t i = 0; i < n; ++i) {
        std::uint32_t byteOff = lbaOff;
        if ((driveHead & 0xC0u) == 0xE0u)
            byteOff = FieldDos::hdAbsLbaOffset(
                static_cast<std::uint32_t>(secNum)
                + (static_cast<std::uint32_t>(cylLo) << 8)
                + (static_cast<std::uint32_t>(cylHi) << 16)
                + (static_cast<std::uint32_t>(driveHead & 0x0Fu) << 24)
                + static_cast<std::uint32_t>(i));
        else if (lbaOff != 0xFFFFFFFFu)
            byteOff = lbaOff + static_cast<std::uint32_t>(i) * FieldDos::HD_SECTOR_BYTES;
        else
            byteOff = 0xFFFFFFFFu;

        if (byteOff == 0xFFFFFFFFu
            || byteOff + FieldDos::HD_SECTOR_BYTES > FieldDos::hdImage.size()) {
            status = STATUS_ERR | STATUS_RDY;
            errorReg = 0x10u;
            raiseIrqIfEnabled();
            return;
        }
        const std::size_t old = dataBuf.size();
        dataBuf.resize(old + FieldDos::HD_SECTOR_BYTES);
        std::memcpy(dataBuf.data() + old, FieldDos::hdImage.data() + byteOff,
                    FieldDos::HD_SECTOR_BYTES);
    }
    status = STATUS_RDY | STATUS_DF | STATUS_DRQ;
    dataReadPhase = true;
    dataHi = 0;
}

inline void beginWriteSectors() noexcept {
    dataBuf.clear();
    dataBuf.resize(static_cast<std::size_t>(secCount ? secCount : 1u) * 512u, 0);
    dataPos = 0;
    writePhase = true;
    dataReadPhase = false;
    status = STATUS_RDY | STATUS_DF | STATUS_DRQ;
    dataHi = 0;
}

inline void finishWriteSectors() noexcept {
    if (!FieldDos::hdReady) {
        status = STATUS_ERR | STATUS_RDY;
        errorReg = 0x04u;
        raiseIrqIfEnabled();
        return;
    }
    std::uint32_t lbaOff = currentLba();
    const std::uint8_t n = secCount ? secCount : 1u;
    for (std::uint8_t i = 0; i < n; ++i) {
        std::uint32_t byteOff = lbaOff;
        if ((driveHead & 0xC0u) == 0xE0u)
            byteOff = FieldDos::hdAbsLbaOffset(
                static_cast<std::uint32_t>(secNum)
                + (static_cast<std::uint32_t>(cylLo) << 8)
                + (static_cast<std::uint32_t>(cylHi) << 16)
                + (static_cast<std::uint32_t>(driveHead & 0x0Fu) << 24)
                + static_cast<std::uint32_t>(i));
        else if (lbaOff != 0xFFFFFFFFu)
            byteOff = lbaOff + static_cast<std::uint32_t>(i) * FieldDos::HD_SECTOR_BYTES;

        if (byteOff == 0xFFFFFFFFu
            || byteOff + FieldDos::HD_SECTOR_BYTES > FieldDos::hdImage.size()) {
            status = STATUS_ERR | STATUS_RDY;
            errorReg = 0x10u;
            raiseIrqIfEnabled();
            return;
        }
        std::memcpy(FieldDos::hdImage.data() + byteOff,
                    dataBuf.data() + static_cast<std::size_t>(i) * 512u, 512u);
    }
    status = STATUS_RDY | STATUS_DF;
    writePhase = false;
    dataReadPhase = false;
    raiseIrqIfEnabled();
}

inline void issueCommand(std::uint8_t cmd) noexcept {
    status = STATUS_BSY | STATUS_RDY;
    switch (cmd) {
    case CMD_READ_SECTORS:
        beginReadSectors();
        break;
    case CMD_WRITE_SECTORS:
        beginWriteSectors();
        break;
    case CMD_IDENTIFY:
    case CMD_SET_FEATURES:
        status = STATUS_RDY | STATUS_DF;
        raiseIrqIfEnabled();
        break;
    default:
        status = STATUS_RDY | STATUS_DF | STATUS_ERR;
        errorReg = 0x04u;
        raiseIrqIfEnabled();
        break;
    }
}

inline void reset() noexcept {
    errorReg = 0x01u;
    secCount = 1u;
    secNum = 0u;
    cylLo = 0u;
    cylHi = 0u;
    driveHead = 0xE0u;
    status = STATUS_RDY | STATUS_DF;
    control = 0x02u;
    dataHi = 0;
    dataReadPhase = false;
    writePhase = false;
    dataBuf.clear();
    dataPos = 0;
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == PORT_CTRL) {
        control = val;
        if ((val & 0x04u) != 0u) reset();
        return;
    }
    if (port < BASE_DATA || port > BASE_STATUS) return;

    switch (port) {
    case BASE_DATA:
        if (writePhase) {
            if (!dataHi) {
                dataHi = val;
            } else {
                if (dataPos < dataBuf.size()) {
                    dataBuf[dataPos++] = dataHi;
                    dataBuf[dataPos++] = val;
                }
                dataHi = 0;
                if (dataPos >= dataBuf.size()) {
                    status = STATUS_RDY | STATUS_DF;
                    finishWriteSectors();
                }
            }
        }
        return;
    case BASE_ERROR:
        return;
    case BASE_SECCNT:
        secCount = val;
        return;
    case BASE_SECNUM:
        secNum = val;
        return;
    case BASE_CYL_LO:
        cylLo = val;
        return;
    case BASE_CYL_HI:
        cylHi = val;
        return;
    case BASE_DRVHD:
        driveHead = val;
        return;
    case BASE_STATUS:
        issueCommand(val);
        return;
    default:
        break;
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port == PORT_CTRL)
        return static_cast<std::uint8_t>((control & 0x02u) ? 0x00u : 0x02u);
    if (port < BASE_DATA || port > BASE_STATUS) return 0xFFu;

    switch (port) {
    case BASE_DATA:
        if (dataReadPhase && dataPos < dataBuf.size()) {
            std::uint8_t v;
            if (!dataHi) {
                v = dataBuf[dataPos++];
                dataHi = 1;
            } else {
                v = dataBuf[dataPos++];
                dataHi = 0;
            }
            if (dataPos >= dataBuf.size()) {
                status = STATUS_RDY | STATUS_DF;
                dataReadPhase = false;
                raiseIrqIfEnabled();
            }
            return v;
        }
        return 0x00u;
    case BASE_ERROR:
        return errorReg;
    case BASE_SECCNT:
        return secCount;
    case BASE_SECNUM:
        return secNum;
    case BASE_CYL_LO:
        return cylLo;
    case BASE_CYL_HI:
        return cylHi;
    case BASE_DRVHD:
        return driveHead;
    case BASE_STATUS:
        return status;
    default:
        break;
    }
    return 0xFFu;
}

inline bool handlesPort(std::uint16_t port) noexcept {
    return (port >= BASE_DATA && port <= BASE_STATUS) || port == PORT_CTRL;
}

} // namespace FieldIde