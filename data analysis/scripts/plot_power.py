"""
PROJECT: Autonomous Smart Maize Planter
MODULE: Data Analysis - Energy Consumption
GOAL: Plot the time-series current draw profile (Figure 18) across concatenated bracket runs.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# 1. Configuration and Paths
FILE_C1 = '../data/BracketC_Run1 (1).csv'
FILE_D3 = '../data/BracketD_Run3.csv'
FILE_D4 = '../data/BracketD_Run4.csv'
OUTPUT_FILE = '../outputs/Figure_18_PowerConsumption.png'
TIME_PER_PULSE_S = 1.5

def main():
    print("--- MECHATRONICS DATA ANALYSIS ---")
    
    # 2. Ingest and Concatenate Data
    df_c1 = pd.read_csv(FILE_C1)
    df_d3 = pd.read_csv(FILE_D3)
    df_d4 = pd.read_csv(FILE_D4)
    
    # Combine them sequentially to simulate a continuous run
    df_all = pd.concat([df_c1, df_d3, df_d4], ignore_index=True)
    
    # Generate an absolute time array (seconds)
    df_all['Time_s'] = np.arange(len(df_all)) * TIME_PER_PULSE_S

    # 3. Initialise the Plot
    plt.figure(figsize=(11, 6))

    # Plot the current draw
    plt.plot(df_all['Time_s'], df_all['Current_A'], color='#d62728', marker='o', linewidth=2, label='Current Draw (A)')

    # 4. Highlight the Peak
    peak_time = df_all.loc[df_all['Current_A'].idxmax(), 'Time_s']
    peak_val = df_all['Current_A'].max()
    plt.annotate(f'Peak Start Load: {peak_val} A', 
                 xy=(peak_time, peak_val), 
                 xytext=(peak_time - 8, peak_val + 0.5),
                 arrowprops=dict(facecolor='black', shrink=0.05, width=1.5, headwidth=6),
                 fontsize=10, fontweight='bold')

    # 5. Segment the Graph by Run (Shaded Regions)
    plt.axvspan(0, 16.5, color='#1f77b4', alpha=0.1, label='Bracket C (Run 1)')
    plt.axvspan(16.5, 33.0, color='#2ca02c', alpha=0.1, label='Bracket D (Run 3)')
    plt.axvspan(33.0, 49.5, color='#ff7f0e', alpha=0.1, label='Bracket D (Run 4)')

    # 6. Styling and Typography
    plt.title('Figure 18: Current Draw Profile During Bracket Calibration', fontsize=14, fontweight='bold', pad=15)
    plt.xlabel('Cumulative Operation Time (Seconds)', fontsize=12)
    plt.ylabel('Current Draw (Amperes)', fontsize=12)
    
    # Give the Y-axis some headroom for the peak annotation
    plt.ylim(0, peak_val + 1.5)

    plt.grid(True, linestyle=':', alpha=0.6)
    
    # Place legend neatly outside the direct line of the data
    plt.legend(loc='lower center', bbox_to_anchor=(0.5, -0.2), ncol=4, frameon=False)
    plt.tight_layout()

    # 7. Export
    os.makedirs('../outputs', exist_ok=True)
    plt.savefig(OUTPUT_FILE, dpi=300)
    print(f"Success! High-resolution graph saved to: {OUTPUT_FILE}")
    plt.show()

if __name__ == "__main__":
    main()