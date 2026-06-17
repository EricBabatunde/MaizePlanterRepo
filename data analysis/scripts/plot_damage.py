"""
PROJECT: Autonomous Smart Maize Planter
MODULE: Data Analysis - Seed Damage Rate
GOAL: Generate a publication-ready pie chart (Figure 16) for the manual bench-test data.
"""

import matplotlib.pyplot as plt
import pandas as pd
import os

# 1. Configuration
OUTPUT_FILE = '../outputs/Figure_16_SeedDamage.png'

def main():
    print("--- MECHATRONICS DATA ANALYSIS ---")
    
    # 2. Hardcode the bench-test data
    data = {
        'Category': ['Undamaged', 'Cracked (Viable)', 'Severely Damaged (Unplantable)'],
        'Count': [195, 2, 3]
    }
    df = pd.DataFrame(data)
    
    # Calculate percentages for the legend
    total_seeds = df['Count'].sum()
    df['Percentage'] = (df['Count'] / total_seeds) * 100

    # 3. Initialise the Plot
    # We use a custom colour palette: Green for good, Amber for warning, Red for failure
    colours = ['#2ca02c', '#ffbf00', '#d62728']
    
    # "Explode" the damaged slices slightly to draw the examiner's eye to them
    explode = (0, 0.1, 0.15) 

    plt.figure(figsize=(9, 6))

    # Create the pie chart
    wedges, texts, autotexts = plt.pie(
        df['Count'], 
        explode=explode, 
        labels=df['Category'], 
        colors=colours, 
        autopct='%1.1f%%', 
        shadow=False, 
        startangle=140,
        textprops=dict(color="black", fontsize=11)
    )

    # Style the percentage text inside the slices
    for autotext in autotexts:
        autotext.set_fontweight('bold')
        autotext.set_color('white')
        
    # Manually fix the text colour for the 'Cracked' slice if the yellow background is too bright
    autotexts[1].set_color('black')

    # 4. Styling and Typography
    plt.title('Figure 16: Seed Damage Assessment ($n=200$)', fontsize=14, fontweight='bold', pad=20)
    
    # Add a summary text box
    summary_text = f"Total Inspected: {total_seeds}\nTotal Damage: 2.5%\nSevere Damage: 1.5%"
    plt.text(1.2, -1.0, summary_text, fontsize=11, bbox=dict(facecolor='white', alpha=0.8, edgecolor='gray'))

    plt.tight_layout()

    # 5. Export
    os.makedirs('../outputs', exist_ok=True)
    plt.savefig(OUTPUT_FILE, dpi=300)
    print(f"Success! High-resolution graph saved to: {OUTPUT_FILE}")
    plt.show()

if __name__ == "__main__":
    main()