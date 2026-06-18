#pragma once

// Headless/visual QA driver for AmouranthOS — env AMOURANTHRTX_AOS_TEST=1

#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthOs.hpp"

#include <cstdlib>

namespace FieldAosTest {

inline bool enabled() noexcept {
    return std::getenv("AMOURANTHRTX_AOS_TEST") != nullptr;
}

inline void tickFrame(std::uint64_t frame, int w, int h) noexcept {
    if (!enabled()) return;
    FieldAmouranthOs::tick(w, h);
    if (frame == 28u) {
        FieldAmouranthOs::startOpen = true;
        FieldAmouranthMenu::rebuildVisible();
    }
}

} // namespace FieldAosTest