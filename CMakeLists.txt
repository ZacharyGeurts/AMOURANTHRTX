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
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wno-unused-but-set-variable -Wno-deprecated-declarations
    -O3 -flto=auto -ffast-math -march=native -mtune=native
    -g -ggdb
)

# Prefer mold linker when available (much faster on Linux host)
find_program(MOLD_EXECUTABLE mold)
if(MOLD_EXECUTABLE)
    add_link_options(-fuse-ld=mold)
    message(STATUS "Linker: mold")
endif()

# Windows-specific hardening (but keep console subsystem)
if(TARGET_PLATFORM STREQUAL "Windows")
    add_compile_options(
        -fstack-protector-strong
        -fcf-protection=full
        -D_FORTIFY_SOURCE=3
        -Wa,-mbig-obj
        -fno-ident
    )
    add_link_options(
        -static-libgcc -static-libstdc++
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

# Use a marker file to track whether assets have been copied
add_custom_command(
    OUTPUT "${ASSETS_DEST_DIR}/.assets_copied"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS_DEST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_SRC_DIR}" "${ASSETS_DEST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${ASSETS_DEST_DIR}/.assets_copied"
    DEPENDS "${ASSETS_SRC_DIR}"  # Re-copy if source assets change
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

# tinyobjloader (optional — keep if you still use OBJ AABB extraction)
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG release
)
FetchContent_MakeAvailable(tinyobjloader)
set(TINYOBJLOADER_SRC "${tinyobjloader_SOURCE_DIR}/tiny_obj_loader.cc")

# =============================================================================
# SOURCE FILES
# =============================================================================
set(ALL_SOURCES
    Navigator/main.cpp
    ${TINYOBJLOADER_SRC}
)

# Prevent compiling CMake's own compiler ID files
list(FILTER ALL_SOURCES EXCLUDE REGEX "CMakeFiles/.*CompilerId.*\\.cpp$")

# =============================================================================
# EXECUTABLE — Console app (no WIN32!)
# =============================================================================
add_executable(amouranth_engine ${ALL_SOURCES})
set_target_properties(amouranth_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
    OUTPUT_NAME "Navigator"
    SUFFIX "${EXE_SUFFIX}"
)

# Force console subsystem on Windows/MinGW
if(TARGET_PLATFORM STREQUAL "Windows" OR MINGW)
    target_link_options(amouranth_engine PRIVATE -mconsole)
endif()

# Disable LTO warnings/noise on MinGW (optional)
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
        setupapi cfgmgr32
    )
endif()

# =============================================================================
# SINGLE SHADER COMPILATION
# =============================================================================
set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/compute/canvas.comp")
set(SHADER_OUT "${BIN_DIR}/compute/canvas.spv")

add_custom_command(
    OUTPUT "${SHADER_OUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN_DIR}/compute"
    COMMAND ${GLSLC}
            -I${CMAKE_CURRENT_SOURCE_DIR}/shaders
            -g -O0
            --target-spv=spv1.6 --target-env=vulkan1.4
            -fshader-stage=comp
            "${SHADER_SRC}" -o "${SHADER_OUT}"
    COMMAND ${SPIRV_VAL} --target-env vulkan1.4 "${SHADER_OUT}"
    DEPENDS "${SHADER_SRC}"
    COMMENT "Compiling single compute shader → compute/canvas.spv"
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
message(STATUS " Target: ${BIN_DIR}/Navigator${EXE_SUFFIX}")
if(TARGET_PLATFORM STREQUAL "Windows")
    message(STATUS " Run: wine ${BIN_DIR}/Navigator.exe")
else()
    message(STATUS " Run: ${BIN_DIR}/Navigator")
endif()
message(STATUS " Assets copied to: ${BIN_DIR}/assets/")
message(STATUS " Single shader: ${SHADER_OUT}")
message(STATUS "==================================================================")