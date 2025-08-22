CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude

SRC := src/Model.cpp src/IO.cpp
TESTBIN := bin/test_model_io
TESTSRC := tests/test_model_io.cpp

all: $(TESTBIN)

$(TESTBIN): $(SRC) $(TESTSRC) | bin
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(TESTSRC)

bin:
	mkdir -p bin

clean:
	rm -rf bin

test: $(TESTBIN)
	$(TESTBIN)

testv: CXXFLAGS += -DTEST_VERBOSE=1
testv: $(TESTBIN)
	$(TESTBIN)