// circuit.hpp — gate-level netlist representation and .bench parser.
//
// Every gate has exactly one output, and in .bench format that output *is*
// the named signal ("10 = NAND(1, 3)" defines signal 10). So a gate and its
// output signal are the same entity here: one Signal per net. Primary inputs
// are Signals with type INPUT and no fanins.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum class GateType : uint8_t { INPUT, AND, OR, NAND, NOR, NOT, BUF, XOR, XNOR };

struct Signal {
    std::string name;
    GateType type = GateType::INPUT;
    std::vector<int> fanins;    // driving signals, in pin order
    std::vector<int> fanouts;   // gates that read this signal (deduplicated)
    int level = 0;              // PIs are level 0; gate = 1 + max fanin level
    bool is_gate = false;
    bool is_pi = false;
    bool is_po = false;
};

struct Circuit {
    std::string name;
    std::vector<Signal> sig;                     // indexed by internal id
    std::unordered_map<std::string, int> index;  // .bench name -> internal id
    std::vector<int> pis;                        // in declaration order
    std::vector<int> pos;                        // in declaration order
    std::vector<int> order;                      // gates in topological order
    int max_level = 0;

    int num_signals() const { return static_cast<int>(sig.size()); }
    int num_gates() const { return static_cast<int>(order.size()); }
};

// Parses a combinational .bench file, builds fanouts and levelizes it.
// Throws std::runtime_error on malformed input, undefined signals, sequential
// elements, or combinational cycles.
Circuit parse_bench(const std::string& path);

// Same, from text already in memory (used by the self-tests).
Circuit parse_bench_text(const std::string& text, const std::string& name);

const char* gate_name(GateType t);
bool is_inverting(GateType t);
