#pragma once

#include "FieldFabric.hpp"

namespace FieldStorage {

struct HyperState {
    bool enabled = false;
    double leadInPeak = 0.0;
    double leadOutPeak = 0.0;
    double entropyFold = 0.0;
    double fabricScale = 1.0;
};

extern HyperState hyper;

void hyperTick(std::uint32_t blockIndex) noexcept;

} // namespace FieldStorage