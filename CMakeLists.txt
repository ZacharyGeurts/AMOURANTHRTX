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

if(TARGET_PLATFORM STREQUAL "Windows")
    add_compile_options(
        -mwindows
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
    assets/audio assets/scripts
    shaders/raytracing shaders/compute
)

foreach(D ${DEVELOPER_DIRS})
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${D}")
endforeach()

# Also ensure output directories exist
foreach(platform IN ITEMS Linux Windows)
    foreach(sub IN ITEMS shaders/raytracing shaders/compute fonts textures models scenes audio scripts)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${platform}/assets/${sub}")
    endforeach()
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

if(TARGET_PLATFORM STREQUAL "Linux")
    pkg_check_modules(SDL3 REQUIRED sdl3)
    pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
endif()

if(TARGET_PLATFORM STREQUAL "Windows")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    find_library(SDL3_LIB      NAMES SDL3      PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_IMAGE_LIB NAMES SDL3_image PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
endif()

# tinyobjloader
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG release
)
FetchContent_MakeAvailable(tinyobjloader)
set(TINYOBJLOADER_SRC "${tinyobjloader_SOURCE_DIR}/tiny_obj_loader.cc")

# =============================================================================
# SOURCE FILES — Navigator/main.cpp + all .cpp in Navigator/engine/
# =============================================================================
file(GLOB_RECURSE ENGINE_SOURCES
    "Navigator/engine/*.cpp"
)

set(ALL_SOURCES
    Navigator/main.cpp
    ${ENGINE_SOURCES}
    ${TINYOBJLOADER_SRC}
)

# Prevent compiling CMake's own compiler ID files
list(FILTER ALL_SOURCES EXCLUDE REGEX "CMakeFiles/.*CompilerId.*\\.cpp$")

add_executable(amouranth_engine ${ALL_SOURCES})
set_target_properties(amouranth_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
    OUTPUT_NAME "Navigator"
    SUFFIX "${EXE_SUFFIX}"
)

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
    VK_KHR_ray_tracing_pipeline
    VK_KHR_acceleration_structure
    VK_KHR_deferred_host_operations
    VK_KHR_buffer_device_address
    VK_EXT_descriptor_indexing
    VK_KHR_ray_query
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
        dl
		m
		pthread
        ${SDL3_LIBRARIES}
        ${SDL3_IMAGE_LIBRARIES}
    )
elseif(TARGET_PLATFORM STREQUAL "Windows")
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIB}
        ${SDL3_IMAGE_LIB}
        -static
        kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid
        setupapi cfgmgr32
    )
endif()

# =============================================================================
# SHADER COMPILATION
# =============================================================================
set(SHADER_INCLUDE_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_DEBUG_FLAGS -g -O0)

file(GLOB_RECURSE POTENTIAL_SHADERS shaders/raytracing/* shaders/compute/*)

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
    set(TARGET_FLAG "")

    if(REL_DIR STREQUAL "raytracing")
        if(NAME MATCHES "[_.](rgen|rmiss|rchit|rahit|rint|rcall)(\\.glsl)?$")
            set(TARGET_FLAG --target-spv=spv1.6 --target-env=vulkan1.4)
            string(REGEX MATCH "[_.](rgen|rmiss|rchit|rahit|rint|rcall)" STAGE_MATCH ${NAME})
            set(STAGE ${CMAKE_MATCH_1})
        endif()
    elseif(REL_DIR MATCHES "compute")
        if(NAME MATCHES "([_.])?comp(\\.glsl)?$")
            set(TARGET_FLAG --target-spv=spv1.6 --target-env=vulkan1.4)
            set(STAGE comp)
        endif()
    endif()

    if(NOT STAGE)
        continue()
    endif()

    set(SPV "${OUT_DIR}/${BASE}.spv")

    add_custom_command(
        OUTPUT ${SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}
        COMMAND ${GLSLC} ${SHADER_INCLUDE_FLAGS} ${TARGET_FLAG} ${SHADER_DEBUG_FLAGS}
                -fshader-stage=${STAGE} "${SRC}" -o "${SPV}"
        COMMAND ${SPIRV_VAL} --target-env vulkan1.4 "${SPV}"
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