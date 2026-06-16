"""
PROJECT: Autonomous Smart Maize Planter
MODULE: Data Analysis - Telemetry Update Rate
GOAL: Ingest MAVLink telemetry intervals and plot the inter-arrival timeline (Figure 13).
"""

import pandas as pd
import matplotlib.pyplot as plt
import os

# 1. Configuration and Paths
# Using relative paths based on our established project architecture
DATA_FILE = '../data/Telemetry_Run1.csv'
OUTPUT_FILE = '../outputs/Figure_13_TelemetryRate.png'

def main():
    print("--- MECHATRONICS DATA ANALYSIS ---")
    print(f"Loading telemetry data from: {DATA_FILE}")

    # Ensure output directory exists
    os.makedirs('../outputs', exist_ok=True)

    # 2. Ingest Data
    try:
        df = pd.read_csv(DATA_FILE)
    except FileNotFoundError:
        print(f"CRITICAL ERROR: Could not find {DATA_FILE}.")
        print("Please ensure you are running this script from inside the 'scripts' folder.")
        return

    # Strip any accidental whitespace from column names [cite: 1]
    df.columns = df.columns.str.strip()

    # Create the X-axis data (Packet Sequence Number) [cite: 1]
    df['Sequence_Number'] = df.index + 1

    # 3. Categorise Data for Visualisation
    # Isolate the normal packets from the dropouts (>500 ms) [cite: 1]
    THRESHOLD_MS = 500
    normal_packets = df[df['Interval_ms'] <= THRESHOLD_MS]
    delayed_packets = df[df['Interval_ms'] > THRESHOLD_MS]

    # 4. Initialise the Plot [cite: 1]
    plt.figure(figsize=(10, 6))

    # Plot normal packets as subtle blue dots
    plt.scatter(normal_packets['Sequence_Number'], normal_packets['Interval_ms'],
                color='#1f77b4', alpha=0.7, s=30, label='Standard Delivery (<500 ms)')

    # Plot delayed packets as bold red dots to highlight faults [cite: 1]
    plt.scatter(delayed_packets['Sequence_Number'], delayed_packets['Interval_ms'],
                color='red', marker='o', s=80, edgecolor='black', zorder=5, label='Delayed (>500 ms)')

    # Draw the target 5 Hz (200 ms) baseline [cite: 1]
    plt.axhline(y=200, color='green', linestyle='--', linewidth=2, label='Target Update Rate (200 ms / 5 Hz)')

    # 5. Styling and Typography
    plt.title('Figure 13: Telemetry Packet Arrival Timeline', fontsize=14, fontweight='bold', pad=15)
    plt.xlabel('Packet Sequence Number', fontsize=12)
    plt.ylabel('Inter-Arrival Time (ms)', fontsize=12)

    # Dynamically scale the Y-axis to give breathing room above the maximum delay
    plt.ylim(0, max(df['Interval_ms'].max() + 50, 700))

    plt.grid(True, linestyle=':', alpha=0.6)
    plt.legend(loc='upper right', framealpha=0.95)
    plt.tight_layout()

    # 6. Export and Display
    plt.savefig(OUTPUT_FILE, dpi=300) # Save as high-resolution PNG for the report
    print(f"Success! High-resolution graph saved to: {OUTPUT_FILE}")
    plt.show() # Open the interactive viewer

if __name__ == "__main__":
    main()