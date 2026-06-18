#pragma once

// FieldDos ↔ FieldRaid glue (included after FieldDos.hpp to avoid circular includes).

#include "FieldRaid.hpp"

namespace FieldDos {

inline std::uint32_t hdMirrorCopyBytes() noexcept {
    return FieldRaid::hdMirrorCopyBytes();
}

inline std::uint8_t readHdByte(std::uint32_t byteOff, const std::uint8_t* guestRamPtr) noexcept {
    return FieldRaid::readHd(byteOff, guestRamPtr);
}

inline void writeHdByte(std::uint32_t byteOff, std::uint8_t val, std::uint8_t* guestRamPtr) noexcept {
    FieldRaid::writeHd(byteOff, val, guestRamPtr);
}

inline void notifyHdHostWrite(std::uint64_t off, std::uint32_t len,
                              std::uint8_t* guestRamPtr) noexcept {
    FieldRaid::onHostRangeTouched(off, len, guestRamPtr);
}

} // namespace FieldDos