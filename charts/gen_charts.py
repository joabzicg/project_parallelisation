"""Generate the final benchmark charts used in the report conclusion."""

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

OUT = Path(__file__).parent
plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 11,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "figure.dpi": 160,
})

BLUE = "#2563eb"
ORANGE = "#ea580c"
GREEN = "#16a34a"
TEAL = "#0f766e"
GRAY = "#6b7280"
RED = "#dc2626"
PURPLE = "#7c3aed"


def annotate_bars(ax, bars, fmt="{:.3f}", dy=0.02):
    for bar in bars:
        value = bar.get_height()
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            value + dy,
            fmt.format(value),
            ha="center",
            va="bottom",
            fontsize=9,
            fontweight="bold",
            color="#111827",
        )


# Figure 1: fair comparison across methods at 5k ants.
labels = ["Seq\nAoS", "SoA", "OpenMP\n12T", "MPI\nP=1", "MPI\nP=2", "MPI\nP=4"]
times_ms = np.array([0.904, 0.874, 0.412, 0.782, 1.971, 3.205])
baseline = times_ms[0]
speedups = baseline / times_ms
colors = [GRAY, TEAL, GREEN, BLUE, ORANGE, RED]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))
x = np.arange(len(labels))

bars1 = ax1.bar(x, times_ms, color=colors, width=0.62, zorder=3)
annotate_bars(ax1, bars1, fmt="{:.3f}", dy=0.05)
ax1.set_xticks(x)
ax1.set_xticklabels(labels)
ax1.set_ylabel("Time per iteration (ms)")
ax1.set_title("Fair comparison at 5k ants")
ax1.yaxis.grid(True, linestyle="--", alpha=0.5, zorder=0)
ax1.set_axisbelow(True)

bars2 = ax2.bar(x, speedups, color=colors, width=0.62, zorder=3)
ax2.axhline(1.0, color=GRAY, linestyle="--", linewidth=1.2)
annotate_bars(ax2, bars2, fmt="{:.2f}x", dy=0.04)
ax2.set_xticks(x)
ax2.set_xticklabels(labels)
ax2.set_ylabel("Speedup vs Seq AoS")
ax2.set_title("Relative gain at 5k ants")
ax2.yaxis.grid(True, linestyle="--", alpha=0.5, zorder=0)
ax2.set_axisbelow(True)

fig.tight_layout()
fig.savefig(OUT / "fig1_fair_comparison.png", bbox_inches="tight")
plt.close(fig)


# Figure 2: OpenMP scaling.
threads = np.array([1, 2, 4, 6, 12])
omp_total = np.array([0.942, 0.670, 0.460, 0.417, 0.412])
omp_evap = np.array([0.189, 0.116, 0.099, 0.099, 0.109])
omp_speedup = omp_total[0] / omp_total

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))

ax1.plot(threads, omp_total, "o-", color=GREEN, linewidth=2.5, markersize=7)
ax1.plot(threads, omp_evap, "s--", color=ORANGE, linewidth=2.0, markersize=6)
for t, total in zip(threads, omp_total):
    ax1.annotate(f"{total:.3f}", (t, total), textcoords="offset points", xytext=(0, 7), ha="center", fontsize=9)
ax1.set_xticks(threads)
ax1.set_xlabel("Threads")
ax1.set_ylabel("Time per iteration (ms)")
ax1.set_title("OpenMP execution time")
ax1.legend(["Total compute", "Evaporation"], fontsize=9)
ax1.yaxis.grid(True, linestyle="--", alpha=0.5)

ax2.plot(threads, omp_speedup, "o-", color=BLUE, linewidth=2.5, markersize=7)
ax2.axhline(1.0, color=GRAY, linestyle="--", linewidth=1.2)
for t, sp in zip(threads, omp_speedup):
    ax2.annotate(f"{sp:.2f}x", (t, sp), textcoords="offset points", xytext=(0, 7), ha="center", fontsize=9)
ax2.set_xticks(threads)
ax2.set_xlabel("Threads")
ax2.set_ylabel("Speedup vs 1 thread")
ax2.set_title("OpenMP scaling at 5k ants")
ax2.yaxis.grid(True, linestyle="--", alpha=0.5)

fig.tight_layout()
fig.savefig(OUT / "fig2_openmp_summary.png", bbox_inches="tight")
plt.close(fig)


# Figure 3: MPI behavior across workloads.
processes = np.array([1, 2, 4])
mpi_totals = {
    "5k ants": np.array([0.782, 1.971, 3.205]),
    "50k ants": np.array([5.672, 4.628, 5.688]),
    "100k ants": np.array([10.723, 7.330, 8.020]),
}
mpi_comm_share = {
    "5k ants": np.array([(0.017 + 0.015) / 0.782, (0.682 + 0.614) / 1.971, (1.292 + 1.249) / 3.205]),
    "50k ants": np.array([(0.028 + 0.018) / 5.672, (0.712 + 0.610) / 4.628, (1.577 + 1.227) / 5.688]),
    "100k ants": np.array([(0.030 + 0.017) / 10.723, (0.732 + 0.594) / 7.330, (1.777 + 1.222) / 8.020]),
}
series_colors = {"5k ants": RED, "50k ants": ORANGE, "100k ants": PURPLE}

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11.2, 4.6))
for label, totals in mpi_totals.items():
    ax1.plot(processes, totals, "o-", linewidth=2.4, markersize=7, color=series_colors[label], label=label)
ax1.set_xticks(processes)
ax1.set_xlabel("MPI processes")
ax1.set_ylabel("Time per iteration (ms)")
ax1.set_title("MPI total time by workload")
ax1.legend(fontsize=9)
ax1.yaxis.grid(True, linestyle="--", alpha=0.5)

for label, shares in mpi_comm_share.items():
    ax2.plot(processes, shares * 100.0, "o-", linewidth=2.4, markersize=7, color=series_colors[label], label=label)
ax2.set_xticks(processes)
ax2.set_xlabel("MPI processes")
ax2.set_ylabel("Communication share of total time (%)")
ax2.set_title("Why MPI needs larger workloads")
ax2.legend(fontsize=9)
ax2.yaxis.grid(True, linestyle="--", alpha=0.5)

fig.tight_layout()
fig.savefig(OUT / "fig3_mpi_summary.png", bbox_inches="tight")
plt.close(fig)

print("Generated final report charts in", OUT)
