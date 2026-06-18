cmake_minimum_required(VERSION 3.21)
project(AMOURANTH_RTX LANGUAGES C CXX)

# =============================================================================
# PRODUCTION + DEBUG TOGGLES
# =============================================================================
option(BUILD_SHARED_LIBS                "Build shared libs (OFF = fully static)" OFF)
option(DISABLE_TRACE_AND_DEBUG_LOGS    "Disable trace & debug logging" ON)
option(ENABLE_EXTRA_WARNINGS           "Enable extra warnings" ON)

option(ENABLE_VALIDATION               "Enable Vulkan Validation Layers + Debug Utils" ON)
option(ENABLE_VERBOSE_VALIDATION       "Enable extra verbose validation output" ON)

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

option(BUILD_FOR_WEB    "Build for Emscripten/WebAssembly + WebGPU" OFF)
option(FIELD_DOS_EMBED_HD "Embed RTX-DOS 32MiB boot slice in engine binary" ON)


# =============================================================================
# FETCHCONTENT SETUP
# =============================================================================
include(FetchContent)
find_package(Git REQUIRED)

set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# =============================================================================
# PLATFORM DETECTION
# =============================================================================
if(BUILD_FOR_WEB)
    set(TARGET_PLATFORM "Web")
    set(EXE_SUFFIX ".html")
    set(BIN_DIR "${CMAKE_BINARY_DIR}/bin/Web")
    message(STATUS "=== WEB BUILD MODE — Output locked to build-web/bin/Web/AMOURANTHRTX.html ===")
    add_definitions(-DUSE_WEBGPU)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Toolchain-emscripten.cmake" CACHE PATH "" FORCE)
else()
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
endif()

set(CMAKE_CXX_COMPILER ${GCC14_BIN})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "Target: ${TARGET_PLATFORM} | Output: ${BIN_DIR}")

# =============================================================================
# COMPILER FLAGS
# =============================================================================
# Strict on logic bugs; skip -Wshadow/-Wconversion on header-heavy TU (Pipeline/RayCanvas).
set(OUR_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wunused)
if(ENABLE_EXTRA_WARNINGS)
    list(APPEND OUR_WARNING_FLAGS -Wformat=2)
endif()

set(COMMON_OPT_FLAGS -O3 -ffast-math -g)

if(TARGET_PLATFORM STREQUAL "Linux" AND NOT EMSCRIPTEN)
    find_program(MOLD_EXECUTABLE mold)
    if(MOLD_EXECUTABLE)
        add_link_options(-fuse-ld=mold)
    endif()
endif()

if(TARGET_PLATFORM STREQUAL "Windows")
    add_compile_options(-fstack-protector-strong -fcf-protection=full -D_FORTIFY_SOURCE=3 -Wa,-mbig-obj)
    add_link_options(-march=native -mtune=native -static -static-libgcc -static-libstdc++ -Wl,--whole-archive -lwinpthread -lgcc -lgcc_eh -Wl,--no-whole-archive)
endif()

if(DISABLE_TRACE_AND_DEBUG_LOGS)
    add_definitions(-DDISABLE_TRACE_AND_DEBUG_LOGS)
endif()

# =============================================================================
# ASSETS
# =============================================================================
set(DEVELOPER_DIRS assets/fonts assets/textures assets/models assets/materials assets/scenes assets/audio assets/scripts)
foreach(D ${DEVELOPER_DIRS})
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${D}")
endforeach()

set(ASSETS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_DST "${BIN_DIR}/assets")

# SG subsystem layout — AmmoOS (windowing + core), DOS, data (registry/icons/journals)
set(SG_AMMOOS_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/SG/AmmoOS")
set(SG_AMMOOS_CORE     "${SG_AMMOOS_DIR}/core")
set(SG_AMMOOS_WINDOWING "${SG_AMMOOS_DIR}/windowing")
set(SG_DOS_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/SG/dos")
set(SG_DATA_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/SG/data")
set(SG_INCLUDE_DIRS    ${SG_AMMOOS_DIR} ${SG_AMMOOS_CORE} ${SG_AMMOOS_WINDOWING} ${SG_DOS_DIR} ${SG_DATA_DIR})
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

if(NOT EMSCRIPTEN)
    find_package(Vulkan REQUIRED)
    if(Vulkan_VERSION VERSION_LESS "1.3.250")
        message(FATAL_ERROR "Vulkan SDK >= 1.3.250 required")
    endif()
    find_package(OpenMP REQUIRED)
    find_program(GLSLC glslc REQUIRED)
    find_program(SPIRV_VAL spirv-val REQUIRED)
else()
    message(STATUS "Web build: Using WebGPU instead of Vulkan")
    add_definitions(-DUSE_WEBGPU)
endif()

# FreeType - 100% static on Windows
if(TARGET_PLATFORM STREQUAL "Windows")
    message(STATUS "FreeType: Vendoring static build - 100% embedded in .exe")
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

# SDL3 + extensions
if(FETCH_SDL3)
    if(TARGET_PLATFORM STREQUAL "Windows")
        set(SDL_HIDAPI_LIBUSB_SHARED OFF CACHE INTERNAL "" FORCE)
        set(SDL_LIBUSB_SHARED OFF CACHE INTERNAL "" FORCE)
    endif()
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG main GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

if(FETCH_SDL3_IMAGE)
    if(EMSCRIPTEN)
        set(SDL_TARGET_LINK_OPTION_VERSION_FILE OFF CACHE INTERNAL "" FORCE)
    endif()
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
    if(EMSCRIPTEN)
        set(SDL_TARGET_LINK_OPTION_VERSION_FILE OFF CACHE INTERNAL "" FORCE)
    endif()
    FetchContent_Declare(SDL3_mixer GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git GIT_TAG main GIT_PROGRESS TRUE)
    set(SDL3MIXER_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL3MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_mixer)
endif()

if(FETCH_SDL3_TTF)
    if(EMSCRIPTEN)
        set(SDL_TARGET_LINK_OPTION_VERSION_FILE OFF CACHE INTERNAL "" FORCE)
    endif()
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
# FieldX86Core — owned x86 decode/exec (RM/PM32 + FPU/MMX), Field Die host CPU
# =============================================================================
set(FIELD_X86_CORE_DIR "${CMAKE_SOURCE_DIR}/Navigator/engine/FieldX86Core")
set(FIELD_X86_CORE_SRCS
    ${FIELD_X86_CORE_DIR}/src/api.c
    ${FIELD_X86_CORE_DIR}/src/decode.c
    ${FIELD_X86_CORE_DIR}/src/fpu.c
    ${FIELD_X86_CORE_DIR}/src/mem.c
    ${FIELD_X86_CORE_DIR}/src/ops.c
    ${FIELD_X86_CORE_DIR}/src/ops2.c
    ${FIELD_X86_CORE_DIR}/src/prim_ops.c
)

add_library(field_x86 STATIC ${FIELD_X86_CORE_SRCS})
add_library(x86emu ALIAS field_x86)
target_include_directories(field_x86 PUBLIC ${FIELD_X86_CORE_DIR}/include)
target_include_directories(field_x86 PRIVATE ${FIELD_X86_CORE_DIR})
target_compile_definitions(field_x86 PUBLIC FIELD_X86_CORE=1)
target_link_libraries(field_x86 PUBLIC m)
set_source_files_properties(${FIELD_X86_CORE_SRCS} PROPERTIES COMPILE_OPTIONS "-w;-O2")
set(LIBX86EMU_DIR "${FIELD_X86_CORE_DIR}")



# =============================================================================
# SOURCES + EXECUTABLE
# =============================================================================
set(OUR_SOURCES Navigator/main.cpp
    Navigator/engine/FieldWebCore.cpp Navigator/engine/FieldBrowserHook.cpp)
set(ALL_SOURCES ${OUR_SOURCES} ${TINYOBJLOADER_SRC})

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(DOS_HD_IMAGE "${CMAKE_SOURCE_DIR}/assets/dos/rtx_dos_hd.img")
set(DOS_SLICE_BIN "${CMAKE_BINARY_DIR}/dos_slice.bin")
set(DOS_EMBED_OBJ "${CMAKE_BINARY_DIR}/dos_slice_embed.o")
if(FIELD_DOS_EMBED_HD AND NOT BUILD_FOR_WEB)
    find_program(CMAKE_OBJCOPY objcopy REQUIRED)
    add_custom_command(
        OUTPUT "${DOS_HD_IMAGE}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/install_dos.py"
        DEPENDS "${CMAKE_SOURCE_DIR}/scripts/install_dos.py"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Build RTX-DOS HD image (rtx_dos_hd.img) if missing"
        VERBATIM)
    add_custom_command(
        OUTPUT "${DOS_SLICE_BIN}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/pack_dos_embed.py"
                "${DOS_HD_IMAGE}" "${DOS_SLICE_BIN}"
        DEPENDS "${CMAKE_SOURCE_DIR}/scripts/pack_dos_embed.py"
                "${DOS_HD_IMAGE}"
        COMMENT "Pack RTX-DOS embed slice (32 MiB)"
        VERBATIM)
    message(STATUS "DOS embed: rtx_dos_hd.img auto-built on first compile (MS-DOS MIT, not in git)")
    get_filename_component(DOS_SLICE_DIR "${DOS_SLICE_BIN}" DIRECTORY)
    get_filename_component(DOS_SLICE_NAME "${DOS_SLICE_BIN}" NAME)
    add_custom_command(
        OUTPUT "${DOS_EMBED_OBJ}"
        COMMAND ${CMAKE_OBJCOPY} -I binary -O elf64-x86-64 -B i386
                "${DOS_SLICE_NAME}" "${DOS_EMBED_OBJ}"
        WORKING_DIRECTORY "${DOS_SLICE_DIR}"
        DEPENDS "${DOS_SLICE_BIN}"
        COMMENT "Link dos_slice.bin as rodata"
        VERBATIM)
    list(APPEND ALL_SOURCES
        "${SG_DOS_DIR}/FieldDosEmbed.cpp"
        "${DOS_EMBED_OBJ}")
endif()

function(field_embed_dos target)
    if(NOT FIELD_DOS_EMBED_HD OR BUILD_FOR_WEB)
        return()
    endif()
    target_sources(${target} PRIVATE
        "${SG_DOS_DIR}/FieldDosEmbed.cpp"
        "${DOS_EMBED_OBJ}")
    target_compile_definitions(${target} PRIVATE
        FIELD_DOS_EMBED_HD=1 FIELD_DOS_EMBED_LINKED=1)
endfunction()

add_executable(amouranth_engine ${ALL_SOURCES})

if(FIELD_DOS_EMBED_HD AND NOT BUILD_FOR_WEB)
    target_compile_definitions(amouranth_engine PRIVATE FIELD_DOS_EMBED_HD=1 FIELD_DOS_EMBED_LINKED=1)
endif()

set_target_properties(amouranth_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
    OUTPUT_NAME "AMOURANTHRTX"
    SUFFIX "${EXE_SUFFIX}"
)

if(TARGET_PLATFORM STREQUAL "Windows")
    target_link_options(amouranth_engine PRIVATE -mconsole)
elseif(TARGET_PLATFORM STREQUAL "Linux")
    target_link_options(amouranth_engine PRIVATE "LINKER:-z,noexecstack")
endif()

# =============================================================================
# VULKAN VALIDATION LAYERS
# =============================================================================
if(ENABLE_VALIDATION AND NOT EMSCRIPTEN)
    message(STATUS "🔧 Vulkan Validation Layers: ENABLED")
    target_compile_definitions(amouranth_engine PRIVATE ENABLE_VULKAN_VALIDATION)
    
    if(ENABLE_VERBOSE_VALIDATION)
        message(STATUS "   → Verbose validation enabled")
        target_compile_definitions(amouranth_engine PRIVATE VULKAN_VALIDATION_VERBOSE)
    endif()
else()
    message(STATUS "Vulkan Validation Layers: DISABLED (production mode)")
endif()

# =============================================================================
# COMPILER FLAGS + INCLUDES
# =============================================================================
foreach(SRC ${OUR_SOURCES})
    set_source_files_properties(${SRC} PROPERTIES COMPILE_OPTIONS "${OUR_WARNING_FLAGS};${COMMON_OPT_FLAGS}")
endforeach()

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

target_include_directories(amouranth_engine PRIVATE Navigator/engine ${SG_INCLUDE_DIRS})
target_include_directories(amouranth_engine SYSTEM BEFORE PRIVATE ${LIBX86EMU_DIR}/include)

target_compile_definitions(amouranth_engine PRIVATE
    GLM_ENABLE_EXPERIMENTAL 
    GLM_FORCE_RADIANS 
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    HAVE_SDL3_MIXER 
    HAVE_SDL3_IMAGE 
    HAVE_SDL3_TTF
)

if(DISABLE_TRACE_AND_DEBUG_LOGS)
    target_compile_definitions(amouranth_engine PRIVATE DISABLE_TRACE_AND_DEBUG_LOGS)
endif()

# =============================================================================
# LINKING
# =============================================================================
find_package(ZLIB REQUIRED)

if(TARGET_PLATFORM STREQUAL "Linux" AND NOT EMSCRIPTEN)
    pkg_check_modules(LIBCURL libcurl)
endif()

if(TARGET_PLATFORM STREQUAL "Windows")
    target_link_libraries(amouranth_engine PRIVATE
        -Wl,--whole-archive
        $<TARGET_FILE:${FREETYPE_TARGET}>
        -Wl,--no-whole-archive
    )
else()
    target_link_libraries(amouranth_engine PRIVATE ${FREETYPE_TARGET})
endif()

target_link_libraries(amouranth_engine PRIVATE
    Vulkan::Vulkan
    OpenMP::OpenMP_CXX
    x86emu
    ZLIB::ZLIB
)


if(LIBCURL_FOUND)
    target_compile_definitions(amouranth_engine PRIVATE HAVE_LIBCURL=1)
    target_include_directories(amouranth_engine SYSTEM PRIVATE ${LIBCURL_INCLUDE_DIRS})
    target_link_libraries(amouranth_engine PRIVATE ${LIBCURL_LIBRARIES})
endif()

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
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(FLUIDSYNTH QUIET fluidsynth)
    endif()
    if(NOT FLUIDSYNTH_FOUND)
        find_library(FLUIDSYNTH_LIB fluidsynth)
    endif()
    if(FLUIDSYNTH_FOUND)
        target_include_directories(amouranth_engine PRIVATE ${FLUIDSYNTH_INCLUDE_DIRS})
        target_link_libraries(amouranth_engine PRIVATE ${FLUIDSYNTH_LIBRARIES})
    elseif(FLUIDSYNTH_LIB)
        target_link_libraries(amouranth_engine PRIVATE ${FLUIDSYNTH_LIB})
    endif()
    target_link_libraries(amouranth_engine PRIVATE dl m pthread)
elseif(TARGET_PLATFORM STREQUAL "Windows")
    target_link_libraries(amouranth_engine PRIVATE
        kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid setupapi cfgmgr32 dbghelp
    )
endif()

# =============================================================================
# SHADERS
# =============================================================================
if(GLSLC AND SPIRV_VAL)
    set(SHADER_COMPUTE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/compute")
    set(SYNC_CANVAS_SCRIPT "${SHADER_COMPUTE_DIR}/sync_canvas_shaders.py")
    set(SYNC_CANVAS_STAMP "${CMAKE_CURRENT_BINARY_DIR}/sync_canvas_shaders.stamp")

    add_custom_command(
        OUTPUT "${SYNC_CANVAS_STAMP}"
        COMMAND python3 "${SYNC_CANVAS_SCRIPT}"
        COMMAND ${CMAKE_COMMAND} -E touch "${SYNC_CANVAS_STAMP}"
        DEPENDS
            "${SHADER_COMPUTE_DIR}/x86.comp"
            "${SHADER_COMPUTE_DIR}/patch_canvas_aos.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/sync_dos_viewport_shaders.py"
            "${SYNC_CANVAS_SCRIPT}"
        COMMENT "Syncing x86 comp compiler (CANVAS + DOS viewport)"
        VERBATIM
    )
    add_custom_target(sync_canvas_shaders DEPENDS "${SYNC_CANVAS_STAMP}")

    set(SHADER_INCLUDE_FLAGS
        -I"${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/raytracing"
        -I"${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/compute"
    )

    if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
        set(SHADER_OPT_FLAGS -g -O0)
    else()
        set(SHADER_OPT_FLAGS -O)
    endif()

    set(SPV_OUTPUTS "")

    file(GLOB RAY_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/raytracing/*")
    foreach(src ${RAY_SHADERS})
        get_filename_component(fullname "${src}" NAME)
        get_filename_component(base "${src}" NAME_WE)
        set(out "${BIN_DIR}/assets/shaders/raytracing/${base}.spv")

        add_custom_command(
            OUTPUT "${out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN_DIR}/assets/shaders/raytracing"
            COMMAND ${GLSLC} ${SHADER_INCLUDE_FLAGS} ${SHADER_OPT_FLAGS}
                    --target-spv=spv1.6 --target-env=vulkan1.4
                    "${src}" -o "${out}"
            COMMAND ${SPIRV_VAL} --target-env vulkan1.4 "${out}"
            DEPENDS "${src}"
            COMMENT "Compiling ${fullname}"
            VERBATIM
        )
        list(APPEND SPV_OUTPUTS "${out}")
    endforeach()

    file(GLOB COMPUTE_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/Navigator/shaders/compute/*.comp")
    foreach(src ${COMPUTE_SHADERS})
        get_filename_component(fullname "${src}" NAME)
        get_filename_component(base "${src}" NAME_WE)
        set(out "${BIN_DIR}/assets/shaders/compute/${base}.spv")

        add_custom_command(
            OUTPUT "${out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN_DIR}/assets/shaders/compute"
            COMMAND ${GLSLC} ${SHADER_INCLUDE_FLAGS} ${SHADER_OPT_FLAGS}
                    --target-spv=spv1.6 --target-env=vulkan1.4
                    -fshader-stage=comp "${src}" -o "${out}"
            COMMAND ${SPIRV_VAL} --target-env vulkan1.4 "${out}"
            DEPENDS "${src}" "${SYNC_CANVAS_STAMP}"
            COMMENT "Compiling ${fullname}"
            VERBATIM
        )
        list(APPEND SPV_OUTPUTS "${out}")
    endforeach()

    add_custom_target(shaders ALL DEPENDS ${SPV_OUTPUTS})
    add_dependencies(shaders sync_canvas_shaders)
else()
    message(WARNING "Skipping shader compilation - missing glslc or spirv-val")
endif()

# =============================================================================
# FINAL TARGET DEPENDENCIES + REPORT
# =============================================================================
add_dependencies(amouranth_engine shaders)
if(TARGET copy_assets)
    add_dependencies(amouranth_engine copy_assets)
endif()

if(NOT EMSCRIPTEN AND NOT CMAKE_CROSSCOMPILING)
    # Host QA binaries pull FieldBios → FieldDevices → FieldMpu401 (FluidSynth).
    function(field_host_test_link target)
        target_sources(${target} PRIVATE
            "${CMAKE_SOURCE_DIR}/Navigator/engine/FieldWebCore.cpp"
            "${CMAKE_SOURCE_DIR}/Navigator/engine/FieldBrowserHook.cpp")
        target_link_libraries(${target} PRIVATE field_x86 m pthread dl ZLIB::ZLIB)
        if(LIBCURL_FOUND)
            target_compile_definitions(${target} PRIVATE HAVE_LIBCURL=1)
            target_include_directories(${target} SYSTEM PRIVATE ${LIBCURL_INCLUDE_DIRS})
            target_link_libraries(${target} PRIVATE ${LIBCURL_LIBRARIES})
        endif()
        if(FLUIDSYNTH_FOUND)
            target_include_directories(${target} PRIVATE ${FLUIDSYNTH_INCLUDE_DIRS})
            target_link_libraries(${target} PRIVATE ${FLUIDSYNTH_LIBRARIES})
        elseif(FLUIDSYNTH_LIB)
            target_link_libraries(${target} PRIVATE ${FLUIDSYNTH_LIB})
        endif()
        if(TARGET SDL3_mixer::SDL3_mixer)
            target_link_libraries(${target} PRIVATE SDL3_mixer::SDL3_mixer)
        endif()
        if(TARGET SDL3::SDL3)
            target_link_libraries(${target} PRIVATE SDL3::SDL3)
        endif()
    endfunction()

    set(FIELD_ENGINE_INCLUDES
        ${CMAKE_SOURCE_DIR}/Navigator/engine
        ${CMAKE_SOURCE_DIR}/Navigator/engine/FieldX86Core/include
        ${SG_INCLUDE_DIRS})

    add_executable(test_libx86emu scripts/test_libx86emu.c)
    target_link_libraries(test_libx86emu PRIVATE x86emu)
    target_include_directories(test_libx86emu PRIVATE ${LIBX86EMU_DIR}/include)
    set_target_properties(test_libx86emu PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}")

    add_executable(qa_dos_int_test scripts/qa_dos_int_test.cpp)
    target_include_directories(qa_dos_int_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_dos_int_test PRIVATE cxx_std_20)
    field_host_test_link(qa_dos_int_test)
    set_target_properties(qa_dos_int_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_doom_host_test scripts/qa_doom_host_test.cpp)
    target_include_directories(qa_doom_host_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_doom_host_test PRIVATE cxx_std_20)
    field_host_test_link(qa_doom_host_test)
    set_target_properties(qa_doom_host_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_toolchain_test scripts/qa_toolchain_test.cpp)
    target_include_directories(qa_toolchain_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_toolchain_test PRIVATE cxx_std_20)
    field_host_test_link(qa_toolchain_test)
    set_target_properties(qa_toolchain_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_cmd_help_test scripts/qa_cmd_help_test.cpp)
    target_include_directories(qa_cmd_help_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_cmd_help_test PRIVATE cxx_std_20)
    field_host_test_link(qa_cmd_help_test)
    set_target_properties(qa_cmd_help_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_dos_embed_test scripts/qa_dos_embed_test.cpp)
    target_include_directories(qa_dos_embed_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_dos_embed_test PRIVATE cxx_std_20)
    field_host_test_link(qa_dos_embed_test)
    field_embed_dos(qa_dos_embed_test)
    set_target_properties(qa_dos_embed_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_doom_build_test scripts/qa_doom_build_test.cpp)
    target_include_directories(qa_doom_build_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_doom_build_test PRIVATE cxx_std_20)
    field_host_test_link(qa_doom_build_test)
    field_embed_dos(qa_doom_build_test)
    set_target_properties(qa_doom_build_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_keen_build_test scripts/qa_keen_build_test.cpp)
    target_include_directories(qa_keen_build_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_keen_build_test PRIVATE cxx_std_20)
    field_host_test_link(qa_keen_build_test)
    field_embed_dos(qa_keen_build_test)
    set_target_properties(qa_keen_build_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_keen_host_test scripts/qa_keen_host_test.cpp)
    target_include_directories(qa_keen_host_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_keen_host_test PRIVATE cxx_std_20)
    field_host_test_link(qa_keen_host_test)
    set_target_properties(qa_keen_host_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(test_lzexe_expand scripts/test_lzexe_expand.cpp)
    target_include_directories(test_lzexe_expand PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(test_lzexe_expand PRIVATE cxx_std_20)
    field_host_test_link(test_lzexe_expand)
    set_target_properties(test_lzexe_expand PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_rtxpm_test scripts/qa_rtxpm_test.cpp)
    target_include_directories(qa_rtxpm_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_rtxpm_test PRIVATE cxx_std_20)
    field_host_test_link(qa_rtxpm_test)
    set_target_properties(qa_rtxpm_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_amouranthos_test scripts/qa_amouranthos_test.cpp)
    target_include_directories(qa_amouranthos_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_amouranthos_test PRIVATE cxx_std_20)
    field_host_test_link(qa_amouranthos_test)
    set_target_properties(qa_amouranthos_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")



    add_executable(qa_audio_test scripts/qa_audio_test.cpp)
    target_include_directories(qa_audio_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_audio_test PRIVATE cxx_std_20)
    field_host_test_link(qa_audio_test)
    set_target_properties(qa_audio_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_ammozip_test scripts/qa_ammozip_test.cpp)
    target_include_directories(qa_ammozip_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_ammozip_test PRIVATE cxx_std_20)
    field_host_test_link(qa_ammozip_test)
    field_embed_dos(qa_ammozip_test)
    set_target_properties(qa_ammozip_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_raid_test scripts/qa_raid_test.cpp)
    target_include_directories(qa_raid_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_raid_test PRIVATE cxx_std_20)
    field_host_test_link(qa_raid_test)
    field_embed_dos(qa_raid_test)
    set_target_properties(qa_raid_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    add_executable(qa_registry_test scripts/qa_registry_test.cpp)
    target_include_directories(qa_registry_test PRIVATE ${FIELD_ENGINE_INCLUDES})
    target_compile_features(qa_registry_test PRIVATE cxx_std_20)
    field_host_test_link(qa_registry_test)
    field_embed_dos(qa_registry_test)
    set_target_properties(qa_registry_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

endif()

message(STATUS "")
message(STATUS "==================================================================")
message(STATUS " AMOURANTHRTX")
message(STATUS " Target:       ${BIN_DIR}/AMOURANTHRTX${EXE_SUFFIX}")
message(STATUS " Validation:   ${ENABLE_VALIDATION}")
if(ENABLE_VALIDATION)
    message(STATUS "   → Run with: VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ${BIN_DIR}/AMOURANTHRTX")
endif()
if(TARGET_PLATFORM STREQUAL "Windows")
    message(STATUS " Run:          wine ${BIN_DIR}/AMOURANTHRTX.exe")
elseif(TARGET_PLATFORM STREQUAL "Web")
    message(STATUS " Run:          ./linux.sh web run")
else()
    message(STATUS " Run:          ${BIN_DIR}/AMOURANTHRTX")
endif()
message(STATUS "==================================================================")