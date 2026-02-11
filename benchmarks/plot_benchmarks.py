import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Read the CSV file
df = pd.read_csv("benchmark_results.csv")

# Remove noop for cleaner graph
plot_df = df[df["logger"] != "noop"]

# Compose readable labels
plot_df["Label"] = (
    plot_df["logger"] + " [" + plot_df["sink"] + ", " + plot_df["mode"] + "]"
)

# Sort by performance (ascending)
plot_df = plot_df.sort_values("usec_per_log")

# Set style
sns.set_theme(style="whitegrid")

plt.figure(figsize=(8, 4))
bar = sns.barplot(
    data=plot_df,
    y="Label",
    x="usec_per_log",
    palette="Blues_d",
    edgecolor="k",
)
plt.xlabel("Time per log entry (μs)")
plt.ylabel("")
plt.title("cLog++ Benchmark Results (lower is better)")
plt.tight_layout()

for container in bar.containers:
    bar.bar_label(container, fmt="%.2f")

plt.savefig("benchmark.png")
print("Graph saved to benchmark.png")
