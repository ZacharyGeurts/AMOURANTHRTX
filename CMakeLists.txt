cmake_minimum_required(VERSION 3.12)
project(AMOURANTH_RTX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global options
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wall -Wextra -Wpedantic -O3 -g)
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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/SDL3"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Vulkan"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/modes"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/modes"
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
message(STATUS "Vulkan include dirs: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan libraries: ${Vulkan_LIBRARIES}")

find_program(GLSLC glslc REQUIRED HINTS /usr/bin $ENV{VULKAN_SDK}/bin /usr/local/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK (vulkan-tools).")
endif()
message(STATUS "Found glslc: ${GLSLC}")

# TBB
find_package(TBB REQUIRED)
if(NOT TBB_FOUND)
    message(FATAL_ERROR "TBB not found. Install libtbb-dev.")
endif()
message(STATUS "TBB include dirs: ${TBB_INCLUDE_DIRS}")
message(STATUS "TBB libraries: ${TBB_LIBRARIES}")

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

# HarfBuzz (using pkg-config)
if(IS_LINUX)
    pkg_check_modules(HarfBuzz REQUIRED harfbuzz)
    if(NOT HarfBuzz_FOUND)
        message(FATAL_ERROR "HarfBuzz not found. Install libharfbuzz-dev.")
    endif()
    message(STATUS "HarfBuzz include dirs: ${HarfBuzz_INCLUDE_DIRS}")
    message(STATUS "HarfBuzz libraries: ${HarfBuzz_LIBRARIES}")

    # SDL3 and extensions
    pkg_check_modules(SDL3 sdl3)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR "SDL3 not found. Install libsdl3-dev or build from source (https://github.com/libsdl-org/SDL).")
    endif()
    message(STATUS "SDL3 include dirs: ${SDL3_INCLUDE_DIRS}")
    message(STATUS "SDL3 libraries: ${SDL3_LIBRARIES}")

    pkg_check_modules(SDL3_ttf sdl3-ttf)
    if(NOT SDL3_ttf_FOUND)
        message(FATAL_ERROR "SDL3_ttf not found. Install sdl3-ttf or build from source (https://github.com/libsdl-org/SDL_ttf).")
    endif()
    message(STATUS "SDL3_ttf include dirs: ${SDL3_ttf_INCLUDE_DIRS}")
    message(STATUS "SDL3_ttf libraries: ${SDL3_ttf_LIBRARIES}")

    pkg_check_modules(SDL3_image sdl3-image)
    if(NOT SDL3_image_FOUND)
        message(FATAL_ERROR "SDL3_image not found. Install sdl3-image or build from source (https://github.com/libsdl-org/SDL_image).")
    endif()
    message(STATUS "SDL3_image include dirs: ${SDL3_image_INCLUDE_DIRS}")
    message(STATUS "SDL3_image libraries: ${SDL3_image_LIBRARIES}")

    pkg_check_modules(SDL3_mixer sdl3-mixer)
    if(NOT SDL3_mixer_FOUND)
        message(FATAL_ERROR "SDL3_mixer not found. Install sdl3-mixer or build from source (https://github.com/libsdl-org/SDL_mixer).")
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

# Windows cross-compilation dependencies
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

# Find libatomic for 128-bit atomic operations
find_library(ATOMIC_LIBRARY NAMES atomic libatomic atomic.so.1 libatomic.so.1 HINTS /usr/lib /usr/lib/x86_64-linux-gnu)
if(NOT ATOMIC_LIBRARY)
    set(ATOMIC_LIBRARY /usr/lib/x86_64-linux-gnu/libatomic.so.1)
    if(NOT EXISTS "${ATOMIC_LIBRARY}")
        message(FATAL_ERROR "libatomic not found at /usr/lib/x86_64-linux-gnu/libatomic.so.1. Install libatomic1 and libatomic-dev.")
    endif()
endif()
message(STATUS "libatomic library: ${ATOMIC_LIBRARY}")

# Directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ENGINE_SRC_DIR ${SRC_DIR}/engine)
set(SDL3_SRC_DIR ${ENGINE_SRC_DIR}/SDL3)
set(VULKAN_SRC_DIR ${ENGINE_SRC_DIR}/Vulkan)
set(MODES_SRC_DIR ${SRC_DIR}/modes)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(ASSET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(LINUX_BIN_DIR ${CMAKE_BINARY_DIR}/bin/Linux)
set(WINDOWS_BIN_DIR ${CMAKE_BINARY_DIR}/bin/Windows)

# Source files
file(GLOB_RECURSE SOURCES "${SRC_DIR}/*.cpp")
if(NOT SOURCES)
    message(FATAL_ERROR "No source files found in ${SRC_DIR}. Ensure source files exist.")
endif()
message(STATUS "Found source files: ${SOURCES}")

# Create executable
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

# Common includes
target_include_directories(amouranth_engine PRIVATE
    ${INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${HarfBuzz_INCLUDE_DIRS}
    ${glm_INCLUDE_DIRS}
)

# Platform-specific includes
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

# Common libraries
target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    TBB::tbb
    OpenMP::OpenMP_CXX
    Freetype::Freetype
    ${HarfBuzz_LIBRARIES}
    ${ATOMIC_LIBRARY}
)

# Platform-specific libraries
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
set(SHADER_EXTS "*.vert" "*.frag" "*.rahit" "*.rchit" "*.rmiss" "*.rgen" "*.rint" "*.rcall" "*.comp")
set(SHADER_OUTPUTS "")
foreach(EXT ${SHADER_EXTS})
    file(GLOB RASTER_SHADERS CONFIGURE_DEPENDS "${SHADER_DIR}/rasterization/${EXT}")
    foreach(SHADER ${RASTER_SHADERS})
        get_filename_component(SHADER_BASE ${SHADER} NAME_WE)
        if(IS_LINUX)
            set(SHADER_OUTPUT "${LINUX_BIN_DIR}/assets/shaders/rasterization/${SHADER_BASE}.spv")
        else()
            set(SHADER_OUTPUT "${WINDOWS_BIN_DIR}/assets/shaders/rasterization/${SHADER_BASE}.spv")
        endif()
        # Validate shader file before compilation
        if(EXISTS "${SHADER}")
            file(READ "${SHADER}" SHADER_CONTENT)
            if("${SHADER_CONTENT}" STREQUAL "")
                message(WARNING "Shader file ${SHADER} is empty. Skipping compilation.")
                continue()
            endif()
        else()
            message(WARNING "Shader file ${SHADER} does not exist. Skipping compilation.")
            continue()
        endif()
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${LINUX_BIN_DIR}/assets/shaders/rasterization"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_BIN_DIR}/assets/shaders/rasterization"
            COMMAND ${GLSLC} ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3
            DEPENDS ${SHADER}
            COMMENT "Compiling rasterization shader ${SHADER_BASE}"
        )
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endforeach()

    file(GLOB RAYTRACE_SHADERS CONFIGURE_DEPENDS "${SHADER_DIR}/raytracing/${EXT}")
    foreach(SHADER ${RAYTRACE_SHADERS})
        get_filename_component(SHADER_BASE ${SHADER} NAME_WE)
        if(IS_LINUX)
            set(SHADER_OUTPUT "${LINUX_BIN_DIR}/assets/shaders/raytracing/${SHADER_BASE}.spv")
        else()
            set(SHADER_OUTPUT "${WINDOWS_BIN_DIR}/assets/shaders/raytracing/${SHADER_BASE}.spv")
        endif()
        # Validate shader file before compilation
        if(EXISTS "${SHADER}")
            file(READ "${SHADER}" SHADER_CONTENT)
            if("${SHADER_CONTENT}" STREQUAL "")
                message(WARNING "Shader file ${SHADER} is empty. Skipping compilation.")
                continue()
            endif()
        else()
            message(WARNING "Shader file ${SHADER} does not exist. Skipping compilation.")
            continue()
        endif()
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${LINUX_BIN_DIR}/assets/shaders/raytracing"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_BIN_DIR}/assets/shaders/raytracing"
            COMMAND ${GLSLC} ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3
            DEPENDS ${SHADER}
            COMMENT "Compiling raytracing shader ${SHADER_BASE}"
        )
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endforeach()

    file(GLOB COMPUTE_SHADERS CONFIGURE_DEPENDS "${SHADER_DIR}/compute/${EXT}")
    foreach(SHADER ${COMPUTE_SHADERS})
        get_filename_component(SHADER_BASE ${SHADER} NAME_WE)
        if(IS_LINUX)
            set(SHADER_OUTPUT "${LINUX_BIN_DIR}/assets/shaders/compute/${SHADER_BASE}.spv")
        else()
            set(SHADER_OUTPUT "${WINDOWS_BIN_DIR}/assets/shaders/compute/${SHADER_BASE}.spv")
        endif()
        # Validate shader file before compilation
        if(EXISTS "${SHADER}")
            file(READ "${SHADER}" SHADER_CONTENT)
            if("${SHADER_CONTENT}" STREQUAL "")
                message(WARNING "Shader file ${SHADER} is empty. Skipping compilation.")
                continue()
            endif()
        else()
            message(WARNING "Shader file ${SHADER} does not exist. Skipping compilation.")
            continue()
        endif()
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${LINUX_BIN_DIR}/assets/shaders/compute"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_BIN_DIR}/assets/shaders/compute"
            COMMAND ${GLSLC} ${SHADER} -o ${SHADER_OUTPUT} --target-env=vulkan1.3
            DEPENDS ${SHADER}
            COMMENT "Compiling compute shader ${SHADER_BASE}"
        )
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endforeach()
endforeach()
if(SHADER_OUTPUTS)
    add_custom_target(shaders DEPENDS ${SHADER_OUTPUTS})
    add_dependencies(amouranth_engine shaders)
endif()

# Copy assets
set(ASSET_TYPES
    "FONT:*.ttf,*.otf:${ASSET_DIR}/fonts:${CMAKE_BINARY_DIR}/bin/Linux/assets/fonts:${CMAKE_BINARY_DIR}/bin/Windows/assets/fonts"
    "TEXTURE:*.png,*.jpg,*.ktx,*.dds,*.hdr,*.exr:${ASSET_DIR}/textures:${CMAKE_BINARY_DIR}/bin/Linux/assets/textures:${CMAKE_BINARY_DIR}/bin/Windows/assets/textures"
    "MODEL:*.obj,*.fbx,*.gltf,*.glb:${ASSET_DIR}/models:${CMAKE_BINARY_DIR}/bin/Linux/assets/models:${CMAKE_BINARY_DIR}/bin/Windows/assets/models"
    "MATERIAL:*.json,*.mat:${ASSET_DIR}/materials:${CMAKE_BINARY_DIR}/bin/Linux/assets/materials:${CMAKE_BINARY_DIR}/bin/Windows/assets/materials"
    "SCENE:*.json,*.scene:${ASSET_DIR}/scenes:${CMAKE_BINARY_DIR}/bin/Linux/assets/scenes:${CMAKE_BINARY_DIR}/bin/Windows/assets/scenes"
    "AUDIO:*.wav,*.ogg,*.mp3:${ASSET_DIR}/audio:${CMAKE_BINARY_DIR}/bin/Linux/assets/audio:${CMAKE_BINARY_DIR}/bin/Windows/assets/audio"
    "SCRIPT:*.lua,*.py:${ASSET_DIR}/scripts:${CMAKE_BINARY_DIR}/bin/Linux/assets/scripts:${CMAKE_BINARY_DIR}/bin/Windows/assets/scripts"
)
foreach(ASSET_TYPE ${ASSET_TYPES})
    string(REPLACE ":" ";" ASSET_INFO ${ASSET_TYPE})
    list(GET ASSET_INFO 0 TYPE_NAME)
    list(GET ASSET_INFO 1 EXTENSIONS)
    list(GET ASSET_INFO 2 ASSET_SRC_DIR)
    list(GET ASSET_INFO 3 LINUX_OUT_DIR)
    list(GET ASSET_INFO 4 WINDOWS_OUT_DIR)
    string(REPLACE "," ";" EXTENSION_LIST ${EXTENSIONS})

    set(ASSET_FILES "")
    foreach(EXT ${EXTENSION_LIST})
        file(GLOB TEMP_FILES CONFIGURE_DEPENDS "${ASSET_SRC_DIR}/${EXT}")
        list(APPEND ASSET_FILES ${TEMP_FILES})
    endforeach()
    list(REMOVE_DUPLICATES ASSET_FILES)
    list(LENGTH ASSET_FILES NUM_ASSETS)
    if(NUM_ASSETS GREATER 0)
        message(STATUS "Found ${NUM_ASSETS} ${TYPE_NAME} assets in ${ASSET_SRC_DIR}")
        if(IS_LINUX)
            add_custom_command(TARGET amouranth_engine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSET_FILES} "${LINUX_OUT_DIR}/"
                COMMENT "Copying ${NUM_ASSETS} ${TYPE_NAME} files to ${LINUX_OUT_DIR}"
            )
        endif()
        if(IS_WINDOWS)
            add_custom_command(TARGET amouranth_engine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ASSET_FILES} "${WINDOWS_OUT_DIR}/"
                COMMENT "Copying ${NUM_ASSETS} ${TYPE_NAME} files to ${WINDOWS_OUT_DIR}"
            )
        endif()
    endif()
endforeach()