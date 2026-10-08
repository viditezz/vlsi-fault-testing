#!/usr/bin/env python3
"""Plots and summary tables from a `faultatpg sweep` results directory.

Usage: plot_results.py results/

Writes:
  results/plots/coverage_vs_work.png   coverage trajectory per method (seed 1)
  results/plots/regret.png             cost relative to the hindsight-best fixed switch
  results/plots/switch_points.png      where the adaptive method switched vs best fixed N
  results/summary_table.md             per-circuit means +- stdev over seeds
"""
import csv
import math
import os
import statistics as st
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# Reference palette (validated: adjacent CVD dE >= 9.1, normal-vision >= 22.9).
# Color follows the method in every chart.
COLOR = {
    "adaptive": "#2a78d6",    # slot 1 blue   — the method under study
    "podem": "#eb6834",       # slot 2 orange
    "fixed-1024": "#1baf7a",  # slot 3 aqua
    "random": "#eda100",      # slot 4 yellow
}
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
GRID = "#e6e5e1"
LABEL = {
    "random": "Pure random",
    "podem": "PODEM only",
    "fixed-128": "Fixed switch @128",
    "fixed-1024": "Fixed switch @1024",
    "fixed-8192": "Fixed switch @8192",
    "adaptive": "Adaptive (this work)",
}
METHODS = ["random", "podem", "fixed-128", "fixed-1024", "fixed-8192", "adaptive"]
FIXED = ["fixed-128", "fixed-1024", "fixed-8192"]


def style(ax):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)
        ax.spines[side].set_linewidth(1)
    ax.tick_params(colors=INK2, labelsize=9, length=0)
    ax.grid(True, color=GRID, linewidth=1, linestyle="-")
    ax.set_axisbelow(True)


def load(d):
    rows = list(csv.DictReader(open(os.path.join(d, "summary.csv"))))
    g = defaultdict(list)
    for r in rows:
        g[(r["circuit"], r["method"])].append(r)
    circuits = list(dict.fromkeys(r["circuit"] for r in rows))
    info = {r["circuit"]: r for r in csv.DictReader(open(os.path.join(d, "circuits.csv")))}
    return g, circuits, info


def mean(g, c, m, k):
    return st.mean(float(r[k]) for r in g[(c, m)])


def sd(g, c, m, k):
    v = [float(r[k]) for r in g[(c, m)]]
    return st.stdev(v) if len(v) > 1 else 0.0


def read_curve(path):
    xs, ys, phase = [], [], []
    with open(path) as f:
        for r in csv.DictReader(f):
            if float(r["work"]) <= 0:
                continue  # the (0, 0) origin cannot sit on a log axis
            xs.append(float(r["work"]))
            ys.append(float(r["coverage_pct"]))
            phase.append(r["phase"])
    return xs, ys, phase


def pattern_cols(path):
    with open(path) as f:
        return [int(r["pattern"]) for r in csv.DictReader(f) if float(r["work"]) > 0]


def plot_curves(d, circuits, out, g):
    switch_at = {}
    for c in circuits:
        for r in g[(c, "adaptive")]:
            if r["seed"] == "1":
                switch_at[c] = int(r["switch_at"])
    show = [c for c in ["c432", "c1908", "c2670", "c3540", "c5315", "c7552"] if c in circuits]
    if not show:
        return
    cols = 3
    rows = math.ceil(len(show) / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(13, 3.6 * rows), facecolor=SURFACE)
    axes = axes.flatten() if hasattr(axes, "flatten") else [axes]
    order = ["random", "podem", "fixed-1024", "adaptive"]  # adaptive drawn last (on top)
    for ax, c in zip(axes, show):
        style(ax)
        lo = 100.0
        xmin = float("inf")
        for m in order:
            p = os.path.join(d, "curves", f"{c}_{m}.csv")
            if not os.path.exists(p):
                continue
            xs, ys, ph = read_curve(p)
            ax.step(xs, ys, where="post", color=COLOR[m], linewidth=2,
                    solid_joinstyle="round", solid_capstyle="round", label=LABEL[m])
            lo = min(lo, ys[-1])
            xmin = min(xmin, xs[0])
            if m == "adaptive" and switch_at.get(c) is not None:
                # switch point: last curve point at or before the recorded switch
                pats = pattern_cols(p)
                k = max((i for i, n in enumerate(pats) if n <= switch_at[c]), default=None)
                if k is not None:
                    ax.plot(xs[k], ys[k], "o", ms=8, color=COLOR[m],
                            markeredgecolor=SURFACE, markeredgewidth=2, zorder=5)
        ax.set_xscale("log")
        ax.set_xlim(left=xmin * 0.7)
        ax.set_ylim(max(0, lo - 6), 100.5)
        ax.set_title(c, loc="left", fontsize=11, color=INK, fontweight="bold")
        ax.set_xlabel("generation work (gate evaluations)", fontsize=9, color=INK2)
        ax.set_ylabel("fault coverage (%)", fontsize=9, color=INK2)
    for ax in axes[len(show):]:
        ax.set_visible(False)
    h, l = axes[0].get_legend_handles_labels()
    fig.legend(h, l, loc="upper center", ncol=4, frameon=False, fontsize=10,
               labelcolor=INK, bbox_to_anchor=(0.5, 1.0))
    fig.text(0.5, 0.925, "Coverage vs. cumulative work, seed 1 — dot marks where the adaptive method switched to PODEM",
             ha="center", fontsize=9, color=INK2)
    fig.tight_layout(rect=(0, 0, 1, 0.91))
    fig.savefig(out, dpi=170, facecolor=SURFACE)
    plt.close(fig)


def regret(g, circuits, key):
    cand = ["podem", "fixed-128", "fixed-1024", "fixed-8192", "adaptive"]
    logs = defaultdict(list)
    per = {}
    for c in circuits:
        if c == "c17":
            continue
        best = min(mean(g, c, m, key) for m in FIXED)
        per[c] = {m: mean(g, c, m, key) / best for m in cand}
        for m in cand:
            logs[m].append(math.log(per[c][m]))
    agg = {m: (math.exp(st.mean(logs[m])), math.exp(max(logs[m]))) for m in cand}
    return cand, per, agg


def plot_regret(g, circuits, out):
    cand, _, agg = regret(g, circuits, "work_total")
    fig, ax = plt.subplots(figsize=(9, 4.2), facecolor=SURFACE)
    style(ax)
    ax.grid(True, axis="x", color=GRID)
    ax.grid(False, axis="y")
    y = list(range(len(cand)))[::-1]
    h = 0.34
    geo = [agg[m][0] for m in cand]
    worst = [agg[m][1] for m in cand]
    b1 = ax.barh([v + h / 2 + 0.01 for v in y], geo, height=h, color="#2a78d6", label="geometric mean over circuits")
    b2 = ax.barh([v - h / 2 - 0.01 for v in y], worst, height=h, color="#eb6834", label="worst circuit")
    for bars in (b1, b2):
        for b in bars:
            ax.text(b.get_width() + 0.03, b.get_y() + b.get_height() / 2, f"{b.get_width():.2f}×",
                    va="center", fontsize=9, color=INK)
    ax.axvline(1.0, color=INK2, linewidth=1)
    ax.set_yticks(y)
    ax.set_yticklabels([LABEL[m] for m in cand], fontsize=10, color=INK)
    ax.set_xlim(0, max(worst) * 1.15)
    ax.set_xlabel("generation work ÷ best fixed switch point chosen in hindsight per circuit", fontsize=9, color=INK2)
    ax.set_title("Cost relative to the hindsight-best fixed switch (1.00× = matched it)", loc="left",
                 fontsize=11, color=INK, fontweight="bold")
    ax.legend(frameon=False, fontsize=9, loc="lower right", labelcolor=INK)
    fig.tight_layout()
    fig.savefig(out, dpi=170, facecolor=SURFACE)
    plt.close(fig)


def plot_switch(g, circuits, out):
    cs = [c for c in circuits if c != "c17"]
    fig, ax = plt.subplots(figsize=(9, 4.4), facecolor=SURFACE)
    style(ax)
    for i, c in enumerate(cs):
        pts = [max(float(r["switch_at"]), 64) for r in g[(c, "adaptive")]]
        ax.plot(pts, [i] * len(pts), "o", ms=8, color="#2a78d6", markeredgecolor=SURFACE,
                markeredgewidth=2, label="Adaptive switch point (one dot per seed)" if i == 0 else None, zorder=4)
        best = min(FIXED, key=lambda m: mean(g, c, m, "work_total"))
        n = int(best.split("-")[1])
        ax.plot([n], [i], "D", ms=9, color="#eb6834", markeredgecolor=SURFACE, markeredgewidth=2,
                label="Best fixed N in hindsight" if i == 0 else None, zorder=3)
    ax.set_xscale("log")
    ax.set_yticks(range(len(cs)))
    ax.set_yticklabels(cs, fontsize=10, color=INK)
    ax.invert_yaxis()
    ax.set_xlabel("random patterns applied before switching to PODEM", fontsize=9, color=INK2)
    ax.set_title("Where the switch happened: chosen live per circuit, no tuning", loc="left",
                 fontsize=11, color=INK, fontweight="bold", pad=28)
    ax.legend(frameon=False, fontsize=9, loc="lower left", bbox_to_anchor=(0, 1.0), ncol=2,
              labelcolor=INK, borderaxespad=0.2)
    fig.tight_layout()
    fig.savefig(out, dpi=170, facecolor=SURFACE)
    plt.close(fig)


def write_table(g, circuits, info, out):
    L = []
    L.append("# Results summary\n")
    seeds = len(g[(circuits[0], "adaptive")])
    L.append(f"Means over {seeds} seeds (± stdev where it is non-zero). FC = detected / collapsed faults; "
             "FE = (detected + proven redundant) / collapsed faults. Work = gate evaluations "
             "(deterministic per seed); time = wall clock on the build machine.\n")
    for key, title in (("work_total", "work"), ("ms_total", "wall-clock time")):
        cand, per, agg = regret(g, circuits, key)
        L.append(f"\n## Cost relative to the hindsight-best fixed switch ({title})\n")
        L.append("| circuit | " + " | ".join(LABEL[m] for m in cand) + " |")
        L.append("|---|" + "---:|" * len(cand))
        for c, v in per.items():
            L.append(f"| {c} | " + " | ".join(f"{v[m]:.3f}" for m in cand) + " |")
        L.append("| **geomean** | " + " | ".join(f"**{agg[m][0]:.3f}**" for m in cand) + " |")
        L.append("| **worst** | " + " | ".join(f"**{agg[m][1]:.3f}**" for m in cand) + " |")
    L.append("\n## Per circuit\n")
    for c in circuits:
        i = info[c]
        L.append(f"\n### {c} — {i['inputs']} PI, {i['outputs']} PO, {i['gates']} gates, "
                 f"{i['faults_collapsed']} collapsed faults ({i['faults_uncollapsed']} uncollapsed)\n")
        L.append("| method | FC % | FE % | aborted | tests (compacted) | random patterns | PODEM calls | work (M) | time (ms) |")
        L.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|")
        for m in METHODS:
            if not g[(c, m)]:
                continue

            def f(k, scale=1.0, p=1):
                mu, s = mean(g, c, m, k) / scale, sd(g, c, m, k) / scale
                return f"{mu:.{p}f}" + (f" ± {s:.{p}f}" if s > 0.5 * 10 ** -p else "")

            L.append(f"| {LABEL[m]} | {f('fc_pct', p=3)} | {f('fe_pct', p=3)} | {f('aborted')} | "
                     f"{f('compacted')} | {f('random_applied', p=0)} | {f('podem_calls', p=0)} | "
                     f"{f('work_total', 1e6, 3)} | {f('ms_total', p=1)} |")
    open(out, "w").write("\n".join(L) + "\n")


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "results"
    g, circuits, info = load(d)
    os.makedirs(os.path.join(d, "plots"), exist_ok=True)
    plot_curves(d, circuits, os.path.join(d, "plots", "coverage_vs_work.png"), g)
    plot_regret(g, circuits, os.path.join(d, "plots", "regret.png"))
    plot_switch(g, circuits, os.path.join(d, "plots", "switch_points.png"))
    write_table(g, circuits, info, os.path.join(d, "summary_table.md"))
    print("wrote plots/ and summary_table.md in", d)


if __name__ == "__main__":
    main()
