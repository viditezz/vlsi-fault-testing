# Results summary

Means over 10 seeds (± stdev where non-zero). FC = detected / collapsed faults; FE = (detected + proved redundant) / collapsed faults. Work = gate evaluations, deterministic per seed. The oracle is the fixed switch point with the lowest mean work over a 19-point grid (64 … 32768 patterns), chosen per circuit in hindsight; "worst single seed" compares each seed with the best fixed N for that same seed. c17 is excluded from ratios (every method finishes in a few patterns). Intervals are 95% percentile bootstraps over seeds, conditional on this set of circuits.


## Coverage

Every flow that ends with the SAT fallback (all but pure random) resolves every fault, so its detected and redundant counts must be identical across methods and seeds. This table is generated from the sweep and fails if they are not.

| circuit | collapsed faults | detected (all flows) | proved redundant | unresolved | pure random: detected (mean, min–max) |
|---|---:|---:|---:|---:|---:|
| c17 | 22 | 22 | 0 | 0 | 22.0 (22–22) |
| c432 | 524 | 520 | 4 | 0 | 520.0 (520–520) |
| c499 | 758 | 750 | 8 | 0 | 750.0 (750–750) |
| c880 | 942 | 942 | 0 | 0 | 942.0 (942–942) |
| c1355 | 1574 | 1566 | 8 | 0 | 1566.0 (1566–1566) |
| c1908 | 1879 | 1870 | 9 | 0 | 1870.0 (1870–1870) |
| c2670 | 2747 | 2630 | 117 | 0 | 2342.0 (2320–2420) |
| c3540 | 3428 | 3291 | 137 | 0 | 3290.8 (3290–3291) |
| c5315 | 5350 | 5291 | 59 | 0 | 5291.0 (5291–5291) |
| c6288 | 7744 | 7710 | 34 | 0 | 7710.0 (7710–7710) |
| c7552 | 7550 | 7419 | 131 | 0 | 7189.6 (7155–7203) |

## Cost relative to the hindsight-best fixed switch — total work

| circuit | PODEM only | Fixed switch @128 | Fixed switch @1024 | Fixed switch @8192 | Plateau rule (6 blocks, ≤1) | Cost-bound test only (stage 1) | Adaptive, two-stage |
|---|---:|---:|---:|---:|---:|---:|---:|
| c432 | 3.021 | 1.452 | 1.001 | 1.052 | 1.001 | 1.002 | 1.002 |
| c499 | 1.137 | 1.014 | 1.000 | 1.020 | 1.000 | 1.001 | 1.001 |
| c880 | 3.660 | 1.929 | 1.000 | 1.531 | 1.003 | 1.024 | 1.000 |
| c1355 | 1.317 | 1.156 | 1.028 | 1.017 | 1.000 | 0.999 | 0.999 |
| c1908 | 1.717 | 1.472 | 1.104 | 1.055 | 1.006 | 1.007 | 1.009 |
| c2670 | 1.035 | 1.015 | 1.000 | 1.031 | 1.000 | 1.001 | 1.000 |
| c3540 | 1.220 | 1.167 | 1.027 | 1.002 | 1.007 | 1.007 | 1.004 |
| c5315 | 2.054 | 1.719 | 1.125 | 1.091 | 1.003 | 0.990 | 1.021 |
| c6288 | 1.067 | 1.003 | 1.010 | 1.093 | 1.004 | 1.004 | 1.004 |
| c7552 | 1.180 | 1.133 | 1.008 | 1.010 | 0.999 | 1.002 | 1.000 |
| geomean | 1.573 | 1.273 | 1.029 | 1.082 | 1.002 | 1.004 | 1.004 |
| geomean, 95% bootstrap interval | 1.553–1.595 | 1.258–1.290 | 1.024–1.037 | 1.074–1.092 | 1.000–1.008 | 1.002–1.008 | 1.002–1.009 |
| worst circuit | 3.660 | 1.929 | 1.125 | 1.531 | 1.007 | 1.024 | 1.021 |
| worst single seed | 4.291 | 2.437 | 1.288 | 1.898 | 1.155 | 1.085 | 1.092 |

Best fixed N per circuit: c432 768, c499 768, c880 1024, c1355 2048, c1908 3072, c2670 1024, c3540 4096, c5315 3072, c6288 192, c7552 2048


## Cost relative to the hindsight-best fixed switch — switch-dependent work

Switch-dependent work = total work minus PODEM work on faults that end the run proved redundant (by PODEM, or by the SAT fallback after PODEM aborted). That set is identical for every flow, and every flow pays for it wherever it switches.

| circuit | PODEM only | Fixed switch @128 | Fixed switch @1024 | Fixed switch @8192 | Plateau rule (6 blocks, ≤1) | Cost-bound test only (stage 1) | Adaptive, two-stage |
|---|---:|---:|---:|---:|---:|---:|---:|
| c432 | 29.131 | 7.291 | 1.019 | 1.725 | 1.017 | 1.032 | 1.025 |
| c499 | 8.865 | 1.801 | 1.020 | 2.132 | 1.021 | 1.052 | 1.052 |
| c880 | 3.660 | 1.929 | 1.000 | 1.531 | 1.003 | 1.024 | 1.000 |
| c1355 | 10.198 | 5.547 | 1.810 | 1.489 | 1.007 | 0.958 | 0.959 |
| c1908 | 4.132 | 3.060 | 1.456 | 1.238 | 1.028 | 1.030 | 1.038 |
| c2670 | 1.979 | 1.428 | 1.000 | 1.859 | 1.008 | 1.018 | 1.012 |
| c3540 | 8.192 | 6.464 | 1.875 | 1.070 | 1.226 | 1.231 | 1.135 |
| c5315 | 7.759 | 5.615 | 1.804 | 1.586 | 1.021 | 0.937 | 1.133 |
| c6288 | 1.137 | 1.006 | 1.020 | 1.190 | 1.008 | 1.009 | 1.009 |
| c7552 | 2.351 | 2.001 | 1.061 | 1.077 | 0.991 | 1.017 | 1.003 |
| geomean | 5.160 | 2.919 | 1.259 | 1.453 | 1.031 | 1.028 | 1.035 |
| geomean, 95% bootstrap interval | 5.045–5.378 | 2.833–3.056 | 1.204–1.337 | 1.442–1.493 | 1.015–1.073 | 1.014–1.066 | 1.021–1.070 |
| worst circuit | 29.131 | 7.291 | 1.875 | 2.132 | 1.226 | 1.231 | 1.135 |
| worst single seed | 37.510 | 13.333 | 5.530 | 2.272 | 2.829 | 2.833 | 2.044 |

Best fixed N per circuit: c432 768, c499 768, c880 1024, c1355 2048, c1908 3072, c2670 1024, c3540 4096, c5315 3072, c6288 192, c7552 2048


## Pairwise differences between rules — total work

Ratio < 1 means A needs less work. "Indistinguishable" means the 95% interval contains 1.

| comparison (A ÷ B) | ratio of geomeans | 95% interval | verdict |
|---|---:|---:|---|
| Plateau rule (6 blocks, ≤1) ÷ Cost-bound test only (stage 1) | 0.999 | 0.995–1.003 | indistinguishable |
| Cost-bound test only (stage 1) ÷ Adaptive, two-stage | 1.000 | 0.997–1.002 | indistinguishable |
| Plateau rule (6 blocks, ≤1) ÷ Adaptive, two-stage | 0.998 | 0.994–1.004 | indistinguishable |
| Plateau rule (6 blocks, ≤1) ÷ Fixed switch @1024 | 0.974 | 0.967–0.981 | A cheaper |
| Adaptive, two-stage ÷ Fixed switch @1024 | 0.975 | 0.968–0.982 | A cheaper |

## Pairwise differences between rules — switch-dependent work

Ratio < 1 means A needs less work. "Indistinguishable" means the 95% interval contains 1.

| comparison (A ÷ B) | ratio of geomeans | 95% interval | verdict |
|---|---:|---:|---|
| Plateau rule (6 blocks, ≤1) ÷ Cost-bound test only (stage 1) | 1.001 | 0.984–1.027 | indistinguishable |
| Cost-bound test only (stage 1) ÷ Adaptive, two-stage | 0.993 | 0.975–1.015 | indistinguishable |
| Plateau rule (6 blocks, ≤1) ÷ Adaptive, two-stage | 0.994 | 0.968–1.029 | indistinguishable |
| Plateau rule (6 blocks, ≤1) ÷ Fixed switch @1024 | 0.821 | 0.775–0.865 | A cheaper |
| Adaptive, two-stage ÷ Fixed switch @1024 | 0.825 | 0.780–0.871 | A cheaper |

## How costly is a switch that is off by 2x or 4x? (fixed N, switch-dependent work)

Cost relative to the best fixed N on the grid, for a fixed switch point 2 and 4 grid steps (about 2x and 4x) earlier or later.

| circuit | best N | N/2 | 2N | N/4 | 4N |
|---|---:|---:|---:|---:|---:|
| c432 | 768 | 1.83× | 1.06× | 3.90× | 1.20× |
| c499 | 768 | 1.14× | 1.09× | 1.52× | 1.33× |
| c880 | 1024 | 1.20× | 1.02× | 1.55× | 1.16× |
| c1355 | 2048 | 1.81× | 1.11× | 2.96× | 1.49× |
| c1908 | 3072 | 1.26× | 1.10× | 1.72× | 1.52× |
| c2670 | 1024 | 1.03× | 1.08× | 1.23× | 1.33× |
| c3540 | 4096 | 1.43× | 1.07× | 1.88× | 1.31× |
| c5315 | 3072 | 1.25× | 1.35× | 2.19× | 2.07× |
| c6288 | 192 | 1.08× | 1.00× | — | 1.01× |
| c7552 | 2048 | 1.06× | 1.00× | 1.32× | 1.08× |

## Test-set size (compacted tests, relative to PODEM only)

Reverse-order fault-simulation compaction applied to every method's test set. Pure random reaches lower coverage than the hybrids wherever its detected count is lower (Coverage table).

| circuit | Pure random | Fixed switch @128 | Fixed switch @1024 | Fixed switch @8192 | Plateau rule (6 blocks, ≤1) | Cost-bound test only (stage 1) | Adaptive, two-stage |
|---|---:|---:|---:|---:|---:|---:|---:|
| c432 | 0.998 | 0.962 | 0.997 | 0.998 | 0.990 | 0.998 | 0.995 |
| c499 | 0.802 | 0.814 | 0.802 | 0.802 | 0.802 | 0.802 | 0.802 |
| c880 | 1.041 | 1.066 | 1.028 | 1.036 | 1.032 | 1.030 | 1.024 |
| c1355 | 0.753 | 0.765 | 0.753 | 0.753 | 0.753 | 0.753 | 0.753 |
| c1908 | 0.936 | 0.992 | 0.958 | 0.937 | 0.937 | 0.937 | 0.937 |
| c2670 | 0.673 | 0.996 | 1.025 | 1.044 | 1.031 | 1.029 | 1.032 |
| c3540 | 1.042 | 1.007 | 1.031 | 1.044 | 1.044 | 1.044 | 1.045 |
| c5315 | 1.019 | 1.022 | 1.036 | 1.019 | 1.008 | 1.014 | 1.015 |
| c6288 | 1.184 | 1.172 | 1.184 | 1.184 | 1.184 | 1.184 | 1.184 |
| c7552 | 0.745 | 0.991 | 1.004 | 0.986 | 1.011 | 0.990 | 0.983 |
| geomean | 0.905 | 0.972 | 0.974 | 0.973 | 0.972 | 0.971 | 0.969 |

## Sensitivity to each rule's own settings

Each row re-runs one rule with one setting changed (all circuits × 10 seeds). Defaults: adaptive α=0.05, confirm=2, probe=16, window μ=6, warm-up 128; plateau 6 blocks, ≤1 fault.

| variant | total geomean | total worst | switch-dep. geomean | switch-dep. worst | worst seed (total) |
|---|---:|---:|---:|---:|---:|
| adaptive (default) | 1.004 | 1.021 | 1.035 | 1.135 | 1.092 |
| stage1 (default) | 1.004 | 1.024 | 1.028 | 1.231 | 1.085 |
| plateau 6/1 (default) | 1.002 | 1.007 | 1.031 | 1.226 | 1.155 |
| adaptive_alpha0.01 | 1.006 | 1.024 | 1.025 | 1.142 | 1.132 |
| adaptive_alpha0.2 | 1.002 | 1.012 | 1.029 | 1.205 | 1.089 |
| adaptive_confirm1 | 1.003 | 1.008 | 1.024 | 1.093 | 1.089 |
| adaptive_confirm3 | 1.006 | 1.036 | 1.052 | 1.232 | 1.092 |
| adaptive_probe32 | 1.005 | 1.033 | 1.045 | 1.212 | 1.132 |
| adaptive_probe8 | 1.003 | 1.016 | 1.018 | 1.070 | 1.086 |
| adaptive_warmup512 | 1.007 | 1.036 | 1.039 | 1.230 | 1.172 |
| adaptive_window12 | 1.010 | 1.068 | 1.082 | 1.435 | 1.270 |
| adaptive_window3 | 1.006 | 1.023 | 1.039 | 1.145 | 1.138 |
| plateau_12-2 | 1.005 | 1.039 | 1.021 | 1.074 | 1.137 |
| plateau_12-3 | 1.004 | 1.032 | 1.014 | 1.064 | 1.149 |
| plateau_4-0 | 1.004 | 1.017 | 1.036 | 1.232 | 1.155 |
| plateau_8-1 | 1.003 | 1.021 | 1.012 | 1.061 | 1.100 |
| plateau_8-2 | 1.002 | 1.017 | 1.014 | 1.225 | 1.127 |
| stage1_alpha0.01 | 1.003 | 1.028 | 1.018 | 1.060 | 1.124 |
| stage1_alpha0.2 | 1.004 | 1.010 | 1.037 | 1.286 | 1.125 |
| stage1_window12 | 1.005 | 1.023 | 1.041 | 1.106 | 1.173 |
| stage1_window3 | 1.005 | 1.020 | 1.035 | 1.169 | 1.155 |

## Held-out check of the tuned settings

The defaults (window μ = 6, plateau 6/1, probe 16, ...) were chosen while looking at these same circuits. To see whether tuning would transfer, each rule's variants from the sensitivity study are ranked on one half of the circuits (by total-work geomean) and the winner is scored on the other half, and compared with the default on that same half. Halves: alternate circuits in size order.

| rule | tuned on | winner | winner on the other half | default on the other half |
|---|---|---|---:|---:|
| plateau | c432, c880, c1908, c3540, c6288 | plateau (default) | 1.001 | 1.001 |
| plateau | c499, c1355, c2670, c5315, c7552 | plateau_8-2 | 1.006 | 1.004 |
| stage1 | c432, c880, c1908, c3540, c6288 | stage1_alpha0.2 | 1.001 | 0.999 |
| stage1 | c499, c1355, c2670, c5315, c7552 | stage1 (default) | 1.009 | 1.009 |
| adaptive | c432, c880, c1908, c3540, c6288 | adaptive_alpha0.2 | 1.002 | 1.004 |
| adaptive | c499, c1355, c2670, c5315, c7552 | adaptive_probe8 | 1.005 | 1.004 |

## Robustness to the cost of PODEM

Full sweeps with PODEM work multiplied by w (a slower or faster ATPG engine relative to fault simulation). Each weight has its own fixed-N oracle. Entries: geomean / worst circuit.

| w | metric | PODEM only | Fixed switch @1024 | Plateau rule (6 blocks, ≤1) | Cost-bound test only (stage 1) | Adaptive, two-stage |
|---|---|---:|---:|---:|---:|---:|
| 1 | total work | 1.573 / 3.66 | 1.029 / 1.13 | 1.002 / 1.01 | 1.004 / 1.02 | 1.004 / 1.02 |
| 1 | switch-dependent work | 5.160 / 29.13 | 1.259 / 1.88 | 1.031 / 1.23 | 1.028 / 1.23 | 1.035 / 1.13 |
| 0.25 | total work | 1.452 / 2.65 | 1.026 / 1.10 | 1.010 / 1.05 | 1.018 / 1.10 | 1.022 / 1.09 |
| 0.25 | switch-dependent work | 2.673 / 8.11 | 1.105 / 1.34 | 1.033 / 1.14 | 1.053 / 1.16 | 1.071 / 1.18 |
| 4 | total work | 1.763 / 8.87 | 1.069 / 1.28 | 1.022 / 1.19 | 1.022 / 1.19 | 1.018 / 1.14 |
| 4 | switch-dependent work | 12.483 / 101.35 | 1.883 / 6.73 | 1.104 / 1.48 | 1.110 / 1.48 | 1.079 / 1.49 |
| 16 | total work | 1.982 / 25.00 | 1.150 / 2.53 | 1.088 / 2.14 | 1.085 / 2.09 | 1.075 / 1.93 |
| 16 | switch-dependent work | 33.084 / 388.57 | 3.207 / 22.58 | 1.428 / 2.64 | 1.410 / 2.62 | 1.330 / 2.73 |

## A smaller PODEM budget, now that SAT resolves every abort

Full sweeps (5 seeds) with the PODEM backtrack limit lowered; 1024 is the default (10 seeds). Left block: how the rules rank against their own fixed-N oracle (total work, geomean / worst circuit). Right block: totals over all circuits for the adaptive method. SAT propagation steps are a different unit from gate evaluations and are not included in total work.

| PODEM backtrack limit | PODEM only | Fixed switch @1024 | Plateau rule (6 blocks, ≤1) | Cost-bound test only (stage 1) | Adaptive, two-stage | adaptive: PODEM aborts → SAT | adaptive: total work (M) | adaptive: SAT propagations (M) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 32 | 2.733 / 5.92 | 1.061 / 1.30 | 1.007 / 1.02 | 1.014 / 1.05 | 1.026 / 1.11 | 297 | 10.5 | 2.76 |
| 128 | 2.066 / 4.07 | 1.056 / 1.25 | 1.004 / 1.01 | 1.010 / 1.05 | 1.018 / 1.07 | 263 | 20.7 | 2.40 |
| 1024 | 1.573 / 3.66 | 1.029 / 1.13 | 1.002 / 1.01 | 1.004 / 1.02 | 1.004 / 1.02 | 215 | 76.5 | 1.99 |

## SAT fallback

Faults PODEM aborted on (1024 backtracks) are handed to the SAT solver once the PODEM phase is done. Means over seeds, adaptive method. Propagation steps are SAT-solver units, not gate evaluations.

| circuit | PODEM aborts → SAT | SAT tests | proved redundant | redundant faults (all) | SAT propagations (M) |
|---|---:|---:|---:|---:|---:|
| c17 | 0.0 | 0.0 | 0.0 | 0.0 | 0.00 |
| c432 | 4.0 | 0.0 | 4.0 | 4.0 | 0.01 |
| c499 | 8.0 | 0.0 | 8.0 | 8.0 | 0.11 |
| c880 | 0.0 | 0.0 | 0.0 | 0.0 | 0.00 |
| c1355 | 8.0 | 0.0 | 8.0 | 8.0 | 0.18 |
| c1908 | 3.0 | 0.0 | 3.0 | 9.0 | 0.01 |
| c2670 | 53.0 | 0.0 | 53.0 | 117.0 | 0.67 |
| c3540 | 53.5 | 0.5 | 53.0 | 137.0 | 0.46 |
| c5315 | 4.0 | 0.0 | 4.0 | 59.0 | 0.04 |
| c6288 | 2.0 | 0.0 | 2.0 | 34.0 | 0.00 |
| c7552 | 79.1 | 0.1 | 79.0 | 131.0 | 0.51 |

## Per circuit

Wall-clock times are single runs of millisecond-scale jobs and vary by tens of percent between repeats; use the work columns for comparisons.


### c17 — 5 PI, 2 PO, 6 gates, 22 collapsed faults (34 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 22.0 | n/a | n/a | 100.000 | 100.000 | 5.9 ± 1.1 | — | 0 | 0.000 | 0.000 | 0.0 |
| PODEM only | 22.0 | 0.0 | 0.0 | 100.000 | 100.000 | 5.5 ± 0.7 | — | 6 ± 1 | 0.000 | 0.000 | 0.0 |
| Fixed switch @128 | 22.0 | 0.0 | 0.0 | 100.000 | 100.000 | 5.9 ± 1.1 | 64 | 0 | 0.000 | 0.000 | 0.0 |
| Fixed switch @1024 | 22.0 | 0.0 | 0.0 | 100.000 | 100.000 | 5.9 ± 1.1 | 64 | 0 | 0.000 | 0.000 | 0.0 |
| Fixed switch @8192 | 22.0 | 0.0 | 0.0 | 100.000 | 100.000 | 5.9 ± 1.1 | 64 | 0 | 0.000 | 0.000 | 0.0 |
| Plateau rule (6 blocks, ≤1) | 22.0 | 0.0 | 0.0 | 100.000 | 100.000 | 5.9 ± 1.1 | 64 | 0 | 0.000 | 0.000 | 0.0 |
| Cost-bound test only (stage 1) | 22.0 | 0.0 | 0.0 | 100.000 | 100.000 | 5.9 ± 1.1 | 64 | 0 | 0.000 | 0.000 | 0.0 |
| Adaptive, two-stage | 22.0 | 0.0 | 0.0 | 100.000 | 100.000 | 5.9 ± 1.1 | 64 | 0 | 0.000 | 0.000 | 0.0 |

### c432 — 36 PI, 7 PO, 160 gates, 524 collapsed faults (864 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 520.0 | n/a | n/a | 99.237 | 99.237 | 57.9 ± 3.8 | — | 0 | 0.109 | 0.109 | 1.1 ± 0.1 |
| PODEM only | 520.0 | 4.0 | 0.0 | 99.237 | 100.000 | 58.0 ± 3.2 | — | 73 ± 3 | 1.080 ± 0.161 | 0.748 ± 0.161 | 23.2 ± 3.9 |
| Fixed switch @128 | 520.0 | 4.0 | 0.0 | 99.237 | 100.000 | 55.8 ± 3.3 | 128 | 20 ± 3 | 0.519 ± 0.082 | 0.187 ± 0.082 | 13.2 ± 2.2 |
| Fixed switch @1024 | 520.0 | 4.0 | 0.0 | 99.237 | 100.000 | 57.8 ± 3.6 | 1024 | 5 ± 1 | 0.358 ± 0.001 | 0.026 ± 0.001 | 10.3 ± 1.2 |
| Fixed switch @8192 | 520.0 | 4.0 | 0.0 | 99.237 | 100.000 | 57.9 ± 3.8 | 8192 | 4 | 0.376 | 0.044 | 9.2 ± 0.2 |
| Plateau rule (6 blocks, ≤1) | 520.0 | 4.0 | 0.0 | 99.237 | 100.000 | 57.4 ± 3.7 | 1005 ± 157 | 5 ± 1 | 0.358 ± 0.001 | 0.026 ± 0.001 | 12.1 ± 0.3 |
| Cost-bound test only (stage 1) | 520.0 | 4.0 | 0.0 | 99.237 | 100.000 | 57.9 ± 3.8 | 1248 ± 304 | 4 ± 1 | 0.358 ± 0.001 | 0.027 ± 0.001 | 11.7 ± 1.0 |
| Adaptive, two-stage | 520.0 | 4.0 | 0.0 | 99.237 | 100.000 | 57.7 ± 3.8 | 1248 ± 304 | 4 | 0.358 ± 0.001 | 0.026 ± 0.001 | 12.4 ± 0.2 |

### c499 — 41 PI, 32 PO, 202 gates, 758 collapsed faults (998 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 750.0 | n/a | n/a | 98.945 | 98.945 | 54.3 ± 0.9 | — | 0 | 0.166 ± 0.001 | 0.166 ± 0.001 | 2.1 ± 0.1 |
| PODEM only | 750.0 | 8.0 | 0.0 | 98.945 | 100.000 | 67.7 ± 1.8 | — | 86 ± 3 | 1.803 ± 0.009 | 0.245 ± 0.009 | 39.7 ± 1.0 |
| Fixed switch @128 | 750.0 | 8.0 | 0.0 | 98.945 | 100.000 | 55.1 ± 1.4 | 128 | 31 ± 3 | 1.608 ± 0.003 | 0.050 ± 0.003 | 25.6 ± 0.8 |
| Fixed switch @1024 | 750.0 | 8.0 | 0.0 | 98.945 | 100.000 | 54.3 ± 0.9 | 1024 | 8 ± 1 | 1.586 ± 0.001 | 0.028 ± 0.001 | 25.4 ± 0.5 |
| Fixed switch @8192 | 750.0 | 8.0 | 0.0 | 98.945 | 100.000 | 54.3 ± 0.9 | 8192 | 8 | 1.617 ± 0.001 | 0.059 ± 0.001 | 29.4 ± 6.5 |
| Plateau rule (6 blocks, ≤1) | 750.0 | 8.0 | 0.0 | 98.945 | 100.000 | 54.3 ± 0.9 | 1030 ± 115 | 8 ± 1 | 1.586 ± 0.001 | 0.028 ± 0.001 | 25.9 ± 0.9 |
| Cost-bound test only (stage 1) | 750.0 | 8.0 | 0.0 | 98.945 | 100.000 | 54.3 ± 0.9 | 1229 ± 124 | 8 ± 1 | 1.587 ± 0.001 | 0.029 ± 0.001 | 26.2 ± 1.4 |
| Adaptive, two-stage | 750.0 | 8.0 | 0.0 | 98.945 | 100.000 | 54.3 ± 0.9 | 1229 ± 124 | 8 ± 1 | 1.587 ± 0.001 | 0.029 ± 0.001 | 25.4 ± 1.5 |

### c880 — 60 PI, 26 PO, 383 gates, 942 collapsed faults (1760 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 942.0 | n/a | n/a | 100.000 | 100.000 | 66.0 ± 2.4 | — | 0 | 0.099 ± 0.034 | 0.099 ± 0.034 | 1.1 ± 0.1 |
| PODEM only | 942.0 | 0.0 | 0.0 | 100.000 | 100.000 | 63.4 ± 1.5 | — | 80 ± 5 | 0.180 ± 0.012 | 0.180 ± 0.012 | 2.7 ± 0.2 |
| Fixed switch @128 | 942.0 | 0.0 | 0.0 | 100.000 | 100.000 | 67.6 ± 3.1 | 128 | 33 ± 4 | 0.095 ± 0.009 | 0.095 ± 0.009 | 1.6 ± 0.2 |
| Fixed switch @1024 | 942.0 | 0.0 | 0.0 | 100.000 | 100.000 | 65.2 ± 2.4 | 1024 | 9 ± 2 | 0.049 ± 0.004 | 0.049 ± 0.004 | 1.1 ± 0.2 |
| Fixed switch @8192 | 942.0 | 0.0 | 0.0 | 100.000 | 100.000 | 65.7 ± 2.1 | 7520 ± 1016 | 1 ± 1 | 0.075 ± 0.007 | 0.075 ± 0.007 | 1.0 |
| Plateau rule (6 blocks, ≤1) | 942.0 | 0.0 | 0.0 | 100.000 | 100.000 | 65.4 ± 3.8 | 1600 ± 615 | 7 ± 3 | 0.049 ± 0.003 | 0.049 ± 0.003 | 1.2 ± 0.2 |
| Cost-bound test only (stage 1) | 942.0 | 0.0 | 0.0 | 100.000 | 100.000 | 65.3 ± 3.6 | 1882 ± 709 | 7 ± 3 | 0.050 ± 0.004 | 0.050 ± 0.004 | 1.2 ± 0.1 |
| Adaptive, two-stage | 942.0 | 0.0 | 0.0 | 100.000 | 100.000 | 64.9 ± 3.1 | 1882 ± 709 | 6 ± 3 | 0.049 ± 0.004 | 0.049 ± 0.004 | 1.2 ± 0.2 |

### c1355 — 41 PI, 32 PO, 546 gates, 1574 collapsed faults (2710 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 1566.0 | n/a | n/a | 99.492 | 99.492 | 86.5 ± 1.0 | — | 0 | 0.395 ± 0.002 | 0.395 ± 0.002 | 3.6 ± 0.3 |
| PODEM only | 1566.0 | 8.0 | 0.0 | 99.492 | 100.000 | 114.9 ± 3.7 | — | 142 ± 8 | 4.020 ± 0.078 | 1.072 ± 0.078 | 60.0 ± 11.3 |
| Fixed switch @128 | 1566.0 | 8.0 | 0.0 | 99.492 | 100.000 | 87.9 ± 2.5 | 128 | 63 ± 4 | 3.531 ± 0.099 | 0.583 ± 0.099 | 58.3 ± 4.4 |
| Fixed switch @1024 | 1566.0 | 8.0 | 0.0 | 99.492 | 100.000 | 86.5 ± 1.0 | 1024 | 17 ± 3 | 3.138 ± 0.142 | 0.190 ± 0.142 | 48.9 ± 10.7 |
| Fixed switch @8192 | 1566.0 | 8.0 | 0.0 | 99.492 | 100.000 | 86.5 ± 1.0 | 8192 | 8 | 3.105 ± 0.002 | 0.156 ± 0.002 | 41.5 ± 0.2 |
| Plateau rule (6 blocks, ≤1) | 1566.0 | 8.0 | 0.0 | 99.492 | 100.000 | 86.5 ± 1.0 | 2080 ± 272 | 9 ± 1 | 3.054 ± 0.022 | 0.106 ± 0.022 | 43.1 ± 1.3 |
| Cost-bound test only (stage 1) | 1566.0 | 8.0 | 0.0 | 99.492 | 100.000 | 86.5 ± 1.0 | 2298 ± 394 | 9 ± 1 | 3.049 ± 0.004 | 0.101 ± 0.004 | 46.4 ± 6.6 |
| Adaptive, two-stage | 1566.0 | 8.0 | 0.0 | 99.492 | 100.000 | 86.5 ± 1.0 | 2298 ± 394 | 9 ± 1 | 3.049 ± 0.004 | 0.101 ± 0.004 | 44.1 ± 2.3 |

### c1908 — 33 PI, 25 PO, 880 gates, 1879 collapsed faults (3816 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 1870.0 | n/a | n/a | 99.521 | 99.521 | 125.6 ± 4.1 | — | 0 | 0.595 ± 0.006 | 0.595 ± 0.006 | 5.1 ± 0.2 |
| PODEM only | 1870.0 | 9.0 | 0.0 | 99.521 | 100.000 | 134.2 ± 2.8 | — | 167 ± 2 | 1.497 ± 0.013 | 0.825 ± 0.013 | 32.4 ± 2.7 |
| Fixed switch @128 | 1870.0 | 9.0 | 0.0 | 99.521 | 100.000 | 133.1 ± 3.4 | 128 | 128 ± 6 | 1.283 ± 0.039 | 0.611 ± 0.039 | 31.8 ± 5.8 |
| Fixed switch @1024 | 1870.0 | 9.0 | 0.0 | 99.521 | 100.000 | 128.6 ± 3.8 | 1024 | 48 ± 5 | 0.963 ± 0.043 | 0.291 ± 0.043 | 21.8 ± 1.3 |
| Fixed switch @8192 | 1870.0 | 9.0 | 0.0 | 99.521 | 100.000 | 125.8 ± 4.2 | 8192 | 10 ± 1 | 0.919 ± 0.007 | 0.247 ± 0.007 | 21.8 ± 3.9 |
| Plateau rule (6 blocks, ≤1) | 1870.0 | 9.0 | 0.0 | 99.521 | 100.000 | 125.7 ± 4.2 | 2893 ± 398 | 18 ± 3 | 0.877 ± 0.023 | 0.205 ± 0.023 | 20.7 ± 1.1 |
| Cost-bound test only (stage 1) | 1870.0 | 9.0 | 0.0 | 99.521 | 100.000 | 125.7 ± 4.2 | 3149 ± 534 | 17 ± 4 | 0.878 ± 0.023 | 0.206 ± 0.023 | 20.6 ± 0.8 |
| Adaptive, two-stage | 1870.0 | 9.0 | 0.0 | 99.521 | 100.000 | 125.7 ± 4.2 | 3507 ± 1008 | 16 ± 3 | 0.879 ± 0.033 | 0.207 ± 0.033 | 21.5 ± 3.5 |

### c2670 — 233 PI, 140 PO, 1269 gates, 2747 collapsed faults (5492 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 2342.0 ± 28.1 | n/a | n/a | 85.257 ± 1.024 | 85.257 ± 1.024 | 76.4 ± 4.3 | — | 0 | 2.266 ± 0.038 | 2.266 ± 0.038 | 47.6 ± 2.1 |
| PODEM only | 2630.0 | 117.0 | 0.0 | 95.741 | 100.000 | 113.6 ± 3.2 | — | 256 ± 4 | 14.981 ± 0.065 | 1.020 ± 0.065 | 346.1 ± 13.6 |
| Fixed switch @128 | 2630.0 | 117.0 | 0.0 | 95.741 | 100.000 | 113.2 ± 4.1 | 128 | 218 ± 6 | 14.696 ± 0.065 | 0.736 ± 0.065 | 343.2 ± 13.9 |
| Fixed switch @1024 | 2630.0 | 117.0 | 0.0 | 95.741 | 100.000 | 116.4 ± 2.7 | 1024 | 194 ± 2 | 14.476 ± 0.046 | 0.516 ± 0.046 | 359.2 ± 33.2 |
| Fixed switch @8192 | 2630.0 | 117.0 | 0.0 | 95.741 | 100.000 | 118.6 ± 3.0 | 8192 | 191 ± 2 | 14.918 ± 0.010 | 0.958 ± 0.010 | 354.1 ± 17.5 |
| Plateau rule (6 blocks, ≤1) | 2630.0 | 117.0 | 0.0 | 95.741 | 100.000 | 117.1 ± 3.2 | 1459 ± 325 | 192 ± 2 | 14.480 ± 0.026 | 0.520 ± 0.026 | 341.3 ± 12.0 |
| Cost-bound test only (stage 1) | 2630.0 | 117.0 | 0.0 | 95.741 | 100.000 | 116.9 ± 3.8 | 1568 ± 338 | 192 ± 2 | 14.485 ± 0.032 | 0.525 ± 0.032 | 341.5 ± 17.6 |
| Adaptive, two-stage | 2630.0 | 117.0 | 0.0 | 95.741 | 100.000 | 117.2 ± 3.5 | 1696 ± 338 | 192 ± 1 | 14.482 ± 0.031 | 0.522 ± 0.031 | 357.4 ± 36.9 |

### c3540 — 50 PI, 22 PO, 1669 gates, 3428 collapsed faults (7080 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 3290.8 ± 0.4 | n/a | n/a | 95.998 ± 0.012 | 95.998 ± 0.012 | 180.4 ± 3.4 | — | 0 | 1.436 ± 0.006 | 1.436 ± 0.006 | 21.2 ± 2.5 |
| PODEM only | 3291.0 | 137.0 | 0.0 | 96.004 | 100.000 | 173.2 ± 4.8 | — | 355 ± 7 | 32.582 ± 0.840 | 6.696 ± 0.840 | 934.4 ± 49.3 |
| Fixed switch @128 | 3291.0 | 137.0 | 0.0 | 96.004 | 100.000 | 174.4 ± 3.6 | 128 | 276 ± 7 | 31.170 ± 0.572 | 5.284 ± 0.572 | 864.1 ± 22.0 |
| Fixed switch @1024 | 3291.0 | 137.0 | 0.0 | 96.004 | 100.000 | 178.6 ± 4.0 | 1024 | 175 ± 4 | 27.419 ± 0.418 | 1.533 ± 0.418 | 760.5 ± 30.6 |
| Fixed switch @8192 | 3291.0 | 137.0 | 0.0 | 96.004 | 100.000 | 180.8 ± 3.1 | 8192 | 140 ± 1 | 26.761 ± 0.256 | 0.875 ± 0.256 | 778.6 ± 26.0 |
| Plateau rule (6 blocks, ≤1) | 3291.0 | 137.0 | 0.0 | 96.004 | 100.000 | 180.9 ± 2.8 | 2707 ± 293 | 148 ± 3 | 26.888 ± 0.375 | 1.002 ± 0.375 | 758.3 ± 34.7 |
| Cost-bound test only (stage 1) | 3291.0 | 137.0 | 0.0 | 96.004 | 100.000 | 180.8 ± 3.1 | 2931 ± 515 | 147 ± 4 | 26.892 ± 0.371 | 1.006 ± 0.371 | 739.4 ± 31.6 |
| Adaptive, two-stage | 3291.0 | 137.0 | 0.0 | 96.004 | 100.000 | 181.0 ± 2.6 | 6214 ± 2633 | 144 ± 4 | 26.814 ± 0.334 | 0.928 ± 0.334 | 747.7 ± 28.4 |

### c5315 — 178 PI, 123 PO, 2307 gates, 5350 collapsed faults (10630 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 5291.0 | n/a | n/a | 98.897 | 98.897 | 142.9 ± 5.3 | — | 0 | 1.448 ± 0.001 | 1.448 ± 0.001 | 13.4 ± 0.3 |
| PODEM only | 5291.0 | 59.0 | 0.0 | 98.897 | 100.000 | 140.2 ± 4.6 | — | 238 ± 10 | 4.271 ± 0.273 | 2.515 ± 0.273 | 107.9 ± 11.1 |
| Fixed switch @128 | 5291.0 | 59.0 | 0.0 | 98.897 | 100.000 | 143.3 ± 6.6 | 128 | 158 ± 5 | 3.576 ± 0.222 | 1.820 ± 0.222 | 96.2 ± 8.2 |
| Fixed switch @1024 | 5291.0 | 59.0 | 0.0 | 98.897 | 100.000 | 145.2 ± 5.2 | 1024 | 71 ± 3 | 2.340 ± 0.206 | 0.585 ± 0.206 | 65.9 ± 5.2 |
| Fixed switch @8192 | 5291.0 | 59.0 | 0.0 | 98.897 | 100.000 | 142.9 ± 5.3 | 8192 | 59 | 2.269 ± 0.001 | 0.514 ± 0.001 | 56.8 ± 3.5 |
| Plateau rule (6 blocks, ≤1) | 5291.0 | 59.0 | 0.0 | 98.897 | 100.000 | 141.3 ± 5.0 | 2054 ± 255 | 61 ± 2 | 2.086 ± 0.109 | 0.331 ± 0.109 | 61.8 ± 5.5 |
| Cost-bound test only (stage 1) | 5291.0 | 59.0 | 0.0 | 98.897 | 100.000 | 142.2 ± 5.9 | 2413 ± 165 | 60 ± 1 | 2.059 ± 0.009 | 0.304 ± 0.009 | 59.3 ± 3.5 |
| Adaptive, two-stage | 5291.0 | 59.0 | 0.0 | 98.897 | 100.000 | 142.3 ± 5.4 | 4141 ± 917 | 60 ± 1 | 2.123 ± 0.031 | 0.367 ± 0.031 | 57.2 ± 4.0 |

### c6288 — 32 PI, 32 PO, 2416 gates, 7744 collapsed faults (12576 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 7710.0 | n/a | n/a | 99.561 | 99.561 | 39.3 ± 3.0 | — | 0 | 2.858 ± 0.024 | 2.858 ± 0.024 | 45.0 ± 3.8 |
| PODEM only | 7710.0 | 34.0 | 0.0 | 99.561 | 100.000 | 33.2 ± 2.7 | — | 70 ± 2 | 3.517 ± 0.399 | 1.832 ± 0.399 | 84.2 ± 10.8 |
| Fixed switch @128 | 7710.0 | 34.0 | 0.0 | 99.561 | 100.000 | 38.9 ± 2.3 | 128 | 34 ± 1 | 3.306 ± 0.021 | 1.621 ± 0.021 | 85.3 ± 3.8 |
| Fixed switch @1024 | 7710.0 | 34.0 | 0.0 | 99.561 | 100.000 | 39.3 ± 3.0 | 1024 | 34 | 3.328 ± 0.024 | 1.643 ± 0.024 | 90.5 ± 4.6 |
| Fixed switch @8192 | 7710.0 | 34.0 | 0.0 | 99.561 | 100.000 | 39.3 ± 3.0 | 8192 | 34 | 3.603 ± 0.024 | 1.918 ± 0.024 | 85.4 ± 1.4 |
| Plateau rule (6 blocks, ≤1) | 7710.0 | 34.0 | 0.0 | 99.561 | 100.000 | 39.3 ± 3.0 | 538 ± 33 | 34 | 3.310 ± 0.024 | 1.625 ± 0.024 | 87.2 ± 2.8 |
| Cost-bound test only (stage 1) | 7710.0 | 34.0 | 0.0 | 99.561 | 100.000 | 39.3 ± 3.0 | 550 ± 176 | 34 | 3.310 ± 0.026 | 1.625 ± 0.026 | 88.1 ± 2.9 |
| Adaptive, two-stage | 7710.0 | 34.0 | 0.0 | 99.561 | 100.000 | 39.3 ± 3.0 | 550 ± 176 | 34 | 3.310 ± 0.026 | 1.625 ± 0.026 | 88.8 ± 5.1 |

### c7552 — 207 PI, 108 PO, 3513 gates, 7550 collapsed faults (15106 uncollapsed)

| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Pure random | 7189.6 ± 14.2 | n/a | n/a | 95.226 ± 0.188 | 95.226 ± 0.188 | 175.9 ± 5.7 | — | 0 | 3.462 ± 0.066 | 3.462 ± 0.066 | 70.7 ± 3.9 |
| PODEM only | 7419.0 | 131.0 | 0.0 | 98.265 | 100.000 | 236.0 ± 5.9 | — | 441 ± 4 | 28.171 ± 0.649 | 7.478 ± 0.649 | 676.0 ± 23.3 |
| Fixed switch @128 | 7419.0 | 131.0 | 0.0 | 98.265 | 100.000 | 233.8 ± 4.8 | 128 | 358 ± 8 | 27.059 ± 0.677 | 6.367 ± 0.677 | 660.2 ± 20.7 |
| Fixed switch @1024 | 7419.0 | 131.0 | 0.0 | 98.265 | 100.000 | 237.0 ± 7.9 | 1024 | 277 ± 4 | 24.067 ± 0.180 | 3.374 ± 0.180 | 588.6 ± 22.1 |
| Fixed switch @8192 | 7419.0 | 131.0 | 0.0 | 98.265 | 100.000 | 232.7 ± 5.3 | 8192 | 240 ± 4 | 24.118 ± 0.178 | 3.425 ± 0.178 | 595.6 ± 28.4 |
| Plateau rule (6 blocks, ≤1) | 7419.0 | 131.0 | 0.0 | 98.265 | 100.000 | 238.5 ± 4.9 | 3053 ± 819 | 256 ± 7 | 23.845 ± 0.141 | 3.152 ± 0.141 | 584.3 ± 12.7 |
| Cost-bound test only (stage 1) | 7419.0 | 131.0 | 0.0 | 98.265 | 100.000 | 233.7 ± 8.0 | 3763 ± 1783 | 253 ± 9 | 23.929 ± 0.119 | 3.236 ± 0.119 | 596.2 ± 27.2 |
| Adaptive, two-stage | 7419.0 | 131.0 | 0.0 | 98.265 | 100.000 | 231.9 ± 6.6 | 4058 ± 1799 | 250 ± 8 | 23.882 ± 0.231 | 3.189 ± 0.231 | 577.1 ± 19.6 |
