# Makefile for DUST v2 Shower Hearer (SDL2 + OpenGL + UniversalEquation)
# Compile with: make          # Native build (Linux/macOS)
# Windows cross-compile: make windows
# Run with: ./shower_hearer [max_dims]  # Native
# Run with: ./shower_hearer.exe [max_dims]  # Windows (on Windows)
# Clean with: make clean

# Native build settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS =
INCLUDES = -I/usr/include/SDL2  # Adjust if SDL2 is in non-standard path
LIBS = -lSDL2 -lSDL2main -lSDL2_mixer -lGL -lGLU

# Windows cross-compile settings (assumes mingw-w64 and cross-compiled SDL2 installed)
# Install on Ubuntu/Debian: sudo apt install mingw-w64-tools gcc-mingw-w64-x86-64
# For static linking: Use precompiled MinGW libs with pkg-config support
CROSS_CXX = x86_64-w64-mingw32-g++
CROSS_CXXFLAGS = -std=c++17 -Wall -Wextra -O2 $(shell x86_64-w64-mingw32-pkg-config --cflags sdl2 SDL2_mixer) -DM_PI=3.14159265358979323846264338327950288
CROSS_INCLUDES =  # Not needed with pkg-config
CROSS_LDFLAGS = -static -static-libgcc -static-libstdc++ $(shell x86_64-w64-mingw32-pkg-config --static --libs sdl2 SDL2_mixer)
CROSS_LIBS = -lopengl32 -lglu32  # OpenGL/GLU (system DLLs; dynamic)

TARGET = shower_hearer
TARGET_WIN = shower_hearer.exe
SOURCES = shower_hearer.cpp universal_equation.cpp
OBJECTS = $(SOURCES:.cpp=.o)
OBJECTS_WIN = $(SOURCES:.cpp=.wo)
HEADERS = universal_equation.hpp

.PHONY: all clean windows

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LIBS)

windows: $(TARGET_WIN)

$(TARGET_WIN): $(OBJECTS_WIN)
	$(CROSS_CXX) $(OBJECTS_WIN) -o $(TARGET_WIN) $(CROSS_LDFLAGS) $(CROSS_LIBS) -mwindows

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

%.wo: %.cpp $(HEADERS)
	$(CROSS_CXX) $(CROSS_CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(OBJECTS_WIN) $(TARGET) $(TARGET_WIN)