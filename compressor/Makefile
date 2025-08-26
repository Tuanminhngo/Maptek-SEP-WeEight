CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude

SRC := src/Model.cpp src/IO.cpp src/Strategy.cpp

TESTBIN_FILE := bin/test_from_file
TESTSRC_FILE := tests/test_from_file.cpp

TESTBIN_STRAT := bin/test_strategy
TESTSRC_STRAT := tests/test_strategy.cpp

all: $(TESTBIN_FILE) $(TESTBIN_STRAT)

$(TESTBIN_FILE): $(SRC) $(TESTSRC_FILE) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(TESTSRC_FILE)

$(TESTBIN_STRAT): $(SRC) $(TESTSRC_STRAT) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(TESTSRC_STRAT)

bin:
	mkdir -p bin

clean:
	rm -rf bin

test-file: $(TESTBIN_FILE)
	$(TESTBIN_FILE)

test-strategy: $(TESTBIN_STRAT)
	$(TESTBIN_STRAT)
