// scoap.cpp
#include "scoap.hpp"

#include <algorithm>

namespace {
constexpr int64_t INF = int64_t(1) << 40;
int64_t sat(int64_t v) { return v > INF ? INF : v; }
}  // namespace

Scoap compute_scoap(const Circuit& c)
{
    const int n = c.num_signals();
    Scoap s;
    s.cc0.assign(n, INF);
    s.cc1.assign(n, INF);
    s.co.assign(n, INF);
    for (int p : c.pis) s.cc0[p] = s.cc1[p] = 1;

    for (int g : c.order) {
        const auto& f = c.sig[g].fanins;
        int64_t sum0 = 0, sum1 = 0, min0 = INF, min1 = INF;
        for (int x : f) {
            sum0 = sat(sum0 + s.cc0[x]);
            sum1 = sat(sum1 + s.cc1[x]);
            min0 = std::min(min0, s.cc0[x]);
            min1 = std::min(min1, s.cc1[x]);
        }
        int64_t c0 = INF, c1 = INF;
        switch (c.sig[g].type) {
            case GateType::AND: c0 = min0; c1 = sum1; break;
            case GateType::NAND: c0 = sum1; c1 = min0; break;
            case GateType::OR: c0 = sum0; c1 = min1; break;
            case GateType::NOR: c0 = min1; c1 = sum0; break;
            case GateType::NOT: c0 = s.cc1[f[0]]; c1 = s.cc0[f[0]]; break;
            case GateType::BUF: c0 = s.cc0[f[0]]; c1 = s.cc1[f[0]]; break;
            case GateType::XOR:
            case GateType::XNOR: {
                // fold pairwise: cost of even (z0) / odd (z1) parity so far
                int64_t z0 = s.cc0[f[0]], z1 = s.cc1[f[0]];
                for (size_t k = 1; k < f.size(); ++k) {
                    int64_t a0 = s.cc0[f[k]], a1 = s.cc1[f[k]];
                    int64_t n0 = std::min(sat(z0 + a0), sat(z1 + a1));
                    int64_t n1 = std::min(sat(z0 + a1), sat(z1 + a0));
                    z0 = n0;
                    z1 = n1;
                }
                if (c.sig[g].type == GateType::XOR) { c0 = z0; c1 = z1; }
                else { c0 = z1; c1 = z0; }
                break;
            }
            case GateType::INPUT: break;
        }
        s.cc0[g] = sat(c0 + 1);
        s.cc1[g] = sat(c1 + 1);
    }

    for (int p : c.pos) s.co[p] = 0;
    for (auto it = c.order.rbegin(); it != c.order.rend(); ++it) {
        int g = *it;
        const Signal& sg = c.sig[g];
        if (s.co[g] >= INF) continue;  // unobservable
        for (size_t k = 0; k < sg.fanins.size(); ++k) {
            int64_t cost = s.co[g] + 1;
            for (size_t j = 0; j < sg.fanins.size(); ++j) {
                if (j == k) continue;
                int x = sg.fanins[j];
                switch (sg.type) {
                    case GateType::AND:
                    case GateType::NAND: cost = sat(cost + s.cc1[x]); break;
                    case GateType::OR:
                    case GateType::NOR: cost = sat(cost + s.cc0[x]); break;
                    case GateType::XOR:
                    case GateType::XNOR: cost = sat(cost + std::min(s.cc0[x], s.cc1[x])); break;
                    default: break;
                }
            }
            int x = sg.fanins[k];
            s.co[x] = std::min(s.co[x], cost);
        }
    }
    return s;
}
