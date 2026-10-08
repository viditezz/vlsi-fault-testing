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
        } else {
            // Aborted: stop targeting it, but keep it in fault simulation so a
            // later vector can still detect it by accident.
            abandoned_[f] = 1;
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
    double probe()
    {
        auto t = Clock::now();
        std::vector<int> pool;
        for (int f : fs_.pending())
            if (!abandoned_[f]) pool.push_back(f);
        std::sort(pool.begin(), pool.end());
        std::shuffle(pool.begin(), pool.end(), rng_);
        double det_work = 0, all_work = 0;
        int det = 0, calls = 0;
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
                det += fs_.num_detected() - d0;  // the target plus anything it dropped
                det_work += w;
            }
        }
        R_.ms_podem += ms_since(t);
        hist_det_ += det;
        hist_det_work_ += det_work;
        if (det > 0) return det / std::max(det_work, 1.0);
        // No test in the whole sample: the residue looks undetectable. Assume
        // half a detection at the cost a detection took in earlier probes (or,
        // with no history, at this probe's average call cost). Random still gets
        // to prove itself in the window test — it only has to beat this bar.
        double unit = hist_det_ > 0 ? hist_det_work_ / hist_det_
                                    : all_work / std::max(calls, 1);
        return 0.5 / std::max(unit, 1.0);
    }

    // Adaptive switch, two stages, both using the same Poisson window test:
    //
    //  Stage 1 (no PODEM calls at all): a deterministic vector can never cost less
    //  than fault-simulating it, and one fault-simulation pass costs about as much
    //  as a whole 64-pattern random block. So PODEM's yield is at most ~1 detection
    //  per block's worth of work, and random cannot be losing while it still finds
    //  >= 1 new fault per block. Test random's rate against that bound.
    //
    //  Stage 2 (after stage 1 rejects): measure PODEM's actual yield with a probe,
    //  then test random's rate against it. Re-probe whenever the target set halves,
    //  since the residue (and so PODEM's cost per fault) changes as it shrinks.
    //
    //  Switch when the stage-2 test rejects `confirm` times in a row.
    void run_adaptive()
    {
        StagnationDetector det(cfg_.alpha, cfg_.confirm);
        double yield = -1;      // < 0: stage 1
        int pending_at_probe = 0;
        double recent_block_work = 0;
        auto t = Clock::now();
        while (targetable() > 0 && R_.random_applied < cfg_.random_budget) {
            int found = random_block(64);
            det.add_block(found, last_block_work_);
            recent_block_work = last_block_work_;
            if (R_.random_applied < cfg_.warmup || targetable() == 0) continue;

            bool need_probe = yield >= 0 && targetable() * 2 <= pending_at_probe;
            if (yield < 0) {
                // Stage 1: bound = one detection per block of work.
                StagnationDecision d;
                double bound = 1.0 / std::max(recent_block_work, 1.0);
                det.test(bound, &d);
                if (d.enough_evidence) {
                    std::ostringstream o;
                    o << "pattern " << R_.random_applied << ": stage 1  k=" << d.k << " over "
                      << d.blocks << " blocks (bound: 1 per block), p=" << d.p
                      << (d.reject ? "  REJECT -> probe PODEM" : "");
                    R_.decisions.push_back(o.str());
                }
                if (!(d.enough_evidence && d.reject)) continue;
                need_probe = true;
            }
            if (need_probe) {
                R_.ms_random += ms_since(t);
                yield = probe();
                t = Clock::now();
                pending_at_probe = targetable();
                det.restart();
                std::ostringstream o;
                o << "pattern " << R_.random_applied << ": probe -> PODEM yield " << yield
                  << " det/work, targetable " << pending_at_probe;
                R_.decisions.push_back(o.str());
                if (targetable() == 0) break;  // nothing left for PODEM to try
                continue;
            }
            StagnationDecision d;
            bool fire = det.test(yield, &d);
            if (d.enough_evidence) {
                std::ostringstream o;
                o << "pattern " << R_.random_applied << ": stage 2  k=" << d.k << " over "
                  << d.blocks << " blocks, break-even mu=" << d.mu << ", p=" << d.p
                  << (d.reject ? "  REJECT" : "") << (fire ? "  -> SWITCH" : "");
                R_.decisions.push_back(o.str());
            }
            if (fire) {
                R_.switch_at = R_.random_applied;
                break;
            }
        }
        R_.ms_random += ms_since(t);
        if (R_.switch_at < 0) R_.switch_at = R_.random_applied;
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
            if (abandoned_[f]) ++R_.aborted;
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
    int hist_det_ = 0;                // all probes: faults detected by PODEM tests
    double hist_det_work_ = 0;        // all probes: work of PODEM calls that made a test
    int64_t patterns_ = 0;
    double last_block_work_ = 0;
};

}  // namespace

RunResult run_atpg(const Circuit& c, const FaultList& fl, const Scoap& sc, const RunConfig& cfg)
{
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
    } else if (cfg.method == "adaptive") {
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
           "switch_at,work_random,work_podem,work_total,ms_random,ms_podem,ms_total,ms_compact";
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
    o << r.ms_random << ',' << r.ms_podem << ',' << r.ms_total << ',' << r.ms_compact;
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
