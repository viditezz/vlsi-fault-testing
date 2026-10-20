#!/usr/bin/env python3
"""How much do the conclusions depend on the SAT weight c in the combined cost?

Usage: c_sensitivity.py <results_dir> [<results_dir> ...]   (each with summary.csv)

combined = work_total + c * work_sat. For each c in 0, 1, 2, 5, 10, 20 this prints
whether the per-circuit oracle is the same as at c = 0, the geometric-mean regret
of each online rule against its own oracle, and the best single fixed N for the
whole set (chosen in hindsight).
"""
import csv
import math
import statistics as st
import sys
from collections import defaultdict

RULES = ["plateau", "stage1", "adaptive"]

for d in sys.argv[1:]:
    g = defaultdict(list)
    for r in csv.DictReader(open(d + "/summary.csv")):
        if r["circuit"] != "c17":
            g[(r["circuit"], r["method"])].append(r)
    cs = sorted({c for c, _ in g})
    fx = sorted({m for _, m in g if m.startswith("fixed-")}, key=lambda m: int(m.split("-")[1]))
    print("==", d)
    base = None
    for c in [0, 1, 2, 5, 10, 20]:
        M = {k: st.mean(float(r["work_total"]) + c * float(r["work_sat"]) for r in v) for k, v in g.items()}
        orc = {cc: min(fx, key=lambda m: M[(cc, m)]) for cc in cs}
        if base is None:
            base = orc
        moved = [f"{cc} {base[cc].split('-')[1]}->{orc[cc].split('-')[1]}" for cc in cs if orc[cc] != base[cc]]
        gm = lambda m: math.exp(st.mean(math.log(M[(cc, m)] / M[(cc, orc[cc])]) for cc in cs))
        best = min(fx, key=gm)
        rules = "  ".join(f"{m} {gm(m):.3f}" for m in RULES)
        print(f"  c={c:>2}: {rules}  best common {best} {gm(best):.3f}  oracle moved: {', '.join(moved) or 'none'}")
