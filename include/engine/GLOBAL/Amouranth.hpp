// include/engine/GLOBAL/Amouranth.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// AMOURANTH™ — PINK PHOTONS APOCALYPSE v3 — NOV 12 2025
// • One-time startup voice: vendor + fingerprint + shields up
// • Called via LOG_AMOURANTH()
// • Defined in main.cpp (StoneKey.hpp included only there)
// =============================================================================

#pragma once

// Forward declaration — defined in main.cpp
void log_amouranth() noexcept;

#define LOG_AMOURANTH() log_amouranth()