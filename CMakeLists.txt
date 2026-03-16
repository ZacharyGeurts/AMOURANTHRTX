cmake_minimum_required(VERSION 3.21)
project(AMOURANTH_RTX LANGUAGES CXX)

# =============================================================================
# PRODUCTION TOGGLES - CHANGE THESE TO ENABLE/DISABLE FEATURES
# =============================================================================
option(BUILD_SHARED_LIBS                "Build shared libs (OFF = fully static)" OFF)
option(DISABLE_TRACE_AND_DEBUG_LOGS    "Disable trace & debug logging" ON)
option(ENABLE_EXTRA_WARNINGS           "Enable extra warnings" ON)

option(FETCH_SDL3       "Fetch & build SDL3 from main"          ON)
option(FETCH_SDL3_IMAGE "Fetch & build SDL3_image from main"    ON)
option(FETCH_SDL3_MIXER "Fetch & build SDL3_mixer from main"    ON)
option(FETCH_SDL3_TTF   "Fetch & build SDL3_ttf from main"      ON)

option(SDL3IMAGE_PNG    "Enable PNG support"  ON)
option(SDL3IMAGE_JPG    "Enable JPEG support" ON)
option(SDL3IMAGE_AVIF   "Enable AVIF support" OFF)
option(SDL3IMAGE_JXL    "Enable JPEG-XL"      OFF)
option(SDL3IMAGE_WEBP   "Enable WebP"         OFF)
option(SDL3IMAGE_TIFF   "Enable TIFF"         OFF)

# =============================================================================
# FETCHCONTENT SETUP
# =============================================================================
include(FetchContent)
find_package(Git REQUIRED)

set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# =============================================================================
# TOOLCHAIN + PLATFORM
# =============================================================================
if(CMAKE_CROSSCOMPILING)
    find_program(GCC14_BIN x86_64-w64-mingw32-g++)
    if(NOT GCC14_BIN)
        message(FATAL_ERROR "MinGW-w64 GCC required for cross-compile")
    endif()
    set(TARGET_PLATFORM "Windows")
    set(EXE_SUFFIX ".exe")
    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin/Windows")
    message(STATUS "Cross-compiling → Windows (MinGW-w64)")
else()
    find_program(GCC14_BIN g++-14)
    if(NOT GCC14_BIN)
        message(FATAL_ERROR "GCC 14+ required for native build")
    endif()
    set(TARGET_PLATFORM "Linux")
    set(EXE_SUFFIX "")
    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin/Linux")
    message(STATUS "Native build → Linux")
endif()

set(CMAKE_CXX_COMPILER ${GCC14_BIN})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "Target: ${TARGET_PLATFORM} | Output: ${BIN_DIR}")

# =============================================================================
# COMPILER FLAGS
# =============================================================================
set(OUR_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wunused)
if(ENABLE_EXTRA_WARNINGS)
    list(APPEND OUR_WARNING_FLAGS -Wdouble-promotion -Wformat=2 -Wnull-dereference)
endif()

set(COMMON_OPT_FLAGS -O3 -ffast-math -march=native -mtune=native -g)

find_program(MOLD_EXECUTABLE mold)
if(MOLD_EXECUTABLE)
    add_link_options(-fuse-ld=mold)
endif()

if(TARGET_PLATFORM STREQUAL "Windows")
    add_compile_options(-fstack-protector-strong -fcf-protection=full -D_FORTIFY_SOURCE=3 -Wa,-mbig-obj)
    add_link_options(-static -static-libgcc -static-libstdc++ -Wl,--whole-archive -lwinpthread -lgcc -lgcc_eh -Wl,--no-whole-archive)
endif()

if(DISABLE_TRACE_AND_DEBUG_LOGS)
    add_definitions(-DDISABLE_TRACE_AND_DEBUG_LOGS)
endif()

# =============================================================================
# ASSETS
# =============================================================================
set(DEVELOPER_DIRS assets/fonts assets/textures assets/models assets/materials assets/scenes assets/audio assets/scripts Navigator/compute)
foreach(D ${DEVELOPER_DIRS})
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${D}")
endforeach()
file(MAKE_DIRECTORY "${BIN_DIR}/compute")

set(ASSETS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_DST "${BIN_DIR}/assets")
if(EXISTS "${ASSETS_SRC}")
    add_custom_command(OUTPUT "${ASSETS_DST}/.copied"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_SRC}" "${ASSETS_DST}"
        COMMAND ${CMAKE_COMMAND} -E touch "${ASSETS_DST}/.copied"
        DEPENDS "${ASSETS_SRC}"
        COMMENT "Copying assets"
    )
    add_custom_target(copy_assets ALL DEPENDS "${ASSETS_DST}/.copied")
endif()

# =============================================================================
# DEPENDENCIES
# =============================================================================
find_package(PkgConfig REQUIRED)
find_package(Vulkan REQUIRED)
if(Vulkan_VERSION VERSION_LESS "1.3.250")
    message(FATAL_ERROR "Vulkan SDK >= 1.3.250 required")
endif()

find_program(GLSLC glslc REQUIRED)
find_program(SPIRV_VAL spirv-val REQUIRED)
find_package(OpenMP REQUIRED)

# FreeType - 100% static on Windows (embedded in exe, no libfreetype.dll ever)
if(TARGET_PLATFORM STREQUAL "Windows")
    message(STATUS "FreeType: Vendoring static build - 100% embedded in .exe (no DLL)")
    FetchContent_Declare(freetype
        GIT_REPOSITORY https://github.com/libsdl-org/freetype.git
        GIT_TAG        VER-2-13-3
        GIT_PROGRESS   TRUE
    )
    set(FT_DISABLE_HARFBUZZ ON CACHE INTERNAL "" FORCE)
    set(FT_DISABLE_ZLIB ON CACHE INTERNAL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE INTERNAL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE INTERNAL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE INTERNAL "" FORCE)
    FetchContent_MakeAvailable(freetype)
    set(FREETYPE_TARGET freetype)
    set(FREETYPE_INCLUDE_DIRS ${freetype_SOURCE_DIR}/include)
else()
    find_package(Freetype REQUIRED)
    set(FREETYPE_TARGET Freetype::Freetype)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# SDL3 core
if(FETCH_SDL3)
    if(TARGET_PLATFORM STREQUAL "Windows")
        set(SDL_HIDAPI_LIBUSB_SHARED OFF CACHE INTERNAL "" FORCE)
        set(SDL_LIBUSB_SHARED OFF CACHE INTERNAL "" FORCE)
    endif()
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG main GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

# SDL3_image
if(FETCH_SDL3_IMAGE)
    if(TARGET_PLATFORM STREQUAL "Windows")
        set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB ON CACHE INTERNAL "" FORCE)
        set(CMAKE_DISABLE_FIND_PACKAGE_PNG  ON CACHE INTERNAL "" FORCE)
        set(SDL3IMAGE_ZLIB_VENDORED ON CACHE INTERNAL "" FORCE)
        set(SDL3IMAGE_PNG_VENDORED  ON CACHE INTERNAL "" FORCE)
        set(SDL3IMAGE_JPG_VENDORED  ON CACHE INTERNAL "" FORCE)
    endif()
    FetchContent_Declare(SDL3_image GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git GIT_TAG main GIT_PROGRESS TRUE)
    set(SDL3IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL3IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL3IMAGE_TESTS   OFF CACHE BOOL "" FORCE)
    set(SDL3IMAGE_PNG ${SDL3IMAGE_PNG} CACHE BOOL "" FORCE)
    set(SDL3IMAGE_JPG ${SDL3IMAGE_JPG} CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_image)
endif()

# SDL3_mixer
if(FETCH_SDL3_MIXER)
    if(TARGET_PLATFORM STREQUAL "Windows")
        set(SDLMIXER_MP3_MPG123 OFF CACHE INTERNAL "" FORCE)
    endif()
    FetchContent_Declare(SDL3_mixer GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git GIT_TAG main GIT_PROGRESS TRUE)
    set(SDL3MIXER_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL3MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_mixer)
endif()

# SDL3_ttf
if(FETCH_SDL3_TTF)
    if(TARGET_PLATFORM STREQUAL "Windows")
        set(SDLTTF_HARFBUZZ OFF CACHE INTERNAL "" FORCE)
        set(SDLTTF_FREETYPE_VENDORED ON CACHE INTERNAL "" FORCE)
        set(CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz ON CACHE INTERNAL "" FORCE)
    endif()
    FetchContent_Declare(SDL3_ttf GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git GIT_TAG main GIT_PROGRESS TRUE)
    set(SDL3TTF_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL3TTF_SAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_ttf)
endif()

# glm + tinyobjloader
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG master)
FetchContent_MakeAvailable(glm)
FetchContent_Declare(tinyobjloader GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git GIT_TAG release)
FetchContent_MakeAvailable(tinyobjloader)
set(TINYOBJLOADER_SRC "${tinyobjloader_SOURCE_DIR}/tiny_obj_loader.cc")
set_source_files_properties(${TINYOBJLOADER_SRC} PROPERTIES COMPILE_OPTIONS "-Wno-sign-conversion;-w")

# =============================================================================
# SOURCES + EXECUTABLE
# =============================================================================
set(OUR_SOURCES Navigator/main.cpp)
set(ALL_SOURCES ${OUR_SOURCES} ${TINYOBJLOADER_SRC})

add_executable(amouranth_engine ${ALL_SOURCES})
set_target_properties(amouranth_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
    OUTPUT_NAME "Navigator"
    SUFFIX "${EXE_SUFFIX}"
)

if(TARGET_PLATFORM STREQUAL "Windows")
    target_link_options(amouranth_engine PRIVATE -mconsole)
endif()

foreach(SRC ${OUR_SOURCES})
    set_source_files_properties(${SRC} PROPERTIES COMPILE_OPTIONS "${OUR_WARNING_FLAGS};${COMMON_OPT_FLAGS}")
endforeach()

# =============================================================================
# INCLUDES + DEFINES
# =============================================================================
target_include_directories(amouranth_engine SYSTEM BEFORE PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${glm_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
)

target_include_directories(amouranth_engine SYSTEM PRIVATE
    ${sdl3_SOURCE_DIR}/include
    ${sdl3_image_SOURCE_DIR}/include
    ${sdl3_mixer_SOURCE_DIR}/include
    ${sdl3_ttf_SOURCE_DIR}/include
)

target_include_directories(amouranth_engine PRIVATE Navigator/engine)

target_compile_definitions(amouranth_engine PRIVATE
    GLM_ENABLE_EXPERIMENTAL GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE
    HAVE_SDL3_MIXER HAVE_SDL3_IMAGE HAVE_SDL3_TTF
)

if(DISABLE_TRACE_AND_DEBUG_LOGS)
    target_compile_definitions(amouranth_engine PRIVATE DISABLE_TRACE_AND_DEBUG_LOGS)
endif()

# =============================================================================
# LINKING - FORCE STATIC EVERYTHING (especially FreeType)
# =============================================================================
if(TARGET_PLATFORM STREQUAL "Windows")
    target_link_libraries(amouranth_engine PRIVATE
        -Wl,--whole-archive
        $<TARGET_FILE:${FREETYPE_TARGET}>
        -Wl,--no-whole-archive
    )
    target_link_options(amouranth_engine PRIVATE
        "-Wl,--allow-multiple-definition"
    )
else()
    target_link_libraries(amouranth_engine PRIVATE ${FREETYPE_TARGET})
endif()

target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    OpenMP::OpenMP_CXX
)

if(FETCH_SDL3 OR FETCH_SDL3_IMAGE OR FETCH_SDL3_MIXER OR FETCH_SDL3_TTF)
    target_link_libraries(amouranth_engine PRIVATE SDL3::SDL3)
endif()

if(FETCH_SDL3_IMAGE)
    target_link_libraries(amouranth_engine PRIVATE SDL3_image::SDL3_image)
endif()
if(FETCH_SDL3_MIXER)
    target_link_libraries(amouranth_engine PRIVATE SDL3_mixer::SDL3_mixer)
endif()
if(FETCH_SDL3_TTF)
    target_link_libraries(amouranth_engine PRIVATE SDL3_ttf::SDL3_ttf)
endif()

if(TARGET_PLATFORM STREQUAL "Linux")
    target_link_libraries(amouranth_engine PRIVATE dl m pthread)
elseif(TARGET_PLATFORM STREQUAL "Windows")
    target_link_libraries(amouranth_engine PRIVATE
        kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid setupapi cfgmgr32 dbghelp
    )
endif()

# =============================================================================
# SHADERS
# =============================================================================
set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/compute/CANVAS.comp")
set(SHADER_OUT "${BIN_DIR}/compute/CANVAS.spv")
add_custom_command(OUTPUT "${SHADER_OUT}"
    COMMAND ${GLSLC} -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders" -O --target-spv=spv1.6 --target-env=vulkan1.4 -fshader-stage=comp "${SHADER_SRC}" -o "${SHADER_OUT}"
    DEPENDS "${SHADER_SRC}"
    COMMENT "Compiling compute shader"
)
add_custom_target(shaders ALL DEPENDS "${SHADER_OUT}")

# =============================================================================
# FINAL TARGET DEPENDENCIES + REPORT
# =============================================================================
add_dependencies(amouranth_engine shaders)
if(TARGET copy_assets)
    add_dependencies(amouranth_engine copy_assets)
endif()

message(STATUS "")
message(STATUS "==================================================================")
message(STATUS " AMOURANTHRTX")
message(STATUS " Target:       ${BIN_DIR}/Navigator${EXE_SUFFIX}")
if(TARGET_PLATFORM STREQUAL "Windows")
    message(STATUS " Run:          wine ${BIN_DIR}/Navigator.exe")
    message(STATUS " FreeType:     100% embedded (no libfreetype.dll - forced with --whole-archive)")
else()
    message(STATUS " Run:          ${BIN_DIR}/Navigator")
endif()
message(STATUS " Static:       YES (SDL + FreeType + all deps)")
message(STATUS " SDL3_image:   PNG:${SDL3IMAGE_PNG} JPG:${SDL3IMAGE_JPG}")
message(STATUS " MP3:          dr_mp3 only")
message(STATUS " TTF:          basic (HarfBuzz OFF on Windows)")
message(STATUS "==================================================================")