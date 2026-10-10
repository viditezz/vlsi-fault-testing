// main.cpp — command-line driver.
//
//   faultatpg info      <bench>                 circuit + fault-list statistics
//   faultatpg sim       <bench> <bits>          fault-free simulation of one vector
//   faultatpg faults    <bench>                 list collapsed faults
//   faultatpg run       <bench> [options]       one ATPG run
//   faultatpg sweep     <bench_dir> <out_dir> [options]
//   faultatpg selftest  <bench_dir>
//   faultatpg calibrate <bench>                 ns per work unit (fault sim vs PODEM)
//
// Options: --method random|podem|fixed|plateau|stage1|adaptive  --seed N  --budget N
//          --switch N  --bt N  --alpha A  --confirm N  --probe N  --warmup N  --window MU
//          --weight W  --plateau-blocks W  --plateau-max K
//          --curve FILE  --tests FILE
// Sweep:   --seeds N  --circuits c17,c432,...  --methods random,podem,fixed,plateau,stage1,adaptive
//          --quiet 1 (summary.csv only)
//          --grid fine|coarse   (fixed switch points: 19 from 64 to 32768, or 128/1024/8192)
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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

int run_selftest(const std::string& bench_dir);

namespace fs = std::filesystem;

namespace {

void usage()
{
    std::cerr <<
        "usage:\n"
        "  faultatpg info      <bench>\n"
        "  faultatpg sim       <bench> <bits>      e.g. sim c17.bench 01101\n"
        "  faultatpg faults    <bench>\n"
        "  faultatpg run       <bench> [--method M] [--seed N] [--budget N] [--switch N]\n"
        "                      [--bt N] [--alpha A] [--confirm N] [--probe N] [--warmup N]\n"
        "                      [--weight W] [--curve FILE] [--tests FILE]\n"
        "  faultatpg sweep     <bench_dir> <out_dir> [--seeds N] [--circuits a,b,...] [--bt N]\n"
        "  faultatpg selftest  <bench_dir>\n"
        "  faultatpg calibrate <bench>\n"
        "methods: random | podem | fixed | adaptive\n";
}

std::map<std::string, std::string> parse_opts(int argc, char** argv, int start)
{
    std::map<std::string, std::string> o;
    for (int i = start; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--", 0) != 0 || i + 1 >= argc)
            throw std::runtime_error("bad option: " + a);
        o[a.substr(2)] = argv[++i];
    }
    return o;
}

RunConfig config_from(const std::map<std::string, std::string>& o)
{
    RunConfig cfg;
    for (const auto& [k, v] : o) {
        if (k == "method") cfg.method = v;
        else if (k == "seed") cfg.seed = std::stoull(v);
        else if (k == "budget") cfg.random_budget = std::stoll(v);
        else if (k == "switch") cfg.fixed_switch = std::stoll(v);
        else if (k == "bt") cfg.backtrack_limit = std::stoi(v);
        else if (k == "alpha") cfg.alpha = std::stod(v);
        else if (k == "confirm") cfg.confirm = std::stoi(v);
        else if (k == "window") cfg.window_mu = std::stod(v);
        else if (k == "stage2") cfg.stage2 = v != "0";
        else if (k == "probe") cfg.probe_size = std::stoi(v);
        else if (k == "warmup") cfg.warmup = std::stoll(v);
        else if (k == "weight") cfg.podem_weight = std::stod(v);
        else if (k == "plateau-blocks") cfg.plateau_blocks = std::stoi(v);
        else if (k == "plateau-max") cfg.plateau_max = std::stoi(v);
    }
    return cfg;
}

void print_circuit_stats(const Circuit& c, const FaultList& fl)
{
    std::map<std::string, int> types;
    for (int g : c.order) types[gate_name(c.sig[g].type)]++;
    std::cout << "circuit      " << c.name << "\n"
              << "inputs       " << c.pis.size() << "\n"
              << "outputs      " << c.pos.size() << "\n"
              << "gates        " << c.num_gates() << "  (";
    bool first = true;
    for (auto& [t, n] : types) {
        std::cout << (first ? "" : ", ") << t << " " << n;
        first = false;
    }
    std::cout << ")\n"
              << "depth        " << c.max_level << " levels\n"
              << "lines        " << fl.num_lines << "  (stems + fanout branches)\n"
              << "faults       " << fl.all.size() << " uncollapsed, " << fl.faults.size()
              << " after equivalence collapsing\n";
}

int cmd_info(const std::string& path)
{
    Circuit c = parse_bench(path);
    FaultList fl = build_fault_list(c, true);
    print_circuit_stats(c, fl);
    if (c.num_gates() <= 40) {
        std::cout << "\nlevelized gates:\n";
        for (int g : c.order) {
            const Signal& s = c.sig[g];
            std::cout << "  L" << s.level << "  " << s.name << " = " << gate_name(s.type) << "(";
            for (size_t k = 0; k < s.fanins.size(); ++k)
                std::cout << (k ? ", " : "") << c.sig[s.fanins[k]].name;
            std::cout << ")\n";
        }
    }
    return 0;
}

int cmd_sim(const std::string& path, const std::string& bits)
{
    Circuit c = parse_bench(path);
    if (bits.size() != c.pis.size())
        throw std::runtime_error("need " + std::to_string(c.pis.size()) + " input bits");
    std::vector<uint8_t> v;
    for (char ch : bits) {
        if (ch != '0' && ch != '1') throw std::runtime_error("bits must be 0/1");
        v.push_back(ch == '1');
    }
    auto vals = simulate_scalar(c, v);
    std::cout << "inputs : ";
    for (size_t i = 0; i < c.pis.size(); ++i) std::cout << c.sig[c.pis[i]].name << "=" << int(v[i]) << " ";
    std::cout << "\noutputs: ";
    for (int p : c.pos) std::cout << c.sig[p].name << "=" << int(vals[p]) << " ";
    std::cout << "\n";
    return 0;
}

int cmd_faults(const std::string& path)
{
    Circuit c = parse_bench(path);
    FaultList fl = build_fault_list(c, true);
    for (size_t i = 0; i < fl.faults.size(); ++i)
        std::cout << i << "\t" << fault_str(c, fl.faults[i]) << "\t(class of "
                  << fl.faults[i].class_size << ")\n";
    return 0;
}

void write_tests(const std::string& path, const Circuit& c, const RunResult& r)
{
    std::ofstream f(path);
    f << "# " << r.circuit << " " << r.method << " seed " << r.seed << ": " << r.test_set.size()
      << " compacted tests, FC " << r.fc() << "%\n# inputs:";
    for (int p : c.pis) f << " " << c.sig[p].name;
    f << "\n";
    for (const auto& v : r.test_set) {
        for (uint8_t b : v) f << int(b);
        f << "\n";
    }
}

void print_result(const RunResult& r)
{
    std::printf("%-8s %-12s seed %-3llu FC %7.3f%%  FE %7.3f%%  det %d/%d  red %d  abort %d\n",
                r.circuit.c_str(),
                (r.method + (r.method == "fixed" ? "-" + std::to_string(r.param) : "")).c_str(),
                static_cast<unsigned long long>(r.seed), r.fc(), r.fe(), r.detected, r.faults,
                r.redundant, r.aborted);
    std::printf("         random applied %lld (kept %d)  podem calls %d (vectors %d, probe %d)\n",
                static_cast<long long>(r.random_applied), r.random_kept, r.podem_calls,
                r.podem_vectors, r.probe_calls);
    std::printf("         tests %d -> %d compacted   work %.3g   time %.2f ms (random %.2f, podem %.2f)\n",
                r.tests, r.compacted, r.work_total, r.ms_total, r.ms_random, r.ms_podem);
    if (r.method == "adaptive" || r.method == "plateau" || r.method == "stage1")
        std::printf("         switched after %lld random patterns\n",
                    static_cast<long long>(r.switch_at));
}

int cmd_run(const std::string& path, const std::map<std::string, std::string>& o)
{
    Circuit c = parse_bench(path);
    FaultList fl = build_fault_list(c, true);
    Scoap sc = compute_scoap(c);
    RunConfig cfg = config_from(o);
    RunResult r = run_atpg(c, fl, sc, cfg);
    print_result(r);
    if (!r.decisions.empty()) {
        std::cout << "\nstagnation test log:\n";
        for (auto& d : r.decisions) std::cout << "  " << d << "\n";
    }
    if (o.count("curve")) write_curve(o.at("curve"), r);
    if (o.count("tests")) write_tests(o.at("tests"), c, r);
    return 0;
}

int cmd_sweep(const std::string& dir, const std::string& out,
              const std::map<std::string, std::string>& o)
{
    std::vector<std::string> circuits = {"c17",   "c432",  "c499",  "c880",  "c1355", "c1908",
                                         "c2670", "c3540", "c5315", "c6288", "c7552"};
    if (o.count("circuits")) {
        circuits.clear();
        std::stringstream ss(o.at("circuits"));
        std::string x;
        while (std::getline(ss, x, ',')) circuits.push_back(x);
    }
    int seeds = o.count("seeds") ? std::stoi(o.at("seeds")) : 5;
    RunConfig base = config_from(o);

    struct M { std::string method; int64_t sw; };
    std::vector<int64_t> grid = {128, 1024, 8192};
    if (!o.count("grid") || o.at("grid") == "fine") {
        grid.clear();  // 64 * {1, 1.5, 2, 3, 4, 6, ...} up to 32768
        for (int64_t n = 64; n <= 32768; n *= 2) {
            grid.push_back(n);
            if (n * 3 / 2 <= 32768 && n < 32768) grid.push_back(n * 3 / 2);
        }
    }
    std::string want = o.count("methods") ? o.at("methods") : "random,podem,fixed,plateau,stage1,adaptive";
    auto wanted = [&](const std::string& m) { return ("," + want + ",").find("," + m + ",") != std::string::npos; };
    std::vector<M> methods;
    if (wanted("random")) methods.push_back({"random", 0});
    if (wanted("podem")) methods.push_back({"podem", 0});
    if (wanted("fixed"))
        for (int64_t n : grid) methods.push_back({"fixed", n});
    if (wanted("plateau")) methods.push_back({"plateau", 0});
    if (wanted("stage1")) methods.push_back({"stage1", 0});
    if (wanted("adaptive")) methods.push_back({"adaptive", 0});
    bool quiet = o.count("quiet") > 0;

    fs::create_directories(out);
    if (!quiet) {
        fs::create_directories(out + "/curves");
        fs::create_directories(out + "/decisions");
        fs::create_directories(out + "/tests");
    }
    std::ofstream sum(out + "/summary.csv");
    sum << summary_header() << "\n";
    std::ofstream info(out + "/circuits.csv");
    info << "circuit,inputs,outputs,gates,depth,lines,faults_uncollapsed,faults_collapsed\n";

    for (const auto& name : circuits) {
        std::string path = dir + "/" + name + ".bench";
        Circuit c = parse_bench(path);
        FaultList fl = build_fault_list(c, true);
        Scoap sc = compute_scoap(c);
        info << c.name << ',' << c.pis.size() << ',' << c.pos.size() << ',' << c.num_gates()
             << ',' << c.max_level << ',' << fl.num_lines << ',' << fl.all.size() << ','
             << fl.faults.size() << "\n";
        info.flush();
        for (const M& m : methods) {
            for (int s = 1; s <= seeds; ++s) {
                RunConfig cfg = base;
                cfg.method = m.method;
                cfg.fixed_switch = m.sw;
                cfg.seed = static_cast<uint64_t>(s);
                RunResult r = run_atpg(c, fl, sc, cfg);
                sum << summary_row(r) << "\n";
                sum.flush();
                std::string label = m.method + (m.method == "fixed" ? "-" + std::to_string(m.sw) : "");
                bool headline = m.method != "fixed" || m.sw == 128 || m.sw == 1024 || m.sw == 8192;
                if (s == 1 && headline && !quiet) {
                    write_curve(out + "/curves/" + name + "_" + label + ".csv", r);
                    if (m.method == "adaptive") {
                        std::ofstream d(out + "/decisions/" + name + ".txt");
                        for (auto& line : r.decisions) d << line << "\n";
                        write_tests(out + "/tests/" + name + "_adaptive.txt", c, r);
                    }
                }
                if (quiet) continue;
                std::printf("%-6s %-12s s%d  FC %7.3f%%  FE %7.3f%%  tests %5d  work %10.3g  %9.1f ms%s\n",
                            name.c_str(), label.c_str(), s, r.fc(), r.fe(), r.compacted,
                            r.work_total, r.ms_total,
                            m.method == "adaptive" || m.method == "plateau" || m.method == "stage1"
                                ? ("  switch@" + std::to_string(r.switch_at)).c_str()
                                : "");
                std::fflush(stdout);
            }
        }
    }
    return 0;
}

// Measures wall-clock nanoseconds per work unit for the fault simulator and PODEM,
// so that their work counters can be put on a common scale.
int cmd_calibrate(const std::string& path)
{
    using Clock = std::chrono::steady_clock;
    Circuit c = parse_bench(path);
    FaultList fl = build_fault_list(c, true);
    Scoap sc = compute_scoap(c);

    FaultSim fsim(c, fl.faults);
    std::mt19937_64 rng(7);
    auto t0 = Clock::now();
    for (int b = 0; b < 256; ++b) {
        std::vector<uint64_t> w(c.pis.size());
        for (auto& x : w) x = rng();
        fsim.simulate_block(w, 64);
    }
    double ns_fs = std::chrono::duration<double, std::nano>(Clock::now() - t0).count() / fsim.work();

    Podem pd(c, sc, 1024);
    t0 = Clock::now();
    int calls = 0;
    for (size_t f = 0; f < fl.faults.size() && calls < 400; f += 1 + fl.faults.size() / 400, ++calls)
        pd.run(fl.faults[f]);
    double ns_pd = std::chrono::duration<double, std::nano>(Clock::now() - t0).count() / pd.work();

    std::printf("%s: fault-sim %.2f ns/unit, PODEM %.2f ns/unit, ratio %.2f\n", c.name.c_str(),
                ns_fs, ns_pd, ns_pd / ns_fs);
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        usage();
        return 1;
    }
    std::string cmd = argv[1];
    try {
        if (cmd == "info") return cmd_info(argv[2]);
        if (cmd == "faults") return cmd_faults(argv[2]);
        if (cmd == "calibrate") return cmd_calibrate(argv[2]);
        if (cmd == "selftest") return run_selftest(argv[2]);
        if (cmd == "sim") {
            if (argc != 4) { usage(); return 1; }
            return cmd_sim(argv[2], argv[3]);
        }
        if (cmd == "run") return cmd_run(argv[2], parse_opts(argc, argv, 3));
        if (cmd == "sweep") {
            if (argc < 4) { usage(); return 1; }
            return cmd_sweep(argv[2], argv[3], parse_opts(argc, argv, 4));
        }
        usage();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
