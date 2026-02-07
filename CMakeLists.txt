cmake_minimum_required(VERSION 3.21)
project(AMOURANTH_RTX LANGUAGES CXX)

# =============================================================================
# ENABLE FETCHCONTENT + GIT FOR EXTERNAL DEPENDENCIES
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
# C++23 TURBO — FULL POWER, NO COMPROMISE
# =============================================================================
if(CMAKE_CROSSCOMPILING)
    find_program(GCC14_BIN x86_64-w64-mingw32-g++)
    if(NOT GCC14_BIN)
        message(FATAL_ERROR "MinGW-w64 GCC 14+ required for cross-compile")
    endif()
    message(STATUS "Using MinGW-w64 GCC 14+: ${GCC14_BIN}")
else()
    find_program(GCC14_BIN g++-14)
    if(NOT GCC14_BIN)
        message(FATAL_ERROR "GCC 14+ required for native build")
    endif()
    message(STATUS "Using native GCC 14+: ${GCC14_BIN}")
endif()
set(CMAKE_CXX_COMPILER ${GCC14_BIN})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_compile_options(
    -Wall -Wextra -Werror    
    -Wno-extra -O3 -march=native -mtune=native
    -flto=auto -ffast-math -Wno-error=unused-but-set-variable
    -Wno-error=deprecated-declarations -Wno-error=shadow
    -Wno-error=unused-variable -Wno-error=unused-function
    -g -ggdb
)

option(DISABLE_TRACE_AND_DEBUG_LOGS "Disable trace & debug logging" ON)
if(DISABLE_TRACE_AND_DEBUG_LOGS)
    add_definitions(-DDISABLE_TRACE_AND_DEBUG_LOGS)
endif()

# ======================================
# ADDRESS SANITIZER COMPLETELY DISABLED
# ======================================
# ASan conflicts with intentional pointer obfuscation
# → NEVER ENABLE IT — EVER
# Removed: -fsanitize=address,undefined,leak
# Removed: link options
# StoneKey now flows pure and undetected

# =============================================================================
# PLATFORM DETECTION
# =============================================================================
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "AMOURANTH RTX builds are ONLY supported on Linux hosts.")
endif()

if(CMAKE_CROSSCOMPILING)
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
# DIRECTORY CREATION
# =============================================================================
set(SOURCE_DIRS
    assets/fonts assets/textures assets/models assets/materials assets/scenes
    assets/audio assets/scripts
    shaders/raytracing shaders/compute
)
foreach(D ${SOURCE_DIRS})
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${D}")
endforeach()

set(OUTPUT_DIRS
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/raytracing
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/compute
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/fonts
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/textures
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/models
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/scenes
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/audio
    ${CMAKE_BINARY_DIR}/bin/Linux/assets/scripts
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders/raytracing
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders/compute
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/fonts
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/textures
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/models
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/scenes
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/audio
    ${CMAKE_BINARY_DIR}/bin/Windows/assets/scripts
)
foreach(D ${OUTPUT_DIRS})
    file(MAKE_DIRECTORY "${D}")
endforeach()

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

if(IS_LINUX)
    pkg_check_modules(SDL3 REQUIRED sdl3)
    pkg_check_modules(SDL3_TTF REQUIRED sdl3-ttf)
    pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
    pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
    pkg_check_modules(X11 REQUIRED x11 xext xrandr xcursor xi xscrnsaver xcb)
    pkg_check_modules(GBM REQUIRED gbm)
    pkg_check_modules(LIBDRM REQUIRED libdrm)
endif()

if(IS_WINDOWS)
    # Save original suffixes and force static libraries only (.a) for SDL3 family
    set(_old_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    find_library(SDL3_LIB NAMES SDL3 PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_TTF_LIB NAMES SDL3_ttf PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_IMAGE_LIB NAMES SDL3_image PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)

    # Restore original suffixes
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_old_suffixes})
    unset(_old_suffixes)
endif()

# =============================================================================
# tinyobjloader
# =============================================================================
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG release
)
FetchContent_MakeAvailable(tinyobjloader)
set(TINYOBJLOADER_SRC "${tinyobjloader_SOURCE_DIR}/tiny_obj_loader.cc")

# =============================================================================
# oneTBB (Intel oneAPI Threading Building Blocks)
# =============================================================================
FetchContent_Declare(
    onetbb
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG v2022.0.0
)
FetchContent_MakeAvailable(onetbb)

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
# INCLUDES + DEFINES + LINKING
# =============================================================================
target_include_directories(amouranth_engine PRIVATE
    include include/modes
    ${Vulkan_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${glm_INCLUDE_DIRS}
    ${tinyobjloader_SOURCE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${onetbb_SOURCE_DIR}/include
)

if(IS_LINUX)
    target_include_directories(amouranth_engine PRIVATE
        ${SDL3_INCLUDE_DIRS}
        ${SDL3_TTF_INCLUDE_DIRS}
        ${SDL3_IMAGE_INCLUDE_DIRS}
        ${HARFBUZZ_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${GBM_INCLUDE_DIRS}
        ${LIBDRM_INCLUDE_DIRS}
    )
endif()

target_compile_definitions(amouranth_engine PRIVATE
    VK_KHR_ray_tracing_pipeline
    VK_KHR_acceleration_structure
    VK_KHR_deferred_host_operations
    VK_EXT_descriptor_indexing
    VK_KHR_buffer_device_address
    VK_KHR_ray_query
    VK_KHR_display
    VK_EXT_hdr_metadata
)

# =============================================================================
# Link libraries — cross-platform safe
# =============================================================================

target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    TBB::tbb
    OpenMP::OpenMP_CXX
    Freetype::Freetype
    atomic              # Needed for some atomics on older GCC/MinGW
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)  # Linux only
    target_link_libraries(amouranth_engine PRIVATE
        dl              # libdl for dynamic loading (dlopen)
        pthread         # POSIX threads
        m               # math library (libm)
        ${SDL3_LIBRARIES}
        ${SDL3_TTF_LIBRARIES}
        ${SDL3_IMAGE_LIBRARIES}
        ${HARFBUZZ_LIBRARIES}
        ${X11_LIBRARIES}
        ${GBM_LIBRARIES}
        ${LIBDRM_LIBRARIES}
    )
elseif(MINGW OR MSVC OR WIN32)  # Windows / MinGW
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIB}
        ${SDL3_TTF_LIB}
        ${SDL3_IMAGE_LIB}

        -static             # Static-link all MinGW runtime libs (eliminates libwinpthread-1.dll and any libgcc/libstdc++ DLLs)

        -mwindows           # GUI subsystem (no console window)

        # Core Windows system libraries
        kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid

        # Additional Windows API libraries required by static SDL3
        setupapi            # SetupDi* functions
        cfgmgr32            # CM_* functions
    )
endif()

# Optional: If you need extra Windows-specific libs (e.g. for MiniDump)
if(WIN32)
    target_link_libraries(amouranth_engine PRIVATE dbghelp)
endif()

# =============================================================================
# SHADER COMPILATION — FIXED
# =============================================================================
set(SHADER_INCLUDE_FLAGS
    -I${CMAKE_CURRENT_SOURCE_DIR}/shaders
)
set(SHADER_DEBUG_FLAGS -g -O0)

file(GLOB_RECURSE POTENTIAL_SHADERS
    shaders/raytracing/*
    shaders/compute/*
)

set(SPV_OUTPUTS "")
foreach(SRC ${POTENTIAL_SHADERS})
    if(IS_DIRECTORY "${SRC}")
        continue()
    endif()

    get_filename_component(NAME ${SRC} NAME)
    get_filename_component(BASE ${SRC} NAME_WE)
    get_filename_component(DIR ${SRC} DIRECTORY)
    get_filename_component(REL_DIR ${DIR} NAME)

    set(OUT_DIR "${BIN_DIR}/assets/shaders/${REL_DIR}")
    set(STAGE "")
    set(TARGET "")

    if(REL_DIR STREQUAL "raytracing")
        if(NAME MATCHES "[_.](rgen|rmiss|rchit|rahit|rint|rcall)(\\.glsl)?$")
            set(TARGET --target-spv=spv1.6 --target-env=vulkan1.4)
            if(NAME MATCHES "[_.]rgen(\\.glsl)?$")
                set(STAGE rgen)
            elseif(NAME MATCHES "[_.]rmiss(\\.glsl)?$")
                set(STAGE rmiss)
            elseif(NAME MATCHES "[_.]rchit(\\.glsl)?$")
                set(STAGE rchit)
            elseif(NAME MATCHES "[_.]rahit(\\.glsl)?$")
                set(STAGE rahit)
            elseif(NAME MATCHES "[_.]rint(\\.glsl)?$")
                set(STAGE rint)
            elseif(NAME MATCHES "[_.]rcall(\\.glsl)?$")
                set(STAGE rcall)
            endif()
        endif()
    elseif(REL_DIR MATCHES "compute")
        if(NAME MATCHES "([_.])?comp(\\.glsl)?$")
            set(TARGET --target-spv=spv1.6 --target-env=vulkan1.4)
            set(STAGE comp)
        endif()
    else()
        if(NAME MATCHES "([_.])?vert(\\.glsl)?$")
            set(TARGET --target-spv=spv1.6 --target-env=vulkan1.4)
            set(STAGE vert)
        elseif(NAME MATCHES "([_.])?frag(\\.glsl)?$")
            set(TARGET --target-spv=spv1.6 --target-env=vulkan1.4)
            set(STAGE frag)
        endif()
    endif()

    if(NOT STAGE)
        continue()
    endif()

    set(SPV "${OUT_DIR}/${BASE}.spv")

    set(VALIDATION_ENV vulkan1.4)
    if(REL_DIR STREQUAL "raytracing")
        set(VALIDATION_ENV vulkan1.4)
    endif()

    add_custom_command(
        OUTPUT ${SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}
        COMMAND ${GLSLC} ${SHADER_INCLUDE_FLAGS} ${TARGET} ${SHADER_DEBUG_FLAGS}
                -fshader-stage=${STAGE} "${SRC}" -o "${SPV}"
        COMMAND ${SPIRV_VAL} --target-env ${VALIDATION_ENV} "${SPV}"
        DEPENDS "${SRC}"
        COMMENT "Compiling shader: ${REL_DIR}/${NAME} → ${BASE}.spv"
        VERBATIM
    )
    list(APPEND SPV_OUTPUTS ${SPV})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPV_OUTPUTS})
add_dependencies(amouranth_engine shaders)

# =============================================================================
# ASSET COPY
# =============================================================================
set(ASSET_MAP
    "fonts:*.ttf,*.otf,*.ttc,*.woff,*.woff2,*.eot"
    "textures:*.png,*.jpg,*.jpeg,*.jpe,*.jfif,*.exr,*.hdr,*.dds,*.ktx,*.ktx2,*.tga,*.bmp,*.gif,*.webp,*.jp2,*.svg,*.ico"
    "models:*.obj,*.gltf,*.glb,*.fbx,*.dae,*.stl,*.ply,*.blend,*.3ds,*.abc"
    "materials:*.json,*.mtl,*.xml,*.yaml,*.yml"
    "scenes:*.json,*.xml,*.yaml,*.yml,*.scene,*.scn"
    "audio:*.wav,*.ogg,*.mp3,*.flac,*.aac,*.m4a,*.wma,*.opus"
    "scripts:*.lua,*.py,*.js,*.ts,*.glsl,*.hlsl,*.cs,*.gd"
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


message(STATUS "")
message(STATUS "==================================================================")
message(STATUS " Target: ${BIN_DIR}/Navigator${EXE_SUFFIX}")
message(STATUS " Run: ./linux.sh --help")
message(STATUS "==================================================================")