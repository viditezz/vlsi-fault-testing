// faults.hpp — single stuck-at fault model with fanout branches, equivalence
// collapsing, and a serial reference fault simulator.
//
// Fault sites ("lines"):
//   * every signal's stem;
//   * when a signal feeds more than one destination (gate input pins and/or a
//     primary output), each destination is a separate branch line.
// Each line carries a stuck-at-0 and a stuck-at-1 fault. This is the standard
// uncollapsed fault universe; c17 has 17 lines -> 34 faults.
//
// Equivalence collapsing (structural):
//   AND : input SA0 == output SA0      NAND: input SA0 == output SA1
//   OR  : input SA1 == output SA1      NOR : input SA1 == output SA0
//   NOT : input SAv == output SA(!v)   BUF : input SAv == output SAv
//   XOR/XNOR: none
// Collapsed c17 = 22 faults, matching the literature.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "circuit.hpp"

enum class Site : uint8_t {
    STEM,       // the signal itself: every reader sees the stuck value
    BRANCH,     // one gate input pin: only that gate sees the stuck value
    PO_BRANCH,  // the primary-output connection only: only that PO sees it
};

struct Fault {
    int sig = -1;          // signal whose stem / branch is faulty
    int gate = -1;         // BRANCH: the reading gate
    int pin = -1;          // BRANCH: pin index into gate's fanins; PO_BRANCH: index into pos
    Site site = Site::STEM;
    uint8_t sv = 0;        // stuck value
    int class_size = 1;    // number of uncollapsed faults this one represents
};

struct FaultList {
    std::vector<Fault> faults;  // collapsed representatives (or everything if not collapsed)
    std::vector<Fault> all;     // full uncollapsed universe
    std::vector<int> rep_of;    // all[i] is equivalent to faults[rep_of[i]]
    int num_lines = 0;
};

FaultList build_fault_list(const Circuit& c, bool collapse = true);

std::string fault_str(const Circuit& c, const Fault& f);

// Serial reference: does the fully-specified 0/1 vector detect fault f?
// Independent of the parallel fault simulator; used only for verification.
bool reference_detects(const Circuit& c, const Fault& f, const std::vector<uint8_t>& pi_vals);
