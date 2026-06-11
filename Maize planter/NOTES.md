
### **1. The Operational Logic: The "Mixing" Strategy**

How do we combine these to ensure the motion is constant while allowing for "micro-corrections"?

#### **The Mathematical Model**

The ESP32 will treat the Throttle ($T$) as the "Base Velocity" and the Steering ($S$) as the "Differential Offset". We map the PWM signals ($1000\mu s$ to $2000\mu s$) to a range of $-1.0$ to $+1.0$.

Let:

- $T$ = Normalized Throttle (Forward/Reverse)
    
- $S$ = Normalized Steering (Left/Right)
    

The individual motor velocities ($V_L$ and $V_R$) are calculated using:

$$V_L = T + S$$

$$V_R = T - S$$

#### **Scenario A: Straight Line Planting**

The PixHawk identifies the target waypoint. It sets **Throttle to 1600** (a steady crawl) and **Steering to 1500** (perfectly neutral).

- $V_L = 1600 + 0 = 1600$
    
- $V_R = 1600 - 0 = 1600$
    
- **Result:** Both motors spin at the same speed; the planter moves in a straight line.
    

#### **Scenario B: Seamless Correction**

The GPS/Compass detects the planter has drifted 5 degrees to the left. The PixHawk (via its internal PID controller) keeps Throttle at **1600** but shifts Steering to **1550** (a slight right correction).

- $V_L = 1600 + 50 = 1650$
    
- $V_R = 1600 - 50 = 1550$
    
- **Result:** The left motor speeds up slightly and the right motor slows down. The planter gently veers back to the right **without stopping forward progress**.
    

#### **Scenario C: Stationary Pivot (U-Turn)**

At the end of a row, the PixHawk sets Throttle to **1500** (neutral) and Steering to **2000** (Full Right).

- $V_L = 1500 + 500 = 2000$
    
- $V_R = 1500 - 500 = 1000$
    
- **Result:** The left wheels spin forward and the right wheels spin backward at equal speed. The planter rotates perfectly on its axis.
    

---

### **Summary of the Plan**

1. **The PixHawk** handles the "Macro" navigation (Where am I? Where should I go?).
    
2. **The ESP32** handles the "Micro" execution (How fast do the motors turn to achieve that goal?).
    
3. **The Correction** is never a "stop and turn"; it is a continuous adjustment of the speed ratio between the wheels, overlaid on top of a constant forward velocity.



### **Engineering Notes on the Logic**

1. **The Deadzone:** PWM signals from the Pixhawk often jitter between 1498μs and 1502μs. Without the `DEADZONE` logic, your motor drivers would constantly hum while the machine is stationary.
    
2. **Mapping Formula:** We use the linear mapping function to convert the signal into a percentage. Mathematically:
    
    Output=InMax​−InMin​(Input−InMin​)×(OutMax​−OutMin​)​+OutMin​
    
    This allows the ESP32 to calculate the mixing ratio (VL​=T+S) using clean integers rather than raw microsecond pulses.
    
1. **Timeout:** The `25000` (25ms) in `pulseIn` is crucial. Standard PWM frequency is 50Hz (one pulse every 20ms). If we don't set a timeout and the wire comes loose, the ESP32 will "hang" for 1 full second waiting for a pulse, crashing your control loop.


### **The Official "Firing Sequence"**

To see values on your ESP32-S3 Serial Monitor, follow this exact order:

1. **Initialise ESP32-S3:** Connect it to your laptop and open the Serial Monitor (115200 baud). It should currently be printing "No PWM signal detected."
    
2. **Power Pixhawk:** Connect the 12V battery to the 3DR Power Module.
    
3. **Connect Pixhawk to PC:** Use the USB cable. Connect in Mission Planner.
    
4. **Wait for 3D Fix:** Ensure the GPS LED is flashing blue/green and the HUD is "Clean."
    
5. **Clear Hardware Safety:** Press the **Safety Button** (Change from flashing to **Solid Red**).
    
6. **Perform the action:**  Perform the action and wait for the results..
    
