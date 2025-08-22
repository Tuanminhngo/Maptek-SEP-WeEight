CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude

SRC := src/Model.cpp src/IO.cpp
TESTBIN_FILE := bin/test_from_file
TESTSRC_FILE := tests/test_from_file.cpp

all: $(TESTBIN_FILE)

$(TESTBIN_FILE): $(SRC) $(TESTSRC_FILE) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(TESTSRC_FILE)

bin:
	mkdir -p bin

clean:
	rm -rf bin

test-file: $(TESTBIN_FILE)
	$(TESTBIN_FILE)
