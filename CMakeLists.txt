cmake_minimum_required(VERSION 3.21)
project(AMOURANTH_RTX LANGUAGES CXX)

# =============================================================================
# C++23 TURBO — FULL POWER, NO COMPROMISE
# =============================================================================
# Force GCC 14+ for full <print> and C++23 support (native + cross)
if(CMAKE_CROSSCOMPILING)
    find_program(GCC14_BIN x86_64-w64-mingw32-g++-14)
    if(NOT GCC14_BIN)
        message(FATAL_ERROR "
    *** MinGW-w64 GCC 14+ REQUIRED FOR CROSS-COMPILE C++23 <print> HEADER ***
    
    Install via MSYS2 (recommended for Windows cross):
        pacman -S mingw-w64-x86_64-gcc  # Gets latest GCC 14+ in MSYS2
    
    Or WinLibs: Download GCC 14+ MinGW-w64 from https://winlibs.com/
    
    Then re-run CMake. Fallback to C++20 if unavailable (revert logging.hpp).
    ")
    endif()
    message(STATUS "Using MinGW-w64 GCC 14+: ${GCC14_BIN}")
else()
    find_program(GCC14_BIN g++-14)
    if(NOT GCC14_BIN)
        message(FATAL_ERROR "
    *** GCC 14+ REQUIRED FOR NATIVE C++23 <print> HEADER ***
    
    Install on Ubuntu/Debian/Mint/Pop!_OS:
        sudo apt update && sudo apt install gcc-14 g++-14
    
    On Fedora/RHEL/CentOS:
        sudo dnf install gcc-toolset-14
    
    On Arch/Manjaro:
        sudo pacman -Syu gcc
    
    Then re-run CMake. Fallback to C++20 if unavailable (revert logging.hpp).
    ")
    endif()
    message(STATUS "Using native GCC 14+: ${GCC14_BIN}")
endif()
set(CMAKE_CXX_COMPILER ${GCC14_BIN})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Global optimizations + warnings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wno-missing-field-initializers
    -Wno-extra -Wno-reorder -Wno-delete-incomplete
    -O3 -march=native -mtune=native
    -flto=auto -ffast-math
    -g -ggdb
)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options(-fsanitize=address,undefined,leak)
    add_link_options(-fsanitize=address,undefined,leak)
endif()

# =============================================================================
# PLATFORM DETECTION — LINUX NATIVE + MINGW64 CROSS ONLY
# =============================================================================
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "AMOURANTH RTX builds are ONLY supported on Linux hosts (native + cross).")
endif()

if(CMAKE_CROSSCOMPILING)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        message(FATAL_ERROR "Cross-compilation REQUIRES x86_64-w64-mingw32-g++ (MinGW64).")
    endif()
    set(IS_WINDOWS TRUE)
    set(IS_LINUX FALSE)
    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin/Windows")
    set(EXE_SUFFIX ".exe")
else()
    set(IS_LINUX TRUE)
    set(IS_WINDOWS FALSE)
    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin/Linux")
    set(EXE_SUFFIX "")
endif()

# =============================================================================
# DIRECTORY CREATION — SOURCE + OUTPUT (Linux & Windows)
# =============================================================================
set(SOURCE_DIRS
    assets/fonts assets/textures assets/models assets/materials assets/scenes
    assets/audio assets/scripts
    shaders/raytracing shaders/rasterization shaders/compute shaders/graphics
    src/engine/SDL3 src/engine/Vulkan include/engine
    src/modes include/modes
)
foreach(D ${SOURCE_DIRS})
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${D}")
endforeach()

set(OUTPUT_DIRS
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/raytracing
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/compute
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/graphics
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/fonts
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/textures
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/models
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/materials
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/scenes
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/audio
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/scripts
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders/raytracing
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders/compute
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders/graphics
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/fonts
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/textures
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/models
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/materials
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/scenes
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/audio
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/scripts
)
foreach(D ${OUTPUT_DIRS})
    file(MAKE_DIRECTORY "${D}")
endforeach()

# =============================================================================
# DEPENDENCY DETECTION — ROBUST + FATAL ON MISSING
# =============================================================================
find_package(PkgConfig REQUIRED)
find_package(Vulkan REQUIRED)
if(Vulkan_VERSION VERSION_LESS "1.3.250")
    message(FATAL_ERROR "Vulkan SDK >= 1.3.250 REQUIRED for full RT + mesh shading.")
endif()

find_program(GLSLC glslc REQUIRED HINTS $ENV{VULKAN_SDK}/bin /usr/bin)
find_program(SPIRV_VAL spirv-val HINTS $ENV{VULKAN_SDK}/bin /usr/bin)

find_package(TBB REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm REQUIRED)

# Linux native deps
if(IS_LINUX)
    pkg_check_modules(SDL3 REQUIRED sdl3)
    pkg_check_modules(SDL3_TTF REQUIRED sdl3-ttf)
    pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
    pkg_check_modules(SDL3_MIXER REQUIRED sdl3-mixer)
    pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
    pkg_check_modules(X11 REQUIRED x11 xext xrandr xcursor xi xscrnsaver xcb)
endif()

# Windows cross deps
if(IS_WINDOWS)
    find_library(SDL3_LIB NAMES SDL3 PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_TTF_LIB NAMES SDL3_ttf PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_IMAGE_LIB NAMES SDL3_image PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_MIXER_LIB NAMES SDL3_mixer PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
endif()

# =============================================================================
# LIBATOMIC — FIXED
# =============================================================================
find_library(ATOMIC_LIB
    NAMES atomic libatomic atomic.so.1 libatomic.so.1
    HINTS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib64
        /lib/x86_64-linux-gnu
    PATH_SUFFIXES lib lib64
)

if(NOT ATOMIC_LIB)
    message(FATAL_ERROR "
        
        *** LIBATOMIC NOT FOUND ***
        
        On Ubuntu/Debian/Mint/Pop!_OS run:
            sudo apt update && sudo apt install libatomic1 libatomic-dev gcc-multilib g++-multilib
        
        On Fedora/RHEL/CentOS:
            sudo dnf install libatomic
        
        On Arch/Manjaro:
            sudo pacman -S libatomic_ops
        
        Then re-run CMake.
    ")
endif()
message(STATUS "Found libatomic: ${ATOMIC_LIB}")

# =============================================================================
# TINYOBJLOADER — BACK TO ORIGINAL WORKING METHOD (release tag)
# =============================================================================
set(TINYOBJLOADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/tinyobjloader")
set(TINYOBJLOADER_REPO "https://github.com/tinyobjloader/tinyobjloader.git")

include(FetchContent)
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY ${TINYOBJLOADER_REPO}
    GIT_TAG release          # ← WORKING TAG FROM ORIGINAL CMAKE
    SOURCE_DIR ${TINYOBJLOADER_DIR}
)

if(NOT tinyobjloader_POPULATED)
    FetchContent_Populate(tinyobjloader)
    message(STATUS "tinyobjloader populated (cloned/updated)")
else()
    message(STATUS "tinyobjloader already populated – skipping clone")
endif()

FetchContent_GetProperties(tinyobjloader)
set(TINYOBJLOADER_SRC "${tinyobjloader_SOURCE_DIR}/tiny_obj_loader.cc")
if(NOT EXISTS "${TINYOBJLOADER_SRC}")
    message(FATAL_ERROR "tinyobjloader failed to download tiny_obj_loader.cc. Check internet or Git.")
endif()
message(STATUS "tinyobjloader ready: ${TINYOBJLOADER_SRC}")

# =============================================================================
# SOURCE COLLECTION
# =============================================================================
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/engine/*.cpp
    src/engine/SDL3/*.cpp
    src/engine/Vulkan/*.cpp
    src/modes/*.cpp
)
list(APPEND SOURCES "${TINYOBJLOADER_SRC}")
list(REMOVE_DUPLICATES SOURCES)

add_executable(amouranth_engine ${SOURCES})
set_target_properties(amouranth_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
    OUTPUT_NAME "Navigator"
    SUFFIX "${EXE_SUFFIX}"
)

# =============================================================================
# RAY TRACING EXTENSIONS
# =============================================================================
target_compile_definitions(amouranth_engine PRIVATE
    VK_KHR_ray_tracing_pipeline
    VK_KHR_acceleration_structure
    VK_KHR_deferred_host_operations
    VK_EXT_descriptor_indexing
    VK_KHR_buffer_device_address
    VK_KHR_ray_query
)

# =============================================================================
# INCLUDES
# =============================================================================
target_include_directories(amouranth_engine PRIVATE
    include include/modes
    ${Vulkan_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${glm_INCLUDE_DIRS}
    ${tinyobjloader_SOURCE_DIR}
)
if(IS_LINUX)
    target_include_directories(amouranth_engine PRIVATE
        ${SDL3_INCLUDE_DIRS}
        ${SDL3_TTF_INCLUDE_DIRS}
        ${SDL3_IMAGE_INCLUDE_DIRS}
        ${SDL3_MIXER_INCLUDE_DIRS}
        ${HARFBUZZ_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
    )
endif()

# =============================================================================
# LINKING
# =============================================================================
target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    TBB::tbb
    OpenMP::OpenMP_CXX
    Freetype::Freetype
    ${ATOMIC_LIB}
)
if(IS_LINUX)
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIBRARIES}
        ${SDL3_TTF_LIBRARIES}
        ${SDL3_IMAGE_LIBRARIES}
        ${SDL3_MIXER_LIBRARIES}
        ${HARFBUZZ_LIBRARIES}
        ${X11_LIBRARIES}
    )
else()
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIB} ${SDL3_TTF_LIB} ${SDL3_IMAGE_LIB} ${SDL3_MIXER_LIB}
        -static-libgcc -static-libstdc++ -mwindows
    )
endif()

# =============================================================================
# SHADER COMPILATION — FIXED INCLUDE PATH + STAGE + VALIDATION FAIL HARD
# =============================================================================
set(SHADER_INCLUDE_FLAGS
    -I${CMAKE_CURRENT_SOURCE_DIR}/include/engine/Vulkan
    -I${CMAKE_CURRENT_SOURCE_DIR}/include/engine
    -I${CMAKE_CURRENT_SOURCE_DIR}/engine/Vulkan   # ← ORIGINAL WORKING PATH
    -I${CMAKE_CURRENT_SOURCE_DIR}/shaders
)

set(SHADER_DEBUG_FLAGS -g -O0)

file(GLOB_RECURSE SHADER_SOURCES
    shaders/raytracing/*.rgen shaders/raytracing/*.rmiss shaders/raytracing/*.rchit
    shaders/raytracing/*.rahit shaders/raytracing/*.rint shaders/raytracing/*.rcall
    shaders/compute/*.comp
    shaders/graphics/*.vert shaders/graphics/*.frag shaders/rasterization/*.vert shaders/rasterization/*.frag
)

set(SPV_OUTPUTS "")
foreach(SRC ${SHADER_SOURCES})
    get_filename_component(NAME ${SRC} NAME)
    get_filename_component(BASE ${SRC} NAME_WE)
    get_filename_component(DIR ${SRC} DIRECTORY)
    get_filename_component(REL_DIR ${DIR} NAME)

    set(OUT_DIR "${BIN_DIR}/assets/shaders/${REL_DIR}")
    set(SPV "${OUT_DIR}/${BASE}.spv")

    if(REL_DIR STREQUAL "raytracing")
        set(TARGET --target-spv=spv1.5 --target-env=vulkan1.3)
        set(STAGE rgen)  # default fallback
        if(NAME MATCHES "\\.rgen$")
            set(STAGE rgen)
        elseif(NAME MATCHES "\\.rmiss$")
            set(STAGE rmiss)
        elseif(NAME MATCHES "\\.rchit$")
            set(STAGE rchit)
        elseif(NAME MATCHES "\\.rahit$")
            set(STAGE rahit)
        elseif(NAME MATCHES "\\.rint$")
            set(STAGE rint)
        elseif(NAME MATCHES "\\.rcall$")
            set(STAGE rcall)
        endif()
    elseif(REL_DIR MATCHES "compute")
        set(TARGET --target-spv=spv1.4 --target-env=vulkan1.2)
        set(STAGE comp)
    elseif(NAME MATCHES "\\.vert$")
        set(TARGET --target-spv=spv1.4 --target-env=vulkan1.2)
        set(STAGE vert)
    elseif(NAME MATCHES "\\.frag$")
        set(TARGET --target-spv=spv1.4 --target-env=vulkan1.2)
        set(STAGE frag)
    else()
        continue()
    endif()

    add_custom_command(
        OUTPUT ${SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}
        COMMAND ${GLSLC} ${SHADER_INCLUDE_FLAGS} ${TARGET} ${SHADER_DEBUG_FLAGS}
               -fshader-stage=${STAGE} "${SRC}" -o "${SPV}"
        COMMAND ${SPIRV_VAL} --target-env vulkan1.3 "${SPV}"
        DEPENDS "${SRC}"
        COMMENT "glslc → ${REL_DIR}/${NAME} (stage=${STAGE})"
        VERBATIM
    )
    list(APPEND SPV_OUTPUTS ${SPV})
endforeach()

# Copy .glsl headers
file(GLOB_RECURSE GLSL_HEADERS shaders/**/*.glsl)
foreach(H ${GLSL_HEADERS})
    get_filename_component(NAME ${H} NAME)
    get_filename_component(DIR ${H} DIRECTORY)
    get_filename_component(REL_DIR ${DIR} NAME)
    set(OUT "${BIN_DIR}/assets/shaders/${REL_DIR}/${NAME}")
    add_custom_command(
        OUTPUT ${OUT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${H} ${OUT}
        DEPENDS ${H}
        COMMENT "Copy .glsl: ${REL_DIR}/${NAME}"
    )
    list(APPEND SPV_OUTPUTS ${OUT})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPV_OUTPUTS})
add_dependencies(amouranth_engine shaders)

# =============================================================================
# ASSET COPY
# =============================================================================
set(ASSET_MAP
    "fonts:*.ttf,*.otf"
    "textures:*.png,*.jpg,*.ktx2,*.dds,*.hdr"
    "models:*.obj,*.gltf,*.glb"
    "materials:*.json"
    "scenes:*.json"
    "audio:*.wav,*.ogg"
    "scripts:*.lua"
)
foreach(MAP ${ASSET_MAP})
    string(REPLACE ":" ";" PAIR ${MAP})
    list(GET PAIR 0 TYPE)
    list(GET PAIR 1 EXTS)
    string(REPLACE "," ";" EXT_LIST ${EXTS})
    set(FILES "")
    foreach(E ${EXT_LIST})
        file(GLOB TEMP "assets/${TYPE}/${E}")
        list(APPEND FILES ${TEMP})
    endforeach()
    if(FILES)
        add_custom_command(TARGET amouranth_engine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FILES} "${BIN_DIR}/assets/${TYPE}/"
            COMMENT "Copying ${TYPE} assets"
        )
    endif()
endforeach()

# =============================================================================
# FINAL MESSAGE — NOVEMBER 07 2025 — FULLY FIXED SHADERS + TINYOBJ
# =============================================================================
message(STATUS "")
message(STATUS "==================================================================")
message(STATUS "")
message(STATUS "    █████╗ ███╗   ███╗ ██████╗ ██╗   ██╗██████╗  █████╗ ███╗   ██╗████████╗██╗  ██╗")
message(STATUS "   ██╔══██╗████╗ ████║██╔═══██╗██║   ██║██╔══██╗██╔══██╗████╗  ██║╚══██╔══╝██║  ██║")
message(STATUS "   ███████║██╔████╔██║██║   ██║██║   ██║██████╔╝███████║██╔██╗ ██║   ██║   ███████║")
message(STATUS "   ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║")
message(STATUS "   ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║")
message(STATUS "   ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝")
message(STATUS "")
message(STATUS "    ██████╗ ████████╗██╗  ██╗    ███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗")
message(STATUS "    ██╔══██╗╚══██╔══╝╚██╗██╔╝    ██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝")
message(STATUS "    ██████╔╝   ██║    ╚███╔╝     █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  ")
message(STATUS "    ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ")
message(STATUS "    ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗")
message(STATUS "    ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝")
message(STATUS "")
message(STATUS "==================================================================")
message(STATUS " AMOURANTH RTX — BUILD FIXED — NOVEMBER 07 2025")
message(STATUS " ✓ tinyobjloader: release tag + tiny_obj_loader.cc")
message(STATUS " ✓ denoiser_post.comp: -Iengine/Vulkan + -fshader-stage=comp")
message(STATUS " ✓ All shaders: explicit stage + validation HARD FAIL")
message(STATUS " Target: ${BIN_DIR}/Navigator${EXE_SUFFIX}")
message(STATUS " Run: rm -rf build && cmake -B build -G Ninja && ninja -j$(nproc)")
message(STATUS "==================================================================")
message(STATUS "")