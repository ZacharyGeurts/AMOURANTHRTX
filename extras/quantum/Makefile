# Makefile for UniversalEquation quantum simulation
# Builds quantum_sim (Linux) or quantum_sim.exe (Windows) from main.cpp, universal_equation.cpp, and universal_equation_quantum.cpp
# Requires g++ (Linux) or MinGW-w64 (Windows), OpenMP, GLM libraries, and libatomic
# Usage: make (or make linux), make windows, make run, make clean, make check-deps

# Compilers
CXX_LINUX = g++
CXX_WINDOWS = x86_64-w64-mingw32-g++

# Compiler flags (C++20 for both)
CXXFLAGS = -std=c++20 -Wall -O2 -fopenmp
LDFLAGS_LINUX = -lgomp -latomic
LDFLAGS_WINDOWS = -lgomp -latomic -static
INCLUDE = $(shell pkg-config --cflags glm 2>/dev/null || echo -I/usr/include/glm)
LIBS = $(shell pkg-config --libs glm 2>/dev/null || echo "")

# Targets and sources
TARGET_LINUX = quantum_sim
TARGET_WINDOWS = quantum_sim.exe
SOURCES = main.cpp universal_equation.cpp
OBJECTS_LINUX = $(SOURCES:.cpp=.o)
OBJECTS_WINDOWS = $(SOURCES:.cpp=.o.windows)

# Default target: Linux
all: check-deps linux

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@command -v $(CXX_LINUX) >/dev/null 2>&1 || { echo "Missing g++"; sudo apt-get update && sudo apt-get install -y g++; }
	@command -v $(CXX_WINDOWS) >/dev/null 2>&1 || { echo "Missing mingw-w64"; sudo apt-get update && sudo apt-get install -y mingw-w64; }
	@pkg-config --modversion glm >/dev/null 2>&1 || { echo "Missing libglm-dev"; sudo apt-get update && sudo apt-get install -y libglm-dev; }
	@ldconfig -p | grep libgomp >/dev/null 2>&1 || { echo "Missing libgomp1"; sudo apt-get update && sudo apt-get install -y libgomp1; }
	@ldconfig -p | grep libatomic >/dev/null 2>&1 || { echo "Missing libatomic1"; sudo apt-get update && sudo apt-get install -y libatomic1; }
	@echo "Dependencies check complete."

# Linux build
linux: $(TARGET_LINUX)

$(TARGET_LINUX): $(OBJECTS_LINUX)
	$(CXX_LINUX) $(OBJECTS_LINUX) -o $(TARGET_LINUX) $(LDFLAGS_LINUX) $(LIBS)

%.o: %.cpp
	$(CXX_LINUX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Windows build
windows: check-deps $(TARGET_WINDOWS)

$(TARGET_WINDOWS): $(OBJECTS_WINDOWS)
	$(CXX_WINDOWS) $(OBJECTS_WINDOWS) -o $(TARGET_WINDOWS) $(LDFLAGS_WINDOWS) $(LIBS)

%.o.windows: %.cpp
	$(CXX_WINDOWS) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Clean
clean:
	rm -f $(OBJECTS_LINUX) $(OBJECTS_WINDOWS) $(TARGET_LINUX) $(TARGET_WINDOWS)

# Run (Linux only)
run: $(TARGET_LINUX)
	./$(TARGET_LINUX)

# Run Windows executable with Wine (if installed)
run-windows: $(TARGET_WINDOWS)
	wine ./$(TARGET_WINDOWS)

.PHONY: all linux windows clean run run-windows check-deps