cmake_minimum_required(VERSION 3.21)
project(AMOURANTH_RTX LANGUAGES CXX)

# =============================================================================
# FETCHCONTENT + GIT HELPERS
# =============================================================================
include(FetchContent)
find_package(Git REQUIRED)

function(git_clone_if_missing URL REPO TAG DEST)
    if(NOT EXISTS "${DEST}/.git")
        message(STATUS "Cloning ${REPO} (${TAG}) → ${DEST}")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch ${TAG} ${URL} "${DEST}"
            RESULT_VARIABLE GIT_RESULT
            ERROR_QUIET
        )
        if(NOT GIT_RESULT EQUAL "0")
            message(FATAL_ERROR "Failed to git clone ${REPO}")
        endif()
    else()
        message(STATUS "${REPO} already present – skipping clone")
    endif()
endfunction()

# =============================================================================
# C++23 + TOOLCHAIN SELECTION
# =============================================================================
if(CMAKE_CROSSCOMPILING)
    find_program(GCC14_BIN x86_64-w64-mingw32-g++)
    if(NOT GCC14_BIN)
        message(FATAL_ERROR "MinGW-w64 GCC 14+ required for cross-compile")
    endif()
    message(STATUS "Toolchain: MinGW-w64 cross-compile (Windows target)")
else()
    find_program(GCC14_BIN g++-14)
    if(NOT GCC14_BIN)
        message(FATAL_ERROR "GCC 14+ required for native build")
    endif()
    message(STATUS "Toolchain: Native GCC 14 (Linux target)")
endif()

set(CMAKE_CXX_COMPILER ${GCC14_BIN})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# =============================================================================
# PLATFORM & TARGET DETECTION
# =============================================================================
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "AMOURANTH RTX builds are ONLY supported on Linux hosts.")
endif()

if(CMAKE_CROSSCOMPILING)
    set(TARGET_PLATFORM "Windows")
    set(EXE_SUFFIX ".exe")
    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin/Windows")
else()
    set(TARGET_PLATFORM "Linux")
    set(EXE_SUFFIX "")
    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin/Linux")
endif()

message(STATUS "Target platform:     ${TARGET_PLATFORM}")
message(STATUS "Output directory:    ${BIN_DIR}")
message(STATUS "Executable suffix:   ${EXE_SUFFIX}")

# =============================================================================
# COMPILER FLAGS — Aggressive & Modern
# =============================================================================
add_compile_options(
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wno-unused-but-set-variable -Wno-deprecated-declarations
    -Wno-unused-function -O3 -ffast-math -march=native -mtune=native
    -g -ggdb
)

# Prefer mold linker when available (much faster on Linux host)
find_program(MOLD_EXECUTABLE mold)
if(MOLD_EXECUTABLE)
    add_link_options(-fuse-ld=mold)
    message(STATUS "Linker: mold")
endif()

# Windows-specific hardening + FULLY STATIC linking
if(TARGET_PLATFORM STREQUAL "Windows")
    add_compile_options(
        -fstack-protector-strong
        -fcf-protection=full
        -D_FORTIFY_SOURCE=3
        -Wa,-mbig-obj
        -fno-ident
    )
    add_link_options(
        -static
        -static-libgcc
        -static-libstdc++
        -Wl,--whole-archive -lwinpthread -lgcc -lgcc_eh -Wl,--no-whole-archive
        -Wl,--dynamicbase,--nxcompat,--high-entropy-va
        -Wl,--build-id
    )
endif()

option(DISABLE_TRACE_AND_DEBUG_LOGS "Disable trace & debug logging" ON)
if(DISABLE_TRACE_AND_DEBUG_LOGS)
    add_definitions(-DDISABLE_TRACE_AND_DEBUG_LOGS)
endif()

# =============================================================================
# DIRECTORY CREATION — Developer-friendly empty folders
# =============================================================================
set(DEVELOPER_DIRS
    assets/fonts assets/textures assets/models assets/materials assets/scenes
    assets/audio assets/scripts Navigator/compute
)

foreach(D ${DEVELOPER_DIRS})
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${D}")
endforeach()

file(MAKE_DIRECTORY "${BIN_DIR}/compute")

# =============================================================================
# COPY ASSETS TO OUTPUT DIRECTORY
# =============================================================================
set(ASSETS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_DEST_DIR "${BIN_DIR}/assets")

add_custom_command(
    OUTPUT "${ASSETS_DEST_DIR}/.assets_copied"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DEST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_SRC_DIR}" "${ASSETS_DEST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${ASSETS_DEST_DIR}/.assets_copied"
    DEPENDS "${ASSETS_SRC_DIR}"
    COMMENT "Copying assets/ → ${ASSETS_DEST_DIR}"
    VERBATIM
)

add_custom_target(copy_assets ALL
    DEPENDS "${ASSETS_DEST_DIR}/.assets_copied"
)

# =============================================================================
# DEPENDENCIES
# =============================================================================
find_package(PkgConfig REQUIRED)
find_package(Vulkan REQUIRED)
if(Vulkan_VERSION VERSION_LESS "1.3.250")
    message(FATAL_ERROR "Vulkan SDK >= 1.3.250 REQUIRED")
endif()

find_program(GLSLC glslc REQUIRED HINTS $ENV{VULKAN_SDK}/bin /usr/bin)
find_program(SPIRV_VAL spirv-val HINTS $ENV{VULKAN_SDK}/bin /usr/bin)

find_package(OpenMP REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm REQUIRED)

if(TARGET_PLATFORM STREQUAL "Linux")
    pkg_check_modules(SDL3 REQUIRED sdl3)
    pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
endif()

if(TARGET_PLATFORM STREQUAL "Windows")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    find_library(SDL3_LIB      NAMES SDL3      PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_IMAGE_LIB NAMES SDL3_image PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
endif()

# tinyobjloader — fetch and silence sign-conversion warning only for its source
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        release
)
FetchContent_MakeAvailable(tinyobjloader)

set(TINYOBJLOADER_SRC "${tinyobjloader_SOURCE_DIR}/tiny_obj_loader.cc")

# Silence tinyobjloader sign-conversion warning (harmless)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${TINYOBJLOADER_SRC}
        PROPERTIES COMPILE_FLAGS "-Wno-sign-conversion"
    )
endif()

# =============================================================================
# SOURCE FILES
# =============================================================================
set(ALL_SOURCES
    Navigator/main.cpp
    ${TINYOBJLOADER_SRC}
)

# =============================================================================
# EXECUTABLE — Console app
# =============================================================================
add_executable(amouranth_engine ${ALL_SOURCES})
set_target_properties(amouranth_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
    OUTPUT_NAME "Navigator"
    SUFFIX "${EXE_SUFFIX}"
)

if(TARGET_PLATFORM STREQUAL "Windows" OR MINGW)
    target_link_options(amouranth_engine PRIVATE -mconsole)
endif()

if(MINGW)
    set_target_properties(amouranth_engine PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

# =============================================================================
# INCLUDES & DEFINES
# =============================================================================
target_include_directories(amouranth_engine PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${glm_INCLUDE_DIRS}
    ${tinyobjloader_SOURCE_DIR}
    Navigator/engine
)

if(TARGET_PLATFORM STREQUAL "Linux")
    target_include_directories(amouranth_engine PRIVATE
        ${SDL3_INCLUDE_DIRS}
        ${SDL3_IMAGE_INCLUDE_DIRS}
    )
endif()

target_compile_definitions(amouranth_engine PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# =============================================================================
# LINKING
# =============================================================================
target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    OpenMP::OpenMP_CXX
    Freetype::Freetype
)

if(TARGET_PLATFORM STREQUAL "Linux")
    target_link_libraries(amouranth_engine PRIVATE
        dl m pthread
        ${SDL3_LIBRARIES}
        ${SDL3_IMAGE_LIBRARIES}
    )
elseif(TARGET_PLATFORM STREQUAL "Windows")
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIB}
        ${SDL3_IMAGE_LIB}
        kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid
        setupapi cfgmgr32 dbghelp
    )
endif()

# =============================================================================
# SHADER COMPILATION — Respect build type (Debug → -g -O0, Release → -O)
# =============================================================================
set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/compute/CANVAS.comp")
set(SHADER_OUT "${BIN_DIR}/compute/CANVAS.spv")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(GLSLC_FLAGS -g -O0)
else()
    set(GLSLC_FLAGS -O)
endif()

add_custom_command(
    OUTPUT "${SHADER_OUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN_DIR}/compute"
    COMMAND ${GLSLC}
            -I${CMAKE_CURRENT_SOURCE_DIR}/shaders
            ${GLSLC_FLAGS}
            --target-spv=spv1.6 --target-env=vulkan1.4
            -fshader-stage=comp
            "${SHADER_SRC}" -o "${SHADER_OUT}"
    COMMAND ${SPIRV_VAL} --target-env vulkan1.4 "${SHADER_OUT}" || true  # don't fail build on validation warning
    DEPENDS "${SHADER_SRC}"
    COMMENT "Compiling compute shader → compute/CANVAS.spv (${CMAKE_BUILD_TYPE})"
    VERBATIM
)

add_custom_target(shaders ALL DEPENDS "${SHADER_OUT}")

# =============================================================================
# MAKE EXECUTABLE DEPEND ON ASSETS + SHADERS
# =============================================================================
add_dependencies(amouranth_engine copy_assets shaders)

# =============================================================================
# FINAL STATUS
# =============================================================================
message(STATUS "")
message(STATUS "==================================================================")
message(STATUS " Target:          ${BIN_DIR}/Navigator${EXE_SUFFIX}")
if(TARGET_PLATFORM STREQUAL "Windows")
    message(STATUS " Run (Linux host): wine ${BIN_DIR}/Navigator.exe")
else()
    message(STATUS " Run:              ${BIN_DIR}/Navigator")
endif()
message(STATUS " Assets:           ${BIN_DIR}/assets/")
message(STATUS " Shader:           ${SHADER_OUT}")
message(STATUS " Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "==================================================================")