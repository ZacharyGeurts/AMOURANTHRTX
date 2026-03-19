# Toolchain-emscripten.cmake
# Toolchain file for Emscripten/WASM builds (WebAssembly + WebGPU)
# Fully compatible with AMOURANTH RTX CMakeLists.txt

# The target system is Emscripten (WASM)
set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

# Tell CMake we are cross-compiling
set(CMAKE_CROSSCOMPILING TRUE)

# Emscripten compilers (set by emsdk_env.sh or auto-setup)
# These are already set when using emcmake, but we force them here for safety
if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER "${EMSDK}/upstream/emscripten/emcc" CACHE PATH "" FORCE)
endif()
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${EMSDK}/upstream/emscripten/em++" CACHE PATH "" FORCE)
endif()
if(NOT CMAKE_AR)
    set(CMAKE_AR "${EMSDK}/upstream/emscripten/emar" CACHE PATH "" FORCE)
endif()
if(NOT CMAKE_RANLIB)
    set(CMAKE_RANLIB "${EMSDK}/upstream/emscripten/emranlib" CACHE PATH "" FORCE)
endif()

# Where to search for headers/libraries/programs
set(CMAKE_FIND_ROOT_PATH "${EMSDK}/upstream/emscripten/system" "${EMSDK}/upstream/emscripten/cache/sysroot")

# Adjust FIND_XXX() behavior
# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Emscripten-specific flags (WebGPU + SDL3 ports)
add_link_options(
    -sUSE_WEBGPU=1
    -sUSE_SDL=3
    -sUSE_SDL_MIXER=2
    -sUSE_SDL_TTF=2
    -sWASM=1
    -sALLOW_MEMORY_GROWTH
    -sNO_EXIT_RUNTIME
    -sEXPORTED_FUNCTIONS="['_main']"
    -sEXPORTED_RUNTIME_METHODS="['ccall','cwrap']"
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sENVIRONMENT=web
)

# Disable version-script (wasm-ld does not support it)
set(SDL_TARGET_LINK_OPTION_VERSION_FILE OFF CACHE INTERNAL "" FORCE)

# Disable desktop backends (X11/Wayland/etc.)
set(SDL_X11 OFF CACHE INTERNAL "" FORCE)
set(SDL_X11_XFIXES OFF CACHE INTERNAL "" FORCE)
set(SDL_X11_XCURSOR OFF CACHE INTERNAL "" FORCE)
set(SDL_X11_XINPUT OFF CACHE INTERNAL "" FORCE)
set(SDL_X11_XRANDR OFF CACHE INTERNAL "" FORCE)
set(SDL_X11_XRENDER OFF CACHE INTERNAL "" FORCE)
set(SDL_X11_XSHAPE OFF CACHE INTERNAL "" FORCE)
set(SDL_X11_XSYNC OFF CACHE INTERNAL "" FORCE)
set(SDL_X11_XTEST OFF CACHE INTERNAL "" FORCE)
set(SDL_WAYLAND OFF CACHE INTERNAL "" FORCE)
set(SDL_KMSDRM OFF CACHE INTERNAL "" FORCE)
set(SDL_IBUS OFF CACHE INTERNAL "" FORCE)
set(SDL_DIRECTX OFF CACHE INTERNAL "" FORCE)
set(SDL_WASAPI OFF CACHE INTERNAL "" FORCE)
set(SDL_PULSEAUDIO OFF CACHE INTERNAL "" FORCE)
set(SDL_ALSA OFF CACHE INTERNAL "" FORCE)
set(SDL_JACK OFF CACHE INTERNAL "" FORCE)
set(SDL_SNDIO OFF CACHE INTERNAL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE INTERNAL "" FORCE)
set(SDL_HIDAPI OFF CACHE INTERNAL "" FORCE)

# Force Emscripten backends
set(SDL_EMSCRIPTEN ON CACHE INTERNAL "" FORCE)
set(SDL_OFFSCREEN ON CACHE INTERNAL "" FORCE)
set(SDL_DUMMYAUDIO ON CACHE INTERNAL "" FORCE)
set(SDL_DUMMYVIDEO ON CACHE INTERNAL "" FORCE)
set(SDL_DUMMYCAMERA ON CACHE INTERNAL "" FORCE)

# Make sure CMake doesn’t try host tools
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

message(STATUS "Emscripten toolchain loaded — targeting WebAssembly + WebGPU")