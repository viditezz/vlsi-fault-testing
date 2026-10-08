// scoap.hpp — SCOAP combinational controllability (CC0/CC1) and observability (CO).
// PODEM uses these to pick which input to backtrace through and which
// D-frontier gate to propagate through.
#pragma once

#include <cstdint>
#include <vector>

#include "circuit.hpp"

struct Scoap {
    std::vector<int64_t> cc0, cc1, co;
};

Scoap compute_scoap(const Circuit& c);
