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

# ISCAS-89 (full scan), held out: none of the settings were tuned on these circuits
sweep89: $(BIN)
	./$(BIN) sweep benchmarks results_iscas89 --circuits s1423,s5378,s9234,s13207,s15850,s38417
	python3 scripts/plot_results.py results_iscas89

# ITC'99 (full scan), held out; several hours, most of it b15 and b17
sweep99: $(BIN)
	scripts/sweep_itc99.sh

# SAT cost, oracle per cost measure, circuit-level intervals, and sensitivity to the SAT weight c
costs:
	for d in results results_iscas89 results_itc99; do python3 scripts/cost_breakdown.py $$d; done
	python3 scripts/c_sensitivity.py results results_iscas89 results_itc99

clean:
	rm -rf build $(BIN)

.PHONY: all test sweep sweep89 sweep99 costs clean
