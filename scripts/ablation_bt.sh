#!/usr/bin/env bash
# What a smaller PODEM backtrack budget does now that the SAT fallback resolves
# every abort exactly. Full sweep with --bt 32 and --bt 128, 5 seeds each.
# Output: results/bt/<limit>/summary.csv   (work_sat there is the fallback's cost,
# in SAT propagation steps, which is not added to work_total)
#   usage: scripts/ablation_bt.sh [results_dir] [parallel_jobs]
set -euo pipefail
OUT=${1:-results}/bt
JOBS=${2:-2}
mkdir -p "$OUT"
printf '%s\n' 32 128 | xargs -P "$JOBS" -I{} sh -c \
  "./faultatpg sweep benchmarks '$OUT/{}' --methods podem,fixed,plateau,stage1,adaptive --quiet 1 --seeds 5 --bt {} && echo done {}"
