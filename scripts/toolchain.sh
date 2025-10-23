#!/bin/bash

set -e

# Check for sudo privileges
if [ "$(id -u)" != "0" ]; then
    echo "Error: This script requires sudo privileges. Please run with sudo."
    exit 1
fi

# Clean option to remove all build artifacts
if [ "$1" = "clean" ]; then
    echo "Cleaning all downloaded repositories and toolchain files..."
    sudo rm -rf zlib bzip2 libffi libpng pixman freetype GMP mpfr mpc icu pcre2 glib cairo harfbuzz fmt spdlog vulkan-sdk vulkan-headers vulkan-validationlayers vulkan-memory-allocator SDL SDL_ttf SDL_image SDL_mixer glm libgomp universal_equation amouranth_rtx parallel-hashmap mimalloc vulkan-utility-libraries libiconv doxygen
    rm -f mingw64-toolchain.cmake mingw64-cross.ini
    echo "Cleanup complete. Run the script again without 'clean' to rebuild."
    exit 0
fi

# Setup environment
echo "Performing pre-build cleanup..."
rm -f mingw64-toolchain.cmake mingw64-cross.ini
sudo rm -rf universal_equation/build amouranth_rtx/build

echo "Updating package lists..."
apt update || { echo "Warning: apt update failed, continuing with build..."; true; }

echo "Installing required tools..."
sudo apt install -y ninja-build meson pkg-config mingw-w64 autoconf automake libtool python3 flex bison texinfo wget curl perl vulkan-tools libvulkan-dev git cmake g++ make

echo "Configuring MinGW-w64 for 64-bit..."
sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix

# Define paths
MINGW_PREFIX=/usr/x86_64-w64-mingw32
BASE_DIR=$(pwd)
sudo mkdir -p $MINGW_PREFIX/lib $MINGW_PREFIX/include $MINGW_PREFIX/lib/pkgconfig

# Create MinGW CMake toolchain file
echo "Creating MinGW CMake toolchain file..."
cat << EOF > mingw64-toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH $MINGW_PREFIX)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(ENV{PKG_CONFIG_PATH} "$MINGW_PREFIX/lib/pkgconfig")
EOF

# Create Meson cross-file
echo "Creating Meson cross-file..."
cat << EOF > mingw64-cross.ini
[binaries]
c = 'x86_64-w64-mingw32-gcc'
cpp = 'x86_64-w64-mingw32-g++'
ar = 'x86_64-w64-mingw32-ar'
strip = 'x86_64-w64-mingw32-strip'
windres = 'x86_64-w64-mingw32-windres'
pkg-config = 'pkg-config'
exe_wrapper = 'wine'

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
pkg_config_path = '$MINGW_PREFIX/lib/pkgconfig'
needs_exe_wrapper = true
EOF

# Verify pkg-config setup
echo "Verifying pkg-config setup..."
export PKG_CONFIG_PATH="$MINGW_PREFIX/lib/pkgconfig"
pkg-config --modversion zlib libpng freetype2 harfbuzz glib-2.0 icu-uc fmt spdlog vulkan || echo "Warning: Some dependencies not found, will be built..."

# Autodetect MinGW compilers
MINGW_GCC=$(which x86_64-w64-mingw32-gcc)
MINGW_GXX=$(which x86_64-w64-mingw32-g++)
MINGW_WINDRES=$(which x86_64-w64-mingw32-windres)

if [ -z "$MINGW_GCC" ] || [ -z "$MINGW_GXX" ] || [ -z "$MINGW_WINDRES" ]; then
    echo "Error: MinGW compilers (x86_64-w64-mingw32-gcc, x86_64-w64-mingw32-g++, x86_64-w64-mingw32-windres) not found."
    exit 1
fi

# Build libiconv (dependency for doxygen)
echo "Building libiconv for MinGW..."
if [ ! -d "libiconv" ]; then
    wget https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz
    tar -xzf libiconv-1.17.tar.gz
    mv libiconv-1.17 libiconv
fi
(
    cd libiconv || { echo "Failed to change to libiconv directory"; exit 1; }
    trap 'echo "libiconv build or install failed"; exit 1' ERR
    ./configure --host=x86_64-w64-mingw32 --prefix="$MINGW_PREFIX" \
        --enable-static --disable-shared
    make -j$(nproc)
    sudo make install
    cd ..
)

# Build doxygen for MinGW
echo "Building doxygen for MinGW..."
if [ ! -d "doxygen" ]; then
    git clone https://github.com/doxygen/doxygen.git
fi
(
    cd doxygen || { echo "Failed to change to doxygen directory"; exit 1; }
    trap 'echo "doxygen build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -Wno-dev \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DIconv_LIBRARY="$MINGW_PREFIX/lib/libiconv.a" \
        -DIconv_INCLUDE_DIR="$MINGW_PREFIX/include"
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build zlib
echo "Building zlib for MinGW..."
if [ ! -d "zlib" ]; then
    git clone https://github.com/madler/zlib.git
fi
(
    cd zlib || { echo "Failed to change to zlib directory"; exit 1; }
    trap 'echo "zlib build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX"
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build bzip2
echo "Building BZip2 for MinGW..."
if [ ! -d "bzip2" ]; then
    git clone https://sourceware.org/git/bzip2.git
fi
(
    cd bzip2 || { echo "Failed to change to bzip2 directory"; exit 1; }
    trap 'echo "BZip2 build or install failed"; exit 1' ERR
    make -j$(nproc) CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar RANLIB=x86_64-w64-mingw32-ranlib libbz2.a
    sudo mkdir -p $MINGW_PREFIX/include $MINGW_PREFIX/lib
    sudo cp bzlib.h $MINGW_PREFIX/include/
    sudo cp libbz2.a $MINGW_PREFIX/lib/
    cat << EOF | sudo tee $MINGW_PREFIX/lib/pkgconfig/libbz2.pc
prefix=$MINGW_PREFIX
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: bzip2
Description: bzip2 compression library
Version: 1.0.8
Libs: -L\${libdir} -lbz2
Cflags: -I\${includedir}
EOF
    cd ..
)

# Build libffi
echo "Building libffi for MinGW..."
if [ ! -d "libffi" ]; then
    git clone https://github.com/libffi/libffi.git
fi
(
    cd libffi || { echo "Failed to change to libffi directory"; exit 1; }
    trap 'echo "libffi build or install failed"; exit 1' ERR
    ./autogen.sh
    ./configure --host=x86_64-w64-mingw32 --prefix="$MINGW_PREFIX"
    make -j$(nproc)
    sudo make install
    cd ..
)

# Build libpng
echo "Building libpng for MinGW..."
if [ ! -d "libpng" ]; then
    git clone https://github.com/glennrp/libpng.git
fi
(
    cd libpng || { echo "Failed to change to libpng directory"; exit 1; }
    trap 'echo "libpng build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DZLIB_LIBRARY="$MINGW_PREFIX/lib/libz.a" \
        -DZLIB_INCLUDE_DIR="$MINGW_PREFIX/include" \
        -DPNG_SHARED=OFF \
        -DPNG_STATIC=ON \
        -DPNG_TESTS=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build pixman
echo "Building pixman for MinGW..."
if [ ! -d "pixman" ]; then
    git clone git://anongit.freedesktop.org/git/pixman.git
fi
(
    cd pixman || { echo "Failed to change to pixman directory"; exit 1; }
    trap 'echo "pixman build or install failed"; exit 1' ERR
    git checkout pixman-0.42.2
    autoreconf -fiv
    PKG_CONFIG_PATH="$MINGW_PREFIX/lib/pkgconfig" \
    ./configure --host=x86_64-w64-mingw32 --prefix="$MINGW_PREFIX"
    make -j$(nproc)
    sudo make install
    cd ..
)

# Build FreeType
echo "Building FreeType for MinGW..."
if [ ! -d "freetype" ]; then
    git clone https://github.com/freetype/freetype.git
fi
(
    cd freetype || { echo "Failed to change to freetype directory"; exit 1; }
    trap 'echo "FreeType build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON \
        -DFT_DISABLE_PNG=ON \
        -DZLIB_LIBRARY="$MINGW_PREFIX/lib/libz.a" \
        -DZLIB_INCLUDE_DIR="$MINGW_PREFIX/include"
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build GMP
echo "Building GMP for MinGW..."
if [ ! -d "GMP" ]; then
    git clone https://github.com/alisw/GMP.git
fi
(
    cd GMP || { echo "Failed to change to GMP directory"; exit 1; }
    trap 'echo "GMP build or install failed"; exit 1' ERR
    ./configure --prefix="$MINGW_PREFIX" \
                --enable-static \
                --disable-shared \
                --host=x86_64-w64-mingw32 \
                --enable-cxx
    make -j$(nproc)
    sudo make install
    cd ..
)

# Build MPFR
echo "Building MPFR for MinGW..."
if [ ! -d "mpfr" ]; then
    wget https://www.mpfr.org/mpfr-4.2.1/mpfr-4.2.1.tar.gz
    tar -xzf mpfr-4.2.1.tar.gz
    mv mpfr-4.2.1 mpfr
fi
(
    cd mpfr || { echo "Failed to change to mpfr directory"; exit 1; }
    trap 'echo "MPFR build or install failed"; exit 1' ERR
    autoreconf -fiv
    PKG_CONFIG_PATH="$MINGW_PREFIX/lib/pkgconfig" \
    ./configure --host=x86_64-w64-mingw32 --prefix="$MINGW_PREFIX" \
        --enable-static --disable-shared \
        --with-gmp="$MINGW_PREFIX"
    make -j$(nproc)
    sudo make install
    cd ..
)

# Build MPC
echo "Building MPC for MinGW..."
if [ ! -d "mpc" ]; then
    git clone https://gitlab.inria.fr/mpc/mpc.git
fi
(
    cd mpc || { echo "Failed to change to mpc directory"; exit 1; }
    trap 'echo "MPC build or install failed"; exit 1' ERR
    autoreconf -fiv
    PKG_CONFIG_PATH="$MINGW_PREFIX/lib/pkgconfig" \
    ./configure --host=x86_64-w64-mingw32 --prefix="$MINGW_PREFIX" \
        --enable-static --disable-shared \
        --with-gmp="$MINGW_PREFIX" \
        --with-mpfr="$MINGW_PREFIX"
    make -j$(nproc)
    sudo make install
    cd ..
)

# Build ICU
echo "Building ICU for host (Linux) and MinGW..."
if [ ! -d "icu" ]; then
    git clone https://github.com/unicode-org/icu
fi
(
    cd icu/icu4c/source || { echo "Failed to change to icu/icu4c/source directory"; exit 1; }
    trap 'echo "ICU build or install failed"; exit 1' ERR
    HOST_BUILD_DIR=$(realpath .)/build-host
    mkdir -p build-host && cd build-host
    ../configure
    make -j$(nproc)
    cd ..
    echo "Building ICU for MinGW..."
    mkdir -p build-mingw && cd build-mingw
    PKG_CONFIG_PATH="$MINGW_PREFIX/lib/pkgconfig" \
    ../configure --host=x86_64-w64-mingw32 --prefix="$MINGW_PREFIX" \
        --with-cross-build="$HOST_BUILD_DIR"
    make -j$(nproc)
    sudo make install
    cd ../..
)

# Build PCRE2
echo "Building PCRE2 for MinGW..."
if [ ! -d "pcre2" ]; then
    git clone https://github.com/PCRE2Project/pcre2.git
fi
(
    cd pcre2 || { echo "Failed to change to pcre2 directory"; exit 1; }
    trap 'echo "PCRE2 build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DPCRE2_BUILD_PCRE2_8=ON \
        -DPCRE2_BUILD_PCRE2_16=OFF \
        -DPCRE2_BUILD_PCRE2_32=OFF \
        -DPCRE2_BUILD_TESTS=OFF \
        -DPCRE2_SUPPORT_LIBZ=OFF \
        -DPCRE2_SUPPORT_LIBBZ2=OFF \
        -DPCRE2_SUPPORT_LIBREADLINE=OFF \
        -DPCRE2_SUPPORT_LIBEDITLINE=OFF \
        -DPCRE2_BUILD_PCRE2GREP=ON \
        -DPCRE2_SUPPORT_JIT=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build GLib
echo "Building GLib for MinGW..."
if [ ! -d "glib" ]; then
    git clone https://gitlab.gnome.org/GNOME/glib.git
fi
(
    cd glib || { echo "Failed to change to glib directory"; exit 1; }
    trap 'echo "GLib build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    meson setup --cross-file="$BASE_DIR/mingw64-cross.ini" \
        --prefix="$MINGW_PREFIX" --buildtype=release \
        -Ddefault_library=shared -Dtests=false -Dsysprof=disabled \
        -Dpkg_config_path="$MINGW_PREFIX/lib/pkgconfig" --reconfigure
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build Cairo
echo "Building Cairo for MinGW..."
if [ ! -d "cairo" ]; then
    git clone https://gitlab.freedesktop.org/cairo/cairo.git
fi
(
    cd cairo || { echo "Failed to change to cairo directory"; exit 1; }
    trap 'echo "Cairo build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    meson setup --cross-file="$BASE_DIR/mingw64-cross.ini" \
        --prefix="$MINGW_PREFIX" --buildtype=release \
        -Dfreetype=enabled -Dpng=enabled -Dzlib=enabled \
        -Dxlib=disabled -Dxcb=disabled -Dxlib-xcb=disabled \
        -Dpkg_config_path="$MINGW_PREFIX/lib/pkgconfig" --reconfigure
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build HarfBuzz
echo "Building HarfBuzz for MinGW..."
if [ ! -d "harfbuzz" ]; then
    git clone https://github.com/harfbuzz/harfbuzz.git
fi
(
    cd harfbuzz || { echo "Failed to change to harfbuzz directory"; exit 1; }
    trap 'echo "HarfBuzz build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    meson setup --cross-file="$BASE_DIR/mingw64-cross.ini" \
        --prefix="$MINGW_PREFIX" --buildtype=release \
        -Dfreetype=enabled -Dglib=enabled -Dicu=enabled -Dcairo=enabled \
        -Dpkg_config_path="$MINGW_PREFIX/lib/pkgconfig" --reconfigure
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build fmt
echo "Building fmt for MinGW..."
if [ ! -d "fmt" ]; then
    git clone https://github.com/fmtlib/fmt.git
fi
(
    cd fmt || { echo "Failed to change to fmt directory"; exit 1; }
    trap 'echo "fmt build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -DFMT_DOC=ON \
        -DDOXYGEN_EXECUTABLE="$MINGW_PREFIX/bin/doxygen.exe"
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build spdlog
echo "Building spdlog for MinGW..."
if [ ! -d "spdlog" ]; then
    git clone https://github.com/gabime/spdlog.git
fi
(
    cd spdlog || { echo "Failed to change to spdlog directory"; exit 1; }
    trap 'echo "spdlog build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -DSPDLOG_FMT_EXTERNAL=ON \
        -DFMT_INCLUDE_DIR="$MINGW_PREFIX/include" \
        -DFMT_LIBRARY="$MINGW_PREFIX/lib/libfmt.a" \
        -DSPDLOG_BUILD_EXAMPLES=ON \
        -DSPDLOG_BUILD_TESTS=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Install parallel_hashmap (phmap) for Vulkan Validation Layers
echo "Installing parallel_hashmap for MinGW..."
if [ ! -d "parallel-hashmap" ]; then
    git clone https://github.com/greg7mdp/parallel-hashmap.git parallel-hashmap
fi
(
    cd parallel-hashmap || { echo "Failed to change to parallel-hashmap directory"; exit 1; }
    trap 'echo "parallel_hashmap build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DPHMAP_BUILD_EXAMPLES=OFF \
        -DPHMAP_BUILD_TESTS=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build mimalloc for Vulkan Validation Layers (optional but recommended)
echo "Building mimalloc for MinGW..."
if [ ! -d "mimalloc" ]; then
    git clone https://github.com/microsoft/mimalloc.git mimalloc
fi
(
    cd mimalloc || { echo "Failed to change to mimalloc directory"; exit 1; }
    trap 'echo "mimalloc build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DMI_BUILD_SHARED=ON \
        -DMI_BUILD_STATIC=ON \
        -DMI_BUILD_OBJECT=ON \
        -DMI_BUILD_TESTS=OFF \
        -DMI_OVERRIDE=ON
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build Vulkan Utility Libraries (dependency for Vulkan Validation Layers)
echo "Building Vulkan Utility Libraries for MinGW..."
if [ ! -d "vulkan-utility-libraries" ]; then
    git clone https://github.com/KhronosGroup/Vulkan-Utility-Libraries.git vulkan-utility-libraries
fi
(
    cd vulkan-utility-libraries || { echo "Failed to change to vulkan-utility-libraries directory"; exit 1; }
    trap 'echo "Vulkan Utility Libraries build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DVULKAN_HEADERS_INSTALL_DIR="$MINGW_PREFIX"
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build SPIRV-Headers (dependency for SPIRV-Tools)
echo "Building SPIRV-Headers for MinGW..."
if [ ! -d "spirv-headers" ]; then
    git clone https://github.com/KhronosGroup/SPIRV-Headers.git spirv-headers
fi
(
    cd spirv-headers || { echo "Failed to change to spirv-headers directory"; exit 1; }
    trap 'echo "SPIRV-Headers build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX"
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build SPIRV-Tools (dependency for Vulkan Validation Layers)
echo "Building SPIRV-Tools for MinGW..."
if [ ! -d "spirv-tools" ]; then
    git clone https://github.com/KhronosGroup/SPIRV-Tools.git spirv-tools
fi
(
    cd spirv-tools || { echo "Failed to change to spirv-tools directory"; exit 1; }
    trap 'echo "SPIRV-Tools build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DSPIRV-Headers_SOURCE_DIR="$BASE_DIR/spirv-headers" \
        -DBUILD_SHARED_LIBS=OFF \
        -DSPIRV_SKIP_TESTS=ON \
        -DSPIRV_SKIP_EXECUTABLES=ON \
        -DSPIRV_TOOLS_INSTALL_INCLUDE_DIR=include
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build Vulkan Validation Layers
echo "Building Vulkan Validation Layers for MinGW..."
if [ ! -d "vulkan-validationlayers" ]; then
    git clone https://github.com/KhronosGroup/Vulkan-ValidationLayers.git vulkan-validationlayers
fi
(
    cd vulkan-validationlayers || { echo "Failed to change to vulkan-validationlayers directory"; exit 1; }
    trap 'echo "Vulkan Validation Layers build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    rm -rf CMakeCache.txt CMakeFiles
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DVULKAN_HEADERS_INSTALL_DIR="$MINGW_PREFIX" \
        -Dphmap_DIR="$MINGW_PREFIX/lib/cmake/phmap" \
        -Dmimalloc_DIR="$MINGW_PREFIX/lib/cmake/mimalloc" \
        -DVulkanUtilityLibraries_DIR="$MINGW_PREFIX/lib/cmake/VulkanUtilityLibraries" \
        -DSPIRV-Tools_DIR="$MINGW_PREFIX/lib/cmake/SPIRV-Tools" \
        -DUSE_CUSTOM_HASH_MAP=OFF \
        -DBUILD_WERROR=OFF \
        -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--no-undefined" \
        -DCMAKE_USE_WIN32_THREADS_INIT=1 \
        -DCMAKE_THREAD_LIBS_INIT="-lwinpthread"
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build Vulkan Memory Allocator (optional for RTX memory management)
echo "Building Vulkan Memory Allocator for MinGW..."
if [ ! -d "vulkan-memory-allocator" ]; then
    git clone https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git vulkan-memory-allocator
fi
(
    cd vulkan-memory-allocator || { echo "Failed to change to vulkan-memory-allocator directory"; exit 1; }
    trap 'echo "Vulkan Memory Allocator build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    rm -rf CMakeCache.txt CMakeFiles
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX"
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build Vulkan Loader
echo "Building Vulkan Loader for MinGW..."
if [ ! -d "vulkan-sdk" ]; then
    git clone https://github.com/KhronosGroup/Vulkan-Loader.git vulkan-sdk
fi
(
    cd vulkan-sdk || { echo "Failed to change to vulkan-sdk directory"; exit 1; }
    trap 'echo "Vulkan Loader build or install failed"; exit 1' ERR
    rm -rf build
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DVULKAN_HEADERS_INSTALL_DIR="$MINGW_PREFIX" \
        -DBUILD_TESTS=OFF \
        -DENABLE_WERROR=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build libusb (dependency for SDL3 HIDAPI)
echo "Building libusb for MinGW..."
if [ ! -d "libusb" ]; then
    git clone https://github.com/libusb/libusb.git libusb
fi
(
    cd libusb || { echo "Failed to change to libusb directory"; exit 1; }
    trap 'echo "libusb build or install failed"; exit 1' ERR
    ./autogen.sh
    ./configure --host=x86_64-w64-mingw32 --prefix="$MINGW_PREFIX" \
        --enable-static --disable-shared \
        --disable-udev --disable-tests
    make -j$(nproc)
    sudo make install
    cd ..
)

# Build libjpeg (dependency for SDL3_image)
echo "Building libjpeg for MinGW..."
if [ ! -d "libjpeg-turbo" ]; then
    git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git libjpeg-turbo
fi
(
    cd libjpeg-turbo || { echo "Failed to change to libjpeg-turbo directory"; exit 1; }
    trap 'echo "libjpeg-turbo build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_SHARED=OFF \
        -DENABLE_STATIC=ON \
        -DENABLE_TESTS=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build libmpg123 (dependency for SDL3_mixer MP3 support)
echo "Building libmpg123 for MinGW..."
if [ ! -d "mpg123" ]; then
    git clone https://github.com/madler/mpg123.git mpg123
fi
(
    cd mpg123 || { echo "Failed to change to mpg123 directory"; exit 1; }
    trap 'echo "mpg123 build or install failed"; exit 1' ERR
    # Ensure Autotools are installed
    command -v autoconf >/dev/null 2>&1 || { echo "autoconf not found, installing..."; sudo apt-get install -y autoconf; }
    command -v automake >/dev/null 2>&1 || { echo "automake not found, installing..."; sudo apt-get install -y automake; }
    command -v libtool >/dev/null 2>&1 || { echo "libtool not found, installing..."; sudo apt-get install -y libtool; }
    # Generate configure script if missing
    [ -f configure ] || ./autogen.sh || autoreconf -i
    ./configure --host=x86_64-w64-mingw32 --prefix="$MINGW_PREFIX" \
        --enable-static --disable-shared \
        --disable-lfs-alias \
        --enable-only-layer1 --enable-only-layer2 --enable-only-layer3 \
        --with-default-audio=win32 \
        --with-audio=win32,dummy \
        --disable-alsa --disable-jack --disable-pulse --disable-oss --disable-sndio
    make -j$(nproc)
    sudo make install
    cd ..
)

# Build SDL3
echo "Building SDL3 for Windows..."
if [ ! -d "SDL" ]; then
    git clone https://github.com/libsdl-org/SDL.git
fi
(
    cd SDL || { echo "Failed to change to SDL directory"; exit 1; }
    trap 'echo "SDL3 build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    rm -rf CMakeCache.txt CMakeFiles
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DSDL_VULKAN=ON \
        -DSDL_OPENGL=ON \
        -DSDL_DIRECTX=ON \
        -DSDL_HIDAPI=ON \
        -DSDL_HIDAPI_LIBUSB=ON \
        -DSDL_LIBUSB_LIBRARY="$MINGW_PREFIX/lib/libusb.a" \
        -DSDL_LIBUSB_INCLUDE_DIR="$MINGW_PREFIX/include" \
        -DSDL_STATIC=ON \
        -DSDL_SHARED=OFF \
        -DSDL_TEST=OFF \
        -DSDL_INSTALL=ON
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build SDL3_ttf
echo "Building SDL3_ttf for Windows..."
if [ ! -d "SDL_ttf" ]; then
    git clone https://github.com/libsdl-org/SDL_ttf.git
fi
(
    cd SDL_ttf || { echo "Failed to change to SDL_ttf directory"; exit 1; }
    trap 'echo "SDL3_ttf build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    rm -rf CMakeCache.txt CMakeFiles
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DSDL3TTF_HARFBUZZ=ON \
        -DFREETYPE_INCLUDE_DIRS="$MINGW_PREFIX/include/freetype2" \
        -DFREETYPE_LIBRARY="$MINGW_PREFIX/lib/libfreetype.a" \
        -DHARFBUZZ_INCLUDE_DIR="$MINGW_PREFIX/include/harfbuzz" \
        -DHARFBUZZ_LIBRARY="$MINGW_PREFIX/lib/libharfbuzz.a" \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDL3TTF_TEST=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build SDL3_image
echo "Building SDL3_image for Windows..."
if [ ! -d "SDL_image" ]; then
    git clone https://github.com/libsdl-org/SDL_image.git
fi
(
    cd SDL_image || { echo "Failed to change to SDL_image directory"; exit 1; }
    trap 'echo "SDL3_image build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    rm -rf CMakeCache.txt CMakeFiles
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DSDL3IMAGE_PNG=ON \
        -DSDL3IMAGE_JPG=ON \
        -DSDL3IMAGE_JPG_LIBRARY="$MINGW_PREFIX/lib/libjpeg.a" \
        -DSDL3IMAGE_JPG_INCLUDE_DIR="$MINGW_PREFIX/include" \
        -DSDL3IMAGE_WEBP=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDL3IMAGE_TEST=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build SDL3_mixer
echo "Building SDL3_mixer for Windows..."
if [ ! -d "SDL_mixer" ]; then
    git clone https://github.com/libsdl-org/SDL_mixer.git
fi
(
    cd SDL_mixer || { echo "Failed to change to SDL_mixer directory"; exit 1; }
    trap 'echo "SDL3_mixer build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    rm -rf CMakeCache.txt CMakeFiles
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DSDL3MIXER_MP3=ON \
        -DSDL3MIXER_MP3_LIBRARY="$MINGW_PREFIX/lib/libmpg123.a" \
        -DSDL3MIXER_MP3_INCLUDE_DIR="$MINGW_PREFIX/include" \
        -DSDL3MIXER_OGG=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDL3MIXER_TEST=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Build glm (header-only library)
echo "Building glm for MinGW..."
if [ ! -d "glm" ]; then
    git clone https://github.com/g-truc/glm.git
fi
(
    cd glm || { echo "Failed to change to glm directory"; exit 1; }
    trap 'echo "glm build or install failed"; exit 1' ERR
    rm -rf build
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$MINGW_GCC" \
        -DCMAKE_CXX_COMPILER="$MINGW_GXX" \
        -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -DGLM_TEST_ENABLE=OFF
    ninja -j$(nproc)
    sudo ninja install
    cd ..
)

# Dependency Checks and Installations
echo "Checking required dependencies..."

command -v git >/dev/null 2>&1 || { echo >&2 "git is required but not installed. Aborting."; exit 1; }
command -v gcc >/dev/null 2>&1 || { echo >&2 "gcc is required but not installed. Aborting."; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo >&2 "g++ is required but not installed. Aborting."; exit 1; }
command -v make >/dev/null 2>&1 || { echo >&2 "make is required but not installed. Aborting."; exit 1; }
command -v pkg-config >/dev/null 2>&1 || { echo >&2 "pkg-config is required but not installed. Aborting."; exit 1; }
command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1 || { echo >&2 "MinGW-w64 cross GCC (x86_64-w64-mingw32-gcc) is required but not installed. Aborting."; exit 1; }
command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1 || { echo >&2 "MinGW-w64 cross G++ (x86_64-w64-mingw32-g++) is required but not installed. Aborting."; exit 1; }
command -v sudo >/dev/null 2>&1 || { echo >&2 "sudo is required but not installed. Aborting."; exit 1; }

# Set MINGW_PREFIX
if [ -z "$MINGW_PREFIX" ]; then
    echo "MINGW_PREFIX is not set. Setting to /usr for default MinGW-w64 installation."
    export MINGW_PREFIX=/usr
fi

# Build libgomp
echo "Building libgomp for MinGW (Windows 64-bit)..."
if [ ! -d "libgomp" ]; then
    git clone https://gcc.gnu.org/git/gcc.git libgomp
fi

(
    cd libgomp || { echo "Failed to change to libgomp directory"; exit 1; }
    trap 'echo "libgomp build or install failed"; exit 1' ERR
    git checkout releases/gcc-13
    # Update config.sub and config.guess with correct URLs
    wget -O config.sub http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=HEAD
    wget -O config.guess http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD
    chmod +x config.sub config.guess
    rm -rf build
    mkdir -p build && cd build
    PKG_CONFIG_PATH="${MINGW_PREFIX}/x86_64-w64-mingw32/lib/pkgconfig" \
    CC_FOR_BUILD=gcc CXX_FOR_BUILD=g++ \
    CC_FOR_TARGET=x86_64-w64-mingw32-gcc CXX_FOR_TARGET=x86_64-w64-mingw32-g++ \
    ../configure \
        --build=x86_64-pc-linux-gnu \
        --host=x86_64-pc-linux-gnu \
        --target=x86_64-w64-mingw32 \
        --prefix="$MINGW_PREFIX" \
        --with-sysroot="${MINGW_PREFIX}/x86_64-w64-mingw32" \
        --enable-languages="c c++" \
        --enable-libgomp \
        --disable-multilib \
        --disable-nls
    make -j$(nproc) all-target-libgcc all-target-libgomp
    sudo make install-target-libgcc install-target-libgomp
    cd ../..
)

# Build universal_equation
echo "Building the universal_equation project..."
if [ ! -d "universal_equation" ]; then
    git clone https://github.com/ZacharyGeurts/universal_equation
fi
(
    cd universal_equation || { echo "Failed to change to universal_equation directory"; exit 1; }
    trap 'echo "universal_equation build or install failed"; exit 1' ERR
    mkdir -p build && cd build
    cmake -S .. -B . -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$BASE_DIR/mingw64-toolchain.cmake" \
        -CMAKE_BUILD_TYPE=Release \
        -CMAKE_C_COMPILER="$MINGW_GCC" \
        -CMAKE_CXX_COMPILER="$MINGW_GXX" \
        -CMAKE_RC_COMPILER="$MINGW_WINDRES" \
        -CMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DVULKAN_SDK="$MINGW_PREFIX"
    ninja -j$(nproc)    
    cd ..
)

# Copy required DLLs
echo "Copying required DLLs..."
sudo mkdir -p universal_equation/build/bin
sudo cp $MINGW_PREFIX/bin/*.dll universal_equation/build/bin/
if [ -d "amouranth_rtx" ]; then
    sudo mkdir -p amouranth_rtx/build/bin
    sudo cp $MINGW_PREFIX/bin/*.dll amouranth_rtx/build/bin/
fi

# Verify installations
echo "Verifying installations..."
ls $MINGW_PREFIX/lib/libz.a $MINGW_PREFIX/lib/libbz2.a $MINGW_PREFIX/lib/libffi.a $MINGW_PREFIX/lib/libpng*.dll.a \
   $MINGW_PREFIX/lib/libpixman-1*.dll.a $MINGW_PREFIX/lib/libfreetype.dll.a $MINGW_PREFIX/lib/libgmp.a \
   $MINGW_PREFIX/lib/libmpfr.a $MINGW_PREFIX/lib/libmpc.a $MINGW_PREFIX/lib/libicu*.dll.a \
   $MINGW_PREFIX/lib/libpcre2-8.a $MINGW_PREFIX/lib/libglib-2.0.dll.a $MINGW_PREFIX/lib/libcairo.dll.a \
   $MINGW_PREFIX/lib/libharfbuzz.dll.a $MINGW_PREFIX/lib/libfmt.a $MINGW_PREFIX/lib/libspdlog.a \
   $MINGW_PREFIX/lib/libvulkan*.dll.a $MINGW_PREFIX/lib/libSDL3*.dll.a $MINGW_PREFIX/lib/libSDL3_ttf*.dll.a \
   $MINGW_PREFIX/lib/libSDL3_image*.dll.a $MINGW_PREFIX/lib/libSDL3_mixer*.dll.a $MINGW_PREFIX/lib/libgomp*.dll.a \
   $MINGW_PREFIX/lib/libglm.a $MINGW_PREFIX/lib/libiconv.a || true
ls $MINGW_PREFIX/include/windows.h $MINGW_PREFIX/include/zlib.h $MINGW_PREFIX/include/bzlib.h $MINGW_PREFIX/include/ffi.h \
   $MINGW_PREFIX/include/png.h $MINGW_PREFIX/include/pixman-1/pixman.h $MINGW_PREFIX/include/freetype2/freetype.h \
   $MINGW_PREFIX/include/gmp.h $MINGW_PREFIX/include/mpfr.h $MINGW_PREFIX/include/mpc.h \
   $MINGW_PREFIX/include/unicode/uchar.h $MINGW_PREFIX/include/pcre2.h $MINGW_PREFIX/include/glib-2.0/glib.h \
   $MINGW_PREFIX/include/cairo/cairo-ft.h $MINGW_PREFIX/include/harfbuzz/hb.h $MINGW_PREFIX/include/fmt/core.h \
   $MINGW_PREFIX/include/spdlog/spdlog.h $MINGW_PREFIX/include/vulkan/vulkan.h $MINGW_PREFIX/include/SDL3/SDL.h \
   $MINGW_PREFIX/include/SDL3/SDL_ttf.h $MINGW_PREFIX/include/SDL3/SDL_image.h $MINGW_PREFIX/include/SDL3/SDL_mixer.h \
   $MINGW_PREFIX/include/glm/glm.hpp $MINGW_PREFIX/include/iconv.h || true
ls $MINGW_PREFIX/bin/doxygen.exe || true
ls universal_equation/build/bin/Navigator.exe || true
if [ -d "amouranth_rtx" ]; then
    ls amouranth_rtx/build/bin/*.exe || true
fi

echo "Build complete!"
echo "Windows executable for universal_equation is in universal_equation/build/bin/Navigator.exe"
echo "Test with: wine universal_equation/build/bin/Navigator.exe"
if [ -d "amouranth_rtx" ]; then
    echo "Windows executable for AMOURANTH RTX is in amouranth_rtx/build/bin/"
    echo "Test with: wine amouranth_rtx/build/bin/<executable_name>.exe"
fi