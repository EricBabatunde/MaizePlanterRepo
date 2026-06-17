"""
PROJECT: Autonomous Smart Maize Planter
MODULE: Data Analysis - Command Latency
GOAL: Generate a box plot illustrating the distribution of command-to-actuator latencies.
"""

import matplotlib.pyplot as plt
import pandas as pd
import os

# 1. Configuration
DATA_FILE = '../data/CommandLatency_Run1.csv'
OUTPUT_FILE = '../outputs/Figure_12_CommandLatency.png'

def main():
    print("--- MECHATRONICS DATA ANALYSIS ---")
    
    # 2. Ingest Data
    try:
        df = pd.read_csv(DATA_FILE)
    except FileNotFoundError:
        print(f"CRITICAL ERROR: Could not find {DATA_FILE}.")
        return

    # Extract the two datasets
    close_range = df['Interval_ms']
    field_range = df['Interval_ms_15m']

    # 3. Initialise the Plot
    plt.figure(figsize=(9, 6))
    
    # Generate the box plot with styling
    box = plt.boxplot([close_range, field_range], 
                      labels=['Close Range (<2m)', 'Field Range (15m)'],
                      patch_artist=True, 
                      widths=0.5,
                      boxprops=dict(facecolor='#1f77b4', color='black', alpha=0.7),
                      capprops=dict(color='black', linewidth=1.5),
                      whiskerprops=dict(color='black', linewidth=1.5),
                      flierprops=dict(marker='o', markerfacecolor='red', markersize=6, alpha=0.6),
                      medianprops=dict(color='orange', linewidth=2.5))

    # Alternate color for the second box to visually differentiate conditions
    box['boxes'][1].set_facecolor('#2ca02c')

    # 4. Add the Critical Engineering Threshold
    plt.axhline(y=500, color='red', linestyle='--', linewidth=2, label='Real-time Target Threshold (<500 ms)')

    # 5. Styling and Typography
    plt.title('Figure 12: Command-to-Actuator Latency Distribution', fontsize=14, fontweight='bold', pad=15)
    plt.ylabel('Latency (ms)', fontsize=12)
    
    # Dynamic Y-axis to give breathing room for the threshold line
    plt.ylim(0, max(550, df.max().max() + 50))
    
    plt.grid(axis='y', linestyle=':', alpha=0.7)
    plt.legend(loc='upper left')
    
    plt.tight_layout()

    # 6. Export
    os.makedirs('../outputs', exist_ok=True)
    plt.savefig(OUTPUT_FILE, dpi=300)
    print(f"Success! High-resolution graph saved to: {OUTPUT_FILE}")
    plt.show()

if __name__ == "__main__":
    main()