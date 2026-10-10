// atpg_flow.hpp — the four test-generation methods compared in the paper.
//
//   random    : pure random patterns until the budget or 100% coverage
//   podem     : PODEM on every fault (with fault dropping) — deterministic baseline
//   fixed     : N random patterns, then PODEM on the rest — fixed switch point
//   plateau   : random patterns until the last W blocks found <= K new faults,
//               then PODEM on the rest — a simple heuristic baseline
//   stage1    : adaptive with stage 2 off — switch as soon as random's rate is
//               significantly below the cost bound of ~1 fault per block
//   adaptive  : random patterns until random's measured detection rate falls
//               below PODEM's measured rate, then PODEM on the rest — this
//               project's method
//
// All methods share the same fault list, fault simulator and PODEM engine, so
// any difference comes from *when* (and whether) the switch happens.
// Every generated test set is re-verified and statically compacted by
// reverse-order fault simulation.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "circuit.hpp"
#include "faults.hpp"
#include "scoap.hpp"

// PODEM's 3-valued gate evaluation costs more than one 64-bit word evaluation in
// the fault simulator. Work is reported in "fault-sim word evaluation" units;
// one PODEM evaluation counts as this many. Calibrated with `faultatpg calibrate`
// (see README): measured ns/unit ratios PODEM:fault-sim were 0.71-1.29 across
// c432..c7552, so the two are weighted equally. Affects only the adaptive comparison.
constexpr double kPodemWorkWeight = 1.0;

struct RunConfig {
    std::string method = "adaptive";  // random | podem | fixed | plateau | stage1 | adaptive
    uint64_t seed = 1;
    int64_t random_budget = 32768;    // max random patterns (random; cap for adaptive)
    int64_t fixed_switch = 1024;      // fixed: random patterns before switching
    int backtrack_limit = 1024;
    double alpha = 0.05;              // adaptive: significance level
    int confirm = 2;                  // adaptive: consecutive rejections to switch
    double window_mu = 6.0;           // adaptive: break-even detections per window
    bool stage2 = true;               // adaptive: measure PODEM before switching (false: switch on stage 1)
    int64_t warmup = 128;             // adaptive: patterns before the first test
    int probe_size = 16;              // adaptive: PODEM calls per yield probe
    double podem_weight = kPodemWorkWeight;
    int plateau_blocks = 6;           // plateau: window length in 64-pattern blocks
    int plateau_max = 1;              // plateau: switch when window found <= this many
};

struct CurvePoint {
    int64_t pattern;   // patterns applied so far (random + deterministic)
    int detected;      // faults detected so far
    double work;       // generation work so far
    double ms;         // wall-clock so far
    char phase;        // 'R' random, 'p' probe PODEM, 'P' PODEM phase
};

struct RunResult {
    std::string circuit, method;
    uint64_t seed = 0;
    int64_t param = 0;            // fixed: N; adaptive: switch point
    int faults = 0, detected = 0, redundant = 0, aborted = 0;
    int64_t random_applied = 0;   // random patterns simulated
    int random_kept = 0;          // random patterns that detected >= 1 new fault
    int podem_vectors = 0;        // deterministic patterns
    int podem_calls = 0;
    int probe_calls = 0;
    int tests = 0;                // random_kept + podem_vectors
    int compacted = 0;            // after reverse-order compaction
    int64_t switch_at = -1;       // random patterns applied when PODEM took over
    double work_random = 0, work_podem = 0, work_total = 0;
    // PODEM work on faults that end the run undetected (proved redundant, or
    // aborted and never detected). Every flow pays it whenever it switches, so
    // work_total - work_unresolved is the part the switch point can change.
    double work_unresolved = 0;
    double work_core() const { return work_total - work_unresolved; }
    double ms_random = 0, ms_podem = 0, ms_total = 0, ms_compact = 0;
    double fc() const { return faults ? 100.0 * detected / faults : 0; }
    double fe() const { return faults ? 100.0 * (detected + redundant) / faults : 0; }

    std::vector<CurvePoint> curve;
    std::vector<std::vector<uint8_t>> test_set;   // compacted, binary
    std::vector<std::string> decisions;           // adaptive: stagnation-test log
};

RunResult run_atpg(const Circuit& c, const FaultList& fl, const Scoap& sc, const RunConfig& cfg);

std::string summary_header();
std::string summary_row(const RunResult& r);
void write_curve(const std::string& path, const RunResult& r, int max_points = 2000);
