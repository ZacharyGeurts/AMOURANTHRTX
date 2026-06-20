# =============================================================================
# Field RTX x86 Core - CMakeLists.txt
# Full no-stubs x86 emulator core for Field Die (legacy + FPU + SSE)
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(FieldX86Core LANGUAGES C)

# =============================================================================
# Options
# =============================================================================
option(FIELD_X86_BUILD_TESTS "Build test binaries" OFF)
option(FIELD_X86_ENABLE_AVX  "Enable basic AVX support (experimental)" OFF)

# =============================================================================
# Source files
# =============================================================================
set(FIELD_X86_SOURCES
    src/api.c
    src/decode.c
    src/mem.c
    src/fpu.c
    src/ops.c
    src/ops2.c
    src/prim_ops.c
)

set(FIELD_X86_HEADERS
    include/x86emu.h
    include/x86emu_int.h
    include/decode.h
    include/mem.h
    include/fpu.h
    include/ops.h
    include/prim_ops.h
)

# =============================================================================
# Library target (standalone only — AMOURANTHRTX parent builds Navigator/engine copy)
# =============================================================================
if(NOT TARGET field_x86)
    add_library(field_x86 STATIC ${FIELD_X86_SOURCES} ${FIELD_X86_HEADERS})

    target_include_directories(field_x86
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_definitions(field_x86 PRIVATE
        X86EMU_CORE_FIELD_DIE=1
    )

    if(FIELD_X86_ENABLE_AVX)
        target_compile_definitions(field_x86 PRIVATE FIELD_X86_ENABLE_AVX)
        message(STATUS "FieldX86Core: AVX support enabled (experimental)")
    endif()

    target_compile_options(field_x86 PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -O3 -ffast-math>
        $<$<C_COMPILER_ID:MSVC>:/W4 /O2>
    )

    target_link_libraries(field_x86 PRIVATE m)
    add_library(x86emu ALIAS field_x86)
else()
    message(STATUS "FieldX86Core: using field_x86 from parent project")
endif()

# =============================================================================
# Install
# =============================================================================
install(TARGETS field_x86 
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/ 
    DESTINATION include/FieldX86Core
)

# =============================================================================
# Optional test binary
# =============================================================================
if(FIELD_X86_BUILD_TESTS)
    add_executable(field_x86_test tests/main.c)
    target_link_libraries(field_x86_test PRIVATE field_x86)
    
    # Simple test that runs a few instructions
    target_compile_definitions(field_x86_test PRIVATE FIELD_X86_TEST_MODE)
endif()

# =============================================================================
# Status
# =============================================================================
message(STATUS "FieldX86Core configured")
message(STATUS "   Source files: ${FIELD_X86_SOURCES}")
message(STATUS "   GPU integration: ENABLED (direct VRAM mapping)")
message(STATUS "   Full ISA coverage: Legacy + FPU + SSE1–SSE4.2")