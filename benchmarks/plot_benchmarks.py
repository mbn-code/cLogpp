#!/usr/bin/env python3
"""Plot cLog++ benchmark results.

Reads benchmark_results.csv (produced by benchmark_logger) and writes
benchmark.png. Only depends on matplotlib + the standard library.

Usage:
    ./benchmark_logger                         # writes benchmark_results.csv
    python plot_benchmarks.py [csv] [png]      # writes benchmark.png
"""

import csv
import sys

import matplotlib

matplotlib.use("Agg")  # headless / CI safe
import matplotlib.pyplot as plt  # noqa: E402

CSV_PATH = sys.argv[1] if len(sys.argv) > 1 else "benchmark_results.csv"
PNG_PATH = sys.argv[2] if len(sys.argv) > 2 else "benchmark.png"

labels, values = [], []
with open(CSV_PATH, newline="") as f:
    for row in csv.DictReader(f):
        labels.append(f"{row['sink']}, {row['mode']}")
        values.append(float(row["usec_per_log"]))

# Sort fastest first.
order = sorted(range(len(values)), key=lambda i: values[i])
labels = [labels[i] for i in order]
values = [values[i] for i in order]

fig, ax = plt.subplots(figsize=(8, 0.45 * len(values) + 1.2))
bars = ax.barh(labels, values, color="#3b7dd8", edgecolor="black", linewidth=0.5)
ax.bar_label(bars, fmt="%.3f", padding=3, fontsize=8)
ax.set_xlabel("Time per log entry (microseconds, lower is better)")
ax.set_title("cLog++ benchmark results (end-to-end, lossless)")
ax.set_xlim(0, max(values) * 1.18)
ax.invert_yaxis()
ax.spines["top"].set_visible(False)
ax.spines["right"].set_visible(False)
fig.tight_layout()
fig.savefig(PNG_PATH, dpi=120)
print(f"Graph saved to {PNG_PATH}")
