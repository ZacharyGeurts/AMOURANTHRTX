# =============================================================================
# Field RTX x86 Core — full historical x86 (8086→Core2) in-tree libx86emu for Field Die
# ISA: RM/PM32 + x87 + MMX + SSE1 (+ museum personas via FieldX86Identity)
# Not for separate distribution. Dual licensed with AMOURANTHRTX (GPL v3 / commercial)
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(FieldX86Core LANGUAGES C)

if(TARGET field_x86)
    message(STATUS "FieldX86Core: using field_x86 from parent AMOURANTHRTX build")
    return()
endif()

option(FIELD_X86_BUILD_TESTS "Build optional FieldX86Core test harness" OFF)
option(FIELD_X86_ENABLE_AVX  "Enable experimental AVX decode paths" OFF)

set(FIELD_X86_SOURCES
    src/api.c
    src/decode.c
    src/fpu.c
    src/mem.c
    src/ops.c
    src/ops2.c
    src/prim_ops.c
)

set(FIELD_X86_HEADERS
    include/decode.h
    include/fpu.h
    include/mem.h
    include/ops.h
    include/prim_ops.h
    include/x86emu.h
    include/x86emu_int.h
)

add_library(field_x86 STATIC ${FIELD_X86_SOURCES} ${FIELD_X86_HEADERS})
add_library(x86emu ALIAS field_x86)

target_include_directories(field_x86
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_definitions(field_x86
    PUBLIC  FIELD_X86_CORE=1
    PRIVATE X86EMU_CORE_FIELD_DIE=1
)

if(FIELD_X86_ENABLE_AVX)
    target_compile_definitions(field_x86 PRIVATE FIELD_X86_ENABLE_AVX=1)
    message(STATUS "FieldX86Core: AVX decode paths enabled (experimental)")
endif()

target_compile_options(field_x86 PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-w -O2>
    $<$<C_COMPILER_ID:MSVC>:/W0 /O2>
)

set_target_properties(field_x86 PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(field_x86 PRIVATE m)

if(FIELD_X86_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/main.c")
    add_executable(field_x86_test tests/main.c)
    target_link_libraries(field_x86_test PRIVATE field_x86)
    target_compile_definitions(field_x86_test PRIVATE FIELD_X86_TEST_MODE=1)
endif()

message(STATUS "FieldX86Core: standalone static library configured (${CMAKE_CURRENT_SOURCE_DIR})")