// faults.cpp — fault universe, equivalence collapsing, reference fault simulation.
#include "faults.hpp"

#include <numeric>
#include <stdexcept>

#include "logic.hpp"

namespace {

struct Dest {
    int gate;  // >= 0: gate input; -1: primary output
    int pin;   // gate pin, or index into pos
};

struct UnionFind {
    std::vector<int> p;
    explicit UnionFind(int n) : p(n) { std::iota(p.begin(), p.end(), 0); }
    int find(int x)
    {
        while (p[x] != x) x = p[x] = p[p[x]];
        return x;
    }
    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b) p[std::max(a, b)] = std::min(a, b);  // smaller id is the root
    }
};

}  // namespace

FaultList build_fault_list(const Circuit& c, bool collapse)
{
    const int n = c.num_signals();

    // Destinations of every signal: each reading gate pin, each PO occurrence.
    std::vector<std::vector<Dest>> dests(n);
    for (int g : c.order)
        for (int k = 0; k < static_cast<int>(c.sig[g].fanins.size()); ++k)
            dests[c.sig[g].fanins[k]].push_back({g, k});
    for (int k = 0; k < static_cast<int>(c.pos.size()); ++k) dests[c.pos[k]].push_back({-1, k});

    // Lines: stems first (line id == signal id), then branches.
    std::vector<Fault> line_proto;  // one per line, sv unset
    line_proto.reserve(n * 2);
    for (int s = 0; s < n; ++s) {
        Fault f;
        f.sig = s;
        f.site = Site::STEM;
        line_proto.push_back(f);
    }
    // input_line[g][k] = line id feeding pin k of gate g
    std::vector<std::vector<int>> input_line(n);
    for (int g : c.order) input_line[g].assign(c.sig[g].fanins.size(), -1);
    for (int s = 0; s < n; ++s) {
        if (dests[s].size() <= 1) {
            for (const Dest& d : dests[s])
                if (d.gate >= 0) input_line[d.gate][d.pin] = s;  // fanout-free: the stem itself
            continue;
        }
        for (const Dest& d : dests[s]) {
            Fault f;
            f.sig = s;
            if (d.gate >= 0) {
                f.site = Site::BRANCH;
                f.gate = d.gate;
                f.pin = d.pin;
                input_line[d.gate][d.pin] = static_cast<int>(line_proto.size());
            } else {
                f.site = Site::PO_BRANCH;
                f.pin = d.pin;
            }
            line_proto.push_back(f);
        }
    }

    const int lines = static_cast<int>(line_proto.size());
    FaultList fl;
    fl.num_lines = lines;
    fl.all.reserve(lines * 2);
    for (int l = 0; l < lines; ++l)
        for (uint8_t v = 0; v < 2; ++v) {
            Fault f = line_proto[l];
            f.sv = v;
            fl.all.push_back(f);
        }
    auto fid = [](int line, int v) { return line * 2 + v; };

    UnionFind uf(lines * 2);
    if (collapse) {
        for (int g : c.order) {
            const Signal& s = c.sig[g];
            for (int k = 0; k < static_cast<int>(s.fanins.size()); ++k) {
                int in = input_line[g][k];
                switch (s.type) {
                    case GateType::AND: uf.unite(fid(in, 0), fid(g, 0)); break;
                    case GateType::NAND: uf.unite(fid(in, 0), fid(g, 1)); break;
                    case GateType::OR: uf.unite(fid(in, 1), fid(g, 1)); break;
                    case GateType::NOR: uf.unite(fid(in, 1), fid(g, 0)); break;
                    case GateType::NOT:
                        uf.unite(fid(in, 0), fid(g, 1));
                        uf.unite(fid(in, 1), fid(g, 0));
                        break;
                    case GateType::BUF:
                        uf.unite(fid(in, 0), fid(g, 0));
                        uf.unite(fid(in, 1), fid(g, 1));
                        break;
                    default: break;  // XOR/XNOR: no structural equivalences
                }
            }
        }
    }

    // One representative per class (the root = smallest fault id, i.e. the
    // fault closest to a stem). Representatives keep fault-id order.
    std::vector<int> root_to_rep(lines * 2, -1);
    fl.rep_of.assign(lines * 2, -1);
    for (int i = 0; i < lines * 2; ++i) {
        int r = uf.find(i);
        if (root_to_rep[r] < 0) {
            root_to_rep[r] = static_cast<int>(fl.faults.size());
            Fault f = fl.all[r];
            f.class_size = 0;
            fl.faults.push_back(f);
        }
        fl.rep_of[i] = root_to_rep[r];
        fl.faults[root_to_rep[r]].class_size++;
    }
    return fl;
}

std::string fault_str(const Circuit& c, const Fault& f)
{
    std::string s = c.sig[f.sig].name;
    if (f.site == Site::BRANCH) s += "->" + c.sig[f.gate].name + "." + std::to_string(f.pin);
    if (f.site == Site::PO_BRANCH) s += "->PO";
    s += f.sv ? " s-a-1" : " s-a-0";
    return s;
}

bool reference_detects(const Circuit& c, const Fault& f, const std::vector<uint8_t>& pi_vals)
{
    std::vector<uint8_t> good = simulate_scalar(c, pi_vals);

    // Faulty machine: full re-simulation with the fault injected.
    std::vector<uint8_t> bad(c.sig.size(), 0);
    for (size_t i = 0; i < c.pis.size(); ++i) bad[c.pis[i]] = pi_vals[i] ? 1 : 0;
    if (f.site == Site::STEM && c.sig[f.sig].is_pi) bad[f.sig] = f.sv;
    for (int g : c.order) {
        const Signal& s = c.sig[g];
        auto in = [&](int k) -> uint8_t {
            if (f.site == Site::BRANCH && g == f.gate && k == f.pin) return f.sv;
            return bad[s.fanins[k]];
        };
        bad[g] = eval3(s.type, static_cast<int>(s.fanins.size()), in);
        if (f.site == Site::STEM && g == f.sig) bad[g] = f.sv;
    }
    for (int k = 0; k < static_cast<int>(c.pos.size()); ++k) {
        int p = c.pos[k];
        uint8_t b = (f.site == Site::PO_BRANCH && k == f.pin) ? f.sv : bad[p];
        if (b != good[p]) return true;
    }
    return false;
}
