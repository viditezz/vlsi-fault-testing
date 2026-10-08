// stagnation.cpp
#include "stagnation.hpp"

#include <cmath>

double poisson_cdf(int64_t k, double mu)
{
    if (k < 0) return 0.0;
    if (mu <= 0) return 1.0;
    // Far above the mean the CDF is 1 to double precision; skip the sum.
    if (static_cast<double>(k) > mu + 12.0 * std::sqrt(mu) + 30.0) return 1.0;
    double log_term = -mu;  // log P(K = 0)
    double sum = std::exp(log_term);
    for (int64_t i = 1; i <= k; ++i) {
        log_term += std::log(mu) - std::log(static_cast<double>(i));
        sum += std::exp(log_term);
    }
    return sum > 1.0 ? 1.0 : sum;
}

bool StagnationDetector::test(double podem_yield, StagnationDecision* out)
{
    StagnationDecision d;
    const double needed = -2.0 * std::log(alpha_);
    for (size_t b = start_; b < det_.size(); ++b) {
        d.k += det_[b];
        d.work += work_[b];
        ++d.blocks;
    }
    d.mu = podem_yield * d.work;
    d.enough_evidence = d.mu >= needed;
    if (d.enough_evidence) {
        d.p = poisson_cdf(d.k, d.mu);
        d.reject = d.p < alpha_;
        streak_ = d.reject ? streak_ + 1 : 0;
        start_ = det_.size();  // close this window; the next one starts fresh
    }
    if (out) *out = d;
    return streak_ >= confirm_;
}
