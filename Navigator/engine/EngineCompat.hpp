#pragma once

// Engine compatibility layer — optional features default off; legacy APIs preserved.

#define AMX_ENGINE_VERSION "3.2-FieldStable"
#define AMX_KEEP_COMPAT 1

namespace amx {

struct EngineOptions {
    bool sdfSuperSample   = true;
    bool fieldDie64MiB     = true;
    bool zeroCostGuards   = true;
    bool wavePeakModulation = true;
    bool extendedFieldDispatch = false;  // --extended-field: non-point wave dispatch
};

inline EngineOptions gOptions{};

} // namespace amx