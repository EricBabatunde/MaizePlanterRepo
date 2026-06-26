### Phase 4 Master Plan: System Validation & Metric Acquisition

#### 4.1. The UI & Telemetry Overhaul (The Digital Dashboard)

Before we take the machine to the field, your Ground Control Station (the Web UI) must give you total situational awareness. We will update the C++ `MavlinkModule` and the JS `script.js` to handle:

- **Live Kinematic Tracking:** The 2D Canvas will plot a moving dot (representing the planter) across the field map in real-time by translating your global GPS coordinates into local `(X,Y)` canvas coordinates.
    
- **Navigation Errors:** We will extract the native `xtrack_error` (CTE) and `nav_bearing` (target heading) from the Pixhawk's `NAV_CONTROLLER_OUTPUT` packet and display them on the UI.
    
- **System Vitals:** We will parse `SYS_STATUS` for Battery Voltage and `GPS_RAW_INT` for the GPS Fix Type (No Fix, 3D Fix, RTK, etc.).
    
- **UI Cleanup:** We will rip out the legacy "Upload Waypoints" manual input and streamline the dashboard exclusively for our autonomous state machine.
    

#### 4.2. Direct-Drive & Kinematics Validation (The "Straight Line" Check)

Since we bypassed the ESP32 for the drive motors, we must validate the Pixhawk's direct control over the BTS7960 drivers on a flat, hard surface before hitting the dirt.

- **The Hardware Check:** Verify that Pixhawk Outputs 1, 2, 3, and 4 are cleanly driving the Left and Right motors forward and backward.
    
- **Skid-Steer Tuning:** We will manually drive the planter in `ACRO` or `MANUAL` mode to ensure the wheels respond linearly.
    
- **The L1 Controller Weave Test:** We will run a 2-waypoint autonomous mission on concrete. We will monitor the live CTE on your new UI. If it weaves (CTE oscillates), we will tune the `NAVL1_PERIOD` and `NAVL1_DAMPING` parameters in Mission Planner until it drives dead straight.
    

#### 4.3. The Mechanical Load & Seeding Test (The Dirt Test)

This is where the rubber meets the soil. Mathematics must survive physical reality.

- **Actuator Power Draw:** Deploy the furrow opener into real, compacted soil. We must verify the BTS7960 does not overheat and the battery does not brown-out the ESP32 under load.
    
- **Traction vs. Anchor:** Verify the drive wheels have enough grip to pull the deployed actuator without slipping. (Wheel slip artificially lowers your ground speed reading, throwing off your seed spacing).
    
- **Concrete Seed Drop:** Run the hopper with real maize seeds on a hard surface and measure the physical distance between dropped seeds. If the spacing is 25cm instead of 30cm, we simply adjust the $RPM = 100 \times v_g$ multiplier in the firmware.
    

#### 4.4. Formal Academic Testing (Data Logging for Capstone)

To definitively prove the success of your machine to your evaluation panel, we will log the telemetry to a CSV via your UI and calculate formal metrics:

- **Cross-Track Error (CTE) Analysis:** We will plot the CTE over time to show the maximum deviation from the path (e.g., "Maintained path within $\pm 0.15\text{m}$").
    
- **Heading Error Variance:** Comparing the actual heading versus the target bearing during row navigation.
    
- **Waypoint RMSE:** The Root Mean Square Error of the planter's arrival precision at each waypoint.
    
- **Field Quality Index (FQI):** A custom metric for your project that combines the CTE variance with the Seed Spacing Accuracy to prove the agricultural viability of the rows.