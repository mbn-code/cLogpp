#!/usr/bin/env python3
"""Plot cLog++ benchmark results.

Reads benchmark_results.csv (produced by benchmark_logger) and writes
benchmark.png. Only depends on matplotlib + the standard library.

Usage:
    ./benchmark_logger            # writes benchmark_results.csv
    python plot_benchmarks.py     # writes benchmark.png
"""
import csv
import sys

import matplotlib
matplotlib.use("Agg")  # headless / CI safe
import matplotlib.pyplot as plt

CSV_PATH = sys.argv[1] if len(sys.argv) > 1 else "benchmark_results.csv"

labels, values = [], []
with open(CSV_PATH, newline="") as f:
    for row in csv.DictReader(f):
        if row["logger"] == "noop":  # baseline, not a logging mode
            continue
        labels.append(f'{row["logger"]} [{row["sink"]}, {row["mode"]}]')
        values.append(float(row["usec_per_log"]))

# Sort fastest first.
order = sorted(range(len(values)), key=lambda i: values[i])
labels = [labels[i] for i in order]
values = [values[i] for i in order]

plt.figure(figsize=(8, 4))
bars = plt.barh(labels, values, color="#3b7dd8", edgecolor="black")
plt.bar_label(bars, fmt="%.3f")
plt.xlabel("Time per log entry (microseconds, lower is better)")
plt.title("cLog++ benchmark results")
plt.gca().invert_yaxis()
plt.tight_layout()
plt.savefig("benchmark.png", dpi=120)
print("Graph saved to benchmark.png")
