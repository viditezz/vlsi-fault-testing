// atpg_flow.cpp
#include "atpg_flow.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

#include "fault_sim.hpp"
#include "logic.hpp"
#include "podem.hpp"
#include "stagnation.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

class Session {
public:
    Session(const Circuit& c, const FaultList& fl, const Scoap& sc, const RunConfig& cfg)
        : c_(c), fl_(fl), cfg_(cfg), fs_(c, fl.faults), podem_(c, sc, cfg.backtrack_limit),
          rng_(cfg.seed * 0x9E3779B97F4A7C15ULL + 12345)
    {
        R_.circuit = c.name;
        R_.method = cfg.method;
        R_.seed = cfg.seed;
        R_.faults = static_cast<int>(fl.faults.size());
        abandoned_.assign(fl.faults.size(), 0);
        abort_work_.assign(fl.faults.size(), 0.0);
        t0_ = Clock::now();
        R_.curve.push_back({0, 0, 0, 0, 'R'});
    }

    double cost() const
    {
        return static_cast<double>(fs_.work()) + cfg_.podem_weight * podem_.work();
    }

    // One block of n random patterns. Keeps only patterns that detect something new.
    int random_block(int n)
    {
        double w0 = cost();
        std::vector<uint64_t> words(c_.pis.size());
        for (auto& w : words) w = rng_();
        std::vector<std::pair<int, int>> newly;
        int found = fs_.simulate_block(words, n, &newly);

        std::vector<int> per(n, 0);
        for (auto& [f, off] : newly) per[off]++;
        int cum = fs_.num_detected() - found;
        double w1 = cost();
        R_.work_random += w1 - w0;
        for (int j = 0; j < n; ++j) {
            if (!per[j]) continue;
            std::vector<uint8_t> v(c_.pis.size());
            for (size_t i = 0; i < v.size(); ++i) v[i] = (words[i] >> j) & 1ULL;
            tests_.push_back(std::move(v));
            ++R_.random_kept;
            cum += per[j];
            R_.curve.push_back({patterns_ + j + 1, cum, w1, ms_since(t0_), 'R'});
        }
        patterns_ += n;
        R_.random_applied += n;
        last_block_work_ = w1 - w0;
        return found;
    }

    // One PODEM call on fault f, plus fault simulation of the resulting vector.
    // Returns the status; *spent receives the work this call cost.
    PodemStatus target(int f, char phase, double* spent = nullptr)
    {
        double w0 = cost();
        PodemResult res = podem_.run(fl_.faults[f]);
        ++R_.podem_calls;
        if (phase == 'p') ++R_.probe_calls;
        if (res.status == PodemStatus::DETECTED) {
            std::vector<uint8_t> v(res.pi.size());
            for (size_t i = 0; i < v.size(); ++i)
                v[i] = res.pi[i] == LX ? static_cast<uint8_t>(rng_() & 1ULL) : res.pi[i];
            fs_.simulate_vector(v);
            if (!fs_.is_detected(f))
                throw std::logic_error("PODEM vector does not detect its target fault: " +
                                       fault_str(c_, fl_.faults[f]));
            tests_.push_back(std::move(v));
            ++R_.podem_vectors;
            ++patterns_;
            R_.curve.push_back({patterns_, fs_.num_detected(), work_total() + (cost() - w0),
                                ms_since(t0_), phase});
        } else if (res.status == PodemStatus::REDUNDANT) {
            fs_.retire(f);  // proven undetectable: no vector can ever detect it
            ++R_.redundant;
            R_.work_unresolved += cost() - w0;
        } else {
            // Aborted: stop targeting it, but keep it in fault simulation so a
            // later vector can still detect it by accident.
            abandoned_[f] = 1;
            abort_work_[f] += cost() - w0;  // unresolved only if it stays undetected
        }
        R_.work_podem += cost() - w0;
        if (spent) *spent = cost() - w0;
        return res.status;
    }

    void podem_phase()
    {
        auto t = Clock::now();
        std::vector<int> order = fs_.pending();
        std::sort(order.begin(), order.end());  // deterministic: fault-list order
        for (int f : order)
            if (fs_.is_pending(f) && !abandoned_[f]) target(f, 'P');
        R_.ms_podem += ms_since(t);
    }

    void run_random(int64_t limit)
    {
        auto t = Clock::now();
        while (fs_.num_pending() > 0 && R_.random_applied < limit) random_block(64);
        R_.ms_random += ms_since(t);
    }

    // Measures PODEM's current yield on a random sample of the faults random
    // testing has not found: faults detected per unit of work, counting only the
    // work of PODEM calls that produced a test.
    //
    // Why only those: a random detection saves exactly the PODEM call that would
    // otherwise have detected that fault. Work PODEM spends proving faults
    // redundant or aborting them is paid whenever the switch happens (random can
    // never resolve those faults), so it must not enter the comparison — otherwise
    // a residue full of redundant faults makes PODEM look hopeless and keeps
    // random running long after it has stopped finding anything.
    //
    // The probe's vectors are real tests and stay in the test set; its redundant
    // and aborted verdicts are final. None of the probe is wasted work.
    struct Probe {
        double yield = 0;   // faults detected per unit of work (detecting calls only)
        int calls = 0;
        int tests = 0;      // calls that produced a test
    };

    Probe probe()
    {
        auto t = Clock::now();
        std::vector<int> pool;
        for (int f : fs_.pending())
            if (!abandoned_[f]) pool.push_back(f);
        std::sort(pool.begin(), pool.end());
        std::shuffle(pool.begin(), pool.end(), rng_);
        double det_work = 0, all_work = 0;
        int det = 0, calls = 0, tests = 0;
        for (int f : pool) {
            // Up to probe_size calls; extend to 3x if no call has produced a test yet.
            if (calls >= cfg_.probe_size && (det > 0 || calls >= 3 * cfg_.probe_size)) break;
            if (!fs_.is_pending(f) || abandoned_[f]) continue;
            int d0 = fs_.num_detected();
            double w = 0;
            PodemStatus st = target(f, 'p', &w);
            ++calls;
            all_work += w;
            if (st == PodemStatus::DETECTED) {
                ++tests;
                det += fs_.num_detected() - d0;  // the target plus anything it dropped
                det_work += w;
            }
        }
        R_.ms_podem += ms_since(t);
        (void)all_work;
        Probe p;
        p.calls = calls;
        p.tests = tests;
        p.yield = det > 0 ? det / std::max(det_work, 1.0) : 0.0;
        return p;
    }

    // Plateau baseline: switch once the last `plateau_blocks` random blocks
    // together found at most `plateau_max` new faults. No statistics, no PODEM
    // measurement — a fixed rule of thumb, used to check what the adaptive
    // rule's statistics and measurements actually add.
    void run_plateau()
    {
        auto t = Clock::now();
        std::vector<int> hist;
        while (targetable() > 0 && R_.random_applied < cfg_.random_budget) {
            hist.push_back(random_block(64));
            if (R_.random_applied < cfg_.warmup) continue;
            const int w = cfg_.plateau_blocks;
            if (static_cast<int>(hist.size()) < w) continue;
            int sum = 0;
            for (int i = 0; i < w; ++i) sum += hist[hist.size() - 1 - i];
            if (sum <= cfg_.plateau_max) break;
        }
        R_.ms_random += ms_since(t);
        R_.switch_at = R_.random_applied;
        podem_phase();
    }

    // Adaptive switch.
    //
    // Both engines remove faults from the target list. Random does it at a rate
    // lambda (faults per unit of work, observed block by block); PODEM does it at
    // a yield y (faults per unit of work on calls that produce a test, measured by
    // a probe). Each fault random finds saves the PODEM call that would have found
    // it, so random is worth continuing while lambda > y.
    //
    // Every decision uses the same window test: a window grows block by block
    // until the break-even expectation mu = y * R reaches window_mu, then
    // H0: lambda >= y is rejected when P(K <= k | Poisson(mu)) < alpha.
    //
    //  Stage 1 (PODEM not measured yet). Measuring PODEM is not free: a probe
    //  early on spends PODEM work on easy faults random would find for less. So
    //  stage 1 tests random against a cheap upper bound on PODEM's yield instead.
    //  A deterministic vector has to be fault-simulated, and simulating one vector
    //  costs about as much as a 64-pattern block (bit-parallel), so PODEM rarely
    //  removes more than about one fault per block's worth of work. While random
    //  finds clearly more than that, PODEM is not worth measuring.
    //
    //  Stage 2 (after stage 1 rejects). Probe PODEM on a random sample of the
    //  targetable faults to measure y, then test random against it; switch after
    //  `confirm` consecutive rejections. Re-probe when the targetable set halves.
    //
    // Two shortcuts, from the same cost argument:
    //  - A probe that produces no test at all (up to 3x probe_size calls) means
    //    the residue is (almost) all undetectable. Random cannot find what is not
    //    there, so switch now; what PODEM does next is redundancy proofs, which
    //    every flow pays for.
    //  - With T targetable faults left, the most random can ever save is T/y,
    //    while one test window costs window_mu/y. If T < window_mu, a window
    //    cannot pay for itself, so switch.
    void run_adaptive()
    {
        StagnationDetector det(cfg_.alpha, cfg_.confirm, cfg_.window_mu);
        double yield = -1;  // < 0: stage 1
        int targetable_at_probe = 0;
        auto t = Clock::now();
        auto log = [&](const std::string& s) {
            R_.decisions.push_back("pattern " + std::to_string(R_.random_applied) + ": " + s);
        };
        while (targetable() > 0 && R_.random_applied < cfg_.random_budget) {
            det.add_block(random_block(64), last_block_work_);
            if (R_.random_applied < cfg_.warmup || targetable() == 0) continue;

            bool need_probe = yield >= 0 && targetable() * 2 <= targetable_at_probe;
            if (yield < 0) {
                StagnationDecision d;
                det.test(1.0 / std::max(last_block_work_, 1.0), &d);
                if (d.enough_evidence) {
                    std::ostringstream o;
                    o << "stage 1  k=" << d.k << " over " << d.blocks
                      << " blocks (bound: 1 per block), p=" << d.p
                      << (d.reject ? "  REJECT -> probe PODEM" : "");
                    log(o.str());
                }
                if (!(d.enough_evidence && d.reject)) continue;
                if (!cfg_.stage2) {
                    log("stage 2 disabled -> SWITCH");
                    break;
                }
                need_probe = true;
            }
            if (need_probe) {
                R_.ms_random += ms_since(t);
                Probe p = probe();
                t = Clock::now();
                yield = p.yield;
                targetable_at_probe = targetable();
                det.restart();
                std::ostringstream o;
                o << "probe " << p.calls << " PODEM calls, " << p.tests
                  << " tests -> yield " << yield << " det/work, targetable " << targetable();
                if (p.tests == 0) o << "  NO TESTS -> SWITCH";
                log(o.str());
                if (p.tests == 0 || targetable() == 0) break;
                continue;
            }
            if (targetable() < cfg_.window_mu) {
                std::ostringstream o;
                o << "targetable " << targetable() << " < " << cfg_.window_mu
                  << ": a test window costs more than PODEM on all of them -> SWITCH";
                log(o.str());
                break;
            }
            StagnationDecision d;
            bool fire = det.test(yield, &d);
            if (d.enough_evidence) {
                std::ostringstream o;
                o << "stage 2  k=" << d.k << " over " << d.blocks << " blocks, break-even mu="
                  << d.mu << ", p=" << d.p << (d.reject ? "  REJECT" : "")
                  << (fire ? "  -> SWITCH" : "");
                log(o.str());
            }
            if (fire) break;
        }
        R_.ms_random += ms_since(t);
        R_.switch_at = R_.random_applied;
        podem_phase();
    }

    // Reverse-order fault simulation: a test is kept only if, simulated in reverse
    // order, it is the first to detect at least one fault. Also re-verifies coverage.
    void compact()
    {
        auto t = Clock::now();
        FaultSim fs2(c_, fl_.faults);
        const int n = static_cast<int>(tests_.size());
        std::vector<uint8_t> keep(n, 0);
        for (int hi = n - 1; hi >= 0; hi -= 64) {
            int cnt = std::min(64, hi + 1);
            std::vector<uint64_t> words(c_.pis.size(), 0);
            for (int j = 0; j < cnt; ++j) {
                const auto& v = tests_[hi - j];
                for (size_t i = 0; i < v.size(); ++i)
                    if (v[i]) words[i] |= 1ULL << j;
            }
            std::vector<std::pair<int, int>> newly;
            fs2.simulate_block(words, cnt, &newly);
            for (auto& [f, off] : newly) keep[hi - off] = 1;
        }
        if (fs2.num_detected() != fs_.num_detected())
            throw std::logic_error("compaction re-simulation disagrees with generation coverage: " + std::to_string(fs2.num_detected()) + " vs " + std::to_string(fs_.num_detected()) + " over " + std::to_string(n) + " tests");
        for (int i = 0; i < n; ++i)
            if (keep[i]) R_.test_set.push_back(tests_[i]);
        R_.compacted = static_cast<int>(R_.test_set.size());
        R_.ms_compact = ms_since(t);
    }

    RunResult finish()
    {
        R_.ms_total = R_.ms_random + R_.ms_podem;
        R_.detected = fs_.num_detected();
        R_.aborted = 0;
        for (int f : fs_.pending())
            if (abandoned_[f]) {
                ++R_.aborted;
                R_.work_unresolved += abort_work_[f];
            }
        R_.tests = static_cast<int>(tests_.size());
        R_.work_total = work_total();
        compact();
        return R_;
    }

    RunResult& result() { return R_; }

private:
    double work_total() const { return R_.work_random + R_.work_podem; }

    // Undetected faults PODEM has not given up on.
    int targetable() const
    {
        int n = 0;
        for (int f : fs_.pending())
            if (!abandoned_[f]) ++n;
        return n;
    }

    const Circuit& c_;
    const FaultList& fl_;
    RunConfig cfg_;
    FaultSim fs_;
    Podem podem_;
    std::mt19937_64 rng_;
    RunResult R_;
    Clock::time_point t0_;
    std::vector<std::vector<uint8_t>> tests_;
    std::vector<uint8_t> abandoned_;  // PODEM aborted: no longer targeted
    std::vector<double> abort_work_;  // PODEM work spent aborting each fault
    int64_t patterns_ = 0;
    double last_block_work_ = 0;
};

}  // namespace

RunResult run_atpg(const Circuit& c, const FaultList& fl, const Scoap& sc, const RunConfig& cfg_in)
{
    RunConfig cfg = cfg_in;
    if (cfg.method == "stage1") cfg.stage2 = false;  // adaptive, switching on the cost-bound test
    Session s(c, fl, sc, cfg);
    if (cfg.method == "random") {
        s.run_random(cfg.random_budget);
        s.result().param = cfg.random_budget;
    } else if (cfg.method == "podem") {
        s.result().switch_at = 0;
        s.podem_phase();
    } else if (cfg.method == "fixed") {
        s.run_random(cfg.fixed_switch);
        s.result().switch_at = s.result().random_applied;
        s.result().param = cfg.fixed_switch;
        s.podem_phase();
    } else if (cfg.method == "plateau") {
        s.run_plateau();
        s.result().param = s.result().switch_at;
    } else if (cfg.method == "adaptive" || cfg.method == "stage1") {
        s.run_adaptive();
        s.result().param = s.result().switch_at;
    } else {
        throw std::runtime_error("unknown method: " + cfg.method);
    }
    return s.finish();
}

std::string summary_header()
{
    return "circuit,method,param,seed,faults,detected,redundant,aborted,fc_pct,fe_pct,"
           "random_applied,random_kept,podem_vectors,podem_calls,probe_calls,tests,compacted,"
           "switch_at,work_random,work_podem,work_total,ms_random,ms_podem,ms_total,ms_compact,"
           "work_unresolved,work_core";
}

std::string summary_row(const RunResult& r)
{
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(4);
    std::string label = r.method;
    if (r.method == "fixed") label += "-" + std::to_string(r.param);
    o << r.circuit << ',' << label << ',' << r.param << ',' << r.seed << ',' << r.faults << ','
      << r.detected << ',' << r.redundant << ',' << r.aborted << ',' << r.fc() << ',' << r.fe()
      << ',' << r.random_applied << ',' << r.random_kept << ',' << r.podem_vectors << ','
      << r.podem_calls << ',' << r.probe_calls << ',' << r.tests << ',' << r.compacted << ','
      << r.switch_at << ',';
    o.precision(0);
    o << r.work_random << ',' << r.work_podem << ',' << r.work_total << ',';
    o.precision(3);
    o << r.ms_random << ',' << r.ms_podem << ',' << r.ms_total << ',' << r.ms_compact << ',';
    o.precision(0);
    o << r.work_unresolved << ',' << r.work_core();
    return o.str();
}

void write_curve(const std::string& path, const RunResult& r, int max_points)
{
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot write " + path);
    f << "pattern,detected,coverage_pct,work,ms,phase\n";
    // Keep every phase change and the last point; otherwise thin to ~max_points
    // evenly spaced in coverage.
    const double step = r.faults > 0 ? static_cast<double>(r.faults) / max_points : 1.0;
    double next = 0;
    char last_phase = 0;
    for (size_t i = 0; i < r.curve.size(); ++i) {
        const CurvePoint& p = r.curve[i];
        bool last = i + 1 == r.curve.size();
        if (p.detected >= next || p.phase != last_phase || last) {
            f << p.pattern << ',' << p.detected << ','
              << (r.faults ? 100.0 * p.detected / r.faults : 0.0) << ',' << p.work << ','
              << p.ms << ',' << p.phase << '\n';
            next = p.detected + step;
            last_phase = p.phase;
        }
    }
}
