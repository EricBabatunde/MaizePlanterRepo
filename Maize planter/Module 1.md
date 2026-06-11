### **Hardware Layer (Physical & Connections)**

The primary goal here is to ensure the chassis can maintain a steady velocity ($v_g$) on soil without mechanical oscillations affecting the GPS/IMU data.

- **Mechanical Refit:**
    
    - **Lug Modification:** Trim front wheel lugs to 20mm. Ensure the "attack angle" of the lugs is consistent across both drive wheels to prevent the planter from pulling to one side.
        
    - **Centre of Gravity (CoG) Check:** Ensure the 12V battery and seed hopper are mounted such that there is sufficient "downforce" on the front drive wheels to prevent tyre slip, but not so much that the rear castors dig into the soil.
        
- **Electrical Integrity:**
    
    - **Vibration Damping:** Check the Pixhawk "Shock Absorber" mount. Ensure the flight controller is not touching the chassis walls, as the mechanical "thumping" of even 20mm lugs can cause EKF (Extended Kalman Filter) "Clipping" in the accelerometers.
        
    - **Wiring Loom:** Secure all TELEM and Servo cables with cable ties. In an agricultural environment, loose connectors are the #1 cause of autonomous mission failure.

We are integrating the **Power Subsystem** and the **Motion Subsystem**. You must use a "Star Ground" configuration to prevent motor noise from crashing the ESP32.
#### **Power & Data Wiring Table**

| **Component**        | **Port/Pin**   | **Connection To**      | **Notes**                                 |
| -------------------- | -------------- | ---------------------- | ----------------------------------------- |
| **12V Battery**      | (+) and (-)    | **Bus Bars**           | Use heavy-gauge wire for the motor lines. |
| **3DR Power Module** | Input          | Bus Bars               | Steps down 12V to 5V for Pixhawk.         |
| **Buck Regulator**   | Input          | Bus Bars               | Step down 12V to **5V** for ESP32.        |
| **Wiper Motor L/R**  | Power          | **Motor Driver**       | (e.g., BTS7960/IBT-2)                     |
| **Motor Driver**     | Control        | **ESP32-S3**           | PWM and Direction pins.                   |
| **Pixhawk Telem 2**  | TX/RX/GND      | **ESP32 (GPIO 4/5)**   | MAVLink Telemetry link.                   |
| **Pixhawk Main Out** | Pins 1, 3, GND | **ESP32 (GPIO 18/19)** | PWM Passthrough (The "Intent").           |

Since we are pivoting to the Web UI for the **Module 1 Velocity Sweep**, the hardware setup remains largely the same, but with the ESP32-S3 acting as an **Access Point (AP)**.

1. **The "Wireless Bridge":**
    
    - The ESP32-S3 creates a Wi-Fi network (e.g., `MaizePlanter_Alpha`).
        
    - You connect your smartphone/tablet to this network.
        
2. **Safety Wiring:** * Ensure your **E-Stop** (Emergency Stop) is physically wired to the motor driver power line. Since we are testing wirelessly, you must be able to cut power manually if the Wi-Fi signal drops or the logic hangs.
    

You must implement a **Hard-Wired E-Stop**. This is a physical, normally-closed (NC) "Mushroom" button.

- **The Logic:** You place this button in the **12V High-Current Line** feeding the motor drivers (the wiper motor lines).
    
- **The Connection:** Battery (+) $\rightarrow$ E-Stop Button $\rightarrow$ Motor Driver Power Terminal.
    
- **The Result:** When pressed, it physically cuts the current to the motors. The ESP32 and Pixhawk stay powered (since they are on the Buck Regulator line), but the machine stops moving regardless of what the software is "thinking."



### **Software Layer (Characterisation Logic)**

Since we are not using drive encoders, we must create a **PWM-to-Velocity Mapping**. We need to know exactly how much "thrust" results in how much "ground speed." 	
**Surface Selection:** Perform this test on the actual soil where you intend to plant. Friction coefficients $(\mu)$ on workshop concrete are vastly different from agricultural fields.

- **The Velocity Sweep Procedure:**
    
    1. We will develop a "Calibration Sketch" for the ESP32-S3 that performs a **PWM Sweep**.
        
    2. The ESP32 will command the motors to start at PWM $1550$ (slow crawl) and increase to $1850$ in increments of $25\mu s$.
        
    3. At each step, the ESP32 will "listen" to the MAVLink ground speed ($v_g$) from the Pixhawk and wait for it to stabilise.
        
    4. **Data Logging:** We will record these pairs $(PWM, v_g)$ to create a linear regression or a Look-Up Table (LUT).
    5. 
	**Software Logic: The "Cruise Sweep" Algorithm**
	
	To ensure the "consistency" you requested, we will implement a non-blocking timer logic for the automated ramp.
	
	**The Sweep Sequence:**
	
	1. **Trigger:** User clicks 'X' on the Web UI.
	    
	2. **Ramp:** Commanded PWM increases: $P_{t} = P_{t-1} + 25\mu s$.
	    
	3. **Stabilisation:** The code waits $3000\text{ms}$ at each step.
	    
	4. **Log:** The system integrates $v_{g}$ over that window to find the average speed and writes to `run_X.csv`.
	    
	5. **Termination:** Stop at $1850\mu s$ or if 'Triangle' is pressed.
	
	6. **Data Analysis:**
	        
	    - Create a "Scatter Plot" with PWM on the X-axis and Speed on the Y-axis.
	        
	    - **The Trendline:** Add a linear trendline. The resulting equation (e.g., $y = 0.005x - 7.5$) is what the ESP32-S3 will use to "guess" how far it has moved when we reach **Module 4**.
	        
- **The Traction Assessment:**

    - We will compare the **Commanded Throttle** to the **Actual Ground Speed**. If the PWM is high but $v_g$ is low, the software must flag a "Traction Loss" state.

1. **Automated Logging:** Capture $PWM$ (input) and $v_g$ (output) simultaneously.
    
2. **Stabilisation Logic:** Implement a delay to allow the rover’s mass and the Pixhawk’s EKF (Extended Kalman Filter) to reach steady-state before recording a data point.
    
3. **CSV Formatting:** Output data to the Serial Monitor in a format ready for Excel or Google Sheets.
    

Instead of a physical button on a PS4 controller, we will use a **Web Trigger**.

We will use the **ESPAsyncWebServer** library to handle the Web UI. It is non-blocking, which is essential for maintaining the MAVLink heartbeat while serving a webpage.

#### **1. The Web UI (Frontend)**

For the "drag sticks," we will use **nipple.js**, a lightweight JavaScript library that creates virtual joysticks perfect for mobile browsers.

- **Left Stick:** Sends Y-axis data for the Left Wiper Motor.
    
- **Right Stick:** Sends Y-axis data for the Right Wiper Motor.
    
- **Buttons:** Standard CSS circles mapped to X (Cross), Triangle, Circle, and Square.

#### **2. The Automated Logging Logic (Cruise Mode)**

To ensure "consistent data," we will implement a `runCount` variable.

1. **First Press (X):** Start Sweep 1. Data saved as `run_1.csv`.
    
2. **Second Press (X):** Start Sweep 2. Data saved as `run_2.csv`.
    
#### **3. The Logging Logic (LittleFS)**

We will use the **LittleFS** library to treat the ESP32’s internal flash like a mini hard drive.

- **Initialise:** `LittleFS.begin()`
    
- **File Creation:** `File f = LittleFS.open("/sweep_data.csv", "a")`
    
- **Data Format:** `f.printf("%d, %.3f\n", commandedPWM, currentGroundSpeed);`
    
#### **4. The Virtual Sweep Trigger**

The Web UI will have two primary buttons:

- **[HOLD TO DRIVE]:** Allows you to position the planter manually.
    
- **[START CALIBRATION SWEEP]:** Once pressed, the ESP32-S3 takes over, executes the $1550\mu s \rightarrow 1800\mu s$ ramp, and logs every $10\mu s$ increment to the internal file.

#### **5. Software Architecture: The "State Machine"**

To handle "Cruise Mode," "Manual Control," and "Emergency Stop" simultaneously, we will use a **State Machine**.

| **State**       | **Action**                                        |
| --------------- | ------------------------------------------------- |
| **MANUAL**      | Joysticks control wheels. Logging is idle.        |
| **CRUISE_RAMP** | Automatic PWM increase ($1550 \rightarrow 1800$). |
| **CRUISE_STAY** | Wait for 3 seconds for speed to stabilise.        |
| **CRUISE_LOG**  | Write $(PWM, v_g)$ to LittleFS.                   |
| **E_STOP**      | Kill all PWM signals ($0\mu s$ or $1500\mu s$).   |

---

### **The Mathematical Model**

We are looking to define the linear relationship:

$$v_g = m \cdot (PWM_{active} - PWM_{neutral}) + c$$

Where:

- $v_g$ = Ground Speed ($m/s$).
    
- $m$ = The "Thrust Coefficient" (Slope).
    
- $PWM_{active}$ = Current pulse width.
    
- $PWM_{neutral}$ = $1500\mu s$.
    

---

### **Failsafe Note**

During this software test, if `currentGroundSpeed` stays at `0.0` while `currentThrottlePWM` is increasing, the ESP32-S3 must interpret this as **Traction Loss (Wheel Spin)**. In a real planting scenario, we would trigger an alert because the seeds would be dumped in a single pile while the machine is stuck.

**Does this logging approach for characterising the traction suit your current field setup?** Once you have gathered this data, we can move to **Module 2** to integrate the **AS5048A encoder** for the seed plate.


