// sat_atpg.cpp
#include "sat_atpg.hpp"

#include <algorithm>

#include "logic.hpp"

namespace {

// Tseitin clauses for out = type(ins). Literals only; new variables for XOR chains.
void encode_gate(SatSolver& s, GateType t, int out, const std::vector<int>& in)
{
    auto implies_all = [&](int a, const std::vector<int>& bs) {  // a -> b for every b
        for (int b : bs) s.add_clause({lit_not(a), b});
    };
    switch (t) {
        case GateType::BUF:
            s.add_clause({lit_not(out), in[0]});
            s.add_clause({out, lit_not(in[0])});
            break;
        case GateType::NOT:
            s.add_clause({lit_not(out), lit_not(in[0])});
            s.add_clause({out, in[0]});
            break;
        case GateType::AND:
        case GateType::NAND: {
            int o = t == GateType::AND ? out : lit_not(out);  // o = AND(in)
            implies_all(o, in);
            std::vector<int> big{o};
            for (int l : in) big.push_back(lit_not(l));
            s.add_clause(big);
            break;
        }
        case GateType::OR:
        case GateType::NOR: {
            int o = t == GateType::OR ? out : lit_not(out);  // o = OR(in)
            for (int l : in) s.add_clause({o, lit_not(l)});
            std::vector<int> big{lit_not(o)};
            for (int l : in) big.push_back(l);
            s.add_clause(big);
            break;
        }
        case GateType::XOR:
        case GateType::XNOR: {
            int acc = in[0];
            for (size_t k = 1; k < in.size(); ++k) {
                bool last = k + 1 == in.size();
                int o = last ? (t == GateType::XOR ? out : lit_not(out)) : mk_lit(s.new_var());
                int a = acc, b = in[k];  // o = a xor b
                s.add_clause({lit_not(o), a, b});
                s.add_clause({lit_not(o), lit_not(a), lit_not(b)});
                s.add_clause({o, lit_not(a), b});
                s.add_clause({o, a, lit_not(b)});
                acc = o;
            }
            if (in.size() == 1) {  // degenerate single-input XOR/XNOR
                int o = t == GateType::XOR ? out : lit_not(out);
                s.add_clause({lit_not(o), in[0]});
                s.add_clause({o, lit_not(in[0])});
            }
            break;
        }
        case GateType::INPUT: break;
    }
}

}  // namespace

SatAtpg::SatAtpg(const Circuit& c) : c_(c), in_cone_(c.sig.size(), 0), needed_(c.sig.size(), 0) {}

SatAtpgResult SatAtpg::run(const Fault& f, int64_t conflict_limit)
{
    SatAtpgResult res;
    const int n = c_.num_signals();
    ++stamp_;

    // PO_BRANCH fault: only that output connection is stuck, so the fault is
    // detected exactly when the good value on it differs from the stuck value.
    // Everything else needs a faulty copy of the fanout cone.
    std::vector<int> cone;  // gates with a faulty copy, topological order
    if (f.site != Site::PO_BRANCH) {
        std::vector<int> seeds;
        if (f.site == Site::STEM) {
            for (int g : c_.sig[f.sig].fanouts) seeds.push_back(g);
        } else {
            seeds.push_back(f.gate);
        }
        std::vector<int> stack;
        for (int g : seeds)
            if (in_cone_[g] != stamp_) {
                in_cone_[g] = stamp_;
                stack.push_back(g);
            }
        while (!stack.empty()) {
            int g = stack.back();
            stack.pop_back();
            for (int h : c_.sig[g].fanouts)
                if (in_cone_[h] != stamp_) {
                    in_cone_[h] = stamp_;
                    stack.push_back(h);
                }
        }
        for (int g : c_.order)
            if (in_cone_[g] == stamp_) cone.push_back(g);
    }

    // Observable primary-output positions.
    std::vector<int> obs;
    if (f.site == Site::PO_BRANCH) {
        obs.push_back(f.pin);
    } else {
        for (int k = 0; k < static_cast<int>(c_.pos.size()); ++k) {
            int p = c_.pos[k];
            bool faulty = in_cone_[p] == stamp_ || (f.site == Site::STEM && p == f.sig);
            if (faulty) obs.push_back(k);
        }
    }
    if (obs.empty()) {  // the fault effect cannot reach any output
        res.status = PodemStatus::REDUNDANT;
        return res;
    }

    // Good-circuit signals needed: the fault site, the cone, and everything
    // in the transitive fanin of those (or of the observed outputs).
    std::vector<int> need;
    auto want = [&](int s) {
        if (needed_[s] != stamp_) {
            needed_[s] = stamp_;
            need.push_back(s);
        }
    };
    if (f.site == Site::PO_BRANCH) want(c_.pos[f.pin]);
    else if (f.site == Site::STEM) want(f.sig);
    for (int g : cone) want(g);
    for (int k : obs) want(c_.pos[k]);
    for (size_t i = 0; i < need.size(); ++i)
        for (int x : c_.sig[need[i]].fanins) want(x);

    SatSolver s;
    std::vector<int> glit(n, -1), flit(n, -1);
    int tv = s.new_var();
    s.add_clause({mk_lit(tv)});
    const int T = mk_lit(tv);
    const int stuck = f.sv ? T : lit_not(T);

    for (int x : need) glit[x] = mk_lit(s.new_var());
    for (int g : c_.order)  // good copy, topological
        if (needed_[g] == stamp_) {
            std::vector<int> in;
            for (int x : c_.sig[g].fanins) in.push_back(glit[x]);
            encode_gate(s, c_.sig[g].type, glit[g], in);
        }

    if (f.site == Site::PO_BRANCH) {
        // The good value on that output must differ from the stuck value.
        const int g = glit[c_.pos[f.pin]];
        s.add_clause({f.sv ? lit_not(g) : g});
    } else {
        auto faulty = [&](int x) { return flit[x] >= 0 ? flit[x] : glit[x]; };
        if (f.site == Site::STEM) flit[f.sig] = stuck;
        for (int g : cone) flit[g] = mk_lit(s.new_var());
        for (int g : cone) {
            std::vector<int> in;
            const auto& fi = c_.sig[g].fanins;
            for (size_t k = 0; k < fi.size(); ++k) {
                if (f.site == Site::BRANCH && g == f.gate && static_cast<int>(k) == f.pin)
                    in.push_back(stuck);
                else
                    in.push_back(faulty(fi[k]));
            }
            encode_gate(s, c_.sig[g].type, flit[g], in);
        }
        std::vector<int> any;
        for (int k : obs) {
            int p = c_.pos[k];
            int a = glit[p], b = faulty(p);
            int d = mk_lit(s.new_var());  // d -> (a xor b)
            s.add_clause({lit_not(d), a, b});
            s.add_clause({lit_not(d), lit_not(a), lit_not(b)});
            any.push_back(d);
        }
        s.add_clause(any);
    }

    SatResult r = s.solve(conflict_limit);
    work_ += s.work();
    res.conflicts = s.conflicts();
    if (r == SatResult::UNSAT) {
        res.status = PodemStatus::REDUNDANT;
    } else if (r == SatResult::SAT) {
        res.status = PodemStatus::DETECTED;
        res.pi.assign(c_.pis.size(), LX);
        for (size_t i = 0; i < c_.pis.size(); ++i) {
            int g = glit[c_.pis[i]];
            if (g >= 0) res.pi[i] = s.model_value(lit_var(g)) ? L1 : L0;
        }
    }
    return res;
}
