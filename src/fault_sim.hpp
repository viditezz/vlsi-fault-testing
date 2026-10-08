// fault_sim.hpp — parallel-pattern single-fault propagation (PPSFP) with fault dropping.
//
// For each block of up to 64 patterns: one fault-free simulation, then for every
// still-undetected fault, an event-driven re-evaluation of only the gates in that
// fault's fanout cone whose values actually change. A fault is detected by the
// lowest-numbered pattern in the block whose faulty PO values differ from the
// good ones; it is then dropped (never simulated again).
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "circuit.hpp"
#include "faults.hpp"

class FaultSim {
public:
    FaultSim(const Circuit& c, const std::vector<Fault>& faults);

    // Simulates n (1..64) patterns. pi_words[i] bit j = PI i in pattern j.
    // Appends (fault index, pattern offset in block) for each newly detected fault.
    // Returns the number of newly detected faults.
    int simulate_block(const std::vector<uint64_t>& pi_words, int n,
                       std::vector<std::pair<int, int>>* newly = nullptr);

    // Convenience: one fully specified 0/1 vector.
    int simulate_vector(const std::vector<uint8_t>& vec,
                        std::vector<std::pair<int, int>>* newly = nullptr);

    // Removes a fault from the target set without detecting it (redundant/aborted).
    void retire(int f);

    bool is_detected(int f) const { return state_[f] == DETECTED; }
    bool is_pending(int f) const { return state_[f] == PENDING; }
    int num_detected() const { return detected_; }
    int num_pending() const { return static_cast<int>(pending_.size()); }
    const std::vector<int>& pending() const { return pending_; }
    int num_faults() const { return static_cast<int>(faults_.size()); }

    // Work counter: gate evaluations performed (each on a 64-bit word).
    uint64_t work() const { return work_; }

    // Detection mask of a single fault against the most recent block, without
    // dropping (used by the self-tests).
    uint64_t probe_mask(int f);

private:
    enum State : uint8_t { PENDING, DETECTED, RETIRED };

    uint64_t propagate(const Fault& f);
    void schedule(int g);
    uint64_t fval(int s) const { return stamp_[s] == cur_ ? fv_[s] : good_[s]; }

    const Circuit& c_;
    std::vector<Fault> faults_;
    std::vector<State> state_;
    std::vector<int> pending_;      // undetected, unretired fault indices
    std::vector<int> pos_in_pending_;
    int detected_ = 0;

    std::vector<uint64_t> good_, fv_;
    std::vector<uint32_t> stamp_, sched_;
    uint32_t cur_ = 0;
    std::vector<std::vector<int>> bucket_;
    std::vector<uint8_t> po_mark_;  // signal is a primary output
    uint64_t valid_ = 0;
    uint64_t work_ = 0;
    int lo_ = 0, hi_ = -1;

    void drop(int f);
};
