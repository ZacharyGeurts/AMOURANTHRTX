cmake_minimum_required(VERSION 3.12)
project(AMOURANTH_RTX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global options
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers -Wno-extra -O3 -g)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

# Platform check (Linux host only)
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Native builds and cross-compilation are only supported on Linux host systems.")
endif()

# Cross-compilation setup for Windows
if(CMAKE_CROSSCOMPILING)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "Cross-compilation requires MinGW64 toolchain (GNU compiler).")
    endif()
    set(IS_WINDOWS TRUE)
    set(IS_LINUX FALSE)
else()
    set(IS_LINUX TRUE)
    set(IS_WINDOWS FALSE)
endif()

# Create source directories if they don't exist
set(SOURCE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/models"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/materials"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/scenes"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/raytracing"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/rasterization"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compute"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/graphics"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/SDL3"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Vulkan"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/engine"
)
foreach(DIR ${SOURCE_DIRS})
    if(NOT EXISTS "${DIR}")
        file(MAKE_DIRECTORY "${DIR}")
        message(STATUS "Created source directory: ${DIR}")
    endif()
endforeach()

# Create output directories
set(OUTPUT_DIRS
    "${CMAKE_BINARY_DIR}/bin/Linux"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/rasterization"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/raytracing"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/compute"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/shaders/graphics"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/fonts"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/textures"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/models"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/materials"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/scenes"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/audio"
    "${CMAKE_BINARY_DIR}/bin/Linux/assets/scripts"
    "${CMAKE_BINARY_DIR}/bin/Windows"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders/raytracing"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders/compute"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/shaders/graphics"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/fonts"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/textures"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/models"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/materials"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/scenes"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/audio"
    "${CMAKE_BINARY_DIR}/bin/Windows/assets/scripts"
)
foreach(DIR ${OUTPUT_DIRS})
    file(MAKE_DIRECTORY "${DIR}")
    message(STATUS "Created output directory: ${DIR}")
endforeach()

# Robust dependency detection
find_package(PkgConfig REQUIRED)

# Vulkan
find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found. Install libvulkan-dev and vulkan-tools.")
endif()
if(Vulkan_VERSION VERSION_LESS "1.2.170")
    message(FATAL_ERROR "Vulkan SDK version ${Vulkan_VERSION} is too old. Ray tracing requires Vulkan SDK 1.2.170 or later.")
endif()
message(STATUS "Vulkan include dirs: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan libraries: ${Vulkan_LIBRARIES}")

# glslc
find_program(GLSLC glslc REQUIRED HINTS /usr/bin $ENV{VULKAN_SDK}/bin /usr/local/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK (vulkan-tools).")
endif()
message(STATUS "Found glslc: ${GLSLC}")
execute_process(COMMAND ${GLSLC} --version OUTPUT_VARIABLE GLSLC_VERSION_OUTPUT RESULT_VARIABLE GLSLC_VERSION_RESULT)
if(NOT GLSLC_VERSION_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to check glslc version.")
endif()
message(STATUS "glslc version output: ${GLSLC_VERSION_OUTPUT}")

# spirv-val
find_program(SPIRV_VAL spirv-val HINTS /usr/bin $ENV{VULKAN_SDK}/bin /usr/local/bin)
if(NOT SPIRV_VAL)
    message(WARNING "spirv-val not found. SPIR-V validation will be skipped.")
else()
    message(STATUS "Found spirv-val: ${SPIRV_VAL}")
endif()

# TBB
find_library(TBB_LIBRARY NAMES tbb REQUIRED HINTS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)
find_path(TBB_INCLUDE_DIR tbb/tbb.h REQUIRED HINTS /usr/include /usr/local/include)
if(NOT TBB_LIBRARY OR NOT TBB_INCLUDE_DIR)
    message(FATAL_ERROR "TBB not found. Install libtbb-dev with: sudo apt install libtbb-dev")
endif()
message(STATUS "TBB include dir: ${TBB_INCLUDE_DIR}")
message(STATUS "TBB library: ${TBB_LIBRARY}")
if(NOT TARGET TBB::tbb)
    add_library(TBB::tbb UNKNOWN IMPORTED)
    set_target_properties(TBB::tbb PROPERTIES
        IMPORTED_LOCATION "${TBB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIR}"
    )
endif()

# OpenMP
find_package(OpenMP REQUIRED)
if(NOT OpenMP_CXX_FOUND)
    message(FATAL_ERROR "OpenMP not found. Install libomp-dev.")
endif()
message(STATUS "OpenMP found: ${OpenMP_CXX_LIBRARIES}")

# Freetype
find_package(Freetype REQUIRED)
if(NOT Freetype_FOUND)
    message(FATAL_ERROR "Freetype not found. Install libfreetype-dev.")
endif()
message(STATUS "Freetype include dirs: ${FREETYPE_INCLUDE_DIRS}")
message(STATUS "Freetype libraries: ${FREETYPE_LIBRARIES}")

# glm
find_package(glm REQUIRED)
if(NOT glm_FOUND)
    message(FATAL_ERROR "glm not found. Install libglm-dev.")
endif()
message(STATUS "glm include dirs: ${glm_INCLUDE_DIRS}")

# HarfBuzz (Linux only)
if(IS_LINUX)
    pkg_check_modules(HarfBuzz REQUIRED harfbuzz)
    if(NOT HarfBuzz_FOUND)
        message(FATAL_ERROR "HarfBuzz not found. Install libharfbuzz-dev.")
    endif()
    message(STATUS "HarfBuzz include dirs: ${HarfBuzz_INCLUDE_DIRS}")
    message(STATUS "HarfBuzz libraries: ${HarfBuzz_LIBRARIES}")

    pkg_check_modules(SDL3 sdl3)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR "SDL3 not found. Install libsdl3-dev.")
    endif()
    message(STATUS "SDL3 include dirs: ${SDL3_INCLUDE_DIRS}")
    message(STATUS "SDL3 libraries: ${SDL3_LIBRARIES}")

    # === CRITICAL FIX: Force SDL3 include path BEFORE system paths ===
    list(GET SDL3_INCLUDE_DIRS 0 SDL3_MAIN_INCLUDE_DIR)
    include_directories(BEFORE SYSTEM
        /usr/local/include/SDL3
        ${SDL3_MAIN_INCLUDE_DIR}
    )
    # ===============================================================

    pkg_check_modules(SDL3_ttf sdl3-ttf)
    if(NOT SDL3_ttf_FOUND)
        message(FATAL_ERROR "SDL3_ttf not found. Install sdl3-ttf.")
    endif()
    message(STATUS "SDL3_ttf include dirs: ${SDL3_ttf_INCLUDE_DIRS}")
    message(STATUS "SDL3_ttf libraries: ${SDL3_ttf_LIBRARIES}")

    pkg_check_modules(SDL3_image sdl3-image)
    if(NOT SDL3_image_FOUND)
        message(FATAL_ERROR "SDL3_image not found. Install sdl3-image.")
    endif()
    message(STATUS "SDL3_image include dirs: ${SDL3_image_INCLUDE_DIRS}")
    message(STATUS "SDL3_image libraries: ${SDL3_image_LIBRARIES}")

    pkg_check_modules(SDL3_mixer sdl3-mixer)
    if(NOT SDL3_mixer_FOUND)
        message(FATAL_ERROR "SDL3_mixer not found. Install sdl3-mixer.")
    endif()
    message(STATUS "SDL3_mixer include dirs: ${SDL3_mixer_INCLUDE_DIRS}")
    message(STATUS "SDL3_mixer libraries: ${SDL3_mixer_LIBRARIES}")

    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(XEXT REQUIRED xext)
    pkg_check_modules(XRANDR REQUIRED xrandr)
    pkg_check_modules(XCURSOR REQUIRED xcursor)
    pkg_check_modules(XI REQUIRED xi)
    pkg_check_modules(XSS REQUIRED xscrnsaver)
    pkg_check_modules(XCB REQUIRED xcb)
endif()

# Windows cross-compilation
if(IS_WINDOWS)
    find_library(SDL3_LIBRARY NAMES SDL3 REQUIRED HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(SDL3_TTF_LIBRARY NAMES SDL3_ttf REQUIRED HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(SDL3_IMAGE_LIBRARY NAMES SDL3_image REQUIRED HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(SDL3_MIXER_LIBRARY NAMES SDL3_mixer REQUIRED HINTS /usr/x86_64-w64-mingw32/lib)
    find_library(WIN_OPENMP_LIBRARY NAMES gomp libgomp REQUIRED HINTS /usr/x86_64-w64-mingw32/lib)
    foreach(LIB SDL3_LIBRARY SDL3_TTF_LIBRARY SDL3_IMAGE_LIBRARY SDL3_MIXER_LIBRARY WIN_OPENMP_LIBRARY)
        if(NOT ${LIB})
            message(FATAL_ERROR "Missing MinGW64 dependency: ${LIB}.")
        else()
            message(STATUS "Found MinGW64 dependency: ${${LIB}}")
        endif()
    endforeach()
endif()

# libatomic
find_library(ATOMIC_LIBRARY NAMES atomic libatomic atomic.so.1 libatomic.so.1 HINTS /usr/lib /usr/lib/x86_64-linux-gnu)
if(NOT ATOMIC_LIBRARY)
    set(ATOMIC_LIBRARY /usr/lib/x86_64-linux-gnu/libatomic.so.1)
    if(NOT EXISTS "${ATOMIC_LIBRARY}")
        message(FATAL_ERROR "libatomic not found. Install libatomic1 and libatomic-dev.")
    endif()
endif()
message(STATUS "libatomic library: ${ATOMIC_LIBRARY}")

# Directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ENGINE_SRC_DIR ${SRC_DIR}/engine)
set(SDL3_SRC_DIR ${ENGINE_SRC_DIR}/SDL3)
set(VULKAN_SRC_DIR ${ENGINE_SRC_DIR}/Vulkan)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(ASSET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(LINUX_BIN_DIR ${CMAKE_BINARY_DIR}/bin/Linux)
set(WINDOWS_BIN_DIR ${CMAKE_BINARY_DIR}/bin/Windows)

# Source files
set(SOURCES "")
file(GLOB TOP_SOURCES "${SRC_DIR}/*.cpp")
list(APPEND SOURCES ${TOP_SOURCES})
file(GLOB SDL3_SOURCES "${SDL3_SRC_DIR}/*.cpp")
list(APPEND SOURCES ${SDL3_SOURCES})
file(GLOB VULKAN_SOURCES "${VULKAN_SRC_DIR}/*.cpp")
list(APPEND SOURCES ${VULKAN_SOURCES})
file(GLOB ENGINE_SOURCES "${ENGINE_SRC_DIR}/*.cpp")
list(APPEND SOURCES ${ENGINE_SOURCES})
list(REMOVE_DUPLICATES SOURCES)
if(NOT SOURCES)
    message(FATAL_ERROR "No source files found in ${SRC_DIR}.")
endif()
message(STATUS "Found source files: ${SOURCES}")

# Executable
add_executable(amouranth_engine ${SOURCES})
if(IS_LINUX)
    set_target_properties(amouranth_engine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${LINUX_BIN_DIR}"
        OUTPUT_NAME "Navigator"
    )
else()
    set_target_properties(amouranth_engine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${WINDOWS_BIN_DIR}"
        OUTPUT_NAME "Navigator"
        SUFFIX ".exe"
    )
endif()

# Ray tracing extensions
target_compile_definitions(amouranth_engine PRIVATE
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
)

# Includes
target_include_directories(amouranth_engine PRIVATE
    ${INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${HarfBuzz_INCLUDE_DIRS}
    ${glm_INCLUDE_DIRS}
)

if(IS_LINUX)
    target_include_directories(amouranth_engine PRIVATE
        ${SDL3_INCLUDE_DIRS}
        ${SDL3_ttf_INCLUDE_DIRS}
        ${SDL3_image_INCLUDE_DIRS}
        ${SDL3_mixer_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
        ${XEXT_INCLUDE_DIRS}
        ${XRANDR_INCLUDE_DIRS}
        ${XCURSOR_INCLUDE_DIRS}
        ${XI_INCLUDE_DIRS}
        ${XSS_INCLUDE_DIRS}
        ${XCB_INCLUDE_DIRS}
    )
endif()

# Link libraries
target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    TBB::tbb
    OpenMP::OpenMP_CXX
    Freetype::Freetype
    ${HarfBuzz_LIBRARIES}
    ${ATOMIC_LIBRARY}
)

if(IS_LINUX)
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIBRARIES}
        ${SDL3_ttf_LIBRARIES}
        ${SDL3_image_LIBRARIES}
        ${SDL3_mixer_LIBRARIES}
        ${X11_LIBRARIES}
        ${XEXT_LIBRARIES}
        ${XRANDR_LIBRARIES}
        ${XCURSOR_LIBRARIES}
        ${XI_LIBRARIES}
        ${XSS_LIBRARIES}
        ${XCB_LIBRARIES}
    )
else()
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIBRARY}
        ${SDL3_TTF_LIBRARY}
        ${SDL3_IMAGE_LIBRARY}
        ${SDL3_MIXER_LIBRARY}
        ${WIN_OPENMP_LIBRARY}
        -static-libgcc
        -static-libstdc++
        -mwindows
    )
endif()

target_compile_options(amouranth_engine PRIVATE -fPIC)

# Shader compilation
set(RAYTRACING_SHADERS
    "raygen.rgen:rgen"
    "miss.rmiss:rmiss"
    "shadowmiss.rmiss:rmiss"
    "closesthit.rchit:rchit"
    "anyhit.rahit:rahit"
    "intersection.rint:rint"
    "callable.rcall:rcall"
    "shadow_anyhit.rahit:rahit"
    "mid_anyhit.rahit:rahit"
    "volumetric_anyhit.rahit:rahit"
)
set(REQUIRED_RAYTRACING_SHADERS "raygen.rgen" "miss.rmiss" "closesthit.rchit")
set(SHADER_EXTS "*.vert" "*.frag" "*.comp")
set(SHADER_OUTPUTS "")

foreach(SHADER ${REQUIRED_RAYTRACING_SHADERS})
    if(NOT EXISTS "${SHADER_DIR}/raytracing/${SHADER}")
        message(FATAL_ERROR "Required ray-tracing shader ${SHADER} is missing.")
    endif()
endforeach()

# Ray-tracing shaders
foreach(SHADER_INFO ${RAYTRACING_SHADERS})
    string(REPLACE ":" ";" SHADER_PAIR ${SHADER_INFO})
    list(GET SHADER_PAIR 0 SHADER_SRC)
    list(GET SHADER_PAIR 1 SHADER_STAGE)
    get_filename_component(SHADER_BASE ${SHADER_SRC} NAME_WE)
    set(SHADER "${SHADER_DIR}/raytracing/${SHADER_SRC}")
    if(NOT EXISTS "${SHADER}")
        message(WARNING "Ray-tracing shader ${SHADER} not found. Skipping.")
        continue()
    endif()
    file(READ "${SHADER}" SHADER_CONTENT)
    if("${SHADER_CONTENT}" STREQUAL "")
        message(WARNING "Ray-tracing shader ${SHADER} is empty. Skipping.")
        continue()
    endif()
    if(IS_LINUX)
        set(SHADER_OUTPUT "${LINUX_BIN_DIR}/assets/shaders/raytracing/${SHADER_BASE}.spv")
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${LINUX_BIN_DIR}/assets/shaders/raytracing"
            COMMAND ${GLSLC} -fshader-stage=${SHADER_STAGE} ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3 --target-spv=spv1.5
            COMMAND ${SPIRV_VAL} ${SHADER_OUTPUT} || echo "SPIR-V validation failed"
            DEPENDS ${SHADER}
            COMMENT "Compiling ray-tracing shader ${SHADER_BASE} (Linux)"
        )
    else()
        set(SHADER_OUTPUT "${WINDOWS_BIN_DIR}/assets/shaders/raytracing/${SHADER_BASE}.spv")
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_BIN_DIR}/assets/shaders/raytracing"
            COMMAND ${GLSLC} -fshader-stage=${SHADER_STAGE} ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3 --target-spv=spv1.5
            COMMAND ${SPIRV_VAL} ${SHADER_OUTPUT} || echo "SPIR-V validation failed"
            DEPENDS ${SHADER}
            COMMENT "Compiling ray-tracing shader ${SHADER_BASE} (Windows)"
        )
    endif()
    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

# Tonemap shaders (graphics)
if(EXISTS "${SHADER_DIR}/graphics/tonemap_vert.glsl")
    if(IS_LINUX)
        set(TONEMAP_VERT_OUT "${LINUX_BIN_DIR}/assets/shaders/graphics/tonemap_vert.spv")
        add_custom_command(
            OUTPUT ${TONEMAP_VERT_OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${LINUX_BIN_DIR}/assets/shaders/graphics"
            COMMAND ${GLSLC} -fshader-stage=vert "${SHADER_DIR}/graphics/tonemap_vert.glsl" -o ${TONEMAP_VERT_OUT} --target-env=vulkan1.3 --target-spv=spv1.3
            COMMAND ${SPIRV_VAL} ${TONEMAP_VERT_OUT} || echo "SPIR-V validation failed"
            DEPENDS "${SHADER_DIR}/graphics/tonemap_vert.glsl"
            COMMENT "Compiling tonemap vertex shader (Linux)"
        )
    else()
        set(TONEMAP_VERT_OUT "${WINDOWS_BIN_DIR}/assets/shaders/graphics/tonemap_vert.spv")
        add_custom_command(
            OUTPUT ${TONEMAP_VERT_OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_BIN_DIR}/assets/shaders/graphics"
            COMMAND ${GLSLC} -fshader-stage=vert "${SHADER_DIR}/graphics/tonemap_vert.glsl" -o ${TONEMAP_VERT_OUT} --target-env=vulkan1.3 --target-spv=spv1.3
            COMMAND ${SPIRV_VAL} ${TONEMAP_VERT_OUT} || echo "SPIR-V validation failed"
            DEPENDS "${SHADER_DIR}/graphics/tonemap_vert.glsl"
            COMMENT "Compiling tonemap vertex shader (Windows)"
        )
    endif()
    list(APPEND SHADER_OUTPUTS ${TONEMAP_VERT_OUT})
endif()

if(EXISTS "${SHADER_DIR}/graphics/tonemap_frag.glsl")
    if(IS_LINUX)
        set(TONEMAP_FRAG_OUT "${LINUX_BIN_DIR}/assets/shaders/graphics/tonemap_frag.spv")
        add_custom_command(
            OUTPUT ${TONEMAP_FRAG_OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${LINUX_BIN_DIR}/assets/shaders/graphics"
            COMMAND ${GLSLC} -fshader-stage=frag "${SHADER_DIR}/graphics/tonemap_frag.glsl" -o ${TONEMAP_FRAG_OUT} --target-env=vulkan1.3 --target-spv=spv1.3
            COMMAND ${SPIRV_VAL} ${TONEMAP_FRAG_OUT} || echo "SPIR-V validation failed"
            DEPENDS "${SHADER_DIR}/graphics/tonemap_frag.glsl"
            COMMENT "Compiling tonemap fragment shader (Linux)"
        )
    else()
        set(TONEMAP_FRAG_OUT "${WINDOWS_BIN_DIR}/assets/shaders/graphics/tonemap_frag.spv")
        add_custom_command(
            OUTPUT ${TONEMAP_FRAG_OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_BIN_DIR}/assets/shaders/graphics"
            COMMAND ${GLSLC} -fshader-stage=frag "${SHADER_DIR}/graphics/tonemap_frag.glsl" -o ${TONEMAP_FRAG_OUT} --target-env=vulkan1.3 --target-spv=spv1.3
            COMMAND ${SPIRV_VAL} ${TONEMAP_FRAG_OUT} || echo "SPIR-V validation failed"
            DEPENDS "${SHADER_DIR}/graphics/tonemap_frag.glsl"
            COMMENT "Compiling tonemap fragment shader (Windows)"
        )
    endif()
    list(APPEND SHADER_OUTPUTS ${TONEMAP_FRAG_OUT})
endif()

# Rasterization & Compute shaders
foreach(EXT ${SHADER_EXTS})
    file(GLOB RASTER_SHADERS "${SHADER_DIR}/rasterization/${EXT}")
    foreach(SHADER ${RASTER_SHADERS})
        get_filename_component(SHADER_BASE ${SHADER} NAME_WE)
        if(IS_LINUX)
            set(SHADER_OUTPUT "${LINUX_BIN_DIR}/assets/shaders/rasterization/${SHADER_BASE}.spv")
        else()
            set(SHADER_OUTPUT "${WINDOWS_BIN_DIR}/assets/shaders/rasterization/${SHADER_BASE}.spv")
        endif()
        if(NOT EXISTS "${SHADER}")
            continue()
        endif()
        file(READ "${SHADER}" SHADER_CONTENT)
        if("${SHADER_CONTENT}" STREQUAL "")
            continue()
        endif()
        string(FIND "${EXT}" ".vert" IS_VERT)
        string(FIND "${EXT}" ".frag" IS_FRAG)
        string(FIND "${EXT}" ".comp" IS_COMP)
        if(IS_VERT GREATER -1)
            set(SHADER_STAGE "vert")
        elseif(IS_FRAG GREATER -1)
            set(SHADER_STAGE "frag")
        elseif(IS_COMP GREATER -1)
            set(SHADER_STAGE "comp")
        else()
            continue()
        endif()
        if(IS_LINUX)
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${LINUX_BIN_DIR}/assets/shaders/rasterization"
                COMMAND ${GLSLC} -fshader-stage=${SHADER_STAGE} ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3 --target-spv=spv1.3
                COMMAND ${SPIRV_VAL} ${SHADER_OUTPUT} || echo "SPIR-V validation failed"
                DEPENDS ${SHADER}
                COMMENT "Compiling rasterization shader ${SHADER_BASE} (Linux)"
            )
        else()
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_BIN_DIR}/assets/shaders/rasterization"
                COMMAND ${GLSLC} -fshader-stage=${SHADER_STAGE} ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3 --target-spv=spv1.3
                COMMAND ${SPIRV_VAL} ${SHADER_OUTPUT} || echo "SPIR-V validation failed"
                DEPENDS ${SHADER}
                COMMENT "Compiling rasterization shader ${SHADER_BASE} (Windows)"
            )
        endif()
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endforeach()

    file(GLOB COMPUTE_SHADERS "${SHADER_DIR}/compute/${EXT}")
    foreach(SHADER ${COMPUTE_SHADERS})
        get_filename_component(SHADER_BASE ${SHADER} NAME_WE)
        if(IS_LINUX)
            set(SHADER_OUTPUT "${LINUX_BIN_DIR}/assets/shaders/compute/${SHADER_BASE}.spv")
        else()
            set(SHADER_OUTPUT "${WINDOWS_BIN_DIR}/assets/shaders/compute/${SHADER_BASE}.spv")
        endif()
        if(NOT EXISTS "${SHADER}")
            continue()
        endif()
        file(READ "${SHADER}" SHADER_CONTENT)
        if("${SHADER_CONTENT}" STREQUAL "")
            continue()
        endif()
        if(IS_LINUX)
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${LINUX_BIN_DIR}/assets/shaders/compute"
                COMMAND ${GLSLC} -fshader-stage=comp ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3 --target-spv=spv1.3
                COMMAND ${SPIRV_VAL} ${SHADER_OUTPUT} || echo "SPIR-V validation failed"
                DEPENDS ${SHADER}
                COMMENT "Compiling compute shader ${SHADER_BASE} (Linux)"
            )
        else()
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_BIN_DIR}/assets/shaders/compute"
                COMMAND ${GLSLC} -fshader-stage=comp ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3 --target-spv=spv1.3
                COMMAND ${SPIRV_VAL} ${SHADER_OUTPUT} || echo "SPIR-V validation failed"
                DEPENDS ${SHADER}
                COMMENT "Compiling compute shader ${SHADER_BASE} (Windows)"
            )
        endif()
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endforeach()
endforeach()

if(SHADER_OUTPUTS)
    add_custom_target(shaders DEPENDS ${SHADER_OUTPUTS})
    add_dependencies(amouranth_engine shaders)
else()
    message(WARNING "No shader outputs generated.")
endif()

# Copy assets
set(ASSET_TYPES
    "FONT:*.ttf,*.otf:${ASSET_DIR}/fonts:${LINUX_BIN_DIR}/assets/fonts:${WINDOWS_BIN_DIR}/assets/fonts"
    "TEXTURE:*.png,*.jpg,*.ktx,*.dds,*.hdr,*.exr:${ASSET_DIR}/textures:${LINUX_BIN_DIR}/assets/textures:${WINDOWS_BIN_DIR}/assets/textures"
    "MODEL:*.obj,*.fbx,*.gltf,*.glb:${ASSET_DIR}/models:${LINUX_BIN_DIR}/assets/models:${WINDOWS_BIN_DIR}/assets/models"
    "MATERIAL:*.json,*.mat:${ASSET_DIR}/materials:${LINUX_BIN_DIR}/assets/materials:${WINDOWS_BIN_DIR}/assets/materials"
    "SCENE:*.json,*.scene:${ASSET_DIR}/scenes:${LINUX_BIN_DIR}/assets/scenes:${WINDOWS_BIN_DIR}/assets/scenes"
    "AUDIO:*.wav,*.ogg,*.mp3:${ASSET_DIR}/audio:${LINUX_BIN_DIR}/assets/audio:${WINDOWS_BIN_DIR}/assets/audio"
    "SCRIPT:*.lua:${ASSET_DIR}/scripts:${LINUX_BIN_DIR}/assets/scripts:${WINDOWS_BIN_DIR}/assets/scripts"
)
foreach(ASSET_TYPE ${ASSET_TYPES})
    string(REPLACE ":" ";" INFO ${ASSET_TYPE})
    list(GET INFO 0 TYPE_NAME)
    list(GET INFO 1 EXTS)
    list(GET INFO 2 SRC_DIR)
    list(GET INFO 3 LINUX_OUT)
    list(GET INFO 4 WINDOWS_OUT)
    string(REPLACE "," ";" EXT_LIST ${EXTS})
    set(FILES "")
    foreach(EXT ${EXT_LIST})
        file(GLOB TEMP "${SRC_DIR}/${EXT}")
        list(APPEND FILES ${TEMP})
    endforeach()
    list(REMOVE_DUPLICATES FILES)
    list(LENGTH FILES COUNT)
    if(COUNT GREATER 0)
        message(STATUS "Found ${COUNT} ${TYPE_NAME} assets in ${SRC_DIR}")
        if(IS_LINUX)
            add_custom_command(TARGET amouranth_engine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FILES} "${LINUX_OUT}/"
                COMMENT "Copying ${COUNT} ${TYPE_NAME} files (Linux)"
            )
        endif()
        if(IS_WINDOWS)
            add_custom_command(TARGET amouranth_engine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FILES} "${WINDOWS_OUT}/"
                COMMENT "Copying ${COUNT} ${TYPE_NAME} files (Windows)"
            )
        endif()
    endif()
endforeach()