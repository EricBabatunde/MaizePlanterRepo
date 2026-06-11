 	
---

### **The Autonomous Development Roadmap**

|**Stage**|**Focus**|**Hardware Task**|**Software Task**|
|---|---|---|---|
|**1**|**The Brain Flash**|Powering the PixHawk via Power Module.|Flashing **ArduRover** Firmware & Mandatory Calibrations.|
|**2**|**The Nervous System**|Wiring the "Passthrough" (PixHawk → ESP32).|Configuring **Skid-Steer** parameters in Mission Planner.|
|**3**|**The Digital Link**|Wiring the TELEM port for MAVLink (UART).|Establishing the **MAVLink Heartbeat** in Arduino IDE.|
|**4**|**The Mission Logic**|GPS M8N Mounting & I2C compass connection.|Coding the **Waypoint-to-Seed** sequence on the ESP32.|

---

### **[[Stage 1]]: The Brain Flash (Immediate Next Step)**

Before we connect the ESP32, we must make the PixHawk functional as a standalone navigator.

#### **1.1 Hardware: Power & Primary Connections**

- **Power:** Connect your **3DR Power Module** between your 12V Battery and the PixHawk's `POWER` port. This provides clean 5V to the PixHawk and monitors your battery voltage.
    
- **Safety:** Plug the **Safety Button** into the `SWITCH` port and the **Buzzer** into the `BUZZER` port. (The PixHawk will scream at you if it’s not happy—this is normal).
    
- **USB:** Connect the PixHawk to your PC via Micro-USB.
    

#### **1.2 Software: Firmware Installation & Calibration**

1. **Open Mission Planner:** Do **NOT** click 'Connect' yet.
    
2. **Flash Firmware:** Go to `SETUP` > `Install Firmware`. Click the icon for **Rover** (it usually looks like a small ground vehicle). Mission Planner will download the latest **ArduRover** firmware and upload it.
    
3. **Connect:** Once uploaded, select your COM port (usually "ArduPilot" or "USB Serial Device") at the top right, set baud to `115200`, and click **Connect**.
    
4. **Mandatory Calibrations:** You must complete these under `SETUP` > `Mandatory Hardware`:
    
    - **Accel Calibration:** Follow the on-screen prompts (Level, Left, Right, Nose Down, Nose Up, Back).
        
    - **Compass Calibration:** You will need to rotate the PixHawk in all axes until the progress bars fill.
        
    - **Flight Modes:** Set Mode 1 to `MANUAL` and Mode 2 to `AUTO`.
        

 set BATT_MONITOR to 4.
 
---

### **Stage 2: The Nervous System (The Passthrough Wiring)**

Once Stage 1 is verified (the HUD moves when you move the PixHawk), we connect the ESP32 to "hear" the PixHawk's steering intent.

#### **2.1 Hardware: Servo Rail to ESP32**

The PixHawk outputs its decisions on the "Servo Rail" (the 3-pin headers at the back).

- **Steering (Output 1):** Signal pin (top row) to **ESP32 GPIO 18**.
    
- **Throttle (Output 3):** Signal pin (top row) to **ESP32 GPIO 19**.
    
- **Ground:** Connect any GND pin on the PixHawk Servo Rail to an **ESP32 GND** pin.
    

#### **2.2 Software: Skid-Steer Configuration**

In Mission Planner, we need to tell the PixHawk that it is driving a "Tank" (Skid-Steer) even though it is outputting standard steering/throttle signals.

1. Go to `CONFIG` > `Full Parameter List`.
    
2. Search for **`SERVO1_FUNCTION`**: Set to `73` (Throttle Left).
    
3. Search for **`SERVO3_FUNCTION`**: Set to `74` (Throttle Right).
    
4. Search for **`PILOT_STEER_TYPE`**: Set to `0` (Default) or `1` depending on your preference.
    
5. Click **Write Params**.
    

---
### **Stage 3: The Digital Link (MAVLink over UART)**

While PWM tells the motors "how fast" to turn, it doesn't tell the ESP32 **where** the planter is or **how fast** it is actually moving over the ground. To achieve your **30cm seed spacing**, the ESP32 needs the Pixhawk's internal data (Ground Speed and GPS status). We get this via **MAVLink**.

#### **3.1 Hardware: The Telemetry Connection**

You will connect the **TELEM 2** port of the Pixhawk to the ESP32-S3.

|**Pixhawk TELEM 2 Port**|**ESP32-S3 Pin**|**Note**|
|---|---|---|
|**TX (Transmit)**|**RX (GPIO 1)**|Data from Pixhawk to ESP32|
|**RX (Receive)**|**TX (GPIO 2)**|Data from ESP32 to Pixhawk|
|**GND (Ground)**|**GND**|Use a common ground|

> **Warning:** Check your ESP32-S3 dev board. Many use **GPIO 43 (TX)** and **GPIO 44 (RX)** as the default for `Serial1`. We will use a secondary Hardware Serial port so you can still use the USB Serial for debugging.

#### **3.2 Software: The MAVLink Heartbeat**

We will now use the MAVLink library to "listen" to the Pixhawk’s telemetry stream.

**The Goal:** The ESP32-S3 should receive the `VFR_HUD` message, which contains **Groundspeed** ($m/s$).

---

### **Stage 3 Roadmap**

- **Step 1 (Hardware):** Wire the TELEM 2 port to the ESP32-S3.
    
- **Step 2 (Configuration):** In Mission Planner, set **`SERIAL2_PROTOCOL = 2`** (MAVLink 2) and **`SERIAL2_BAUD = 57`** (57600).
    
- **Step 3 (Software):** Upload a MAVLink "Listener" sketch to the ESP32-S3 to extract Groundspeed.