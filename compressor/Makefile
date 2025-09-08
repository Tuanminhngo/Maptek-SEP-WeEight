CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude
LDFLAGS :=

WINXX := x86_64-w64-mingw32-g++
WINXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude
WINLDFLAGS := -static -static-libstdc++ -static-libgcc

# Core sources (now include Worker + App)
SRC := src/Model.cpp src/IO.cpp src/Strategy.cpp src/Worker.cpp src/App.cpp

# Tests
TESTBIN_FILE := bin/test_from_file
TESTSRC_FILE := tests/test_from_file.cpp

TESTBIN_STRAT := bin/test_strategy
TESTSRC_STRAT := tests/test_strategy.cpp

# Main binaries
BUILDBIN := bin/compressor
MAINSRC  := src/main.cpp

BUILDEXEMAC := bin/compressor1.exe
BUILDEXE    := bin/compressor2.exe

.PHONY: all clean test-file test-strategy run build-exe build-exe-mac

all: $(TESTBIN_FILE) $(TESTBIN_STRAT) $(BUILDBIN)

$(TESTBIN_FILE): $(SRC) $(TESTSRC_FILE) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(TESTSRC_FILE) $(LDFLAGS)

$(TESTBIN_STRAT): $(SRC) $(TESTSRC_STRAT) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(TESTSRC_STRAT) $(LDFLAGS)

$(BUILDBIN): $(SRC) $(MAINSRC) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(MAINSRC) $(LDFLAGS)

$(BUILDEXEMAC): $(SRC) $(MAINSRC) | bin
	$(WINXX) $(WINXXFLAGS) $(SRC) $(MAINSRC) $(WINLDFLAGS) -o $@

$(BUILDEXE): $(SRC) $(MAINSRC) | bin
	$(CXX) $(CXXFLAGS) $(SRC) $(MAINSRC) $(WINLDFLAGS) -o $@

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
