# Toolchain-mingw64.cmake
# Cross-compilation toolchain for 64-bit Windows using MinGW-w64 (GCC 14+)
# Fully compatible with your AMOURANTH RTX CMakeLists.txt

# The target system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Tell CMake we are cross-compiling
set(CMAKE_CROSSCOMPILING TRUE)

# Specify the cross compilers (adjust the prefix if you installed a different one)
# Common prefixes on Debian/Ubuntu/Fedora/Arch:
#   x86_64-w64-mingw32-
#   x86_64-w64-mingw32posix-   (POSIX threads, recommended)
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# If you installed the posix version (highly recommended for C++20/23 threads support)
# set(TOOLCHAIN_PREFIX x86_64-w64-mingw32posix)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc   CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++   CACHE STRING "C++ compiler")
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres  CACHE STRING "RC compiler")

# If gcc-14 is not in PATH with the -14 suffix, use plain version and CMake will find it:
# set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
# set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

# Where to search for headers/libraries/programs
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX} /usr/lib/gcc/${TOOLCHAIN_PREFIX}/14*)

# Adjust the default behaviour of the FIND_XXX() commands:
# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Crucial for Vulkan + SDL3 cross-compile
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Windows-specific flags
add_compile_options(-march=x86-64-v3 -mtune=generic)  # safe modern baseline
add_link_options(-static-libgcc -static-libstdc++ -mwindows)

# Make sure CMake does not try to use wine or anything weird
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Optional: silence some warnings that MinGW always emits
add_compile_options(-Wno-unknown-pragmas -Wno-attributes)

message(STATUS "MinGW-w64 cross-toolchain loaded — targeting Windows 64-bit")
