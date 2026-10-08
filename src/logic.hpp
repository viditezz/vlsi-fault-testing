// logic.hpp — gate evaluation in three domains:
//   * 64-bit words  (bit j = pattern j)    used by the logic and fault simulators
//   * 3-valued 0/1/X                        used by PODEM
// plus the fault-free logic simulators built on them.
#pragma once

#include <cstdint>
#include <vector>

#include "circuit.hpp"

// ---------------------------------------------------------------------------
// 64-pattern parallel evaluation. `in(k)` returns the word on pin k.
// ---------------------------------------------------------------------------
template <class In>
inline uint64_t eval_word(GateType t, int n, In in)
{
    uint64_t r;
    switch (t) {
        case GateType::AND:
        case GateType::NAND:
            r = ~0ULL;
            for (int k = 0; k < n; ++k) r &= in(k);
            return t == GateType::AND ? r : ~r;
        case GateType::OR:
        case GateType::NOR:
            r = 0;
            for (int k = 0; k < n; ++k) r |= in(k);
            return t == GateType::OR ? r : ~r;
        case GateType::XOR:
        case GateType::XNOR:
            r = 0;
            for (int k = 0; k < n; ++k) r ^= in(k);
            return t == GateType::XOR ? r : ~r;
        case GateType::NOT: return ~in(0);
        case GateType::BUF: return in(0);
        case GateType::INPUT: break;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 3-valued evaluation: values are L0, L1, LX.
// ---------------------------------------------------------------------------
constexpr uint8_t L0 = 0, L1 = 1, LX = 2;

inline uint8_t inv3(uint8_t v) { return v == LX ? LX : static_cast<uint8_t>(1 - v); }

template <class In>
inline uint8_t eval3(GateType t, int n, In in)
{
    switch (t) {
        case GateType::AND:
        case GateType::NAND: {
            uint8_t r = L1;
            for (int k = 0; k < n; ++k) {
                uint8_t v = in(k);
                if (v == L0) { r = L0; break; }
                if (v == LX) r = LX;
            }
            return t == GateType::AND ? r : inv3(r);
        }
        case GateType::OR:
        case GateType::NOR: {
            uint8_t r = L0;
            for (int k = 0; k < n; ++k) {
                uint8_t v = in(k);
                if (v == L1) { r = L1; break; }
                if (v == LX) r = LX;
            }
            return t == GateType::OR ? r : inv3(r);
        }
        case GateType::XOR:
        case GateType::XNOR: {
            uint8_t r = L0;
            for (int k = 0; k < n; ++k) {
                uint8_t v = in(k);
                if (v == LX) return LX;
                r ^= v;
            }
            return t == GateType::XOR ? r : inv3(r);
        }
        case GateType::NOT: return inv3(in(0));
        case GateType::BUF: return in(0);
        case GateType::INPUT: break;
    }
    return LX;
}

// Fault-free simulation of up to 64 patterns at once.
// pi_words[i] holds primary input i (circuit.pis order) for all patterns.
// vals is resized to num_signals and filled with every signal's word.
void simulate_words(const Circuit& c, const std::vector<uint64_t>& pi_words,
                    std::vector<uint64_t>& vals);

// Plain scalar simulation of one 0/1 vector (circuit.pis order).
// Returns every signal's value. Deliberately written independently of the
// word simulator so the self-tests can cross-check the two.
std::vector<uint8_t> simulate_scalar(const Circuit& c, const std::vector<uint8_t>& pi_vals);

// Primary output values from a full value vector.
std::vector<uint8_t> po_values(const Circuit& c, const std::vector<uint8_t>& vals);
