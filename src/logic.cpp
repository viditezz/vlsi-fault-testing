// logic.cpp — fault-free logic simulators.
#include "logic.hpp"

#include <stdexcept>

void simulate_words(const Circuit& c, const std::vector<uint64_t>& pi_words,
                    std::vector<uint64_t>& vals)
{
    if (pi_words.size() != c.pis.size())
        throw std::runtime_error("simulate_words: wrong number of input words");
    vals.assign(c.sig.size(), 0);
    for (size_t i = 0; i < c.pis.size(); ++i) vals[c.pis[i]] = pi_words[i];
    for (int g : c.order) {
        const Signal& s = c.sig[g];
        const int* f = s.fanins.data();
        vals[g] = eval_word(s.type, static_cast<int>(s.fanins.size()),
                            [&](int k) { return vals[f[k]]; });
    }
}

std::vector<uint8_t> simulate_scalar(const Circuit& c, const std::vector<uint8_t>& pi_vals)
{
    if (pi_vals.size() != c.pis.size())
        throw std::runtime_error("input vector size does not match number of primary inputs");
    std::vector<uint8_t> v(c.sig.size(), 0);
    for (size_t i = 0; i < c.pis.size(); ++i) v[c.pis[i]] = pi_vals[i] ? 1 : 0;
    for (int g : c.order) {
        const Signal& s = c.sig[g];
        bool r = false;
        switch (s.type) {
            case GateType::AND:
            case GateType::NAND:
                r = true;
                for (int f : s.fanins) r = r && v[f];
                if (s.type == GateType::NAND) r = !r;
                break;
            case GateType::OR:
            case GateType::NOR:
                r = false;
                for (int f : s.fanins) r = r || v[f];
                if (s.type == GateType::NOR) r = !r;
                break;
            case GateType::XOR:
            case GateType::XNOR:
                r = false;
                for (int f : s.fanins) r = r != static_cast<bool>(v[f]);
                if (s.type == GateType::XNOR) r = !r;
                break;
            case GateType::NOT: r = !v[s.fanins[0]]; break;
            case GateType::BUF: r = v[s.fanins[0]]; break;
            case GateType::INPUT: throw std::runtime_error("INPUT in gate order");
        }
        v[g] = r ? 1 : 0;
    }
    return v;
}

std::vector<uint8_t> po_values(const Circuit& c, const std::vector<uint8_t>& vals)
{
    std::vector<uint8_t> out;
    out.reserve(c.pos.size());
    for (int p : c.pos) out.push_back(vals[p]);
    return out;
}
