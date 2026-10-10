// fault_sim.cpp — PPSFP fault simulator.
#include "fault_sim.hpp"

#include <stdexcept>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "logic.hpp"

namespace {
// Index of the lowest set bit of a nonzero word (portable count-trailing-zeros).
inline int lowest_bit(uint64_t m)
{
#if defined(_MSC_VER)
    unsigned long i;
    _BitScanForward64(&i, m);
    return static_cast<int>(i);
#else
    return __builtin_ctzll(m);
#endif
}
}  // namespace

FaultSim::FaultSim(const Circuit& c, const std::vector<Fault>& faults)
    : c_(c), faults_(faults)
{
    const int n = c.num_signals();
    state_.assign(faults_.size(), PENDING);
    pending_.resize(faults_.size());
    pos_in_pending_.resize(faults_.size());
    for (int i = 0; i < static_cast<int>(faults_.size()); ++i) {
        pending_[i] = i;
        pos_in_pending_[i] = i;
    }
    good_.assign(n, 0);
    fv_.assign(n, 0);
    stamp_.assign(n, 0);
    sched_.assign(n, 0);
    bucket_.assign(c.max_level + 1, {});
    po_mark_.assign(n, 0);
    for (int p : c.pos) po_mark_[p] = 1;
}

void FaultSim::drop(int f)
{
    int pos = pos_in_pending_[f];
    int last = pending_.back();
    pending_[pos] = last;
    pos_in_pending_[last] = pos;
    pending_.pop_back();
    pos_in_pending_[f] = -1;
}

void FaultSim::retire(int f)
{
    if (state_[f] != PENDING) return;
    state_[f] = RETIRED;
    drop(f);
}

void FaultSim::schedule(int g)
{
    if (sched_[g] == cur_) return;
    sched_[g] = cur_;
    int l = c_.sig[g].level;
    bucket_[l].push_back(g);
    if (l < lo_) lo_ = l;
    if (l > hi_) hi_ = l;
}

uint64_t FaultSim::propagate(const Fault& f)
{
    ++cur_;
    if (cur_ == 0) {  // stamp wrap-around: clear and restart
        std::fill(stamp_.begin(), stamp_.end(), 0);
        std::fill(sched_.begin(), sched_.end(), 0);
        cur_ = 1;
    }
    const uint64_t stuck = f.sv ? ~0ULL : 0ULL;
    uint64_t det = 0;
    lo_ = c_.max_level + 1;
    hi_ = -1;

    if (f.site == Site::PO_BRANCH) return (stuck ^ good_[f.sig]) & valid_;

    int origin;
    uint64_t ov;
    if (f.site == Site::STEM) {
        origin = f.sig;
        ov = stuck;
    } else {  // BRANCH: re-evaluate the reading gate with one pin forced
        origin = f.gate;
        const Signal& g = c_.sig[origin];
        const int* fi = g.fanins.data();
        ov = eval_word(g.type, static_cast<int>(g.fanins.size()),
                       [&](int k) { return k == f.pin ? stuck : good_[fi[k]]; });
        ++work_;
    }
    uint64_t diff = (ov ^ good_[origin]) & valid_;
    if (!diff) return 0;  // fault not activated (or masked at its gate) in any pattern
    fv_[origin] = ov;
    stamp_[origin] = cur_;
    if (po_mark_[origin]) det |= diff;
    for (int fo : c_.sig[origin].fanouts) schedule(fo);

    for (int l = lo_; l <= hi_; ++l) {
        auto& b = bucket_[l];
        for (size_t i = 0; i < b.size(); ++i) {
            int g = b[i];
            const Signal& s = c_.sig[g];
            const int* fi = s.fanins.data();
            uint64_t v = eval_word(s.type, static_cast<int>(s.fanins.size()),
                                   [&](int k) { return fval(fi[k]); });
            ++work_;
            uint64_t d = (v ^ good_[g]) & valid_;
            if (!d) continue;  // fault effect died here
            fv_[g] = v;
            stamp_[g] = cur_;
            if (po_mark_[g]) det |= d;
            for (int fo : s.fanouts) schedule(fo);
        }
        b.clear();
    }
    return det & valid_;
}

int FaultSim::simulate_block(const std::vector<uint64_t>& pi_words, int n,
                             std::vector<std::pair<int, int>>* newly)
{
    if (n < 1 || n > 64) throw std::runtime_error("simulate_block: n must be 1..64");
    valid_ = n == 64 ? ~0ULL : ((1ULL << n) - 1);
    simulate_words(c_, pi_words, good_);
    work_ += c_.num_gates();

    int found = 0;
    // Iterate over a snapshot: dropping reorders pending_.
    std::vector<int> targets = pending_;
    for (int fi : targets) {
        uint64_t m = propagate(faults_[fi]);
        if (!m) continue;
        int first = lowest_bit(m);
        state_[fi] = DETECTED;
        drop(fi);
        ++detected_;
        ++found;
        if (newly) newly->push_back({fi, first});
    }
    return found;
}

int FaultSim::simulate_vector(const std::vector<uint8_t>& vec,
                              std::vector<std::pair<int, int>>* newly)
{
    std::vector<uint64_t> w(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) w[i] = vec[i] ? 1ULL : 0ULL;
    return simulate_block(w, 1, newly);
}

uint64_t FaultSim::probe_mask(int f) { return propagate(faults_[f]); }
