
### **Stage 1: The Brain Flash (Immediate Next Step)**

Before we connect the ESP32, we must make the PixHawk functional as a standalone navigator.

#### **1.1 Hardware: Power & Primary Connections**

- **Power:** Connect your **3DR Power Module** between your 12V Battery and the PixHawk's `POWER` port. This provides clean 5V to the PixHawk and monitors your battery voltage.
    
- **Safety:** Plug the **Safety Button** into the `SWITCH` port and the **Buzzer** into the `BUZZER` port. (The PixHawk will scream at you if it’s not happy—this is normal).

Don't do above during calibrations and shii, only power PixHawk with USB/Micro-USB!!
    
- **USB:** Connect the PixHawk to your PC via Micro-USB.
    

#### **1.2 Software: Firmware Installation & Calibration**

1. **Open Mission Planner:** Use latest Mission Planner software. Do **NOT** click 'Connect' yet.
    
2. **Flash Firmware:** Go to `SETUP` > `Install Firmware`. Click the icon for **Rover** (it usually looks like a small ground vehicle). Mission Planner will download the latest **ArduRover** firmware and upload it. Choose fmuv3 for one of the options. Reboot as instructed.
    
3. **Connect:** Once uploaded, select your COM port (usually "ArduPilot" or "USB Serial Device") at the top right, set baud to `115200`, and click **Connect**.
    
4. **Mandatory Calibrations:** You must complete these under `SETUP` > `Mandatory Hardware`:
	    Setup a box to fix the PixHawk and the GPS with Compass to aid compass and accelerometer calibration. It is important to stay away from any metallic objects/components to avoid EMI.
    - **Accel Calibration:** Follow the on-screen prompts (Level, Left, Right, Nose Down, Nose Up, Back).
        
    - **Compass Calibration:** You will need to rotate the PixHawk in all axes until the progress bars fill. Remove missing compasses if necessary. Set fitness to Very Strict to ensure High accuracy. Make sure both the PixHawk and GPS are both pointing in the same direction - North (as approximate as possible). 
        
    - **Flight Modes:** Set Mode 1 to `MANUAL` and Mode 2 to `AUTO`.
        

 set BATT_MONITOR to 4.
 
---


### Verification tests to ensure Stage 1 is complete and PixHawk is setup for Stage 2

### **1. The "Arming" Logic & Safety Button Test**

In the ArduRover firmware, the Pixhawk will not output any PWM signals (the instructions for your motors) unless it is **Armed**.

- **The Test:** Press and hold the **Safety Button** until it stops flashing and stays solid red.
    
- **Verification:** Look at the Mission Planner HUD. It should say "DISARMED" in large red letters initially. Once the button is solid, you can attempt to "Arm" via Mission Planner (`Actions` tab > `Arm/Disarm`).
    
- **Success Criteria:** The HUD must display a green **"ARMED"** message. If it refuses, read the "Pre-arm" error message (e.g., "PreArm: Compass not healthy" or "PreArm: GPS Speed error"). Before anything (Arming), the HUD must display 'Ready to Arm'.

Wait for a while to ensure that there is a 3D GPS lock. 

### **2. GPS 3D Fix Verification**

Since this is an autonomous planter, the Pixhawk is "blind" without a 3D GPS lock.

- **The Test:** Take the setup near a window or outdoors. Watch the LED on the **M8N GPS module** and the Pixhawk’s main LED. It must start flashing blue first, this indicates that it is looking for a GPS lock. 
    
- **Verification:** In Mission Planner, check the `GPS: No Fix` status. It must change to `GPS: 3D Fix` or `GPS: 3D dgps`.
    
- **Success Criteria:** You should see your current coordinates on the map in Mission Planner, and the main Pixhawk LED should be **flashing green** (Armed/Disarmed status pending) rather than blue. Flashing green is normal - it has a GPS lock and Disarmed, solid green is for when it is Armed.
    

### **3. Power Module Calibration**

We must ensure the Pixhawk accurately monitors the battery. If it thinks the battery is dead, it will trigger a "Failsafe" and stop the motors—likely when you are in the middle of a row.

- **The Test:** Go to `SETUP` > `Optional Hardware` > `Battery Monitor`.
    
- **Settings:** * Monitor: `Analog Voltage and Current`
    
    - Sensor: `4: 3DR Power Module`
        
    - HW Ver: `0: Pixhawk`
        
- **Verification:** Use a multimeter to measure your actual battery voltage. Compare it to the "Battery Voltage" displayed in Mission Planner.
    
- **Success Criteria:** The measured voltage and displayed voltage should be within **0.1V** of each other.
    

---

### **Engineering Checklist for Stage 1 Completion**

|**Task**|**Status**|**Requirement**|
|---|---|---|
|**Firmware**|[ ]|ArduRover 4.x installed.|
|**Safety Switch**|[ ]|Solid red when pressed; stops PWM lockout.|
|**Arming**|[ ]|HUD shows "ARMED" without errors.|
|**GPS Fix**|[ ]|3D Fix achieved; coordinates visible on map.|
|**Power**|[ ]|Voltage monitoring calibrated and accurate.|