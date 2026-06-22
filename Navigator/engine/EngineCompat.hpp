// AMOURANTHRTX Engine Compatibility Layer v3.1
// Do not break backward compat. All old APIs preserved.
#pragma once

#define AMX_ENGINE_VERSION "3.1-FieldStable"
#define AMX_KEEP_COMPAT 1

// New optional features with defaults
inline bool g_EnableAdvancedThermo = false; // opt-in

namespace amx {
  // Safe additions only
  struct EngineOptions {
    bool sdfSuperSample = true;
    bool fieldDie64MiB = true; // preserved
    // ... old behavior default
  };
}

// TODO: integrate into main headers via #include "EngineCompat.hpp" 