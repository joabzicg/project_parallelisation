"""Generate benchmark charts for the project report."""

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
from pathlib import Path

OUT = Path(__file__).parent
plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 11,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "figure.dpi": 150,
})

BLUE   = "#2563eb"
ORANGE = "#ea580c"
GREEN  = "#16a34a"
PURPLE = "#7c3aed"
GRAY   = "#6b7280"
RED    = "#dc2626"

# ── Figure 1: Normalized phase breakdown ─────────────────────────────────────
# Use "non-evaporation compute" to avoid over-claiming that total-evap is pure
# ant movement. Values are normalized to ms per 1k ants per iteration.
labels  = ["Non-evaporation compute", "Evaporation"]
configs = ["Seq AoS\n5k ants", "SoA\n5k ants", "OpenMP 4T\n5k ants", "MPI P=2\n100k ants"]

seq_total, seq_evap = 0.623, 0.117
soa_total, soa_evap = 0.677, 0.066
omp_total_4t, omp_evap_4t = 0.651, 0.079
mpi2_total, mpi2_evap = 7.086, 0.065

data = np.array([
    [(seq_total - seq_evap) / 5.0, seq_evap / 5.0],
    [(soa_total - soa_evap) / 5.0, soa_evap / 5.0],
    [(omp_total_4t - omp_evap_4t) / 5.0, omp_evap_4t / 5.0],
    [(mpi2_total - mpi2_evap) / 100.0, mpi2_evap / 100.0],
])
colors  = [BLUE, ORANGE]

fig, ax = plt.subplots(figsize=(8, 4.5))
x = np.arange(len(configs))
width = 0.55
bottoms = np.zeros(len(configs))
for i, (lbl, col) in enumerate(zip(labels, colors)):
    bars = ax.bar(x, data[:, i], width, bottom=bottoms, color=col, label=lbl, zorder=3)
    for bar, val in zip(bars, data[:, i]):
        if val > 0.02:
            ax.text(bar.get_x() + bar.get_width() / 2,
                    bar.get_y() + val / 2,
                    f"{val:.3f}", ha="center", va="center",
                    fontsize=8.5, color="white", fontweight="bold")
    bottoms += data[:, i]

for xi, total in zip(x, bottoms):
    ax.text(xi, total + 0.003, f"{total:.3f}", ha="center", va="bottom",
            fontsize=9, fontweight="bold", color="#111827")

ax.set_xticks(x)
ax.set_xticklabels(configs)
ax.set_ylabel("ms per 1k ants per iteration")
ax.set_title("Normalized phase breakdown by strategy")
ax.legend(loc="upper left", fontsize=9)
ax.set_ylim(0, max(bottoms) * 1.25)
ax.yaxis.grid(True, linestyle="--", alpha=0.5, zorder=0)
ax.set_axisbelow(True)
ax.text(0.01, 0.98,
    "All bars normalized by ant count\n(5k for Seq/SoA/OpenMP, 100k for MPI).",
    transform=ax.transAxes, ha="left", va="top", fontsize=8.5,
    bbox=dict(facecolor="white", edgecolor="#d1d5db", alpha=0.95))
fig.tight_layout()
fig.savefig(OUT / "fig1_phase_breakdown.png", bbox_inches="tight")
plt.close()
print("fig1 done")

# ── Figure 2: OpenMP scaling ──────────────────────────────────────────────────
threads      = [1, 2, 4, 6, 12]
omp_total    = [0.609, 0.673, 0.651, 0.667, 0.635]
omp_evap     = [0.113, 0.103, 0.079, 0.084, 0.083]
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4))

# left: total compute speedup
sp_total = [omp_total[0] / v for v in omp_total]
sp_evap  = [omp_evap[0]  / v for v in omp_evap]
ax1.plot(threads, sp_total, "o-", color=BLUE,   label="Total compute", linewidth=2, markersize=7)
ax1.plot(threads, sp_evap,  "s-", color=ORANGE, label="Evaporation only", linewidth=2, markersize=7)
ax1.plot(threads, [1]*len(threads), "--", color=GRAY, linewidth=1, label="No gain (1×)")
ax1.set_xticks(threads)
ax1.set_xlabel("OMP_NUM_THREADS")
ax1.set_ylabel("Speedup vs 1 thread")
ax1.set_title("OpenMP speedup (5k ants)")
ax1.legend(fontsize=9)
ax1.yaxis.grid(True, linestyle="--", alpha=0.5)
ax1.set_ylim(0.7, 1.6)
for t, sv, sc in zip(threads, sp_evap, sp_total):
    ax1.annotate(f"{sv:.2f}×", (t, sv), textcoords="offset points",
                 xytext=(5, 4), fontsize=8, color=ORANGE)

# right: time breakdown stacked bar
w = 0.4
omp_non_evap = [t - e for t, e in zip(omp_total, omp_evap)]
ax2.bar(threads, omp_non_evap,  w, color=BLUE,   label="Non-evaporation compute", zorder=3)
ax2.bar(threads, omp_evap, w, bottom=omp_non_evap, color=ORANGE, label="Evaporation", zorder=3)
ax2.set_xticks(threads)
ax2.set_xlabel("OMP_NUM_THREADS")
ax2.set_ylabel("Time per iteration (ms)")
ax2.set_title("OpenMP time split (5k ants)")
ax2.legend(fontsize=9)
ax2.yaxis.grid(True, linestyle="--", alpha=0.5)
ax2.set_axisbelow(True)
ax2.text(0.02, 0.98,
         "Blue = total compute - evaporation\n(ant movement + update + residual overhead)",
         transform=ax2.transAxes, ha="left", va="top", fontsize=8.2,
         bbox=dict(facecolor="white", edgecolor="#d1d5db", alpha=0.95))

fig.tight_layout()
fig.savefig(OUT / "fig2_openmp_scaling.png", bbox_inches="tight")
plt.close()
print("fig2 done")

# ── Figure 3: MPI stacked bar + speedup ───────────────────────────────────────
P         = [1, 2, 4, 6]
ant_mv    = [9.967, 5.571, 5.243, 4.530]
mpi_merge = [0.028, 0.809, 1.999, 3.249]
evap      = [0.126, 0.065, 0.048, 0.042]
mpi_sync  = [0.014, 0.639, 1.314, 2.690]
phen_upd  = [0.001, 0.002, 0.003, 0.005]
totals    = [10.136, 7.086, 8.607, 10.514]
speedup   = [1.00, 1.43, 1.18, 0.96]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))

stacks = [ant_mv, mpi_merge, evap, mpi_sync, phen_upd]
labels2 = ["Ant movement", "MPI merge (MAX)", "Evaporation", "MPI sync (MIN)", "Pheromone update"]
colors2 = [BLUE, RED, ORANGE, PURPLE, GREEN]
w = 0.55
x = np.arange(len(P))
bot = np.zeros(len(P))
for vals, lbl, col in zip(stacks, labels2, colors2):
    bars = ax1.bar(x, vals, w, bottom=bot, color=col, label=lbl, zorder=3)
    for bar, val in zip(bars, vals):
        if val > 0.15:
            ax1.text(bar.get_x() + bar.get_width() / 2,
                     bar.get_y() + val / 2,
                     f"{val:.2f}", ha="center", va="center",
                     fontsize=8.5, color="white", fontweight="bold")
    bot += np.array(vals)
for xi, tot in zip(x, totals):
    ax1.text(xi, tot + 0.1, f"{tot:.2f} ms", ha="center", va="bottom",
             fontsize=9, fontweight="bold", color="#111827")

ax1.set_xticks(x)
ax1.set_xticklabels([f"P={p}" for p in P])
ax1.set_ylabel("Time per iteration (ms)")
ax1.set_title("MPI phase breakdown (100k ants, 120 iters)")
ax1.legend(loc="upper right", fontsize=8.5)
ax1.yaxis.grid(True, linestyle="--", alpha=0.5, zorder=0)
ax1.set_axisbelow(True)
ax1.set_ylim(0, max(totals) * 1.17)

# Speedup line chart
ax2.plot(P, speedup, "D-", color=BLUE, linewidth=2.5, markersize=9, zorder=3, label="Measured speedup")
ax2.axhline(1.0, color=GRAY, linestyle="--", linewidth=1.2, label="No gain (1×)")
ideal = [P[0] / p for p in P]
ax2.fill_between(P, speedup, 1.0,
                 where=[s >= 1 for s in speedup],
                 alpha=0.15, color=GREEN, label="Net gain")
ax2.fill_between(P, speedup, 1.0,
                 where=[s < 1 for s in speedup],
                 alpha=0.15, color=RED, label="Net loss")
for p, s in zip(P, speedup):
    ax2.annotate(f"{s:.2f}×", (p, s), textcoords="offset points",
                 xytext=(6, 4), fontsize=10, fontweight="bold",
                 color=GREEN if s >= 1 else RED)
ax2.set_xticks(P)
ax2.set_xticklabels([f"P={p}" for p in P])
ax2.set_xlabel("Number of MPI ranks")
ax2.set_ylabel("Speedup vs P=1")
ax2.set_title("MPI speedup (100k ants, 120 iters)")
ax2.legend(fontsize=9)
ax2.yaxis.grid(True, linestyle="--", alpha=0.5)
ax2.set_ylim(0.7, 1.65)

fig.tight_layout()
fig.savefig(OUT / "fig3_mpi_scaling.png", bbox_inches="tight")
plt.close()
print("fig3 done")

# ── Figure 4: Cross-strategy normalized throughput ────────────────────────────
configs4  = ["Seq AoS", "SoA", "OpenMP\n4T", "MPI\nP=1", "MPI\nP=2", "MPI\nP=4", "MPI\nP=6"]
norm_rate = [0.1246, 0.1354, 0.1302, 0.1014, 0.0709, 0.0861, 0.1051]
base      = norm_rate[0]
speedups4 = [base / r for r in norm_rate]
col4      = [GRAY, GRAY, ORANGE, BLUE, GREEN, BLUE, ORANGE]

fig, ax = plt.subplots(figsize=(9, 4.5))
bars = ax.bar(range(len(configs4)), speedups4, color=col4, zorder=3, width=0.6, edgecolor="white")
ax.axhline(1.0, color=GRAY, linestyle="--", linewidth=1.2, zorder=2)
for bar, sp, rate in zip(bars, speedups4, norm_rate):
    ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.03,
            f"{sp:.2f}×\n({rate:.3f})", ha="center", va="bottom",
            fontsize=9, fontweight="bold")
ax.set_xticks(range(len(configs4)))
ax.set_xticklabels(configs4)
ax.set_ylabel("Throughput speedup vs Seq AoS")
ax.set_title("Normalized throughput: ms per 1 000 ants per iteration")
ax.yaxis.grid(True, linestyle="--", alpha=0.5, zorder=0)
ax.set_axisbelow(True)
ax.set_ylim(0, max(speedups4) * 1.2)

legend_items = [
    mpatches.Patch(color=GRAY,   label="SoA / Seq reference"),
    mpatches.Patch(color=ORANGE, label="OpenMP / MPI P=6"),
    mpatches.Patch(color=BLUE,   label="MPI P=1 / P=4"),
    mpatches.Patch(color=GREEN,  label="MPI P=2 (best)"),
]
ax.legend(handles=legend_items, fontsize=8.5, loc="upper left")
fig.tight_layout()
fig.savefig(OUT / "fig4_cross_strategy.png", bbox_inches="tight")
plt.close()
print("fig4 done")

print("All charts saved to", OUT)
