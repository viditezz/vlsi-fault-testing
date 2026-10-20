#!/usr/bin/env python3
"""Cost breakdown, oracle definitions and circuit-level uncertainty for a sweep.

Usage: cost_breakdown.py <results_dir> [<results_dir> ...]   (each with summary.csv)

For every benchmark set given, prints:
  1. SAT cost per flow: SAT calls, SAT propagation steps, SAT time share.
  2. A combined cost in gate-evaluation equivalents,
        combined = work_total + c * work_sat,
     where c is the measured time of one SAT propagation step divided by the
     measured time of one gate evaluation (summed over every run given).
  3. Regret against an oracle chosen separately for each cost measure
     (total work, switch-dependent work, combined cost, end-to-end wall-clock
     time), and the cross terms: the cost of a rule on measure A divided by the
     cost, on A, of the fixed point that is optimal for measure B.
  4. A two-level bootstrap (circuits, then seeds within each circuit) of the
     geometric-mean ratio, which covers circuit-to-circuit variation as well as
     seed noise.
Wall-clock time here is ms_total + ms_compact (random + PODEM + SAT + compaction).
"""
import csv
import math
import statistics as st
import sys
from collections import defaultdict

import numpy as np

RULES = ["podem", "fixed-1024", "fixed-8192", "plateau", "stage1", "adaptive"]
NAME = {"podem": "PODEM only", "fixed-1024": "Fixed @1024", "fixed-8192": "Fixed @8192",
        "plateau": "Plateau", "stage1": "Stage 1", "adaptive": "Two-stage", "random": "Pure random"}


def load(d):
    g = defaultdict(list)
    for r in csv.DictReader(open(d + "/summary.csv")):
        if r["circuit"] == "c17":
            continue
        r = dict(r)
        r["ms_sat"] = float(r["ms_total"]) - float(r["ms_random"]) - float(r["ms_podem"])
        r["ms_e2e"] = float(r["ms_total"]) + float(r["ms_compact"])
        g[(r["circuit"], r["method"])].append(r)
    return g


def fixed(g, c):
    return sorted([m for (cc, m) in g if cc == c and m.startswith("fixed-")], key=lambda m: int(m.split("-")[1]))


def mean(rs, k):
    return st.mean(float(r[k]) for r in rs)


def main():
    dirs = sys.argv[1:]
    sets = [(d, load(d)) for d in dirs]
    # c = (time per SAT propagation step) / (time per gate evaluation), measured on all runs:
    # SAT time over SAT steps, and random + PODEM time over gate-evaluation work.
    tg = wg = ts = ws = 0.0
    for _, g in sets:
        for rs in g.values():
            for r in rs:
                tg += float(r["ms_random"]) + float(r["ms_podem"]); wg += float(r["work_total"])
                ts += r["ms_sat"]; ws += float(r["work_sat"])
    c = (ts / ws) / (tg / wg)
    print(f"measured: {1e6*tg/wg:.1f} ns per gate evaluation, {1e6*ts/ws:.1f} ns per SAT propagation step "
          f"-> c = {c:.2f} gate-evaluation equivalents per SAT step\n")
    for _, g in sets:
        for rs in g.values():
            for r in rs:
                r["work_comb"] = float(r["work_total"]) + c * float(r["work_sat"])

    for d, g in sets:
        cs = sorted({cc for cc, _ in g if fixed(g, cc)})
        print(f"=== {d}  ({len(cs)} circuits)")
        # 1. SAT cost
        print(f"{'flow':14s} {'SAT calls':>10s} {'SAT prop (M)':>13s} {'SAT % of time':>14s} {'SAT % of comb.':>15s}")
        for m in ["random"] + RULES:
            if not all((cc, m) in g for cc in cs):
                continue
            calls = sum(mean(g[(cc, m)], "sat_calls") for cc in cs)
            prop = sum(mean(g[(cc, m)], "work_sat") for cc in cs)
            tsat = sum(mean(g[(cc, m)], "ms_sat") for cc in cs)
            ttot = sum(mean(g[(cc, m)], "ms_total") for cc in cs)
            csat = sum(c * mean(g[(cc, m)], "work_sat") for cc in cs)
            ctot = sum(mean(g[(cc, m)], "work_comb") for cc in cs)
            print(f"{NAME[m]:14s} {calls:10.1f} {prop/1e6:13.2f} {100*tsat/ttot:13.1f}% {100*csat/ctot:14.1f}%")
        # 2/3. oracles
        keys = [("work_total", "total"), ("work_core", "switch-dep."), ("work_comb", "combined"), ("ms_e2e", "wall-clock")]
        best = {k: {cc: min(fixed(g, cc), key=lambda m: mean(g[(cc, m)], k)) for cc in cs} for k, _ in keys}
        print("\nbest fixed N per circuit by measure:")
        for k, lab in keys:
            print(f"  {lab:12s} " + ", ".join(f"{cc} {best[k][cc].split('-')[1]}" for cc in cs))
        print("\nregret (geomean over circuits) = rule cost on A / cost on A of the fixed point optimal for B")
        hdr = "".join(f"{lab[:11]:>12s}" for _, lab in keys)
        for k, lab in keys:
            print(f"  A = {lab}; B across: {hdr}")
            for m in RULES:
                vals = []
                for kb, _ in keys:
                    lr = [math.log(mean(g[(cc, m)], k) / mean(g[(cc, best[kb][cc])], k)) for cc in cs]
                    vals.append(math.exp(st.mean(lr)))
                print(f"    {NAME[m]:12s}" + "".join(f"{v:12.3f}" for v in vals))
        # 4. two-level bootstrap on switch-dependent work and on combined cost
        rng = np.random.default_rng(7)
        B = 4000
        print("\ntwo-level bootstrap (circuits and seeds), geomean ratio to own-measure oracle, 95%:")
        for k, lab in [("work_core", "switch-dep."), ("work_comb", "combined")]:
            F = {cc: np.array([[float(r[k]) for r in sorted(g[(cc, f)], key=lambda r: int(r["seed"]))] for f in fixed(g, cc)]) for cc in cs}
            M = {(cc, m): np.array([float(r[k]) for r in sorted(g[(cc, m)], key=lambda r: int(r["seed"]))]) for cc in cs for m in RULES}
            out = {m: [] for m in RULES}
            diff = []
            diff2 = []
            for _ in range(B):
                pick = rng.integers(0, len(cs), len(cs))
                logs = {m: [] for m in RULES}
                for i in pick:
                    cc = cs[i]
                    n = F[cc].shape[1]
                    idx = rng.integers(0, n, n)
                    orc = F[cc][:, idx].mean(axis=1).min()
                    for m in RULES:
                        logs[m].append(math.log(M[(cc, m)][idx].mean() / orc))
                for m in RULES:
                    out[m].append(math.exp(np.mean(logs[m])))
                diff.append(math.exp(np.mean(logs["plateau"]) - np.mean(logs["adaptive"])))
                diff2.append(math.exp(np.mean(logs["plateau"]) - np.mean(logs["fixed-1024"])))
            for m in ["plateau", "stage1", "adaptive", "fixed-1024"]:
                lo, hi = np.percentile(out[m], [2.5, 97.5])
                print(f"  {lab:12s} {NAME[m]:12s} [{lo:.3f}, {hi:.3f}]")
            lo, hi = np.percentile(diff, [2.5, 97.5])
            print(f"  {lab:12s} plateau / two-stage [{lo:.3f}, {hi:.3f}]")
            lo, hi = np.percentile(diff2, [2.5, 97.5])
            print(f"  {lab:12s} plateau / fixed @1024 [{lo:.3f}, {hi:.3f}]")
        print()


if __name__ == "__main__":
    main()
