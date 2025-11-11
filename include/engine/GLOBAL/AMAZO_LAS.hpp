// include/engine/GLOBAL/AMAZO_LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMAZO_LAS – ultra-thin forwarder to the real LIGHT_WARRIORS_LAS singleton
// • No dependencies except the real LAS header (included later)
// • Protected by StoneKey – the real implementation is compiled only when the
//   key is present (see StoneKey.hpp)
// =============================================================================

#pragma once

#include "engine/GLOBAL/StoneKey.hpp"   // <-- guards the real LAS implementation

// -----------------------------------------------------------------------------
// The real LAS lives in LAS.hpp – we only expose a *named* singleton here.
// -----------------------------------------------------------------------------
#if defined(kStone1) && defined(kStone2) && kStone1 && kStone2
    #include "engine/GLOBAL/LAS.hpp"
    // Forward the real class under the old name expected by the renderer
    using AMAZO_LAS = LIGHT_WARRIORS_LAS;
#else
    // -----------------------------------------------------------------
    // StoneKey not satisfied → stub that throws at runtime.
    // -----------------------------------------------------------------
    #include <stdexcept>
    class AMAZO_LAS {
    public:
        static AMAZO_LAS& get() {
            static AMAZO_LAS dummy;
            return dummy;
        }
        void setHypertraceEnabled(bool) const { throw std::runtime_error("StoneKey breach – LAS disabled"); }
        // …any other member the renderer calls → throw
    };
#endif

// -----------------------------------------------------------------------------
// Global alias that the renderer uses everywhere
// -----------------------------------------------------------------------------
inline auto& AMAZO_LAS_GET() noexcept { return AMAZO_LAS::get(); }