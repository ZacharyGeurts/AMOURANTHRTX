# Toolchain-emscripten.cmake
# Polished toolchain file for Emscripten/WASM + WebGPU builds
# Compatible with AMOURANTH RTX
# Assuming 90-100% fine. CMake txt needs work for success.

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

set(CMAKE_CROSSCOMPILING TRUE)

# Force Emscripten tools
set(CMAKE_C_COMPILER "${EMSDK}/upstream/emscripten/emcc" CACHE PATH "" FORCE)
set(CMAKE_CXX_COMPILER "${EMSDK}/upstream/emscripten/em++" CACHE PATH "" FORCE)
set(CMAKE_AR "${EMSDK}/upstream/emscripten/emar" CACHE PATH "" FORCE)
set(CMAKE_RANLIB "${EMSDK}/upstream/emscripten/emranlib" CACHE PATH "" FORCE)

# Emscripten sysroot only
set(CMAKE_FIND_ROOT_PATH
    "${EMSDK}/upstream/emscripten/system"
    "${EMSDK}/upstream/emscripten/cache/sysroot"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# =============================================================================
# Emscripten Link Flags (SHELL: protected)
# =============================================================================
add_link_options(
    "SHELL:-s WASM=1"
    "SHELL:-s ENVIRONMENT=web"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s NO_EXIT_RUNTIME=1"
    "SHELL:-s EXPORTED_FUNCTIONS=['_main']"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "SHELL:-s USE_WEBGPU=1"
    "SHELL:-s USE_SDL=3"
    "SHELL:-s USE_SDL_IMAGE=3"
    "SHELL:-s USE_SDL_MIXER=3"
    "SHELL:-s USE_SDL_TTF=3"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
)

# Disable version-script (fixes SDL3_image fatal error)
set(SDL_TARGET_LINK_OPTION_VERSION_FILE OFF CACHE INTERNAL "" FORCE)

# Disable desktop backends (grouped)
foreach(BACKEND X11 X11_XFIXES X11_XCURSOR X11_XINPUT X11_XRANDR X11_XRENDER X11_XSHAPE X11_XSYNC X11_XTEST WAYLAND KMSDRM IBUS DIRECTX WASAPI PULSEAUDIO ALSA JACK SNDIO PIPEWIRE HIDAPI)
    set(SDL_${BACKEND} OFF CACHE INTERNAL "" FORCE)
endforeach()

# Force Emscripten backends
set(SDL_EMSCRIPTEN ON CACHE INTERNAL "" FORCE)
set(SDL_OFFSCREEN ON CACHE INTERNAL "" FORCE)
set(SDL_DUMMYAUDIO ON CACHE INTERNAL "" FORCE)
set(SDL_DUMMYVIDEO ON CACHE INTERNAL "" FORCE)
set(SDL_DUMMYCAMERA ON CACHE INTERNAL "" FORCE)

# Prevent host executable runs during configure
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE INTERNAL "" FORCE)

message(STATUS "Emscripten toolchain loaded")
message(STATUS "  - WebGPU + SDL3 ports enabled")
message(STATUS "  - Desktop backends disabled")
message(STATUS "  - Version-script disabled (SDL3_image fix)")