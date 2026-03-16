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
# Auto-download latest Vulkan SDK to project folder (like original)
# =============================================================================
set(VULKAN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/VulkanSDK")

if(NOT EXISTS "${VULKAN_SDK_DIR}/x86_64/include/vulkan/vulkan.h")
    message(STATUS "Downloading latest Vulkan SDK to project folder...")
    FetchContent_Declare(vulkan_sdk
        URL https://sdk.lunarg.com/sdk/download/latest/linux/vulkan-sdk.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_DIR "${VULKAN_SDK_DIR}"
    )
    FetchContent_MakeAvailable(vulkan_sdk)
endif()

set(VULKAN_SDK_ROOT "${VULKAN_SDK_DIR}/x86_64")
if(NOT EXISTS "${VULKAN_SDK_ROOT}/include/vulkan/vulkan.h")
    message(FATAL_ERROR "Vulkan SDK extraction failed — x86_64/include/vulkan.h not found")
endif()

set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_ROOT}/include")
set(Vulkan_LIBRARY      "${VULKAN_SDK_ROOT}/lib/libvulkan.so")

find_program(GLSLC glslc REQUIRED HINTS "${VULKAN_SDK_ROOT}/bin")
find_program(SPIRV_VAL spirv-val REQUIRED HINTS "${VULKAN_SDK_ROOT}/bin")

message(STATUS "Vulkan SDK: ${VULKAN_SDK_ROOT}")
message(STATUS "  Headers: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "  Library: ${Vulkan_LIBRARY}")
message(STATUS "  glslc:   ${GLSLC}")
message(STATUS "  spirv-val: ${SPIRV_VAL}")

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
# ASSETS + SHADER OUTPUT DIRECTORIES
# =============================================================================
set(DEVELOPER_DIRS assets/fonts assets/textures assets/models assets/materials assets/scenes assets/audio assets/scripts)
foreach(D ${DEVELOPER_DIRS})
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${D}")
endforeach()

file(MAKE_DIRECTORY
    "${BIN_DIR}/assets"
    "${BIN_DIR}/assets/shaders/raytracing"
    "${BIN_DIR}/assets/shaders/compute"
)

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
find_package(OpenMP REQUIRED)

# FreeType - 100% static on Windows
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

# SDL3 family
if(FETCH_SDL3)
    if(TARGET_PLATFORM STREQUAL "Windows")
        set(SDL_HIDAPI_LIBUSB_SHARED OFF CACHE INTERNAL "" FORCE)
        set(SDL_LIBUSB_SHARED OFF CACHE INTERNAL "" FORCE)
    endif()
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG main GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

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

if(FETCH_SDL3_MIXER)
    if(TARGET_PLATFORM STREQUAL "Windows")
        set(SDLMIXER_MP3_MPG123 OFF CACHE INTERNAL "" FORCE)
    endif()
    FetchContent_Declare(SDL3_mixer GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git GIT_TAG main GIT_PROGRESS TRUE)
    set(SDL3MIXER_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL3MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_mixer)
endif()

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
# LINKING
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
    ${Vulkan_LIBRARY}
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
# SHADERS — raytracing + CANVAS.comp
# =============================================================================
set(SHADER_INCLUDE_FLAGS
    -I"${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    -I"${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders"
    -I"${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/raytracing"
    -I"${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/compute"
)

# Debug → unoptimized + debug info, otherwise optimized
if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
    set(SHADER_OPT_FLAGS -g -O0)
else()
    set(SHADER_OPT_FLAGS -O)
endif()

set(SPV_OUTPUTS "")

# Ray tracing shaders
set(RAY_SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/raytracing/raygen.rgen"
    "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/raytracing/miss.rmiss"
    "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/raytracing/closesthit.rchit"
    "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/raytracing/anyhit.rahit"
    "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/raytracing/callable.rcall"
)

foreach(SRC ${RAY_SHADERS})
    if(NOT EXISTS "${SRC}")
        message(WARNING "Missing ray tracing shader: ${SRC}")
        continue()
    endif()

    get_filename_component(BASE ${SRC} NAME_WE)
    set(OUT_DIR "${BIN_DIR}/assets/shaders/raytracing")
    set(STAGE "")

    if(BASE STREQUAL "raygen")
        set(STAGE rgen)
    elseif(BASE STREQUAL "miss")
        set(STAGE rmiss)
    elseif(BASE STREQUAL "closesthit")
        set(STAGE rchit)
    elseif(BASE STREQUAL "anyhit")
        set(STAGE rahit)
    elseif(BASE STREQUAL "callable")
        set(STAGE rcall)
    endif()

    if(NOT STAGE)
        message(WARNING "Unknown stage for ${SRC}")
        continue()
    endif()

    set(SPV "${OUT_DIR}/${BASE}.spv")

    add_custom_command(
        OUTPUT "${SPV}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
        COMMAND ${GLSLC} ${SHADER_INCLUDE_FLAGS} ${SHADER_OPT_FLAGS}
                --target-spv=spv1.6 --target-env=vulkan1.4
                -fshader-stage=${STAGE} "${SRC}" -o "${SPV}"
        COMMAND ${SPIRV_VAL} --target-env vulkan1.3 "${SPV}"
        DEPENDS "${SRC}"
        COMMENT "Compiling raytracing/${BASE} → assets/shaders/raytracing/${BASE}.spv"
        VERBATIM
    )
    list(APPEND SPV_OUTPUTS "${SPV}")
endforeach()

# Compute shader: CANVAS.comp
set(CANVAS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/compute/CANVAS.comp")
if(EXISTS "${CANVAS_SRC}")
    set(CANVAS_OUT "${BIN_DIR}/assets/shaders/compute/CANVAS.spv")

    add_custom_command(
        OUTPUT "${CANVAS_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN_DIR}/assets/shaders/compute"
        COMMAND ${GLSLC} ${SHADER_INCLUDE_FLAGS} ${SHADER_OPT_FLAGS}
                --target-spv=spv1.6 --target-env=vulkan1.4
                -fshader-stage=comp "${CANVAS_SRC}" -o "${CANVAS_OUT}"
        COMMAND ${SPIRV_VAL} --target-env vulkan1.4 "${CANVAS_OUT}"
        DEPENDS "${CANVAS_SRC}"
        COMMENT "Compiling CANVAS.comp → assets/shaders/compute/CANVAS.spv"
        VERBATIM
    )
    list(APPEND SPV_OUTPUTS "${CANVAS_OUT}")
else()
    message(WARNING "CANVAS.comp not found at ${CANVAS_SRC} — compute shader skipped")
endif()

add_custom_target(shaders ALL DEPENDS ${SPV_OUTPUTS})

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
    message(STATUS " Vulkan DLL:   Copy vulkan-1.dll from VulkanSDK/x86_64/bin/ to bin/Windows/")
    message(STATUS " FreeType:     100% embedded")
else()
    message(STATUS " Run:          ${BIN_DIR}/Navigator")
endif()
message(STATUS " Shaders:      ${BIN_DIR}/assets/shaders/raytracing/*.spv")
message(STATUS "               ${BIN_DIR}/assets/shaders/compute/CANVAS.spv")
message(STATUS " Static:       YES (SDL + FreeType + most deps)")
message(STATUS " SDL3_image:   PNG:${SDL3IMAGE_PNG} JPG:${SDL3IMAGE_JPG}")
message(STATUS "==================================================================")