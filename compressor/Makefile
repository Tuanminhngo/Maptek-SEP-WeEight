CXX := g++
CXXFLAGS := -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Iinclude

# Mingw-w64 cross toolchains
WIN64XX := x86_64-w64-mingw32-g++
WIN32XX := i686-w64-mingw32-g++

WINFLAGS := -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Iinclude
WINLDFLAGS := -static -static-libstdc++ -static-libgcc

SRC := src/Model.cpp src/IO.cpp src/Strategy.cpp
BUILDSRC := src/main.cpp

# Binaries
BUILDBIN := bin/compressor               # native
WIN64BIN := bin/compressor2-SER.exe
WIN32BIN := bin/compressor1.exe

all: $(BUILDBIN)

$(BUILDBIN): $(SRC) $(BUILDSRC) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(BUILDSRC)

$(WIN64BIN): $(SRC) $(BUILDSRC) | bin
	$(WIN64XX) $(WINFLAGS) -o $@ $(SRC) $(BUILDSRC) $(WINLDFLAGS)

$(WIN32BIN): $(SRC) $(BUILDSRC) | bin
	$(WIN32XX) $(WINFLAGS) -o $@ $(SRC) $(BUILDSRC) $(WINLDFLAGS)

bin:
	mkdir -p bin

clean:
	rm -rf bin
	rm -f tests/output.txt

.PHONY: build-exe build-exe32 run test-file test-strategy clean

# convenience targets
build-exe: $(WIN64BIN)
build-exe32: $(WIN32BIN)

run: $(BUILDBIN)
	cat tests/input.txt | $(BUILDBIN) > tests/output.txt
