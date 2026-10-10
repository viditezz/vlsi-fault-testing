#!/usr/bin/env bash
# Robustness to the relative cost of PODEM vs fault simulation: re-runs the full
# sweep (fixed-N grid included, so each weight gets its own oracle) with PODEM
# work weighted 0.25x, 4x and 16x. Output: results/weight/<w>/summary.csv
#   usage: scripts/cost_weight.sh [results_dir] [parallel_jobs]
set -euo pipefail
OUT=${1:-results}/weight
JOBS=${2:-2}
mkdir -p "$OUT"
printf '%s\n' 0.25 4 16 | xargs -P "$JOBS" -I{} sh -c \
  "./faultatpg sweep benchmarks '$OUT/{}' --methods podem,fixed,plateau,stage1,adaptive --quiet 1 --weight {} && echo done {}"
