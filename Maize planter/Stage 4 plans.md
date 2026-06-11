
### **[[Module 1]]: Mechanical Characterisation & Traction**

We cannot program precision until we understand the mechanical reality of your "plant-on-the-move" system.

- **Refabrication:** Shorten lugs to $20\text{ mm}$. This reduces the "cam effect" on the chassis, protecting your electronics from high-frequency vibration.
    
- **The Velocity Look-Up Table (LUT):** Even without encoders, we need a mathematical relationship between the ESP32 PWM output and the Ground Speed ($v_{g}$) reported by the Pixhawk.
    
- **Success Criterion:** A stable $v_{g}$ at a constant PWM without the chassis "bucking."
    

### **Module 2: The Seeding Unit (The "Precision Pulse")**

This is treated as an isolated mechatronic unit before it ever touches the chassis.

- **Speed Calibration:** Identify the "Latencies." How long does the seed plate take to accelerate from $0$ to the required $RPM$?
    
- **The Spacing Formula:** With a $100\text{ mm}$ diameter plate and 2 grooves, one full rotation drops 2 seeds.
    
    - Distance per seed = $0.3\text{ m}$.
        
    - Chassis travel per rotation must = $0.6\text{ m}$.
        
    - We must calculate the Angular Velocity ($\omega$) of the plate relative to the Ground Speed ($v_{g}$):
        
$$\omega_{plate} = \frac{v_{g}}{0.6} \times 2\pi \text{ rad/s}$$
        
- **Success Criterion:** Consistent seed dropping on a bench-top test at varying simulated ground speeds.
    

### **Module 3: Geometry & Web UI Integration**

The bridge between the farmer's intent and the robot's coordinates.

- **Waypoint Synthesis:** The ESP32-S3 must host a web server where the farmer enters $Length$ and $Width$. The logic then generates a **"Serpentine Path"** (Serrated pattern).
    
    - Row 1: Start to End ($Y$ coordinate).
        
    - Turn: $60\text{ cm}$ shift ($X$ coordinate).
        
    - Row 2: End to Start (Reverse $Y$).
        
- **GPS Translation:** Converting these "Local Meters" into "Global Lat/Lon" offsets from the robot's starting position.
    
- **Success Criterion:** The ESP32 successfully generates a list of 4–10 coordinates and displays them on the UI map.
    

### **Module 4: Mission State Machine & MAVLink Handshake**

Integrating the "Brain" (ESP32) and the "Navigator" (Pixhawk).

- **Mission Upload Logic:** The ESP32 sends the generated waypoints to the Pixhawk via the `MISSION_ITEM` protocol we discussed.
    
- **Synchronised Trigger:** The ESP32 "listens" to the Pixhawk. When the Pixhawk says "Navigating to Waypoint X," the ESP32 activates the **Module 2** timing logic.
    
- **Linear Actuator Placeholder:** Even though the actuator is missing, we must include a `plantingMode` boolean in the code. When true, the ESP32 _acts_ as if the trench is open.
    
- **Success Criterion:** The rover follows a $60\text{ cm}$ wide "U-turn" path autonomously while the seed motor spins at the correct proportional speed.
    

### **Module 5: Perception extras (The "Safety Layer")**

Adding the robustness required for a real farm.

- **Obstacle Avoidance:** Integrating Ultrasonic/LiDAR. If an object is $<\text{ Distance Threshold } (\delta)$, the ESP32 forces the Throttle PWM to $1500\mu s$ (Neutral) immediately, bypassing Pixhawk intent.
    
- **Seed Level Monitoring:** IR break-beam or Ultrasonic sensor in the hopper to alert the Web UI when the seeds are low.
    
- **Success Criterion:** Robot stops before hitting a fence/person and sends a "Hopper Empty" alert to the laptop.
    

---

### **Engineering Summary of Tasks**

|**Priority**|**Module**|**Dependency**|
|---|---|---|
|**1**|Mechanical Refit|Wheel lug reduction (immediate).|
|**2**|Seeding Characterisation|Bench-test motor speed vs. seed plate grooves.|
|**3**|MAVLink Mission Upload|Establish how to send a full "Row" to the Pixhawk.|
|**4**|UI/UX Integration|Farmer input $\rightarrow$ coordinate generation.|
|**5**|Failsafes|Obstacle avoidance & seed monitoring.|

**Does this modular breakdown look correct to you?** Specifically, are you happy with **Module 2** being done entirely on the bench before we bolt the seeder to the chassis?