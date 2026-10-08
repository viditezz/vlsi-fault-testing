# Adaptive fault-coverage-driven test generation — build
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
SRC      := $(wildcard src/*.cpp)
OBJ      := $(SRC:src/%.cpp=build/%.o)
BIN      := faultatpg

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp $(wildcard src/*.hpp) | build
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build:
	mkdir -p build

test: $(BIN)
	./$(BIN) selftest benchmarks

sweep: $(BIN)
	./$(BIN) sweep benchmarks results
	python3 scripts/plot_results.py results

clean:
	rm -rf build $(BIN)

.PHONY: all test sweep clean
