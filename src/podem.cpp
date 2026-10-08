// podem.cpp
#include "podem.hpp"

#include <limits>
#include <stdexcept>

#include "logic.hpp"


Podem::Podem(const Circuit& c, const Scoap& sc, int backtrack_limit)
    : c_(c), sc_(sc), limit_(backtrack_limit)
{
    const int n = c.num_signals();
    gv_.assign(n, LX);
    fv_.assign(n, LX);
    inq_.assign(n, 0);
    mark_.assign(n, 0);
    in_cone_.assign(n, 0);
    bucket_.assign(c.max_level + 1, {});
    po_mark_.assign(n, 0);
    for (int p : c.pos) po_mark_[p] = 1;
}

// Faulty-machine value seen on pin k of gate g (a branch fault overrides one pin).
uint8_t Podem::bad_pin(int g, int k) const
{
    if (f_.site == Site::BRANCH && g == f_.gate && k == f_.pin) return f_.sv;
    return fv_[c_.sig[g].fanins[k]];
}

bool Podem::pin_has_d(int g, int k) const
{
    uint8_t a = gv_[c_.sig[g].fanins[k]], b = bad_pin(g, k);
    return a != LX && b != LX && a != b;
}

void Podem::schedule(int g)
{
    if (inq_[g]) return;
    inq_[g] = 1;
    int l = c_.sig[g].level;
    bucket_[l].push_back(g);
    if (l < lo_) lo_ = l;
    if (l > hi_) hi_ = l;
    ++pending_;
}

// Event-driven implication: re-evaluate scheduled gates level by level, and
// schedule fanouts only of gates whose good or faulty value changed.
void Podem::propagate()
{
    for (int l = lo_; pending_ > 0 && l <= hi_; ++l) {
        auto& b = bucket_[l];
        for (size_t i = 0; i < b.size(); ++i) {
            int g = b[i];
            inq_[g] = 0;
            --pending_;
            const Signal& s = c_.sig[g];
            const int n = static_cast<int>(s.fanins.size());
            const int* fi = s.fanins.data();
            uint8_t ng = eval3(s.type, n, [&](int k) { return gv_[fi[k]]; });
            uint8_t nb = (f_.site == Site::STEM && g == f_.sig)
                             ? f_.sv
                             : eval3(s.type, n, [&](int k) { return bad_pin(g, k); });
            ++work_;
            if (ng == gv_[g] && nb == fv_[g]) continue;
            gv_[g] = ng;
            fv_[g] = nb;
            for (int fo : s.fanouts) schedule(fo);
        }
        b.clear();
    }
    lo_ = c_.max_level + 1;
    hi_ = -1;
}

void Podem::assign_pi(int pi, uint8_t v)
{
    gv_[pi] = v;
    fv_[pi] = (f_.site == Site::STEM && f_.sig == pi) ? f_.sv : v;
    for (int fo : c_.sig[pi].fanouts) schedule(fo);
    propagate();
}

bool Podem::detected() const
{
    for (int k = 0; k < static_cast<int>(c_.pos.size()); ++k) {
        int p = c_.pos[k];
        uint8_t g = gv_[p];
        uint8_t b = (f_.site == Site::PO_BRANCH && k == f_.pin) ? f_.sv : fv_[p];
        if (g != LX && b != LX && g != b) return true;
    }
    return false;
}

void Podem::collect_dfrontier()
{
    dfront_.clear();
    for (int h : cone_) {
        ++work_;
        if (gv_[h] != LX && fv_[h] != LX) continue;  // output already decided
        const int n = static_cast<int>(c_.sig[h].fanins.size());
        for (int k = 0; k < n; ++k)
            if (pin_has_d(h, k)) {
                dfront_.push_back(h);
                break;
            }
    }
}

// Is there a path of undecided (X) gates from some D-frontier gate to a PO?
bool Podem::xpath_exists()
{
    if (++mstamp_ == 0) {
        std::fill(mark_.begin(), mark_.end(), 0);
        mstamp_ = 1;
    }
    std::vector<int> stack(dfront_.begin(), dfront_.end());
    for (int h : stack) mark_[h] = mstamp_;
    while (!stack.empty()) {
        int h = stack.back();
        stack.pop_back();
        ++work_;
        if (po_mark_[h]) return true;
        for (int fo : c_.sig[h].fanouts) {
            if (mark_[fo] == mstamp_) continue;
            if (gv_[fo] != LX && fv_[fo] != LX) continue;
            mark_[fo] = mstamp_;
            stack.push_back(fo);
        }
    }
    return false;
}

static uint8_t noncontrolling(GateType t)
{
    return (t == GateType::OR || t == GateType::NOR) ? L0 : L1;
}

// Objective = (line, value) to achieve, and which machine it must be achieved in.
// Normally the good machine. When a D-frontier gate's only undecided inputs are
// undecided in the faulty machine alone (reconvergent fault effects), the
// objective is set on the faulty machine instead.
bool Podem::objective(int& line, uint8_t& val, bool& faulty) const
{
    faulty = false;
    if (gv_[f_.sig] == LX) {  // excite the fault first
        line = f_.sig;
        val = static_cast<uint8_t>(1 - f_.sv);
        return true;
    }
    for (int pass = 0; pass < 2; ++pass) {
        const bool fm = pass == 1;
        // Most observable D-frontier gate with an input undecided in this machine.
        int best_gate = -1;
        int64_t best_co = std::numeric_limits<int64_t>::max();
        for (int h : dfront_) {
            const int n = static_cast<int>(c_.sig[h].fanins.size());
            bool settable = false;
            for (int k = 0; k < n && !settable; ++k)
                settable = (fm ? bad_pin(h, k) : gv_[c_.sig[h].fanins[k]]) == LX;
            if (!settable) continue;
            int64_t key = sc_.co[h] + jitter(h);
            if (key < best_co) {
                best_co = key;
                best_gate = h;
            }
        }
        if (best_gate < 0) continue;
        const Signal& g = c_.sig[best_gate];
        uint8_t nc = noncontrolling(g.type);
        bool parity = g.type == GateType::XOR || g.type == GateType::XNOR;
        int pick = -1;
        int64_t pick_cost = std::numeric_limits<int64_t>::max();
        uint8_t pick_val = nc;
        for (int k = 0; k < static_cast<int>(g.fanins.size()); ++k) {
            if ((fm ? bad_pin(best_gate, k) : gv_[g.fanins[k]]) != LX) continue;
            int x = g.fanins[k];
            int64_t c0 = sc_.cc0[x], c1 = sc_.cc1[x];
            int64_t cost = (parity ? std::min(c0, c1) : (nc == L1 ? c1 : c0)) + jitter(x);
            // Prefer side inputs outside the fault's fanout cone: they take the same
            // value in both machines, so they cannot carry a cancelling fault effect.
            if (in_cone_[x] == cone_stamp_) cost += int64_t(1) << 32;
            if (cost < pick_cost) {
                pick_cost = cost;
                pick = x;
                pick_val = parity ? (c0 <= c1 ? L0 : L1) : nc;
            }
        }
        if (parity && pick >= 0 && in_cone_[pick] == cone_stamp_) {
            // An XOR side input inside the fault cone must not carry the fault
            // effect, or the two paths cancel. Ask for the value its driver gives
            // when *controlled* — that can be forced by one off-path input,
            // independently of the fault.
            int y = pick;
            uint8_t inv = 0;
            while (c_.sig[y].type == GateType::NOT || c_.sig[y].type == GateType::BUF) {
                if (c_.sig[y].type == GateType::NOT) inv ^= 1;
                y = c_.sig[y].fanins[0];
            }
            switch (c_.sig[y].type) {
                case GateType::AND: pick_val = L0 ^ inv; break;
                case GateType::NAND: pick_val = L1 ^ inv; break;
                case GateType::OR: pick_val = L1 ^ inv; break;
                case GateType::NOR: pick_val = L0 ^ inv; break;
                default: break;
            }
        }
        line = pick;
        val = pick_val;
        faulty = fm;
        return true;
    }
    return false;
}

// Walk the objective back to an unassigned PI through lines that are X in the
// chosen machine (good, or faulty with the fault's own forced pin excluded).
int Podem::backtrace(int line, uint8_t val, bool faulty, uint8_t& pi_val) const
{
    while (!c_.sig[line].is_pi) {
        const int gate = line;
        const Signal& s = c_.sig[gate];
        const int n = static_cast<int>(s.fanins.size());
        auto in = [&](int k) { return faulty ? bad_pin(gate, k) : gv_[s.fanins[k]]; };
        uint8_t v = is_inverting(s.type) ? static_cast<uint8_t>(1 - val) : val;
        int pick = -1;
        uint8_t next = v;
        switch (s.type) {
            case GateType::AND:
            case GateType::NAND:
            case GateType::OR:
            case GateType::NOR: {
                // controlling value c: one input at c sets the output -> take the easiest;
                // otherwise every input must be non-controlling -> take the hardest first.
                uint8_t ctrl = (s.type == GateType::AND || s.type == GateType::NAND) ? L0 : L1;
                bool easiest = (v == ctrl);
                int64_t best = easiest ? std::numeric_limits<int64_t>::max() : -1;
                for (int k = 0; k < n; ++k) {
                    if (in(k) != LX) continue;
                    int x = s.fanins[k];
                    int64_t cost = (v == L0 ? sc_.cc0[x] : sc_.cc1[x]) + jitter(x);
                    // One input decides the output: prefer an input outside the
                    // fault cone, so the result holds in both machines.
                    if (easiest && in_cone_[x] == cone_stamp_) cost += int64_t(1) << 32;
                    if (easiest ? cost < best : cost > best) {
                        best = cost;
                        pick = x;
                    }
                }
                break;
            }
            case GateType::XOR:
            case GateType::XNOR: {
                uint8_t parity = 0;
                int64_t best = std::numeric_limits<int64_t>::max();
                for (int k = 0; k < n; ++k) {
                    uint8_t iv = in(k);
                    int x = s.fanins[k];
                    if (iv == LX) {
                        int64_t cost = std::min(sc_.cc0[x], sc_.cc1[x]) + jitter(x);
                        if (in_cone_[x] == cone_stamp_) cost += int64_t(1) << 32;
                        if (cost < best) { best = cost; pick = x; }
                    } else {
                        parity ^= iv;
                    }
                }
                next = static_cast<uint8_t>(v ^ parity);
                break;
            }
            case GateType::NOT:
            case GateType::BUF: pick = s.fanins[0]; break;
            case GateType::INPUT: break;
        }
        if (pick < 0 || (faulty ? fv_[pick] : gv_[pick]) != LX)
            throw std::logic_error("PODEM backtrace reached a line with no X input");
        line = pick;
        val = next;
    }
    pi_val = val;
    return line;
}

Podem::Rec Podem::search()
{
    if (detected()) return OK;
    uint8_t site = gv_[f_.sig];
    if (site != LX && site == f_.sv) return FAIL;  // cannot excite under this assignment
    if (site != LX) {
        collect_dfrontier();
        if (dfront_.empty()) return FAIL;          // fault effect blocked everywhere
        if (!xpath_exists()) return FAIL;          // no undecided path to any PO
    }

    int line;
    uint8_t val, pv;
    int pi;
    bool faulty = false;
    if (objective(line, val, faulty)) {
        pi = backtrace(line, val, faulty, pv);
        ++objective_hits_;
    } else {
        // No objective reachable through the good machine (the D-frontier gates'
        // remaining X values are faulty-machine only). Branch on any free PI so the
        // search stays complete.
        ++fallbacks_;
        pi = -1;
        for (int p : c_.pis)
            if (gv_[p] == LX) { pi = p; break; }
        if (pi < 0) return FAIL;
        pv = L0;
    }

    if (flip_values_ && (rand_() & 1)) pv = static_cast<uint8_t>(1 - pv);
    assign_pi(pi, pv);
    Rec r = search();
    if (r != FAIL) return r;
    if (++backtracks_ > limit_) return ABORT;
    assign_pi(pi, static_cast<uint8_t>(1 - pv));
    r = search();
    if (r != FAIL) return r;
    assign_pi(pi, LX);
    return FAIL;
}

PodemResult Podem::run(const Fault& f)
{
    f_ = f;
    backtracks_ = 0;
    std::fill(gv_.begin(), gv_.end(), LX);
    std::fill(fv_.begin(), fv_.end(), LX);
    work_ += c_.sig.size();

    // Fanout cone of the fault site (where the D-frontier can live).
    cone_.clear();
    if (++mstamp_ == 0) {
        std::fill(mark_.begin(), mark_.end(), 0);
        mstamp_ = 1;
    }
    std::vector<int> stack;
    if (f.site == Site::STEM) {
        for (int fo : c_.sig[f.sig].fanouts) stack.push_back(fo);
    } else if (f.site == Site::BRANCH) {
        stack.push_back(f.gate);
    }
    for (int g : stack) mark_[g] = mstamp_;
    if (++cone_stamp_ == 0) {
        std::fill(in_cone_.begin(), in_cone_.end(), 0);
        cone_stamp_ = 1;
    }
    if (f.site == Site::STEM) in_cone_[f.sig] = cone_stamp_;
    while (!stack.empty()) {
        int g = stack.back();
        stack.pop_back();
        cone_.push_back(g);
        in_cone_[g] = cone_stamp_;
        for (int fo : c_.sig[g].fanouts)
            if (mark_[fo] != mstamp_) {
                mark_[fo] = mstamp_;
                stack.push_back(fo);
            }
    }

    // The stuck value is present from the start in the faulty machine.
    if (f.site == Site::STEM) {
        fv_[f.sig] = f.sv;
        for (int fo : c_.sig[f.sig].fanouts) schedule(fo);
    } else if (f.site == Site::BRANCH) {
        schedule(f.gate);
    }
    propagate();

    // Search with randomized restarts. Attempt 0 is plain SCOAP-guided PODEM;
    // later attempts perturb the tie-breaking and value order. The total
    // backtrack budget is the same, split across attempts, so one bad early
    // decision cannot consume all of it. A REDUNDANT verdict is only accepted
    // from an attempt that exhausted its search space (that is a proof
    // regardless of the heuristic used).
    PodemResult res;
    Rec r = ABORT;
    int total_bt = 0;
    // Budget split: a short plain try (most faults finish here), two short
    // randomized tries (escape a bad early decision), then one long plain search
    // with 3/4 of the budget (long exhaustive searches are what prove redundancy).
    const int full_limit = limit_;
    const int attempts = full_limit >= 64 ? kAttempts : 1;
    const int budget[kAttempts] = {full_limit / 16, full_limit / 16, full_limit / 8,
                                   full_limit - full_limit / 16 * 2 - full_limit / 8};
    rand_.seed(0x5eed + static_cast<uint64_t>(f.sig) * 31 + f.sv * 7 + (f.gate + 1) * 131 + f.pin);
    for (int a = 0; a < attempts && r == ABORT; ++a) {
        if (a > 0) {
            for (int pi : c_.pis) if (gv_[pi] != LX) assign_pi(pi, LX);  // undo partial assignment
        }
        const bool randomized = attempts > 1 && (a == 1 || a == 2);
        jitter_scale_ = randomized ? 1 + a : 0;
        flip_values_ = randomized;
        limit_ = attempts > 1 ? std::max(1, budget[a]) : full_limit;
        backtracks_ = 0;
        r = search();
        total_bt += backtracks_;
    }
    limit_ = full_limit;
    jitter_scale_ = 0;
    flip_values_ = false;
    backtracks_ = total_bt;
    res.backtracks = backtracks_;
    if (r == OK) {
        res.status = PodemStatus::DETECTED;
        res.pi.resize(c_.pis.size());
        for (size_t i = 0; i < c_.pis.size(); ++i) res.pi[i] = gv_[c_.pis[i]];
    } else if (r == FAIL) {
        res.status = PodemStatus::REDUNDANT;
    } else {
        res.status = PodemStatus::ABORTED;
    }
    // Leave the bucket queues clean for the next call.
    for (auto& b : bucket_) b.clear();
    std::fill(inq_.begin(), inq_.end(), 0);
    pending_ = 0;
    lo_ = c_.max_level + 1;
    hi_ = -1;
    return res;
}
