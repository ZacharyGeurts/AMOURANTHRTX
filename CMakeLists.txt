cmake_minimum_required(VERSION 3.21)
project(AMOURANTH_RTX LANGUAGES CXX)

# =============================================================================
# ENABLE FETCHCONTENT + GIT FOR EXTERNAL DEPENDENCIES (OLD CMAKE FIX)
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
    find_program(GCC14_BIN x86_64-w64-mingw32-g++-14)
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
    -Wall -Wextra -Wpedantic -Werror
    -Wno-missing-field-initializers
    -Wno-extra -Wno-reorder -Wno-delete-incomplete
    -O3 -march=native -mtune=native
    -flto=auto -ffast-math
    -g -ggdb
)

option(DISABLE_TRACE_AND_DEBUG_LOGS "Disable trace & debug logging" ON)
if(DISABLE_TRACE_AND_DEBUG_LOGS)
    add_definitions(-DDISABLE_TRACE_AND_DEBUG_LOGS)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options(-fsanitize=address,undefined,leak)
    add_link_options(-fsanitize=address,undefined,leak)
endif()

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
# DEPENDENCIES
# =============================================================================
find_package(PkgConfig REQUIRED)
find_package(Vulkan REQUIRED)
if(Vulkan_VERSION VERSION_LESS "1.3.250")
    message(FATAL_ERROR "Vulkan SDK >= 1.3.250 REQUIRED")
endif()

find_program(GLSLC glslc REQUIRED HINTS $ENV{VULKAN_SDK}/bin /usr/bin)
find_program(SPIRV_VAL spirv-val HINTS $ENV{VULKAN_SDK}/bin /usr/bin)

find_package(TBB REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Freetype REQUIRED)
find_package(glm REQUIRED)

if(IS_LINUX)
    pkg_check_modules(SDL3 REQUIRED sdl3)
    pkg_check_modules(SDL3_TTF REQUIRED sdl3-ttf)
    pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
    pkg_check_modules(SDL3_MIXER REQUIRED sdl3-mixer)
    pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
    pkg_check_modules(X11 REQUIRED x11 xext xrandr xcursor xi xscrnsaver xcb)
    pkg_check_modules(GBM REQUIRED gbm)
    pkg_check_modules(LIBDRM REQUIRED libdrm)
endif()

if(IS_WINDOWS)
    find_library(SDL3_LIB NAMES SDL3 PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_TTF_LIB NAMES SDL3_ttf PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_IMAGE_LIB NAMES SDL3_image PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
    find_library(SDL3_MIXER_LIB NAMES SDL3_mixer PATHS /usr/x86_64-w64-mingw32/lib REQUIRED)
endif()

find_library(ATOMIC_LIB NAMES atomic libatomic atomic.so.1 libatomic.so.1
    HINTS /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib64)
if(NOT ATOMIC_LIB)
    message(FATAL_ERROR "libatomic not found")
endif()
message(STATUS "Found libatomic: ${ATOMIC_LIB}")

# =============================================================================
# Dear ImGui — ONLY SDL3 + Vulkan, NO examples, NO other backends, NO junk EVER
# =============================================================================
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/imgui")

git_clone_if_missing(
    https://github.com/ocornut/imgui.git
    imgui
    v1.91.3
    ${IMGUI_DIR}
)

# Explicitly list ONLY the files we want compiled
set(IMGUI_CORE_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp                 # optional but harmless + useful
)

set(IMGUI_BACKEND_SOURCES
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# Mark EVERYTHING else in the imgui folder as HEADER_FILE_ONLY so CMake NEVER compiles it
file(GLOB_RECURSE IMGUI_ALL_FILES "${IMGUI_DIR}/*")
list(REMOVE_ITEM IMGUI_ALL_FILES ${IMGUI_CORE_SOURCES} ${IMGUI_BACKEND_SOURCES})
set_source_files_properties(${IMGUI_ALL_FILES} PROPERTIES HEADER_FILE_ONLY ON)

# Now build only what we want
add_library(imgui STATIC ${IMGUI_CORE_SOURCES} ${IMGUI_BACKEND_SOURCES})

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${Vulkan_INCLUDE_DIRS}
    ${SDL3_INCLUDE_DIRS}
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan ${SDL3_LIBRARIES})
target_compile_features(imgui PUBLIC cxx_std_17)

message(STATUS "ImGui configured — ONLY SDL3+Vulkan backend — examples & other backends completely ignored")

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
)

if(IS_LINUX)
    target_include_directories(amouranth_engine PRIVATE
        ${SDL3_INCLUDE_DIRS}
        ${SDL3_TTF_INCLUDE_DIRS}
        ${SDL3_IMAGE_INCLUDE_DIRS}
        ${SDL3_MIXER_INCLUDE_DIRS}
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

target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    TBB::tbb
    OpenMP::OpenMP_CXX
    Freetype::Freetype
    ${ATOMIC_LIB}
    imgui
)

if(IS_LINUX)
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIBRARIES}
        ${SDL3_TTF_LIBRARIES}
        ${SDL3_IMAGE_LIBRARIES}
        ${SDL3_MIXER_LIBRARIES}
        ${HARFBUZZ_LIBRARIES}
        ${X11_LIBRARIES}
        ${GBM_LIBRARIES}
        ${LIBDRM_LIBRARIES}
    )
else()
    target_link_libraries(amouranth_engine PRIVATE
        ${SDL3_LIB} ${SDL3_TTF_LIB} ${SDL3_IMAGE_LIB} ${SDL3_MIXER_LIB}
        -static-libgcc -static-libstdc++ -mwindows
    )
endif()

# =============================================================================
# SHADER COMPILATION — FIXED (no more stray parentheses!)
# =============================================================================
set(SHADER_INCLUDE_FLAGS
    -I${CMAKE_CURRENT_SOURCE_DIR}/include/engine/Vulkan
    -I${CMAKE_CURRENT_SOURCE_DIR}/include/engine
    -I${CMAKE_CURRENT_SOURCE_DIR}/shaders
)
set(SHADER_DEBUG_FLAGS -g -O0)

file(GLOB_RECURSE POTENTIAL_SHADERS
    shaders/raytracing/*
    shaders/compute/*
    shaders/graphics/*
    shaders/rasterization/*
    shaders/tonemaps/*
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

    if(REL_DIR STREQUAL "tonemaps")
        set(REL_DIR "graphics")
    endif()

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
        set(VALIDATION_ENV vulkan1.3)
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
    "fonts:*.ttf,*.otf,*.font,*.bdf,*.fnt"
    "textures:*.png,*.jpg,*.ktx2,*.dds,*.hdr,*.ico,*.bmp,*.gif,*.tga,*.webp,*.jp2,*.svg"
    "models:*.obj,*.gltf,*.glb,*.fbx,*.stl,*.blend"
    "materials:*.json,*.mtl"
    "scenes:*.json,*.xml,*.yaml"
    "audio:*.wav,*.ogg,*.mp3,*.flac,*.aac,*.m4a,*.wma"
    "scripts:*.lua,*.py,*.js,*.glsl"
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
# LEGENDARY BANNER — NOVEMBER 17 2025 — FINAL FIXED EDITION
# =============================================================================
message(STATUS "")
message(STATUS "==================================================================")
message(STATUS "")
message(STATUS "")
message(STATUS "              ██╔══██║██║╚██╔╝██║██║   ██║██║   ██║██╔══██╗██╔══██║██║╚██╗██║   ██║   ██╔══██║")
message(STATUS "              ██║  ██║██║ ╚═╝ ██║╚██████╔╝╚██████╔╝██║  ██║██║  ██║██║ ╚████║   ██║   ██║  ██║")
message(STATUS "              ╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝")
message(STATUS "")
message(STATUS "               ██╔══██╗   ██║    ██╔██╗     ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  ")
message(STATUS "               ██║  ██║   ██║   ██╔╝ ██╗    ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗")
message(STATUS "               ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝")
message(STATUS "")
message(STATUS "        ██████████████████████████████████████████████████████████████████████████████████████")
message(STATUS "")
message(STATUS "==================================================================")
message(STATUS " AMOURANTH RTX — FULLY FIXED + Dear ImGui (SDL3+Vulkan) — NOVEMBER 17 2025")
message(STATUS " Target: ${BIN_DIR}/Navigator${EXE_SUFFIX}")
message(STATUS " ImGui location: src/engine/imgui (auto-cloned on first build)")
message(STATUS " Run: ./linux.sh clean")
message(STATUS " HDR: 10-bit invisible HDR via GBM — works even on Cinnamon/GNOME")
message(STATUS "==================================================================")