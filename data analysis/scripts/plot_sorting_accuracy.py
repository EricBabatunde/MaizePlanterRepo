"""
PROJECT: Autonomous Smart Maize Planter
MODULE: Data Analysis - Seed Plate Sorting Accuracy
GOAL: Generate a bar chart illustrating singulation performance.
"""

import matplotlib.pyplot as plt
import pandas as pd
import os

# 1. Configuration
OUTPUT_FILE = '../outputs/Figure_17_SortingAccuracy.png'

def main():
    print("--- MECHATRONICS DATA ANALYSIS ---")
    
    # 2. Ingest the manual bench-test data
    categories = ['Single Seed\n(Target)', 'Double Seed\n(Over-sort)', 'Triple+ Seed\n(Severe Over-sort)']
    counts = [165, 32, 3]
    total_seeds = sum(counts)
    
    # Calculate percentages
    percentages = [(x / total_seeds) * 100 for x in counts]

    # 3. Initialise the Plot
    plt.figure(figsize=(9, 6))
    
    # Custom colours: Green for success, Amber for moderate error, Red for severe error
    bar_colours = ['#2ca02c', '#ffbf00', '#d62728']
    
    bars = plt.bar(categories, percentages, color=bar_colours, edgecolor='black', width=0.6)

    # 4. Add data labels on top of each bar
    for bar, count, pct in zip(bars, counts, percentages):
        height = bar.get_height()
        # Display both the raw count and the percentage
        plt.text(bar.get_x() + bar.get_width() / 2, height + 1.5, 
                 f'n={count}\n({pct:.1f}%)', 
                 ha='center', va='bottom', fontweight='bold', fontsize=11)

    # 5. Styling and Typography
    plt.title('Figure 17: Seed Plate Singulation Accuracy (n=200)', fontsize=14, fontweight='bold', pad=20)
    plt.ylabel('Frequency (%)', fontsize=12)
    
    # Set Y-axis limit slightly higher than 100 to give the labels room
    plt.ylim(0, 105)
    
    # Draw a reference line for the 90% engineering target
    plt.axhline(y=90, color='blue', linestyle='--', linewidth=1.5, label='Design Target (>90%)')
    plt.legend(loc='upper right')
    
    # Clean up the grid
    plt.grid(axis='y', linestyle=':', alpha=0.7)
    plt.gca().set_axisbelow(True)

    plt.tight_layout()

    # 6. Export
    os.makedirs('../outputs', exist_ok=True)
    plt.savefig(OUTPUT_FILE, dpi=300)
    print(f"Success! High-resolution graph saved to: {OUTPUT_FILE}")
    plt.show()

if __name__ == "__main__":
    main()