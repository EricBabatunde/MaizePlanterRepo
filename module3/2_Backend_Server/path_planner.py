import math
import matplotlib.pyplot as plt

def generate_waypoints(lat0, lon0, heading_deg, length, width, row_spacing=0.6):
    """
    Generates a serpentine waypoint path for the planter.
    Assumes planter starts at Bottom-Right (0,0) and turns LEFT for new rows.
    """
    waypoints = []
    
    # Calculate how many rows we need based on width
    # math.ceil ensures we cover the full width even if it's not perfectly divisible by 0.6
    num_rows = math.ceil(width / row_spacing) + 1
    
    # Convert compass heading to radians
    heading_rad = math.radians(heading_deg)
    
    # Earth's radius in metres (used for flat-earth coordinate translation)
    R = 6378137.0 

    print(f"--- Generating Mission ---")
    print(f"Target: {num_rows} rows, {length}m long.")

    for i in range(num_rows):
        # 1. Local Cartesian Grid (x, y)
        # Planter starts bottom-right. Turning left means moving in the negative X direction.
        x_local = -i * row_spacing
        
        # Determine forward or return pass (Alternating Y coordinates)
        if i % 2 == 0:
            y_start = 0.0
            y_end = float(length)
        else:
            y_start = float(length)
            y_end = 0.0
            
        # We only generate two waypoints per row (the "Long Vector" strategy)
        row_points = [(x_local, y_start), (x_local, y_end)]
        
        for lx, ly in row_points:
            # 2. Rotation Matrix (Aligning local grid to True North)
            # Standard clockwise rotation for geographic compass bearings:
            delta_north = (ly * math.cos(heading_rad)) - (lx * math.sin(heading_rad))
            delta_east = (ly * math.sin(heading_rad)) + (lx * math.cos(heading_rad))
            
            # 3. Geographic Translation (Metres to Lat/Lon)
            lat_new = lat0 + (delta_north / R) * (180.0 / math.pi)
            lon_new = lon0 + (delta_east / (R * math.cos(math.radians(lat0)))) * (180.0 / math.pi)
            
            waypoints.append({
                "row": i,
                "lat": lat_new,
                "lon": lon_new,
                "local_x": lx, # Kept for plotting
                "local_y": ly  # Kept for plotting
            })

    return waypoints

# ==========================================
# UNIT TEST: The "Test-Before-Integrate" Bench
# ==========================================
if __name__ == "__main__":
    # Dummy Start Location (E.g., Federal University of Agriculture, Abeokuta)
    START_LAT = 7.2225
    START_LON = 3.4408
    
    # Farmer Inputs
    INITIAL_HEADING = 45.0  # Facing North-East
    FIELD_LENGTH = 20.0     # 20 metres long
    FIELD_WIDTH = 5.0       # 5 metres wide
    ROW_SPACING = 0.6       # 60 cm
    
    # Run the brain
    mission_plan = generate_waypoints(START_LAT, START_LON, INITIAL_HEADING, FIELD_LENGTH, FIELD_WIDTH, ROW_SPACING)
    
    # Print the first few coordinates to terminal
    for wp in mission_plan[:4]:
        print(f"Row {wp['row']} -> Lat: {wp['lat']:.7f}, Lon: {wp['lon']:.7f}")
        
    # --- VISUALISATION ---
    # We plot the local coordinates to verify the pure geometry
    x_vals = [wp['local_x'] for wp in mission_plan]
    y_vals = [wp['local_y'] for wp in mission_plan]
    
    plt.figure(figsize=(6, 8))
    plt.plot(x_vals, y_vals, marker='o', linestyle='-', color='green', label='Planter Path')
    plt.plot(x_vals[0], y_vals[0], marker='^', color='blue', markersize=12, label='Start (Bottom-Right)')
    plt.plot(x_vals[-1], y_vals[-1], marker='s', color='red', markersize=12, label='End Mission')
    
    plt.title(f"Serpentine Logic Verification ({ROW_SPACING}m spacing)")
    plt.xlabel("Cross-Track (Metres)")
    plt.ylabel("Along-Track (Metres)")
    plt.grid(True)
    plt.legend()
    plt.axis('equal') # Ensures 1m in X looks exactly like 1m in Y
    plt.show()