#!/usr/bin/env python3
"""Plots and summary tables from a `faultatpg sweep` results directory.

Usage: plot_results.py results/

Reads results/summary.csv (and results/sensitivity/*/summary.csv if present).
Writes:
  results/plots/regret.png             cost relative to the hindsight-best fixed switch
  results/plots/cost_vs_switch.png     switch-dependent work vs fixed N, with each rule's switch
  results/plots/switch_points.png      where each rule switched vs the best fixed N
  results/plots/coverage_vs_work.png   coverage trajectory per method (seed 1)
  results/summary_table.md             all tables: regret, sensitivity, per circuit

The oracle for each circuit is the fixed switch point N (from the grid in the
sweep, 64..32768) with the lowest mean work over seeds, chosen in hindsight.
Two work measures are reported:
  total  - all generation work (gate evaluations; the SAT fallback is separate)
  core   - total minus PODEM work on faults that end the run PROVED REDUNDANT
           (by PODEM, or by the SAT fallback after PODEM aborted). That set is a
           property of the circuit, identical for every flow, and every flow pays
           for it wherever it switches, so it dilutes the differences between rules.

Confidence intervals are percentile bootstraps over seeds (seeds are resampled
with replacement per circuit, the same draw for every method, and the oracle is
recomputed inside each resample). They describe seed-to-seed noise on this
benchmark set only; they say nothing about other circuits.
"""
import csv
import glob
import math
import os
import statistics as st
import sys
from collections import defaultdict

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# Reference palette (validated: adjacent CVD dE >= 9.1, normal-vision >= 22.9).
BLUE, ORANGE, AQUA, YELLOW = "#2a78d6", "#eb6834", "#1baf7a", "#eda100"
SURFACE, INK, INK2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e6e5e1"
COLOR = {"adaptive": BLUE, "podem": ORANGE, "fixed-1024": AQUA, "random": YELLOW,
         "stage1": AQUA, "plateau": YELLOW}
LABEL = {
    "random": "Pure random",
    "podem": "PODEM only",
    "fixed-128": "Fixed switch @128",
    "fixed-1024": "Fixed switch @1024",
    "fixed-8192": "Fixed switch @8192",
    "plateau": "Plateau rule (6 blocks, ≤1)",
    "stage1": "Cost-bound test only (stage 1)",
    "adaptive": "Adaptive, two-stage (proposed)",
}
RULES = ["podem", "fixed-128", "fixed-1024", "fixed-8192", "plateau", "stage1", "adaptive"]
TABLE_METHODS = ["random", "podem", "fixed-128", "fixed-1024", "fixed-8192", "plateau", "stage1", "adaptive"]
METRIC = {"work_total": "total work", "work_core": "switch-dependent work"}


def style(ax):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)
    ax.tick_params(colors=INK2, labelsize=9, length=0)
    ax.grid(True, color=GRID, linewidth=1)
    ax.set_axisbelow(True)


def read_rows(path):
    g = defaultdict(list)
    for r in csv.DictReader(open(path)):
        g[(r["circuit"], r["method"])].append(r)
    return g


def load(d):
    g = read_rows(os.path.join(d, "summary.csv"))
    circuits = list(dict.fromkeys(c for c, _ in g))
    info = {r["circuit"]: r for r in csv.DictReader(open(os.path.join(d, "circuits.csv")))}
    return g, circuits, info


def mean(rows, k):
    return st.mean(float(r[k]) for r in rows)


def sd(rows, k):
    v = [float(r[k]) for r in rows]
    return st.stdev(v) if len(v) > 1 else 0.0


def fixed_methods(g, c):
    ms = [m for (cc, m) in g if cc == c and m.startswith("fixed-")]
    return sorted(ms, key=lambda m: int(m.split("-")[1]))


class Oracle:
    """Hindsight-best fixed N per circuit (mean over seeds) and per seed."""

    def __init__(self, g, circuits, key):
        self.key = key
        self.best, self.best_n, self.seed_best = {}, {}, {}
        for c in circuits:
            fm = fixed_methods(g, c)
            if not fm:
                continue
            m = min(fm, key=lambda m: mean(g[(c, m)], key))
            self.best[c] = mean(g[(c, m)], key)
            self.best_n[c] = int(m.split("-")[1])
            sb = defaultdict(lambda: float("inf"))
            for mm in fm:
                for r in g[(c, mm)]:
                    sb[r["seed"]] = min(sb[r["seed"]], float(r[key]))
            self.seed_best[c] = dict(sb)

    def ratios(self, rows, c):
        """(mean ratio, worst per-seed ratio) for one method on one circuit."""
        mr = mean(rows, self.key) / self.best[c]
        ps = max(float(r[self.key]) / self.seed_best[c][r["seed"]] for r in rows)
        return mr, ps


def aggregate(g, circuits, key, methods, extra=None):
    """For each method: per-circuit ratios, geomean, worst circuit, worst seed."""
    o = Oracle(g, circuits, key)
    cs = [c for c in circuits if c != "c17" and c in o.best]
    out = {}
    for m in methods:
        src = extra[m] if extra and m in extra else g
        per = {c: o.ratios(src[(c, m.split(":")[0])], c) for c in cs if src.get((c, m.split(":")[0]))}
        if len(per) < len(cs):
            continue
        logs = [math.log(v[0]) for v in per.values()]
        out[m] = {"per": per, "geo": math.exp(st.mean(logs)),
                  "worst": max(v[0] for v in per.values()),
                  "worst_c": max(per, key=lambda c: per[c][0]),
                  "seed_worst": max(v[1] for v in per.values())}
    return o, cs, out


def seed_vec(g, c, m, key):
    rows = sorted(g[(c, m)], key=lambda r: int(r["seed"]))
    return np.array([float(r[key]) for r in rows])


def bootstrap(g, circuits, key, methods, B=4000, seed=12345):
    """Per-circuit log(cost ratio to the oracle) for B seed-resamples."""
    rng = np.random.default_rng(seed)
    cs = [c for c in circuits if c != "c17" and fixed_methods(g, c)]
    logs = {m: np.zeros((B, len(cs))) for m in methods}
    for ci, c in enumerate(cs):
        F = np.stack([seed_vec(g, c, m, key) for m in fixed_methods(g, c)])
        n = F.shape[1]
        idx = rng.integers(0, n, size=(B, n))
        orc = F[:, idx].mean(axis=2).min(axis=0)
        for m in methods:
            v = seed_vec(g, c, m, key)
            logs[m][:, ci] = np.log(v[idx].mean(axis=1) / orc)
    return cs, logs


def ci(x, lo=2.5, hi=97.5):
    return float(np.percentile(x, lo)), float(np.percentile(x, hi))


def plot_regret(g, circuits, out):
    fig, axes = plt.subplots(1, 2, figsize=(12.5, 4.6), facecolor=SURFACE, sharey=True)
    xmax = 1.8
    for ax, key in zip(axes, ("work_total", "work_core")):
        style(ax)
        ax.grid(False, axis="y")
        _, _, agg = aggregate(g, circuits, key, RULES)
        ms = [m for m in RULES if m in agg]
        _, blogs = bootstrap(g, circuits, key, ms)
        y = list(range(len(ms)))[::-1]
        h = 0.34
        for vals, off, col, lab in (([agg[m]["geo"] for m in ms], h / 2 + 0.01, BLUE, "geometric mean over circuits"),
                                    ([agg[m]["worst"] for m in ms], -h / 2 - 0.01, ORANGE, "worst circuit")):
            shown = [min(v, xmax) for v in vals]
            bars = ax.barh([v + off for v in y], shown, height=h, color=col, label=lab)
            for b, v, m in zip(bars, vals, ms):
                txt = f"{v:.2f}×" if v <= xmax else f"{v:.1f}× →"
                xt = min(v, xmax) + 0.015
                if col == BLUE and v <= xmax:  # 95% bootstrap CI on the geometric mean
                    lo_, hi_ = ci(np.exp(blogs[m].mean(axis=1)))
                    yc = b.get_y() + b.get_height() / 2
                    ax.plot([lo_, hi_], [yc, yc], color=INK, linewidth=1.1, zorder=5)
                    ax.plot([lo_, lo_], [yc - 0.05, yc + 0.05], color=INK, linewidth=1.1, zorder=5)
                    ax.plot([hi_, hi_], [yc - 0.05, yc + 0.05], color=INK, linewidth=1.1, zorder=5)
                    xt = max(xt, hi_ + 0.015)
                ax.text(xt, b.get_y() + b.get_height() / 2, txt, va="center", fontsize=8.5, color=INK)
        ax.axvline(1.0, color=INK2, linewidth=1)
        ax.set_xlim(0.9, xmax + 0.25)
        ax.set_yticks(y)
        ax.set_yticklabels([LABEL[m] for m in ms], fontsize=9.5, color=INK)
        ax.set_title(METRIC[key].capitalize(), loc="left", fontsize=11, color=INK, fontweight="bold")
        ax.set_xlabel("work ÷ best fixed switch point (19-point grid, chosen in hindsight)",
                      fontsize=8.5, color=INK2)
    axes[0].legend(frameon=False, fontsize=9, loc="lower right", labelcolor=INK)
    fig.suptitle("Cost relative to the hindsight-best fixed switch point (1.00× = matched it)",
                 x=0.01, ha="left", fontsize=12, color=INK, fontweight="bold")
    fig.text(0.01, 0.905, "Whiskers: 95% bootstrap interval over seeds (geometric mean). Switch-dependent work leaves out "
             "PODEM work on faults proved redundant, which every rule pays.", fontsize=9, color=INK2)
    fig.tight_layout(rect=(0, 0, 1, 0.9))
    fig.savefig(out, dpi=170, facecolor=SURFACE)
    plt.close(fig)


def plot_cost_vs_switch(g, circuits, out):
    show = [c for c in ["c432", "c880", "c1908", "c2670", "c3540", "c5315", "c6288", "c7552"] if c in circuits]
    cols = 4
    rows = math.ceil(len(show) / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(14, 3.3 * rows), facecolor=SURFACE)
    axes = list(axes.flatten())
    marks = [("plateau", YELLOW, "s"), ("stage1", AQUA, "^"), ("adaptive", BLUE, "o")]
    for ax, c in zip(axes, show):
        style(ax)
        fm = fixed_methods(g, c)
        ns = [int(m.split("-")[1]) for m in fm]
        ys = [mean(g[(c, m)], "work_core") / 1e3 for m in fm]
        ax.plot(ns, ys, color=INK2, linewidth=1.6, marker=".", ms=5, label="Fixed switch at N")
        for m, col, mk in marks:
            rs = g[(c, m)]
            ax.plot(mean(rs, "switch_at"), mean(rs, "work_core") / 1e3, mk, ms=9, color=col,
                    markeredgecolor=SURFACE, markeredgewidth=1.5, label=LABEL[m], zorder=5)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(c, loc="left", fontsize=11, color=INK, fontweight="bold")
        ax.set_xlabel("random patterns before switching", fontsize=8.5, color=INK2)
        ax.set_ylabel("switch-dependent work (k)", fontsize=8.5, color=INK2)
    for ax in axes[len(show):]:
        ax.set_visible(False)
    h, lab = axes[0].get_legend_handles_labels()
    fig.legend(h, lab, loc="upper center", ncol=4, frameon=False, fontsize=9.5, labelcolor=INK,
               bbox_to_anchor=(0.5, 1.0))
    nseeds = len(g[(show[0], "adaptive")])
    fig.text(0.5, 0.925, f"Mean over {nseeds} seeds, log-log axes. The minimum is shallow on most circuits, but a "
             "switch 2-4x too early or too late still costs tens of percent on several (see summary_table.md).",
             ha="center", fontsize=9, color=INK2)
    fig.tight_layout(rect=(0, 0, 1, 0.91))
    fig.savefig(out, dpi=170, facecolor=SURFACE)
    plt.close(fig)


def plot_switch(g, circuits, out):
    cs = [c for c in circuits if c != "c17"]
    o = Oracle(g, circuits, "work_core")
    fig, ax = plt.subplots(figsize=(9.5, 4.8), facecolor=SURFACE)
    style(ax)
    series = [("adaptive", BLUE, "o", -0.2), ("stage1", AQUA, "^", 0.0), ("plateau", YELLOW, "s", 0.2)]
    for i, c in enumerate(cs):
        for m, col, mk, dy in series:
            pts = [max(float(r["switch_at"]), 64) for r in g[(c, m)]]
            ax.plot(pts, [i + dy] * len(pts), mk, ms=6.5, color=col, markeredgecolor=SURFACE,
                    markeredgewidth=1.2, label=LABEL[m] if i == 0 else None, zorder=4)
        ax.plot([o.best_n[c]], [i], "D", ms=9, color=ORANGE, markeredgecolor=SURFACE, markeredgewidth=1.5,
                label="Best fixed N in hindsight (switch-dependent work)" if i == 0 else None, zorder=3)
    ax.set_xscale("log")
    ax.set_yticks(range(len(cs)))
    ax.set_yticklabels(cs, fontsize=10, color=INK)
    ax.invert_yaxis()
    ax.set_xlabel("random patterns applied before switching to PODEM", fontsize=9, color=INK2)
    ax.set_title("Where each rule switched (one mark per seed)", loc="left", fontsize=11, color=INK, fontweight="bold", pad=44)
    ax.legend(frameon=False, fontsize=8.5, loc="lower left", bbox_to_anchor=(0, 1.0), ncol=2,
              labelcolor=INK, borderaxespad=0.2)
    fig.tight_layout()
    fig.savefig(out, dpi=170, facecolor=SURFACE)
    plt.close(fig)


def read_curve(path):
    xs, ys = [], []
    with open(path) as f:
        for r in csv.DictReader(f):
            if float(r["work"]) <= 0:
                continue  # the (0, 0) origin cannot sit on a log axis
            xs.append(float(r["work"]))
            ys.append(float(r["coverage_pct"]))
    return xs, ys


def plot_curves(d, circuits, out, g):
    show = [c for c in ["c432", "c1908", "c2670", "c3540", "c5315", "c7552"] if c in circuits]
    cols = 3
    rows = math.ceil(len(show) / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(13, 3.6 * rows), facecolor=SURFACE)
    axes = list(axes.flatten())
    order = ["random", "podem", "fixed-1024", "adaptive"]
    for ax, c in zip(axes, show):
        style(ax)
        lo, xmin = 100.0, float("inf")
        for m in order:
            p = os.path.join(d, "curves", f"{c}_{m}.csv")
            if not os.path.exists(p):
                continue
            xs, ys = read_curve(p)
            ax.step(xs, ys, where="post", color=COLOR[m], linewidth=2, label=LABEL[m])
            lo, xmin = min(lo, ys[-1]), min(xmin, xs[0])
        ax.set_xscale("log")
        ax.set_xlim(left=xmin * 0.7)
        ax.set_ylim(max(0, lo - 6), 100.5)
        ax.set_title(c, loc="left", fontsize=11, color=INK, fontweight="bold")
        ax.set_xlabel("generation work (gate evaluations)", fontsize=9, color=INK2)
        ax.set_ylabel("fault coverage (%)", fontsize=9, color=INK2)
    for ax in axes[len(show):]:
        ax.set_visible(False)
    h, lab = axes[0].get_legend_handles_labels()
    fig.legend(h, lab, loc="upper center", ncol=4, frameon=False, fontsize=10, labelcolor=INK,
               bbox_to_anchor=(0.5, 1.0))
    fig.text(0.5, 0.925, "Coverage vs. cumulative work, seed 1. The long flat tails are PODEM "
             "proving faults redundant or aborting them.", ha="center", fontsize=9, color=INK2)
    fig.tight_layout(rect=(0, 0, 1, 0.91))
    fig.savefig(out, dpi=170, facecolor=SURFACE)
    plt.close(fig)


def sensitivity(d, g, circuits):
    """Variants in results/sensitivity/<name>/summary.csv, scored against the main oracle."""
    rows = []
    for p in sorted(glob.glob(os.path.join(d, "sensitivity", "*", "summary.csv"))):
        name = os.path.basename(os.path.dirname(p))
        vg = read_rows(p)
        method = name.split("_")[0]
        merged = dict(g)
        for k, v in vg.items():
            merged[k] = v
        res = {}
        for key in ("work_total", "work_core"):
            _, _, agg = aggregate(merged, circuits, key, [method])
            res[key] = agg.get(method)
        if res["work_total"]:
            rows.append((name, res))
    return rows


def heldout(L, d, g, circuits):
    """Pick the best setting of each rule on half the circuits, score it on the other half."""
    cs = [c for c in circuits if c != "c17" and fixed_methods(g, c)]
    cs = sorted(cs, key=lambda c: int(c[1:]))
    halves = (cs[0::2], cs[1::2])
    variants = defaultdict(list)  # rule -> [(name, merged groups)]
    for fam in ("plateau", "stage1", "adaptive"):
        variants[fam].append((fam + " (default)", g))
    for p in sorted(glob.glob(os.path.join(d, "sensitivity", "*", "summary.csv"))):
        name = os.path.basename(os.path.dirname(p))
        merged = dict(g)
        merged.update(read_rows(p))
        variants[name.split("_")[0]].append((name, merged))
    if not any(len(v) > 1 for v in variants.values()):
        return
    L.append("\n## Held-out check of the tuned settings\n")
    L.append("The defaults (window μ = 6, plateau 6/1, probe 16, ...) were chosen while looking at these same "
             "circuits. To see whether tuning would transfer, each rule's variants from the sensitivity study "
             "are ranked on one half of the circuits (by total-work geomean) and the winner is scored on the "
             "other half, and compared with the default on that same half. Halves: alternate circuits in size order.\n")
    L.append("| rule | tuned on | winner | winner on the other half | default on the other half |")
    L.append("|---|---|---|---:|---:|")
    for fam, vs in variants.items():
        if len(vs) < 2:
            continue
        for tr, te in ((halves[0], halves[1]), (halves[1], halves[0])):
            score = {}
            for name, mg in vs:
                agg = aggregate(mg, tr, "work_total", [fam])[2].get(fam)
                if agg:
                    score[name] = agg["geo"]
            win = min(score, key=score.get)
            wg = dict(vs)[win]
            on_win = aggregate(wg, te, "work_total", [fam])[2][fam]["geo"]
            on_def = aggregate(g, te, "work_total", [fam])[2][fam]["geo"]
            L.append(f"| {fam} | {', '.join(tr)} | {win} | {on_win:.3f} | {on_def:.3f} |")


def fmt_ratio_table(L, agg, cs, methods, boot=None):
    L.append("| circuit | " + " | ".join(LABEL[m] for m in methods) + " |")
    L.append("|---|" + "---:|" * len(methods))
    for c in cs:
        L.append(f"| {c} | " + " | ".join(f"{agg[m]['per'][c][0]:.3f}" for m in methods) + " |")
    L.append("| geomean | " + " | ".join(f"{agg[m]['geo']:.3f}" for m in methods) + " |")
    if boot is not None:
        cells = []
        for m in methods:
            lo, hi = ci(np.exp(boot[m].mean(axis=1)))
            cells.append(f"{lo:.3f}–{hi:.3f}")
        L.append("| geomean, 95% bootstrap interval | " + " | ".join(cells) + " |")
    L.append("| worst circuit | " + " | ".join(f"{agg[m]['worst']:.3f}" for m in methods) + " |")
    L.append("| worst single seed | " + " | ".join(f"{agg[m]['seed_worst']:.3f}" for m in methods) + " |")


def pairwise(L, boot, pairs):
    L.append("| comparison (A ÷ B) | ratio of geomeans | 95% interval | verdict |")
    L.append("|---|---:|---:|---|")
    for a, b in pairs:
        d = np.exp(boot[a].mean(axis=1) - boot[b].mean(axis=1))
        lo, hi = ci(d)
        est = float(np.median(d))
        verdict = "A cheaper" if hi < 1 else "B cheaper" if lo > 1 else "indistinguishable"
        L.append(f"| {LABEL[a]} ÷ {LABEL[b]} | {est:.3f} | {lo:.3f}–{hi:.3f} | {verdict} |")


def flatness(L, g, circuits, key):
    """Cost of switching 2 grid steps (about 2x) before or after the best fixed N."""
    L.append("| circuit | best N | N/2 | 2N | N/4 | 4N |")
    L.append("|---|---:|---:|---:|---:|---:|")
    for c in circuits:
        fm = fixed_methods(g, c)
        if c == "c17" or not fm:
            continue
        ns = [int(m.split("-")[1]) for m in fm]
        cost = [mean(g[(c, m)], key) for m in fm]
        i = min(range(len(ns)), key=lambda k: cost[k])

        def at(j):
            return f"{cost[j] / cost[i]:.2f}×" if 0 <= j < len(ns) else "—"
        L.append(f"| {c} | {ns[i]} | {at(i - 2)} | {at(i + 2)} | {at(i - 4)} | {at(i + 4)} |")


def test_sizes(L, g, circuits, methods):
    cs = [c for c in circuits if c != "c17" and g.get((c, "podem"))]
    L.append("| circuit | " + " | ".join(LABEL[m] for m in methods) + " |")
    L.append("|---|" + "---:|" * len(methods))
    logs = {m: [] for m in methods}
    for c in cs:
        ref = mean(g[(c, "podem")], "compacted")
        cells = []
        for m in methods:
            r = mean(g[(c, m)], "compacted") / ref
            logs[m].append(math.log(r))
            cells.append(f"{r:.3f}")
        L.append(f"| {c} | " + " | ".join(cells) + " |")
    L.append("| geomean | " + " | ".join(f"{math.exp(st.mean(logs[m])):.3f}" for m in methods) + " |")


def weight_rows(d, circuits):
    """results/weight/<w>/summary.csv: full sweeps with PODEM work scaled by w."""
    out = []
    for p in sorted(glob.glob(os.path.join(d, "weight", "*", "summary.csv")),
                    key=lambda p: float(os.path.basename(os.path.dirname(p)))):
        w = os.path.basename(os.path.dirname(p))
        wg = read_rows(p)
        res = {}
        for key in ("work_total", "work_core"):
            _, _, res[key] = aggregate(wg, circuits, key, ["podem", "fixed-1024", "plateau", "stage1", "adaptive"])
        out.append((w, res))
    return out


def coverage_rows(g, circuits):
    """Per circuit: faults, redundant, detected (identical for all resolving flows), random stats."""
    rows = []
    for c in circuits:
        det, red, unres = set(), set(), set()
        for (cc, m), rs in g.items():
            if cc != c or m == "random":
                continue
            for r in rs:
                det.add(int(r["detected"]))
                red.add(int(r["redundant"]))
                unres.add(int(r["aborted"]))
        faults = int(g[(c, "podem")][0]["faults"])
        rnd = [int(r["detected"]) for r in g[(c, "random")]]
        rows.append({"c": c, "faults": faults, "det": det, "red": red, "unres": unres,
                     "rand": (min(rnd), max(rnd), sum(rnd) / len(rnd))})
    return rows


def bt_rows(d):
    out = []
    for p in sorted(glob.glob(os.path.join(d, "bt", "*", "summary.csv")),
                    key=lambda p: int(os.path.basename(os.path.dirname(p)))):
        out.append((os.path.basename(os.path.dirname(p)), read_rows(p)))
    return out


def write_table(d, g, circuits, info, out):
    L = ["# Results summary\n"]
    seeds = len(g[(circuits[0], "adaptive")])
    L.append(f"Means over {seeds} seeds (± stdev where non-zero). FC = detected / collapsed faults; "
             "FE = (detected + proved redundant) / collapsed faults. Work = gate evaluations, "
             "deterministic per seed. The oracle is the fixed switch point with the lowest mean work "
             "over a 19-point grid (64 … 32768 patterns), chosen per circuit in hindsight; "
             "\"worst single seed\" compares each seed with the best fixed N for that same seed. "
             "c17 is excluded from ratios (every method finishes in a few patterns). "
             "Intervals are 95% percentile bootstraps over seeds, conditional on this set of circuits.\n")
    L.append("\n## Coverage\n")
    L.append("Every flow that ends with the SAT fallback (all but pure random) resolves every fault, so "
             "its detected and redundant counts must be identical across methods and seeds. "
             "This table is generated from the sweep and fails if they are not.\n")
    L.append("| circuit | collapsed faults | detected (all flows) | proved redundant | unresolved | pure random: detected (mean, min–max) |")
    L.append("|---|---:|---:|---:|---:|---:|")
    for r in coverage_rows(g, circuits):
        ok = len(r["det"]) == 1 and len(r["red"]) == 1 and r["unres"] == {0}
        if not ok:
            raise SystemExit(f"coverage differs between flows on {r['c']}: {r}")
        det, red = next(iter(r["det"])), next(iter(r["red"]))
        if det + red != r["faults"]:
            raise SystemExit(f"{r['c']}: detected + redundant != faults")
        mn, mx, avg = r["rand"]
        L.append(f"| {r['c']} | {r['faults']} | {det} | {red} | 0 | {avg:.1f} ({mn}–{mx}) |")
    boots = {}
    for key in ("work_total", "work_core"):
        o, cs, agg = aggregate(g, circuits, key, RULES)
        ms = [m for m in RULES if m in agg]
        _, boots[key] = bootstrap(g, circuits, key, ms)
        L.append(f"\n## Cost relative to the hindsight-best fixed switch — {METRIC[key]}\n")
        if key == "work_core":
            L.append("Switch-dependent work = total work minus PODEM work on faults that end the run "
                     "proved redundant (by PODEM, or by the SAT fallback after PODEM aborted). That set is "
                     "identical for every flow, and every flow pays for it wherever it switches.\n")
        fmt_ratio_table(L, agg, cs, ms, boots[key])
        L.append("\nBest fixed N per circuit: " + ", ".join(f"{c} {o.best_n[c]}" for c in cs) + "\n")

    pairs = [("plateau", "stage1"), ("stage1", "adaptive"), ("plateau", "adaptive"),
             ("plateau", "fixed-1024"), ("adaptive", "fixed-1024")]
    for key in ("work_total", "work_core"):
        L.append(f"\n## Pairwise differences between rules — {METRIC[key]}\n")
        L.append("Ratio < 1 means A needs less work. \"Indistinguishable\" means the 95% interval contains 1.\n")
        pairwise(L, boots[key], pairs)

    L.append("\n## How costly is a switch that is off by 2x or 4x? (fixed N, switch-dependent work)\n")
    L.append("Cost relative to the best fixed N on the grid, for a fixed switch point 2 and 4 grid steps "
             "(about 2x and 4x) earlier or later.\n")
    flatness(L, g, circuits, "work_core")

    L.append("\n## Test-set size (compacted tests, relative to PODEM only)\n")
    L.append("Reverse-order fault-simulation compaction applied to every method's test set. "
             "The random-only set is larger and reaches lower coverage on c2670 and c7552.\n")
    test_sizes(L, g, circuits, ["random", "fixed-128", "fixed-1024", "fixed-8192", "plateau", "stage1", "adaptive"])

    sens = sensitivity(d, g, circuits)
    if sens:
        L.append("\n## Sensitivity to each rule's own settings\n")
        L.append(f"Each row re-runs one rule with one setting changed (all circuits × {seeds} seeds). "
                 "Defaults: adaptive α=0.05, confirm=2, probe=16, window μ=6, warm-up 128; "
                 "plateau 6 blocks, ≤1 fault.\n")
        L.append("| variant | total geomean | total worst | switch-dep. geomean | switch-dep. worst | worst seed (total) |")
        L.append("|---|---:|---:|---:|---:|---:|")
        base = {}
        for key in ("work_total", "work_core"):
            _, _, agg = aggregate(g, circuits, key, ["plateau", "stage1", "adaptive"])
            base[key] = agg
        for m, nm in (("adaptive", "adaptive (default)"), ("stage1", "stage1 (default)"), ("plateau", "plateau 6/1 (default)")):
            t, c = base["work_total"][m], base["work_core"][m]
            L.append(f"| {nm} | {t['geo']:.3f} | {t['worst']:.3f} | {c['geo']:.3f} | {c['worst']:.3f} | {t['seed_worst']:.3f} |")
        for name, res in sens:
            t, c = res["work_total"], res["work_core"]
            L.append(f"| {name} | {t['geo']:.3f} | {t['worst']:.3f} | {c['geo']:.3f} | {c['worst']:.3f} | {t['seed_worst']:.3f} |")

    heldout(L, d, g, circuits)

    wr = weight_rows(d, circuits)
    if wr:
        L.append("\n## Robustness to the cost of PODEM\n")
        L.append("Full sweeps with PODEM work multiplied by w (a slower or faster ATPG engine relative "
                 "to fault simulation). Each weight has its own fixed-N oracle. Entries: geomean / worst circuit.\n")
        ms = ["podem", "fixed-1024", "plateau", "stage1", "adaptive"]
        L.append("| w | metric | " + " | ".join(LABEL[m] for m in ms) + " |")
        L.append("|---|---|" + "---:|" * len(ms))
        for w, res in [("1", None)] + wr:
            for key in ("work_total", "work_core"):
                agg = aggregate(g, circuits, key, ms)[2] if res is None else res[key]
                L.append(f"| {w} | {METRIC[key]} | " + " | ".join(
                    f"{agg[m]['geo']:.3f} / {agg[m]['worst']:.2f}" if m in agg else "—" for m in ms) + " |")

    br = bt_rows(d)
    if br:
        L.append("\n## A smaller PODEM budget, now that SAT resolves every abort\n")
        L.append("Full sweeps (5 seeds) with the PODEM backtrack limit lowered; 1024 is the default (10 seeds). "
                 "Left block: how the rules rank against their own fixed-N oracle (total work, geomean / worst circuit). "
                 "Right block: totals over all circuits for the adaptive method. SAT propagation steps are a "
                 "different unit from gate evaluations and are not included in total work.\n")
        ms = ["podem", "fixed-1024", "plateau", "stage1", "adaptive"]
        L.append("| PODEM backtrack limit | " + " | ".join(LABEL[m] for m in ms) +
                 " | adaptive: PODEM aborts → SAT | adaptive: total work (M) | adaptive: SAT propagations (M) |")
        L.append("|---|" + "---:|" * (len(ms) + 3))
        cs0 = [c for c in circuits if c != "c17"]
        for lim, bg in br + [("1024", g)]:
            agg = aggregate(bg, circuits, "work_total", ms)[2]
            cells = [f"{agg[m]['geo']:.3f} / {agg[m]['worst']:.2f}" if m in agg else "—" for m in ms]
            tw = sum(mean(bg[(c, "adaptive")], "work_total") for c in cs0 if bg.get((c, "adaptive"))) / 1e6
            sw = sum(mean(bg[(c, "adaptive")], "work_sat") for c in cs0 if bg.get((c, "adaptive"))) / 1e6
            sc = sum(mean(bg[(c, "adaptive")], "sat_calls") for c in cs0 if bg.get((c, "adaptive")))
            L.append(f"| {lim} | " + " | ".join(cells) + f" | {sc:.0f} | {tw:.1f} | {sw:.2f} |")

    L.append("\n## SAT fallback\n")
    L.append("Faults PODEM aborted on (1024 backtracks) are handed to the SAT solver once the PODEM phase is done. "
             "Means over seeds, adaptive method. Propagation steps are SAT-solver units, not gate evaluations.\n")
    L.append("| circuit | PODEM aborts → SAT | SAT tests | proved redundant | redundant faults (all) | SAT propagations (M) |")
    L.append("|---|---:|---:|---:|---:|---:|")
    for c in circuits:
        rs = g[(c, "adaptive")]
        L.append(f"| {c} | {mean(rs, 'sat_calls'):.1f} | {mean(rs, 'sat_vectors'):.1f} | "
                 f"{mean(rs, 'sat_redundant'):.1f} | {mean(rs, 'redundant'):.1f} | {mean(rs, 'work_sat') / 1e6:.2f} |")

    L.append("\n## Per circuit\n")
    L.append("Wall-clock times are single runs of millisecond-scale jobs and vary by tens of percent "
             "between repeats; use the work columns for comparisons.\n")
    for c in circuits:
        i = info[c]
        L.append(f"\n### {c} — {i['inputs']} PI, {i['outputs']} PO, {i['gates']} gates, "
                 f"{i['faults_collapsed']} collapsed faults ({i['faults_uncollapsed']} uncollapsed)\n")
        L.append("| method | detected | redundant | unresolved | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |")
        L.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for m in TABLE_METHODS:
            rs = g[(c, m)]
            if not rs:
                continue

            def f(k, scale=1.0, p=1):
                mu, s = mean(rs, k) / scale, sd(rs, k) / scale
                return f"{mu:.{p}f}" + (f" ± {s:.{p}f}" if s > 0.5 * 10 ** -p else "")

            sw = "—" if m in ("random", "podem") else f("switch_at", p=0)
            unres = f("aborted") if m != "random" else "n/a"
            red = f("redundant") if m != "random" else "n/a"
            L.append(f"| {LABEL[m]} | {f('detected')} | {red} | {unres} | "
                     f"{f('fc_pct', p=3)} | {f('fe_pct', p=3)} | {f('compacted')} | {sw} | "
                     f"{f('podem_calls', p=0)} | {f('work_total', 1e6, 3)} | {f('work_core', 1e6, 3)} | "
                     f"{f('ms_total', p=1)} |")
    open(out, "w").write("\n".join(L) + "\n")
    return sens


def headline(g, circuits):
    for key in ("work_total", "work_core"):
        _, cs, agg = aggregate(g, circuits, key, RULES + ["random"])
        ms = [m for m in RULES if m in agg]
        _, logs = bootstrap(g, circuits, key, ms)
        print(f"== {METRIC[key]}  (geomean [95% CI] / worst circuit / worst seed)")
        for m in RULES + ["random"]:
            if m in agg:
                a = agg[m]
                lo, hi = ci(np.exp(logs[m].mean(axis=1))) if m in logs else (float("nan"),) * 2
                cis = "[%.3f, %.3f]" % (lo, hi) if lo == lo else "[  —  ,  —  ]"
                print(f"  {LABEL[m]:34s} {a['geo']:7.3f} {cis} {a['worst']:7.3f} ({a['worst_c']}) {a['seed_worst']:7.3f}")


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "results"
    g, circuits, info = load(d)
    os.makedirs(os.path.join(d, "plots"), exist_ok=True)
    plot_regret(g, circuits, os.path.join(d, "plots", "regret.png"))
    plot_cost_vs_switch(g, circuits, os.path.join(d, "plots", "cost_vs_switch.png"))
    plot_switch(g, circuits, os.path.join(d, "plots", "switch_points.png"))
    plot_curves(d, circuits, os.path.join(d, "plots", "coverage_vs_work.png"), g)
    write_table(d, g, circuits, info, os.path.join(d, "summary_table.md"))
    headline(g, circuits)
    print("wrote plots/ and summary_table.md in", d)


if __name__ == "__main__":
    main()
