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
  total  - all generation work
  core   - total minus PODEM work on faults that end the run undetected (proved
           redundant, or aborted and never detected). Every flow pays that part
           whenever it switches, so it dilutes the differences between rules.
"""
import csv
import glob
import math
import os
import statistics as st
import sys
from collections import defaultdict

import matplotlib

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


def plot_regret(g, circuits, out):
    fig, axes = plt.subplots(1, 2, figsize=(12.5, 4.6), facecolor=SURFACE, sharey=True)
    xmax = 1.8
    for ax, key in zip(axes, ("work_total", "work_core")):
        style(ax)
        ax.grid(False, axis="y")
        _, _, agg = aggregate(g, circuits, key, RULES)
        ms = [m for m in RULES if m in agg]
        y = list(range(len(ms)))[::-1]
        h = 0.34
        for vals, off, col, lab in (([agg[m]["geo"] for m in ms], h / 2 + 0.01, BLUE, "geometric mean over circuits"),
                                    ([agg[m]["worst"] for m in ms], -h / 2 - 0.01, ORANGE, "worst circuit")):
            shown = [min(v, xmax) for v in vals]
            bars = ax.barh([v + off for v in y], shown, height=h, color=col, label=lab)
            for b, v in zip(bars, vals):
                txt = f"{v:.2f}×" if v <= xmax else f"{v:.1f}× →"
                ax.text(min(v, xmax) + 0.015, b.get_y() + b.get_height() / 2, txt,
                        va="center", fontsize=8.5, color=INK)
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
    fig.text(0.01, 0.905, "Switch-dependent work leaves out PODEM work on faults no flow detects "
             "(redundancy proofs and aborts), which every rule pays.", fontsize=9, color=INK2)
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
    fig.text(0.5, 0.925, "Mean over 5 seeds. Near the optimum the curve is flat on every circuit, "
             "so any rule that lands in the basin costs about the same.", ha="center", fontsize=9, color=INK2)
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


def fmt_ratio_table(L, agg, cs, methods):
    L.append("| circuit | " + " | ".join(LABEL[m] for m in methods) + " |")
    L.append("|---|" + "---:|" * len(methods))
    for c in cs:
        L.append(f"| {c} | " + " | ".join(f"{agg[m]['per'][c][0]:.3f}" for m in methods) + " |")
    L.append("| **geomean** | " + " | ".join(f"**{agg[m]['geo']:.3f}**" for m in methods) + " |")
    L.append("| **worst circuit** | " + " | ".join(f"**{agg[m]['worst']:.3f}**" for m in methods) + " |")
    L.append("| worst single seed | " + " | ".join(f"{agg[m]['seed_worst']:.3f}" for m in methods) + " |")


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


def write_table(d, g, circuits, info, out):
    L = ["# Results summary\n"]
    seeds = len(g[(circuits[0], "adaptive")])
    L.append(f"Means over {seeds} seeds (± stdev where non-zero). FC = detected / collapsed faults; "
             "FE = (detected + proved redundant) / collapsed faults. Work = gate evaluations, "
             "deterministic per seed. The oracle is the fixed switch point with the lowest mean work "
             "over a 19-point grid (64 … 32768 patterns), chosen per circuit in hindsight; "
             "\"worst single seed\" compares each seed with the best fixed N for that same seed. "
             "c17 is excluded from ratios (every method finishes in a few patterns).\n")
    for key in ("work_total", "work_core"):
        o, cs, agg = aggregate(g, circuits, key, RULES)
        L.append(f"\n## Cost relative to the hindsight-best fixed switch — {METRIC[key]}\n")
        if key == "work_core":
            L.append("Switch-dependent work = total work minus PODEM work on faults that end the run "
                     "undetected (proved redundant, or aborted and never detected). That part is paid "
                     "by every flow whichever switch point it picks.\n")
        fmt_ratio_table(L, agg, cs, [m for m in RULES if m in agg])
        L.append("\nBest fixed N per circuit: " + ", ".join(f"{c} {o.best_n[c]}" for c in cs) + "\n")

    sens = sensitivity(d, g, circuits)
    if sens:
        L.append("\n## Sensitivity to each rule's own settings\n")
        L.append("Each row re-runs one rule with one setting changed (all circuits × 5 seeds). "
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
            L.append(f"| **{nm}** | {t['geo']:.3f} | {t['worst']:.3f} | {c['geo']:.3f} | {c['worst']:.3f} | {t['seed_worst']:.3f} |")
        for name, res in sens:
            t, c = res["work_total"], res["work_core"]
            L.append(f"| {name} | {t['geo']:.3f} | {t['worst']:.3f} | {c['geo']:.3f} | {c['worst']:.3f} | {t['seed_worst']:.3f} |")

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

    L.append("\n## Per circuit\n")
    L.append("Wall-clock times are single runs of millisecond-scale jobs and vary by tens of percent "
             "between repeats; use the work columns for comparisons.\n")
    for c in circuits:
        i = info[c]
        L.append(f"\n### {c} — {i['inputs']} PI, {i['outputs']} PO, {i['gates']} gates, "
                 f"{i['faults_collapsed']} collapsed faults ({i['faults_uncollapsed']} uncollapsed)\n")
        L.append("| method | detected | redundant | aborted | FC % | FE % | tests (compacted) | switch at | PODEM calls | work (M) | switch-dep. work (M) | time (ms) |")
        L.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for m in TABLE_METHODS:
            rs = g[(c, m)]
            if not rs:
                continue

            def f(k, scale=1.0, p=1):
                mu, s = mean(rs, k) / scale, sd(rs, k) / scale
                return f"{mu:.{p}f}" + (f" ± {s:.{p}f}" if s > 0.5 * 10 ** -p else "")

            sw = "—" if m in ("random", "podem") else f("switch_at", p=0)
            L.append(f"| {LABEL[m]} | {f('detected')} | {f('redundant')} | {f('aborted')} | "
                     f"{f('fc_pct', p=3)} | {f('fe_pct', p=3)} | {f('compacted')} | {sw} | "
                     f"{f('podem_calls', p=0)} | {f('work_total', 1e6, 3)} | {f('work_core', 1e6, 3)} | "
                     f"{f('ms_total', p=1)} |")
    open(out, "w").write("\n".join(L) + "\n")
    return sens


def headline(g, circuits):
    for key in ("work_total", "work_core"):
        _, cs, agg = aggregate(g, circuits, key, RULES + ["random"])
        print(f"== {METRIC[key]}  (geomean / worst circuit / worst seed)")
        for m in RULES + ["random"]:
            if m in agg:
                a = agg[m]
                print(f"  {LABEL[m]:34s} {a['geo']:7.3f} {a['worst']:7.3f} ({a['worst_c']}) {a['seed_worst']:7.3f}")


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
