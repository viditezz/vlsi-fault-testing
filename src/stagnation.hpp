// stagnation.hpp — the statistical stagnation test (the project's switching rule).
//
// Question asked after every 64-pattern random block:
//   "Is random testing still at least as productive, per unit of work, as PODEM
//    would be on the faults that are left?"
//
// Inputs, all measured live on this run:
//   k   = new faults detected by random patterns in the recent window
//   R   = work spent on those random patterns
//   y_p = PODEM's measured yield (faults detected per unit work), from a small
//         probe of PODEM calls on randomly sampled undetected faults
//
// Model: new detections in the window ~ Poisson(lambda * R).
// Break-even rate: lambda* = y_p, i.e. expected k* = y_p * R.
// H0: lambda >= lambda*  (random still pays for itself)
// Reject H0 when  P(K <= k | Poisson(y_p * R)) < alpha.
//
// Windows are not a fixed pattern count. Each window starts where the previous
// test ended and grows forward block by block until the break-even expectation
// y_p * R reaches -2 ln(alpha) — the smallest window in which seeing zero
// detections would be significant. Then it is tested once and a fresh window
// starts. Non-overlapping windows keep the early high-yield burst out of later
// tests (the detection rate decays, so mixing regimes would hide stagnation)
// and make consecutive tests independent: requiring `confirm` = 2 consecutive
// rejections bounds the false-switch probability per decision by alpha^2.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

double poisson_cdf(int64_t k, double mu);

struct StagnationDecision {
    bool enough_evidence = false;  // window reached the required expectation (a test ran)
    bool reject = false;           // random's rate is significantly below break-even
    int64_t k = 0;                 // detections in window
    double work = 0;               // work in window
    double mu = 0;                 // break-even expected detections
    double p = 1;                  // P(K <= k | mu)
    int blocks = 0;                // window length in blocks
};

class StagnationDetector {
public:
    // window_mu: expected detections at break-even that a window must reach
    // before it is tested (its length). alpha: rejection level for the tail test.
    StagnationDetector(double alpha, int confirm, double window_mu)
        : alpha_(alpha), confirm_(confirm), window_mu_(window_mu) {}

    void add_block(int new_detections, double work)
    {
        det_.push_back(new_detections);
        work_.push_back(work);
    }

    // Evaluates the current window for PODEM yield y_p. If the window has enough
    // evidence it is tested and closed. Returns true when the switch fires.
    bool test(double podem_yield, StagnationDecision* out = nullptr);

    // Called after a yield probe: the break-even point moved, start over.
    void restart()
    {
        streak_ = 0;
        start_ = det_.size();
    }
    int streak() const { return streak_; }

private:
    double alpha_;
    int confirm_;
    double window_mu_;
    int streak_ = 0;
    size_t start_ = 0;  // first block of the open window
    std::vector<int> det_;
    std::vector<double> work_;
};
