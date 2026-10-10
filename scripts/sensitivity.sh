#!/usr/bin/env bash
# Sensitivity of the switching rules to their own settings.
# Each variant re-runs one method on all circuits x 5 seeds into
# results/sensitivity/<name>/summary.csv; plot_results.py compares them with the
# fixed-N oracle from results/summary.csv.
#   usage: scripts/sensitivity.sh [results_dir] [parallel_jobs]
set -euo pipefail
OUT=${1:-results}/sensitivity
JOBS=${2:-2}
mkdir -p "$OUT"
variants=(
  "adaptive_alpha0.01  adaptive --alpha 0.01"
  "adaptive_alpha0.2   adaptive --alpha 0.2"
  "adaptive_confirm1   adaptive --confirm 1"
  "adaptive_confirm3   adaptive --confirm 3"
  "adaptive_probe8     adaptive --probe 8"
  "adaptive_probe32    adaptive --probe 32"
  "adaptive_window3    adaptive --window 3"
  "adaptive_window12   adaptive --window 12"
  "adaptive_warmup512  adaptive --warmup 512"
  "stage1_alpha0.01    stage1 --alpha 0.01"
  "stage1_alpha0.2     stage1 --alpha 0.2"
  "stage1_window3      stage1 --window 3"
  "stage1_window12     stage1 --window 12"
  "plateau_4-0         plateau --plateau-blocks 4 --plateau-max 0"
  "plateau_8-1         plateau --plateau-blocks 8 --plateau-max 1"
  "plateau_8-2         plateau --plateau-blocks 8 --plateau-max 2"
  "plateau_12-2        plateau --plateau-blocks 12 --plateau-max 2"
  "plateau_12-3        plateau --plateau-blocks 12 --plateau-max 3"
)
for v in "${variants[@]}"; do echo "$v"; done |
  xargs -P "$JOBS" -I{} bash -c 'set -- {}; name=$1; method=$2; shift 2;
    ./faultatpg sweep benchmarks "'"$OUT"'/$name" --methods "$method" --quiet 1 "$@" &&
    echo "done $name"'
