import os

import matplotlib.pyplot as plt

#        'pi_hw_intel_rdrand',
#        'pi_hw_kiss11',
#        'pi_sw_kiss11',
# List of directories to process
directories = [
    "pi_hw_cong",
    "pi_hw_drand48",
    "pi_hw_glibc_crand",
    "pi_hw_jkiss",
    "pi_hw_jkiss32",
    "pi_hw_mt19937",
    "pi_hw_pcgbasic",
    "pi_hw_taus113",
    "pi_hw_taus88",
    "pi_hw_xorshift128",
    "pi_hw_xorshift32",
    "pi_hw_xorwow",
    "pi_hw_xoshiro128pp",
    "pi_sw_cong",
    "pi_sw_drand48",
    "pi_sw_glibc_crand",
    "pi_sw_jkiss",
    "pi_sw_jkiss32",
    "pi_sw_mt19937",
    "pi_sw_pcgbasic",
    "pi_sw_taus113",
    "pi_sw_taus88",
    "pi_sw_xorshift128",
    "pi_sw_xorshift32",
    "pi_sw_xorwow",
    "pi_sw_xoshiro128pp",
]


def extract_sim_ticks(dir_path):
    """Extracts the numeric value of simTicks from stats.txt."""
    file_path = os.path.join(dir_path, "stats.txt")
    if not os.path.exists(file_path):
        return None
    try:
        with open(file_path) as f:
            for line in f:
                if "simTicks" in line:
                    # Split by whitespace and take the second element
                    parts = line.split()
                    if len(parts) >= 2:
                        return int(parts[1])
    except (ValueError, IndexError, OSError):
        return None
    return None


def main():
    # Dictionary to store tick values grouped by strategy
    # Structure: { 'strategy_name': {'hw': ticks, 'sw': ticks} }
    results = {}

    for d in directories:
        ticks = extract_sim_ticks(d)
        if ticks is None:
            continue

        # Determine mode (hw/sw) and strategy name from directory string
        if d.startswith("pi_hw_"):
            mode, strategy = "hw", d[6:]
        elif d.startswith("pi_sw_"):
            mode, strategy = "sw", d[6:]
        else:
            continue

        if strategy not in results:
            results[strategy] = {}
        results[strategy][mode] = ticks

    # Calculate Relative Speedup (SW / HW)
    # We only plot strategies that have both a hardware and software data point
    plot_labels = []
    speedup_values = []

    # Sort strategies alphabetically for a clean x-axis
    for strat in sorted(results.keys()):
        data = results[strat]
        if "sw" in data and "hw" in data:
            speedup = data["sw"] / data["hw"]
            plot_labels.append(strat)
            speedup_values.append(speedup)

    if not speedup_values:
        print("Error: No matching HW/SW pairs found to calculate speedup.")
        return

    # Plotting logic
    plt.figure(figsize=(20, 7))
    bars = plt.bar(
        plot_labels,
        speedup_values,
        color="steelblue",
        edgecolor="black",
        alpha=0.85,
        zorder=3,
    )
    plt.ylim(0.8, 1.4)

    # Labeling and Formatting
    plt.ylabel("Relative Speedup ($Ticks_{SW} / Ticks_{HW}$)", fontsize=16)
    plt.xlabel("PRNG Strategy", fontsize=16)
    plt.title(
        "Hardware Acceleration Speedup per PRNG Strategy",
        fontsize=18,
        fontweight="bold",
    )
    plt.xticks(rotation=45, ha="right", fontsize=16)
    plt.grid(axis="y", linestyle="--", alpha=0.6, zorder=0)

    # Add data labels on top of each bar
    for bar in bars:
        height = bar.get_height()
        plt.text(
            bar.get_x() + bar.get_width() / 2.0,
            height + 0.02,
            f"{height:.2f}x",
            ha="center",
            va="bottom",
            fontsize=10,
        )

    plt.tight_layout()
    plt.savefig("prng_speedup_plot.pdf", bbox_inches="tight")
    print("Success: Plot saved as 'prng_speedup_plot.pdf'")


if __name__ == "__main__":
    main()
