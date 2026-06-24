#### Step 1: The Network & Safety Handshake

1. Power up the system. Wait for the Pixhawk to acquire a 3D GPS Fix (Green LED).
    
2. Connect your smartphone/laptop to the `MAIZE_PLANTER_GCS` Wi-Fi network and open `http://192.168.4.1`.
    
3. Verify the UI is displaying your current coordinates and the heading rotates as you physically turn the chassis.
    
4. **The E-STOP Test:** Click the massive red E-STOP on the UI.
    
    - _Check:_ Did the ESP32 Serial Monitor print the override transmission?
        
    - _Check:_ Does Mission Planner (if connected via USB/Telemetry radio) show Channels 1 and 3 forced to $1500\mu s$?
        

#### Step 2: The Actuator Timers (State Transition)

1. In the UI Mission Control panel, input a dummy field size (e.g., $10\text{m} \times 5\text{m}$) and click "Start Mission".
    
2. The ESP32 should instantly shift from `IDLE` to `DEPLOYING`.
    
3. _Check:_ Does the Actuator BTS7960 activate the downward stroke for exactly the duration of `ACTUATOR_DEPLOY_MS` (e.g., 3 seconds) and then stop?
    

#### Step 3: The Synchronisation Loop (The Push Test)

Now the ESP32 is in the `PLANTING` state, waiting for movement.

1. Disarm the drive motors in Mission Planner (so the Pixhawk doesn't spin the wheels), or put the Pixhawk in `MANUAL` mode so you can push it freely.
    
2. Physically push the planter forward at a brisk walking pace.
    
3. _Check:_ Watch the Ground Speed ($v_g$) on your mobile UI. As it rises above $0.0\text{ m/s}$, the Seeding Motor should immediately begin spinning.
    
4. _Check:_ Push it faster. Does the seeding motor noticeably increase its RPM? Stop pushing. Does the motor instantly halt?
    

#### Step 4: The Headland Turn Simulation

1. Pick up the chassis and physically carry/push it to the end of your dummy $10\text{m}$ row.
    
2. As the calculated distance to the waypoint drops below $0.5\text{m}$ (or the Pixhawk flags it as reached), the ESP32 should shift to `RETRACTING`.
    
3. _Check:_ Does the Seeding Motor instantly stop? Does the Actuator pull _up_ for `ACTUATOR_RETRACT_MS`? 