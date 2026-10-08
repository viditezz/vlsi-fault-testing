// podem.hpp — PODEM deterministic test generation for single stuck-at faults.
//
// Search space: assignments to primary inputs only. Each step:
//   1. if a PO shows D/D'  -> test found
//   2. if the fault cannot be excited, the D-frontier is empty, or no X-path
//      leads from the D-frontier to a PO -> this branch fails
//   3. objective: excite the fault (site = !stuck value), else propagate
//      through the most observable D-frontier gate (one X input = non-controlling)
//   4. backtrace the objective to an unassigned PI (SCOAP-guided), assign it,
//      imply (event-driven 3-valued simulation of the good and faulty machines),
//      recurse; on failure try the other value; on failure unassign.
// Exhausting the search proves the fault redundant. Exceeding the backtrack
// limit aborts it.
//
// The engine takes one fault at a time, so the same code serves both as the
// standalone ATPG baseline (called on the full fault list) and as the second
// phase of the adaptive method (called only on the faults random testing left).
#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "circuit.hpp"
#include "faults.hpp"
#include "scoap.hpp"

enum class PodemStatus { DETECTED, REDUNDANT, ABORTED };

struct PodemResult {
    PodemStatus status = PodemStatus::ABORTED;
    std::vector<uint8_t> pi;  // per PI: L0, L1 or LX (don't care); valid if DETECTED
    int backtracks = 0;
};

class Podem {
public:
    Podem(const Circuit& c, const Scoap& sc, int backtrack_limit);

    PodemResult run(const Fault& f);

    // Work counter: 3-valued gate evaluations + gates scanned for the D-frontier.
    uint64_t work() const { return work_; }
    void set_backtrack_limit(int b) { limit_ = b; }
    uint64_t fallbacks() const { return fallbacks_; }
    uint64_t objective_hits() const { return objective_hits_; }

private:
    enum Rec { OK, FAIL, ABORT };

    Rec search();
    bool detected() const;
    void collect_dfrontier();
    bool xpath_exists();
    bool objective(int& line, uint8_t& val, bool& faulty) const;
    int backtrace(int line, uint8_t val, bool faulty, uint8_t& pi_val) const;
    void assign_pi(int pi, uint8_t v);
    void schedule(int g);
    void propagate();
    uint8_t bad_pin(int g, int k) const;
    bool pin_has_d(int g, int k) const;

    const Circuit& c_;
    const Scoap& sc_;
    int limit_;
    int backtracks_ = 0;

    Fault f_;
    std::vector<uint8_t> gv_, fv_;   // good / faulty machine, 3-valued
    std::vector<uint8_t> inq_;
    std::vector<std::vector<int>> bucket_;
    int lo_ = 0, hi_ = -1, pending_ = 0;

    std::vector<int> cone_;          // gates in the fault's fanout cone
    std::vector<int> dfront_;
    std::vector<uint32_t> mark_;
    std::vector<uint32_t> in_cone_;
    uint32_t cone_stamp_ = 0;
    uint32_t mstamp_ = 0;
    std::vector<uint8_t> po_mark_;
    uint64_t work_ = 0;
    uint64_t fallbacks_ = 0, objective_hits_ = 0;

    static constexpr int kAttempts = 4;
    int jitter_scale_ = 0;          // 0 on the first attempt: pure SCOAP
    bool flip_values_ = false;
    mutable std::mt19937_64 rand_;
    // Random perturbation of SCOAP costs on restart attempts. Scaled to the
    // circuit's typical controllability so it reorders near-ties, not everything.
    int64_t jitter(int /*line*/) const
    {
        return jitter_scale_ ? static_cast<int64_t>(rand_() % (8 * jitter_scale_)) : 0;
    }
};
