#pragma once

#include "../../FieldStorage.hpp"

namespace FieldChips {

inline std::uint32_t scaledDieCycles(std::uint32_t base) noexcept {
    double scale = FieldStorage::chipsFabricScale();
    if (FieldStorage::hyperEnabled()) {
        const double lead = FieldStorage::hyperLeadInPeak() * 1.15;
        const double out = FieldStorage::hyperLeadOutPeak() * 0.87;
        const double fold = FieldStorage::hyperEntropyFold();
        scale *= 1.0 + lead * 0.12 + out * 0.08 + fold * 0.05;
    }
    return static_cast<std::uint32_t>(static_cast<double>(base) * scale);
}

} // namespace FieldChips