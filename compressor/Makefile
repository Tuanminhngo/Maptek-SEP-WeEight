CXX := g++
# Enable higher optimization and NDEBUG in release-like builds
CXXFLAGS := -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Iinclude

WINXX := x86_64-w64-mingw32-g++
WINXXFLAGS := -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Iinclude
WINLDFLAGS := -static -static-libstdc++ -static-libgcc

SRC := src/Model.cpp src/IO.cpp src/Strategy.cpp

TESTBIN_FILE := bin/test_from_file
TESTSRC_FILE := tests/test_from_file.cpp

TESTBIN_STRAT := bin/test_strategy
TESTSRC_STRAT := tests/test_strategy.cpp

BUILDBIN := bin/compressor
BUILDSRC := src/main.cpp

BUILDEXEMAC := bin/compressor-mac.exe
BUILDEXE := bin/compressor-win.exe

all: $(TESTBIN_FILE) $(TESTBIN_STRAT)

$(TESTBIN_FILE): $(SRC) $(TESTSRC_FILE) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(TESTSRC_FILE)

$(TESTBIN_STRAT): $(SRC) $(TESTSRC_STRAT) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(TESTSRC_STRAT)

$(BUILDBIN): $(SRC) $(BUILDSRC) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(BUILDSRC)

$(BUILDEXEMAC): $(SRC) $(BUILDSRC) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(BUILDSRC)

$(BUILDEXE): $(SRC) $(BUILDSRC) | bin
	$(WINXX) $(WINXXFLAGS) -o $@ $(SRC) $(BUILDSRC) $(WINLDFLAGS)

bin:
	mkdir -p bin

clean:
	rm -rf bin
	rm -f tests/output.txt

test-file: $(TESTBIN_FILE)
	$(TESTBIN_FILE)

test-strategy: $(TESTBIN_STRAT)
	$(TESTBIN_STRAT)

run: $(BUILDBIN) 
	cat tests/input.txt | $(BUILDBIN) > tests/output.txt

build-exe: $(BUILDEXE)

build-exe-mac: $(BUILDEXEMAC)
	
