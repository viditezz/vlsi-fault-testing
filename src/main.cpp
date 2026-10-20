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
// `faultatpg` with no arguments prints every option and its default.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
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
        "  faultatpg run       <bench> [options]\n"
        "  faultatpg sweep     <bench_dir> <out_dir> [options]\n"
        "  faultatpg selftest  <bench_dir>\n"
        "  faultatpg calibrate <bench>\n"
        "run options:\n"
        "  --method M         random | podem | fixed | plateau | stage1 | adaptive   (adaptive)\n"
        "  --seed N           random seed                                             (1)\n"
        "  --budget N         random pattern cap                                      (32768)\n"
        "  --switch N         fixed: random patterns before PODEM                     (1024)\n"
        "  --bt N             PODEM backtrack limit                                   (1024)\n"
        "  --alpha A          adaptive/stage1: significance level, 0 < A < 1          (0.05)\n"
        "  --confirm N        consecutive rejections needed to switch                 (2)\n"
        "  --window MU        break-even detections a test window must reach          (6)\n"
        "  --probe N          PODEM calls per yield probe (adaptive)                  (16)\n"
        "  --warmup N         random patterns before the first test                   (128)\n"
        "  --stage2 0|1       adaptive: measure PODEM before switching                (1)\n"
        "  --plateau-blocks W plateau: window length in 64-pattern blocks             (6)\n"
        "  --plateau-max K    plateau: switch when the window found <= K faults       (1)\n"
        "  --weight W         cost of one PODEM evaluation vs one fault-sim evaluation (1)\n"
        "  --resolve 0|1      SAT fallback for PODEM aborts (not used by random)      (1)\n"
        "  --sat-limit N      conflict limit per SAT call                             (2000000)\n"
        "  --curve FILE       write the coverage-vs-work curve\n"
        "  --tests FILE       write the compacted test set\n"
        "sweep options: --seeds N  --circuits a,b,...  --methods m1,m2,...  --grid fine|coarse\n"
        "               --switches n1,n2,... (fixed points to run instead of the grid; raise --budget above 32768)\n"
        "               --quiet 1 (summary.csv only), plus every run option except\n"
        "               --method, --seed, --curve and --tests\n";
}

const std::set<std::string> kConfigKeys = {
    "budget", "switch", "bt", "alpha", "confirm", "window", "probe", "warmup", "stage2",
    "plateau-blocks", "plateau-max", "weight", "resolve", "sat-limit"};

// Parses "--key value" pairs; anything not in `allowed` is an error (a typo
// must not silently fall back to a default).
std::map<std::string, std::string> parse_opts(int argc, char** argv, int start,
                                              const std::set<std::string>& allowed)
{
    std::map<std::string, std::string> o;
    for (int i = start; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--", 0) != 0) throw std::runtime_error("unexpected argument: " + a);
        std::string key = a.substr(2);
        if (!allowed.count(key)) throw std::runtime_error("unknown option: " + a + " (see usage)");
        if (i + 1 >= argc) throw std::runtime_error("option " + a + " needs a value");
        o[key] = argv[++i];
    }
    return o;
}

template <class T>
T number(const std::string& key, const std::string& v, T lo, T hi)
{
    T x{};
    try {
        size_t used = 0;
        if constexpr (std::is_floating_point_v<T>) x = static_cast<T>(std::stod(v, &used));
        else x = static_cast<T>(std::stoll(v, &used));
        if (used != v.size()) throw std::invalid_argument(v);
    } catch (const std::exception&) {
        throw std::runtime_error("--" + key + ": '" + v + "' is not a number");
    }
    if (x < lo || x > hi)
        throw std::runtime_error("--" + key + ": " + v + " is out of range");
    return x;
}

RunConfig config_from(const std::map<std::string, std::string>& o)
{
    RunConfig cfg;
    const int64_t big = int64_t(1) << 40;
    for (const auto& [k, v] : o) {
        if (k == "method") {
            static const std::set<std::string> ok = {"random", "podem", "fixed", "plateau",
                                                     "stage1", "adaptive"};
            if (!ok.count(v)) throw std::runtime_error("--method: unknown method '" + v + "'");
            cfg.method = v;
        }
        else if (k == "seed") cfg.seed = static_cast<uint64_t>(number<int64_t>(k, v, 0, big));
        else if (k == "budget") cfg.random_budget = number<int64_t>(k, v, 64, big);
        else if (k == "switch") cfg.fixed_switch = number<int64_t>(k, v, 0, big);
        else if (k == "bt") cfg.backtrack_limit = number<int>(k, v, 1, 1 << 30);
        else if (k == "alpha") cfg.alpha = number<double>(k, v, 1e-12, 0.999999);
        else if (k == "confirm") cfg.confirm = number<int>(k, v, 1, 1000);
        else if (k == "window") cfg.window_mu = number<double>(k, v, 0.1, 1e6);
        else if (k == "stage2") cfg.stage2 = number<int>(k, v, 0, 1) != 0;
        else if (k == "probe") cfg.probe_size = number<int>(k, v, 1, 100000);
        else if (k == "warmup") cfg.warmup = number<int64_t>(k, v, 0, big);
        else if (k == "weight") cfg.podem_weight = number<double>(k, v, 1e-6, 1e6);
        else if (k == "plateau-blocks") cfg.plateau_blocks = number<int>(k, v, 1, 100000);
        else if (k == "plateau-max") cfg.plateau_max = number<int>(k, v, 0, 100000000);
        else if (k == "resolve") cfg.resolve = number<int>(k, v, 0, 1) != 0;
        else if (k == "sat-limit") cfg.sat_conflicts = number<int64_t>(k, v, 1, big);
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
    std::printf("         tests %d -> %d compacted   work %.3g   time %.2f ms (random %.2f, podem %.2f, sat %.2f)\n",
                r.tests, r.compacted, r.work_total, r.ms_total, r.ms_random, r.ms_podem, r.ms_sat);
    if (r.sat_calls > 0)
        std::printf("         SAT fallback: %d PODEM aborts -> %d tests, %d proved redundant, %.3g propagation steps\n",
                    r.sat_calls, r.sat_vectors, r.sat_redundant, r.work_sat);
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
    int seeds = o.count("seeds") ? number<int>("seeds", o.at("seeds"), 1, 10000) : 10;
    if (o.count("grid") && o.at("grid") != "fine" && o.at("grid") != "coarse")
        throw std::runtime_error("--grid: expected fine or coarse");
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
    if (o.count("switches")) {  // explicit list of fixed switch points, e.g. to extend the grid
        grid.clear();
        std::stringstream ss(o.at("switches"));
        std::string v;
        while (std::getline(ss, v, ',')) {
            int64_t n = number<int64_t>("switches", v, 64, base.random_budget);
            grid.push_back(n);
        }
    }
    std::string want = o.count("methods") ? o.at("methods") : "random,podem,fixed,plateau,stage1,adaptive";
    {
        std::stringstream ss(want);
        std::string m;
        static const std::set<std::string> ok = {"random", "podem", "fixed", "plateau", "stage1", "adaptive"};
        while (std::getline(ss, m, ','))
            if (!ok.count(m)) throw std::runtime_error("--methods: unknown method '" + m + "'");
    }
    auto wanted = [&](const std::string& m) { return ("," + want + ",").find("," + m + ",") != std::string::npos; };
    std::vector<M> methods;
    if (wanted("random")) methods.push_back({"random", 0});
    if (wanted("podem")) methods.push_back({"podem", 0});
    if (wanted("fixed"))
        for (int64_t n : grid) methods.push_back({"fixed", n});
    if (wanted("plateau")) methods.push_back({"plateau", 0});
    if (wanted("stage1")) methods.push_back({"stage1", 0});
    if (wanted("adaptive")) methods.push_back({"adaptive", 0});
    bool quiet = o.count("quiet") > 0 && o.at("quiet") != "0";

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
        if (cmd == "run") {
            std::set<std::string> allowed = kConfigKeys;
            allowed.insert({"method", "seed", "curve", "tests"});
            return cmd_run(argv[2], parse_opts(argc, argv, 3, allowed));
        }
        if (cmd == "sweep") {
            if (argc < 4) { usage(); return 1; }
            std::set<std::string> allowed = kConfigKeys;
            allowed.insert({"seeds", "circuits", "methods", "grid", "quiet", "switches"});
            return cmd_sweep(argv[2], argv[3], parse_opts(argc, argv, 4, allowed));
        }
        usage();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
