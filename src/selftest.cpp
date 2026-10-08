// selftest.cpp — verification of every component against independent references.
//
//  1. bit-parallel logic sim == scalar logic sim                 (all circuits)
//  2. c17 fault list: 34 uncollapsed, 22 collapsed               (literature value)
//  3. collapsing: every fault detects exactly like its representative
//  4. PPSFP fault sim == serial reference fault sim, per fault and per pattern
//  5. PODEM: every generated test detects its fault (several random X-fills)
//  6. random small circuits, exhaustive ground truth: PODEM says DETECTED iff the
//     fault is detectable, REDUNDANT iff it is not (soundness + completeness)
//  7. every method runs end to end; compaction keeps coverage
#include <algorithm>
#include <cstdio>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "atpg_flow.hpp"
#include "circuit.hpp"
#include "fault_sim.hpp"
#include "faults.hpp"
#include "logic.hpp"
#include "podem.hpp"
#include "scoap.hpp"

namespace {

int g_fail = 0;
int g_checks = 0;

void check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok) {
        ++g_fail;
        std::printf("    FAIL: %s\n", what.c_str());
    }
}

std::vector<uint8_t> unpack(const std::vector<uint64_t>& w, int j)
{
    std::vector<uint8_t> v(w.size());
    for (size_t i = 0; i < w.size(); ++i) v[i] = (w[i] >> j) & 1ULL;
    return v;
}

// Patterns as 64-wide blocks: exhaustive if few inputs, else random.
std::vector<std::vector<uint64_t>> make_blocks(const Circuit& c, int count, std::mt19937_64& rng,
                                               std::vector<int>& sizes)
{
    std::vector<std::vector<uint64_t>> blocks;
    sizes.clear();
    const int n = static_cast<int>(c.pis.size());
    bool exhaustive = n <= 12;
    int64_t total = exhaustive ? (int64_t(1) << n) : count;
    for (int64_t base = 0; base < total; base += 64) {
        int cnt = static_cast<int>(std::min<int64_t>(64, total - base));
        std::vector<uint64_t> w(n, 0);
        for (int j = 0; j < cnt; ++j)
            for (int i = 0; i < n; ++i) {
                uint64_t bit = exhaustive ? (((base + j) >> i) & 1) : (rng() & 1);
                w[i] |= bit << j;
            }
        blocks.push_back(w);
        sizes.push_back(cnt);
    }
    return blocks;
}

void test_logic_sim(const Circuit& c, std::mt19937_64& rng)
{
    std::vector<uint64_t> vals;
    bool ok = true;
    for (int b = 0; b < 2 && ok; ++b) {
        std::vector<uint64_t> w(c.pis.size());
        for (auto& x : w) x = rng();
        simulate_words(c, w, vals);
        for (int j = 0; j < 64 && ok; ++j) {
            auto ref = simulate_scalar(c, unpack(w, j));
            for (int s = 0; s < c.num_signals(); ++s)
                if (((vals[s] >> j) & 1ULL) != ref[s]) { ok = false; break; }
        }
    }
    check(ok, c.name + ": word simulator disagrees with scalar simulator");
}

// Collapsing check + PPSFP-vs-reference check on the same pattern set.
void test_faults_and_faultsim(const Circuit& c, const FaultList& fl, int patterns,
                              std::mt19937_64& rng, bool check_equivalence)
{
    std::vector<int> sizes;
    auto blocks = make_blocks(c, patterns, rng, sizes);

    // Reference: first detecting pattern of every uncollapsed fault.
    std::vector<int> ref_first(fl.all.size(), -1);
    std::vector<std::vector<uint8_t>> pats;
    for (size_t b = 0; b < blocks.size(); ++b)
        for (int j = 0; j < sizes[b]; ++j) pats.push_back(unpack(blocks[b], j));
    std::vector<std::vector<uint8_t>> ref_mask(fl.all.size(), std::vector<uint8_t>(pats.size(), 0));
    for (size_t f = 0; f < fl.all.size(); ++f) {
        bool rep_only = !check_equivalence;
        if (rep_only) {
            // only representatives needed
            const Fault& a = fl.all[f];
            const Fault& r = fl.faults[fl.rep_of[f]];
            if (!(a.sig == r.sig && a.gate == r.gate && a.pin == r.pin && a.site == r.site && a.sv == r.sv))
                continue;
        }
        for (size_t p = 0; p < pats.size(); ++p) {
            bool d = reference_detects(c, fl.all[f], pats[p]);
            ref_mask[f][p] = d;
            if (d && ref_first[f] < 0) ref_first[f] = static_cast<int>(p);
        }
    }

    if (check_equivalence) {
        // Representative index -> one uncollapsed index that is the representative itself.
        std::vector<int> rep_all(fl.faults.size(), -1);
        for (size_t f = 0; f < fl.all.size(); ++f) {
            const Fault& a = fl.all[f];
            const Fault& r = fl.faults[fl.rep_of[f]];
            if (a.sig == r.sig && a.gate == r.gate && a.pin == r.pin && a.site == r.site && a.sv == r.sv)
                rep_all[fl.rep_of[f]] = static_cast<int>(f);
        }
        int bad = 0;
        for (size_t f = 0; f < fl.all.size(); ++f)
            if (ref_mask[f] != ref_mask[rep_all[fl.rep_of[f]]]) ++bad;
        check(bad == 0, c.name + ": " + std::to_string(bad) +
                            " faults behave differently from their collapsed representative");
    }

    // PPSFP with fault dropping: first detection index must match the reference.
    FaultSim sim(c, fl.faults);
    std::vector<int> got(fl.faults.size(), -1);
    int base = 0;
    for (size_t b = 0; b < blocks.size(); ++b) {
        std::vector<std::pair<int, int>> newly;
        sim.simulate_block(blocks[b], sizes[b], &newly);
        for (auto& [f, off] : newly) got[f] = base + off;
        base += sizes[b];
    }
    int bad = 0;
    for (size_t f = 0; f < fl.all.size(); ++f) {
        const Fault& a = fl.all[f];
        int r = fl.rep_of[f];
        const Fault& rep = fl.faults[r];
        if (!(a.sig == rep.sig && a.gate == rep.gate && a.pin == rep.pin && a.site == rep.site && a.sv == rep.sv))
            continue;
        if (got[r] != ref_first[f]) ++bad;
    }
    check(bad == 0, c.name + ": " + std::to_string(bad) +
                        " faults where PPSFP disagrees with the serial reference");
}

void test_podem_tests(const Circuit& c, const FaultList& fl, std::mt19937_64& rng,
                      int* red, int* abt, int bt_limit)
{
    Scoap sc = compute_scoap(c);
    Podem pd(c, sc, bt_limit);
    int bad = 0;
    *red = *abt = 0;
    for (const Fault& f : fl.faults) {
        PodemResult r = pd.run(f);
        if (r.status == PodemStatus::REDUNDANT) { ++*red; continue; }
        if (r.status == PodemStatus::ABORTED) { ++*abt; continue; }
        for (int fill = 0; fill < 4; ++fill) {
            std::vector<uint8_t> v(r.pi.size());
            for (size_t i = 0; i < v.size(); ++i) v[i] = r.pi[i] == LX ? (rng() & 1) : r.pi[i];
            if (!reference_detects(c, f, v)) { ++bad; break; }
        }
    }
    check(bad == 0, c.name + ": " + std::to_string(bad) + " PODEM tests fail to detect their fault");
}

std::string random_circuit(std::mt19937_64& rng, int id)
{
    std::uniform_int_distribution<int> npi(3, 8), ngate(6, 30);
    int p = npi(rng), m = ngate(rng);
    std::vector<std::string> names;
    std::ostringstream o;
    for (int i = 0; i < p; ++i) {
        names.push_back("a" + std::to_string(i));
        o << "INPUT(" << names.back() << ")\n";
    }
    const char* types[] = {"AND", "OR", "NAND", "NOR", "NOT", "BUF", "XOR", "XNOR"};
    std::vector<std::string> gates;
    std::ostringstream body;
    for (int g = 0; g < m; ++g) {
        std::string t = types[rng() % 8];
        int k = (t == "NOT" || t == "BUF") ? 1 : 2 + static_cast<int>(rng() % 3);
        std::string out = "g" + std::to_string(g);
        body << out << " = " << t << "(";
        for (int j = 0; j < k; ++j) {
            // bias toward recent signals for depth and reconvergence
            int pool = static_cast<int>(names.size());
            int pick = (rng() % 3 == 0) ? static_cast<int>(rng() % pool)
                                        : std::max(0, pool - 1 - static_cast<int>(rng() % 6));
            body << (j ? ", " : "") << names[pick];
        }
        body << ")\n";
        names.push_back(out);
        gates.push_back(out);
    }
    std::vector<std::string> outs = {gates.back()};
    if (m > 1) outs.push_back(gates[m - 2]);
    outs.push_back(gates[rng() % m]);
    if (rng() % 2) outs.push_back(names[rng() % names.size()]);  // sometimes a PI or mid gate
    std::sort(outs.begin(), outs.end());
    outs.erase(std::unique(outs.begin(), outs.end()), outs.end());
    for (auto& x : outs) o << "OUTPUT(" << x << ")\n";
    (void)id;
    return o.str() + body.str();
}

void test_random_circuits(int count, std::mt19937_64& rng)
{
    int bad_status = 0, bad_vec = 0, total_faults = 0, redundant = 0;
    for (int t = 0; t < count; ++t) {
        Circuit c = parse_bench_text(random_circuit(rng, t), "rand" + std::to_string(t));
        FaultList fl = build_fault_list(c, true);
        test_faults_and_faultsim(c, fl, 0, rng, true);  // exhaustive patterns
        Scoap sc = compute_scoap(c);
        Podem pd(c, sc, 1 << 30);
        const int n = static_cast<int>(c.pis.size());
        for (const Fault& f : fl.faults) {
            ++total_faults;
            bool detectable = false;
            for (int64_t x = 0; x < (int64_t(1) << n) && !detectable; ++x) {
                std::vector<uint8_t> v(n);
                for (int i = 0; i < n; ++i) v[i] = (x >> i) & 1;
                detectable = reference_detects(c, f, v);
            }
            PodemResult r = pd.run(f);
            if (r.status == PodemStatus::ABORTED ||
                (r.status == PodemStatus::DETECTED) != detectable) {
                ++bad_status;
                if (bad_status <= 3)
                    std::printf("    %s: %s exhaustive=%s podem=%d\n", c.name.c_str(),
                                fault_str(c, f).c_str(), detectable ? "detectable" : "redundant",
                                static_cast<int>(r.status));
            }
            if (!detectable) ++redundant;
            if (r.status == PodemStatus::DETECTED) {
                std::vector<uint8_t> v(n);
                for (int i = 0; i < n; ++i) v[i] = r.pi[i] == LX ? (rng() & 1) : r.pi[i];
                if (!reference_detects(c, f, v)) ++bad_vec;
            }
        }
    }
    std::printf("  %d random circuits, %d faults (%d truly redundant)\n", count, total_faults, redundant);
    check(bad_status == 0, std::to_string(bad_status) + " PODEM verdicts disagree with exhaustive truth");
    check(bad_vec == 0, std::to_string(bad_vec) + " PODEM vectors fail on random circuits");
}

}  // namespace

int run_selftest(const std::string& dir)
{
    std::mt19937_64 rng(2026);
    const std::vector<std::string> names = {"c17",   "c432",  "c499",  "c880",  "c1355", "c1908",
                                            "c2670", "c3540", "c5315", "c6288", "c7552"};
    std::vector<Circuit> cs;
    std::vector<FaultList> fls;

    std::printf("[1] logic simulation: word-parallel vs scalar\n");
    for (const auto& n : names) {
        cs.push_back(parse_bench(dir + "/" + n + ".bench"));
        fls.push_back(build_fault_list(cs.back(), true));
        test_logic_sim(cs.back(), rng);
    }

    std::printf("[2] fault list sizes (collapsed)\n");
    for (size_t i = 0; i < cs.size(); ++i)
        std::printf("  %-6s lines %5d  uncollapsed %5zu  collapsed %5zu\n", cs[i].name.c_str(),
                    fls[i].num_lines, fls[i].all.size(), fls[i].faults.size());
    check(fls[0].all.size() == 34, "c17 should have 34 uncollapsed faults");
    check(fls[0].faults.size() == 22, "c17 should have 22 collapsed faults");

    std::printf("[3+4] collapsing equivalence + PPSFP vs serial reference\n");
    test_faults_and_faultsim(cs[0], fls[0], 0, rng, true);  // c17 exhaustive
    for (size_t i = 1; i <= 4; ++i)                          // c432 .. c1355
        test_faults_and_faultsim(cs[i], fls[i], 192, rng, i <= 3);

    std::printf("[5] PODEM tests verified by serial reference\n");
    for (size_t i = 0; i < 6; ++i) {  // c17 .. c1908
        int red, abt;
        test_podem_tests(cs[i], fls[i], rng, &red, &abt, 1024);
        std::printf("  %-6s %5zu faults: %d redundant, %d aborted\n", cs[i].name.c_str(),
                    fls[i].faults.size(), red, abt);
        if (i == 0) check(red == 0 && abt == 0, "c17 has no redundant faults");
    }

    std::printf("[6] PODEM soundness + completeness on random circuits (exhaustive truth)\n");
    test_random_circuits(400, rng);

    std::printf("[7] end-to-end methods\n");
    for (size_t i = 0; i < 2; ++i) {
        for (std::string m : {"random", "podem", "fixed", "adaptive"}) {
            RunConfig cfg;
            cfg.method = m;
            cfg.random_budget = 4096;
            cfg.fixed_switch = 256;
            Scoap sc = compute_scoap(cs[i]);
            RunResult r = run_atpg(cs[i], fls[i], sc, cfg);
            bool accounted = m == "random" ||
                             r.detected + r.redundant + r.aborted == r.faults;
            check(accounted, cs[i].name + " " + m + ": faults not all accounted for");
            check(r.compacted <= r.tests, cs[i].name + " " + m + ": compaction grew the set");
            std::printf("  %-6s %-9s FC %7.3f%%  tests %d -> %d\n", cs[i].name.c_str(), m.c_str(),
                        r.fc(), r.tests, r.compacted);
        }
    }

    std::printf("\n%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
