#pragma once

#include "../../FieldStorage.hpp"

namespace FieldChips {

inline std::uint32_t scaledDieCycles(std::uint32_t base) noexcept {
    const double scale = FieldStorage::chipsFabricScale();
    return static_cast<std::uint32_t>(static_cast<double>(base) * scale);
}

} // namespace FieldChips