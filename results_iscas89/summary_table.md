# Results summary

Means over 10 seeds (± stdev where non-zero). FC = detected / collapsed faults; FE = (detected + proved redundant) / collapsed faults. Work = gate evaluations, deterministic per seed. The oracle is the fixed switch point with the lowest mean work over a 19-point grid (64 … 32768 patterns), chosen per circuit in hindsight; "worst single seed" compares each seed with the best fixed N for that same seed. c17 is excluded from ratios (every method finishes in a few patterns). Intervals are 95% percentile bootstraps over seeds, conditional on this set of circuits.


## Coverage

Every flow that ends with the SAT fallback (all but pure random) resolves every fault, so its detected and redundant counts must be identical across methods and seeds. This table is generated from the sweep and fails if they are not.

| circuit | collapsed faults | detected (all flows) | proved redundant | unresolved | pure random: detected (mean, min–max) |
|---|---:|---:|---:|---:|---:|
| s1423 | 1515 | 1501 | 14 | 0 | 1499.6 (1498–1501) |
| s5378 | 4551 | 4511 | 40 | 0 | 4501.2 (4497–4504) |
| s9234 | 6927 | 6475 | 452 | 0 | 6013.4 (5981–6045) |
| s13207 | 9815 | 9664 | 151 | 0 | 9514.1 (9471–9565) |
| s15850 | 11725 | 11336 | 389 | 0 | 10861.5 (10804–10912) |
| s38417 | 31180 | 31015 | 165 | 0 | 29577.4 (29430–29682) |

## Cost relative to the hindsight-best fixed switch — total work

| circuit | PODEM only | Fixed switch @128 | Fixed switch @1024 | Fixed switch @8192 | Plateau rule (6 blocks, ≤1) | Cost-bound test only (stage 1) | Adaptive, two-stage |
|---|---:|---:|---:|---:|---:|---:|---:|
| s1423 | 1.158 | 1.077 | 1.007 | 1.023 | 1.004 | 1.004 | 1.030 |
| s5378 | 2.253 | 1.949 | 1.330 | 1.030 | 0.998 | 1.001 | 1.056 |
| s9234 | 1.028 | 1.022 | 1.006 | 1.004 | 1.005 | 1.005 | 1.005 |
| s13207 | 1.494 | 1.416 | 1.256 | 1.034 | 1.010 | 1.000 | 1.002 |
| s15850 | 1.110 | 1.087 | 1.038 | 1.001 | 1.001 | 1.002 | 1.002 |
| s38417 | 1.396 | 1.346 | 1.244 | 1.044 | 1.003 | 1.002 | 0.996 |
| geomean | 1.356 | 1.282 | 1.139 | 1.023 | 1.003 | 1.002 | 1.015 |
| geomean, 95% bootstrap interval | 1.352–1.360 | 1.278–1.288 | 1.136–1.143 | 1.020–1.026 | 1.001–1.007 | 1.000–1.005 | 1.011–1.019 |
| worst circuit | 2.253 | 1.949 | 1.330 | 1.044 | 1.010 | 1.005 | 1.056 |
| worst single seed | 2.331 | 2.098 | 1.386 | 1.080 | 1.060 | 1.043 | 1.106 |

Best fixed N per circuit: s1423 2048, s5378 4096, s9234 3072, s13207 16384, s15850 6144, s38417 16384


## Cost relative to the hindsight-best fixed switch — switch-dependent work

Switch-dependent work = total work minus PODEM work on faults that end the run proved redundant (by PODEM, or by the SAT fallback after PODEM aborted). That set is identical for every flow, and every flow pays for it wherever it switches.

| circuit | PODEM only | Fixed switch @128 | Fixed switch @1024 | Fixed switch @8192 | Plateau rule (6 blocks, ≤1) | Cost-bound test only (stage 1) | Adaptive, two-stage |
|---|---:|---:|---:|---:|---:|---:|---:|
| s1423 | 3.700 | 2.316 | 1.121 | 1.397 | 1.068 | 1.075 | 1.505 |
| s5378 | 3.205 | 2.669 | 1.581 | 1.054 | 0.996 | 1.002 | 1.098 |
| s9234 | 1.440 | 1.337 | 1.095 | 1.069 | 1.079 | 1.080 | 1.082 |
| s13207 | 2.274 | 2.074 | 1.659 | 1.087 | 1.026 | 1.001 | 1.006 |
| s15850 | 1.744 | 1.591 | 1.258 | 1.010 | 1.004 | 1.014 | 1.013 |
| s38417 | 1.642 | 1.561 | 1.396 | 1.072 | 1.005 | 1.003 | 0.994 |
| geomean | 2.193 | 1.869 | 1.335 | 1.108 | 1.029 | 1.028 | 1.104 |
| geomean, 95% bootstrap interval | 2.171–2.219 | 1.838–1.902 | 1.318–1.355 | 1.095–1.122 | 1.018–1.044 | 1.017–1.043 | 1.076–1.133 |
| worst circuit | 3.700 | 2.669 | 1.659 | 1.397 | 1.079 | 1.080 | 1.505 |
| worst single seed | 4.277 | 3.012 | 1.751 | 1.550 | 1.414 | 1.414 | 2.315 |

Best fixed N per circuit: s1423 2048, s5378 4096, s9234 3072, s13207 16384, s15850 6144, s38417 16384


## Pairwise differences between rules — total work

Ratio < 1 means A needs less work. "Indistinguishable" means the 95% interval contains 1.

| comparison (A ÷ B) | ratio of geomeans | 95% interval | verdict |
|---|---:|---:|---|
| Plateau rule (6 blocks, ≤1) ÷ Cost-bound test only (stage 1) | 1.001 | 0.998–1.004 | indistinguishable |
| Cost-bound test only (stage 1) ÷ Adaptive, two-stage | 0.988 | 0.984–0.992 | A cheaper |
| Plateau rule (6 blocks, ≤1) ÷ Adaptive, two-stage | 0.989 | 0.984–0.993 | A cheaper |
| Plateau rule (6 blocks, ≤1) ÷ Fixed switch @1024 | 0.881 | 0.877–0.885 | A cheaper |
| Adaptive, two-stage ÷ Fixed switch @1024 | 0.891 | 0.887–0.894 | A cheaper |

## Pairwise differences between rules — switch-dependent work

Ratio < 1 means A needs less work. "Indistinguishable" means the 95% interval contains 1.

| comparison (A ÷ B) | ratio of geomeans | 95% interval | verdict |
|---|---:|---:|---|
| Plateau rule (6 blocks, ≤1) ÷ Cost-bound test only (stage 1) | 1.001 | 0.992–1.009 | indistinguishable |
| Cost-bound test only (stage 1) ÷ Adaptive, two-stage | 0.932 | 0.908–0.956 | A cheaper |
| Plateau rule (6 blocks, ≤1) ÷ Adaptive, two-stage | 0.933 | 0.906–0.960 | A cheaper |
| Plateau rule (6 blocks, ≤1) ÷ Fixed switch @1024 | 0.771 | 0.760–0.783 | A cheaper |
| Adaptive, two-stage ÷ Fixed switch @1024 | 0.827 | 0.804–0.850 | A cheaper |

## How costly is a switch that is off by 2x or 4x? (fixed N, switch-dependent work)

Cost relative to the best fixed N on the grid, for a fixed switch point 2 and 4 grid steps (about 2x and 4x) earlier or later.

| circuit | best N | N/2 | 2N | N/4 | 4N |
|---|---:|---:|---:|---:|---:|
| s1423 | 2048 | 1.12× | 1.08× | 1.38× | 1.40× |
| s5378 | 4096 | 1.18× | 1.05× | 1.58× | 1.44× |
| s9234 | 3072 | 1.05× | 1.03× | 1.12× | 1.15× |
| s13207 | 16384 | 1.09× | 1.18× | 1.30× | — |
| s15850 | 6144 | 1.05× | 1.06× | 1.17× | 1.26× |
| s38417 | 16384 | 1.07× | 1.03× | 1.17× | — |

## Test-set size (compacted tests, relative to PODEM only)

Reverse-order fault-simulation compaction applied to every method's test set. Pure random reaches lower coverage than the hybrids wherever its detected count is lower (Coverage table).

| circuit | Pure random | Fixed switch @128 | Fixed switch @1024 | Fixed switch @8192 | Plateau rule (6 blocks, ≤1) | Cost-bound test only (stage 1) | Adaptive, two-stage |
|---|---:|---:|---:|---:|---:|---:|---:|
| s1423 | 1.021 | 1.055 | 0.989 | 1.012 | 1.015 | 1.041 | 1.007 |
| s5378 | 1.007 | 0.988 | 1.026 | 1.025 | 1.021 | 1.018 | 1.022 |
| s9234 | 0.769 | 1.005 | 0.998 | 1.022 | 1.021 | 1.031 | 1.029 |
| s13207 | 0.942 | 1.002 | 1.008 | 1.005 | 1.010 | 1.010 | 1.010 |
| s15850 | 0.740 | 1.004 | 1.010 | 1.027 | 1.015 | 1.023 | 1.015 |
| s38417 | 0.635 | 1.006 | 0.995 | 0.970 | 0.956 | 0.946 | 0.939 |
| geomean | 0.839 | 1.010 | 1.004 | 1.010 | 1.006 | 1.011 | 1.003 |

## SAT fallback

Faults PODEM aborted on (1024 backtracks) are handed to the SAT solver once the PODEM phase is done. Means over seeds, adaptive method. Propagation steps are SAT-solver units, not gate evaluations.

| circuit | PODEM aborts → SAT | SAT tests | proved redundant | redundant faults (all) | SAT propagations (M) |
|---|---:|---:|---:|---:|---:|
| s1423 | 8.0 | 0.0 | 8.0 | 14.0 | 0.00 |
| s5378 | 1.0 | 0.0 | 1.0 | 40.0 | 0.00 |
| s9234 | 210.0 | 0.0 | 210.0 | 452.0 | 0.26 |
| s13207 | 2.0 | 0.0 | 2.0 | 151.0 | 0.00 |
| s15850 | 6.0 | 0.0 | 6.0 | 389.0 | 0.00 |
| s38417 | 54.9 | 2.9 | 52.0 | 165.0 | 2.27 |

## Per circuit

Wall-clock times are single runs of millisecond-scale jobs and vary by tens of percent between repeats; use the work columns for comparisons.


### s1423 — 91 PI, 79 PO, 657 gates, 1515 collapsed faults (2846 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 1499.6 ± 1.0 | n/a | n/a | 98.984 ± 0.064 | 98.984 ± 0.064 | 74.5 ± 3.4 | — | 0 | 0.391 ± 0.002 | 0.391 ± 0.002 | 4.2 ± 0.1 |
| PODEM only | 1501.0 | 14.0 | 0.0 | 99.076 | 100.000 | 73.0 ± 3.3 | — | 108 ± 5 | 2.057 ± 0.024 | 0.385 ± 0.024 | 42.0 ± 2.2 |
| Fixed switch @128 | 1501.0 | 14.0 | 0.0 | 99.076 | 100.000 | 77.0 ± 3.6 | 128 | 68 ± 4 | 1.913 ± 0.027 | 0.241 ± 0.027 | 25.4 ± 0.3 |
| Fixed switch @1024 | 1501.0 | 14.0 | 0.0 | 99.076 | 100.000 | 72.2 ± 3.2 | 1024 | 32 ± 3 | 1.789 ± 0.011 | 0.117 ± 0.011 | 24.7 ± 0.4 |
| Fixed switch @8192 | 1501.0 | 14.0 | 0.0 | 99.076 | 100.000 | 73.9 ± 4.5 | 8192 | 18 ± 2 | 1.817 ± 0.008 | 0.145 ± 0.008 | 24.8 ± 0.3 |
| Plateau rule (6 blocks, ≤1) | 1501.0 | 14.0 | 0.0 | 99.076 | 100.000 | 74.1 ± 3.2 | 1722 ± 296 | 29 ± 3 | 1.783 ± 0.010 | 0.111 ± 0.010 | 24.4 ± 0.1 |
| Cost-bound test only (stage 1) | 1501.0 | 14.0 | 0.0 | 99.076 | 100.000 | 76.0 ± 2.8 | 2131 ± 523 | 27 ± 3 | 1.784 ± 0.009 | 0.112 ± 0.009 | 24.4 ± 0.2 |
| Adaptive, two-stage | 1501.0 | 14.0 | 0.0 | 99.076 | 100.000 | 73.5 ± 3.5 | 6931 ± 2876 | 24 ± 3 | 1.828 ± 0.037 | 0.157 ± 0.037 | 24.9 ± 0.3 |

### s5378 — 214 PI, 213 PO, 2779 gates, 4551 collapsed faults (10538 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 4501.2 ± 2.3 | n/a | n/a | 98.906 ± 0.052 | 98.906 ± 0.052 | 271.2 ± 8.6 | — | 0 | 1.773 ± 0.014 | 1.773 ± 0.014 | 11.2 ± 0.2 |
| PODEM only | 4511.0 | 40.0 | 0.0 | 99.121 | 100.000 | 269.4 ± 5.7 | — | 374 ± 9 | 3.131 ± 0.073 | 2.531 ± 0.073 | 24.7 ± 0.6 |
| Fixed switch @128 | 4511.0 | 40.0 | 0.0 | 99.121 | 100.000 | 266.3 ± 4.2 | 128 | 316 ± 7 | 2.707 ± 0.054 | 2.108 ± 0.054 | 21.7 ± 0.4 |
| Fixed switch @1024 | 4511.0 | 40.0 | 0.0 | 99.121 | 100.000 | 276.3 ± 6.3 | 1024 | 190 ± 4 | 1.848 ± 0.030 | 1.249 ± 0.030 | 17.1 ± 0.2 |
| Fixed switch @8192 | 4511.0 | 40.0 | 0.0 | 99.121 | 100.000 | 276.1 ± 7.6 | 8192 | 77 ± 4 | 1.432 ± 0.034 | 0.832 ± 0.034 | 15.4 ± 0.4 |
| Plateau rule (6 blocks, ≤1) | 4511.0 | 40.0 | 0.0 | 99.121 | 100.000 | 275.1 ± 7.1 | 5453 ± 1063 | 90 ± 10 | 1.386 ± 0.037 | 0.787 ± 0.037 | 15.0 ± 0.2 |
| Cost-bound test only (stage 1) | 4511.0 | 40.0 | 0.0 | 99.121 | 100.000 | 274.2 ± 5.8 | 5606 ± 1067 | 89 ± 10 | 1.391 ± 0.035 | 0.791 ± 0.035 | 15.1 ± 0.3 |
| Adaptive, two-stage | 4511.0 | 40.0 | 0.0 | 99.121 | 100.000 | 275.4 ± 7.0 | 8691 ± 1439 | 79 ± 7 | 1.466 ± 0.038 | 0.867 ± 0.038 | 15.6 ± 0.2 |

### s9234 — 247 PI, 250 PO, 5597 gates, 6927 collapsed faults (18468 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 6013.4 ± 22.2 | n/a | n/a | 86.811 ± 0.320 | 86.811 ± 0.320 | 316.8 ± 6.6 | — | 0 | 8.332 ± 0.184 | 8.332 ± 0.184 | 136.2 ± 3.9 |
| PODEM only | 6475.0 | 452.0 | 0.0 | 93.475 | 100.000 | 411.9 ± 8.3 | — | 961 ± 8 | 117.849 ± 0.132 | 10.522 ± 0.132 | 1714.3 ± 19.1 |
| Fixed switch @128 | 6475.0 | 452.0 | 0.0 | 93.475 | 100.000 | 413.8 ± 8.3 | 128 | 921 ± 8 | 117.097 ± 0.172 | 9.771 ± 0.172 | 1695.6 ± 11.1 |
| Fixed switch @1024 | 6475.0 | 452.0 | 0.0 | 93.475 | 100.000 | 411.0 ± 9.5 | 1024 | 822 ± 14 | 115.325 ± 0.289 | 7.999 ± 0.289 | 1620.4 ± 23.2 |
| Fixed switch @8192 | 6475.0 | 452.0 | 0.0 | 93.475 | 100.000 | 421.0 ± 8.8 | 8192 | 695 ± 6 | 115.134 ± 0.158 | 7.807 ± 0.158 | 1642.0 ± 18.9 |
| Plateau rule (6 blocks, ≤1) | 6475.0 | 452.0 | 0.0 | 93.475 | 100.000 | 420.7 ± 7.5 | 8237 ± 2374 | 700 ± 12 | 115.207 ± 0.513 | 7.880 ± 0.513 | 1623.6 ± 11.8 |
| Cost-bound test only (stage 1) | 6475.0 | 452.0 | 0.0 | 93.475 | 100.000 | 424.7 ± 7.2 | 9542 ± 2556 | 687 ± 16 | 115.218 ± 0.599 | 7.891 ± 0.599 | 1622.6 ± 19.5 |
| Adaptive, two-stage | 6475.0 | 452.0 | 0.0 | 93.475 | 100.000 | 423.9 ± 11.8 | 9696 ± 2568 | 687 ± 16 | 115.230 ± 0.623 | 7.903 ± 0.623 | 1623.1 ± 20.9 |

### s13207 — 700 PI, 790 PO, 7951 gates, 9815 collapsed faults (26358 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 9514.1 ± 34.5 | n/a | n/a | 96.934 ± 0.352 | 96.934 ± 0.352 | 461.3 ± 9.1 | — | 0 | 5.379 ± 0.032 | 5.379 ± 0.032 | 77.3 ± 25.2 |
| PODEM only | 9664.0 | 151.0 | 0.0 | 98.462 | 100.000 | 489.8 ± 6.0 | — | 729 ± 8 | 21.239 ± 0.176 | 12.535 ± 0.176 | 224.7 ± 3.4 |
| Fixed switch @128 | 9664.0 | 151.0 | 0.0 | 98.462 | 100.000 | 490.8 ± 6.7 | 128 | 680 ± 8 | 20.133 ± 0.172 | 11.429 ± 0.172 | 211.8 ± 3.5 |
| Fixed switch @1024 | 9664.0 | 151.0 | 0.0 | 98.462 | 100.000 | 493.5 ± 8.4 | 1024 | 564 ± 13 | 17.848 ± 0.266 | 9.144 ± 0.266 | 196.1 ± 3.2 |
| Fixed switch @8192 | 9664.0 | 151.0 | 0.0 | 98.462 | 100.000 | 492.1 ± 10.3 | 8192 | 356 ± 10 | 14.696 ± 0.220 | 5.992 ± 0.220 | 181.5 ± 2.3 |
| Plateau rule (6 blocks, ≤1) | 9664.0 | 151.0 | 0.0 | 98.462 | 100.000 | 494.8 ± 6.1 | 12019 ± 2078 | 312 ± 26 | 14.358 ± 0.308 | 5.654 ± 0.308 | 182.4 ± 1.9 |
| Cost-bound test only (stage 1) | 9664.0 | 151.0 | 0.0 | 98.462 | 100.000 | 494.5 ± 8.0 | 14931 ± 2487 | 285 ± 21 | 14.220 ± 0.180 | 5.516 ± 0.180 | 182.6 ± 1.8 |
| Adaptive, two-stage | 9664.0 | 151.0 | 0.0 | 98.462 | 100.000 | 494.9 ± 6.8 | 15251 ± 2506 | 284 ± 20 | 14.249 ± 0.141 | 5.545 ± 0.141 | 183.4 ± 1.7 |

### s15850 — 611 PI, 684 PO, 9772 gates, 11725 collapsed faults (31694 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 10861.5 ± 30.9 | n/a | n/a | 92.635 ± 0.263 | 92.635 ± 0.263 | 341.2 ± 9.5 | — | 0 | 7.854 ± 0.073 | 7.854 ± 0.073 | 106.4 ± 3.7 |
| PODEM only | 11336.0 | 389.0 | 0.0 | 96.682 | 100.000 | 460.9 ± 6.2 | — | 952 ± 7 | 68.307 ± 0.277 | 15.830 ± 0.277 | 812.1 ± 4.5 |
| Fixed switch @128 | 11336.0 | 389.0 | 0.0 | 96.682 | 100.000 | 462.7 ± 6.4 | 128 | 902 ± 8 | 66.918 ± 0.289 | 14.440 ± 0.289 | 756.6 ± 41.5 |
| Fixed switch @1024 | 11336.0 | 389.0 | 0.0 | 96.682 | 100.000 | 465.3 ± 5.7 | 1024 | 798 ± 11 | 63.900 ± 0.429 | 11.423 ± 0.429 | 715.2 ± 9.7 |
| Fixed switch @8192 | 11336.0 | 389.0 | 0.0 | 96.682 | 100.000 | 473.4 ± 5.6 | 8192 | 656 ± 5 | 61.646 ± 0.191 | 9.168 ± 0.191 | 704.5 ± 5.3 |
| Plateau rule (6 blocks, ≤1) | 11336.0 | 389.0 | 0.0 | 96.682 | 100.000 | 467.6 ± 9.8 | 7437 ± 972 | 660 ± 10 | 61.589 ± 0.245 | 9.111 ± 0.245 | 695.5 ± 4.8 |
| Cost-bound test only (stage 1) | 11336.0 | 389.0 | 0.0 | 96.682 | 100.000 | 471.3 ± 9.6 | 8602 ± 1796 | 655 ± 10 | 61.681 ± 0.293 | 9.204 ± 0.293 | 718.6 ± 27.1 |
| Adaptive, two-stage | 11336.0 | 389.0 | 0.0 | 96.682 | 100.000 | 467.9 ± 7.1 | 8954 ± 1822 | 650 ± 9 | 61.673 ± 0.394 | 9.196 ± 0.394 | 699.8 ± 9.5 |

### s38417 — 1664 PI, 1742 PO, 22179 gates, 31180 collapsed faults (76678 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 29577.4 ± 80.2 | n/a | n/a | 94.860 ± 0.257 | 94.860 ± 0.257 | 658.3 ± 14.7 | — | 0 | 21.736 ± 0.436 | 21.736 ± 0.436 | 335.4 ± 56.8 |
| PODEM only | 31015.0 | 165.0 | 0.0 | 99.471 | 100.000 | 1037.3 ± 12.8 | — | 1542 ± 17 | 115.134 ± 1.614 | 83.580 ± 1.614 | 1185.6 ± 39.8 |
| Fixed switch @128 | 31015.0 | 165.0 | 0.0 | 99.471 | 100.000 | 1043.4 ± 11.0 | 128 | 1478 ± 13 | 111.001 ± 0.827 | 79.448 ± 0.827 | 1120.1 ± 15.0 |
| Fixed switch @1024 | 31015.0 | 165.0 | 0.0 | 99.471 | 100.000 | 1032.3 ± 9.5 | 1024 | 1318 ± 18 | 102.588 ± 1.498 | 71.034 ± 1.498 | 1052.3 ± 40.1 |
| Fixed switch @8192 | 31015.0 | 165.0 | 0.0 | 99.471 | 100.000 | 1006.5 ± 14.8 | 8192 | 965 ± 30 | 86.113 ± 2.472 | 54.559 ± 2.472 | 965.4 ± 25.9 |
| Plateau rule (6 blocks, ≤1) | 31015.0 | 165.0 | 0.0 | 99.471 | 100.000 | 991.5 ± 15.5 | 16333 ± 1949 | 822 ± 42 | 82.692 ± 2.019 | 51.139 ± 2.019 | 987.0 ± 21.1 |
| Cost-bound test only (stage 1) | 31015.0 | 165.0 | 0.0 | 99.471 | 100.000 | 981.8 ± 13.3 | 18214 ± 2510 | 800 ± 38 | 82.582 ± 1.949 | 51.028 ± 1.949 | 1005.2 ± 26.5 |
| Adaptive, two-stage | 31015.0 | 165.0 | 0.0 | 99.471 | 100.000 | 974.1 ± 20.9 | 18413 ± 2485 | 793 ± 41 | 82.134 ± 1.687 | 50.581 ± 1.687 | 994.3 ± 17.0 |
