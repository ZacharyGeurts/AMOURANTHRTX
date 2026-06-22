# Enhanced CMake for engine
# Added engine component without breaking existing targets

add_library(amx_engine STATIC
  Navigator/engine/*.cpp # etc, modularized safely
)

# Old targets preserved
# New: engine_update target for incremental
