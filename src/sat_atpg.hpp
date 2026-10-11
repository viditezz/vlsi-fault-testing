// sat_atpg.hpp — SAT-based test generation for one stuck-at fault.
//
// Fallback for faults PODEM aborts on. Builds a miter: the fault-free circuit
// restricted to what the fault can influence, a faulty copy of the fault's
// fanout cone, and the requirement that some reachable primary output differs.
//   SAT   -> the model is a test vector (DETECTED)
//   UNSAT -> no vector detects the fault (REDUNDANT, a proof)
// A conflict limit turns runaway instances into ABORTED, but with the default
// limit every fault in the ISCAS-85 suite resolves.
#pragma once

#include <cstdint>
#include <vector>

#include "circuit.hpp"
#include "faults.hpp"
#include "podem.hpp"  // PodemStatus
#include "sat.hpp"

struct SatAtpgResult {
    PodemStatus status = PodemStatus::ABORTED;
    std::vector<uint8_t> pi;  // per PI: L0/L1, or LX if the instance does not constrain it
    uint64_t conflicts = 0;
};

class SatAtpg {
public:
    explicit SatAtpg(const Circuit& c);

    SatAtpgResult run(const Fault& f, int64_t conflict_limit);

    // Work counter: unit-propagation watcher visits, summed over all runs.
    // Not comparable to the gate-evaluation units used for PODEM and fault simulation.
    uint64_t work() const { return work_; }

private:
    const Circuit& c_;
    std::vector<uint32_t> in_cone_, needed_;
    uint32_t stamp_ = 0;
    uint64_t work_ = 0;
};
