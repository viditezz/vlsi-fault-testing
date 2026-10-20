#!/bin/bash
# ITC'99 (full scan), held out: none of the settings were tuned on these circuits.
# The five small circuits get 10 seeds; the five large ones 3 seeds (b17 alone takes hours).
# Every sweep is independent, so they can run in parallel on separate cores.
set -e
cd "$(dirname "$0")/.."
OUT=results_itc99
mkdir -p $OUT/parts
./faultatpg sweep benchmarks $OUT/parts/small --circuits b04,b05,b11,b12,b14 --seeds 10 --quiet 1
for c in b15 b17 b20 b21 b22; do
  ./faultatpg sweep benchmarks $OUT/parts/$c --circuits $c --seeds 3 --quiet 1
done
head -1 $OUT/parts/small/summary.csv > $OUT/summary.csv
head -1 $OUT/parts/small/circuits.csv > $OUT/circuits.csv
for p in small b20 b15 b21 b22 b17; do
  tail -n +2 $OUT/parts/$p/summary.csv >> $OUT/summary.csv
  tail -n +2 $OUT/parts/$p/circuits.csv >> $OUT/circuits.csv
done
python3 scripts/plot_results.py $OUT
# grid extension where the oracle sits at or near the top of the 64..32768 grid
for c in b04 b14; do ./faultatpg sweep benchmarks $OUT/parts/ext_$c --circuits $c --methods fixed --switches 49152,65536 --budget 65536 --seeds 10 --quiet 1; done
for c in b20 b22; do ./faultatpg sweep benchmarks $OUT/parts/ext_$c --circuits $c --methods fixed --switches 49152,65536 --budget 65536 --seeds 3 --quiet 1; done
