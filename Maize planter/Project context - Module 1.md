# Project Context: Autonomous Mechatronic Maize Planter

## 1. Project Overview

This project is an autonomous agricultural maize planter. The current development phase (Module 1) focuses on the core mechatronic integration: establishing real-time differential drive control, parsing Pixhawk telemetry, logging empirical data to internal flash memory, and executing automated ISO-standard agronomic testing routines.

The system is designed to operate 100% offline in remote agricultural fields, utilizing a local Wi-Fi Access Point and an embedded asynchronous Web UI for control and data acquisition.

## 2. Hardware Architecture

- **Microcontroller:** ESP32-S3 (WROOM)
    
- **Flight Controller:** Pixhawk (connected via Serial MAVLink for GPS/Telemetry)
    
- **Drive System:** Differential drive using two Wiper Motors.
    
- **Seed Metering:** Single Wiper Motor driving a mechanical seed plate.
    
- **Motor Drivers:** 3x BTS7960 (Left Drive, Right Drive, Seed Meter).
    
- **Power:** 12V Li-ion Battery. Current sensing is achieved by reading the internal `IS` (Current Sense) pins of the BTS7960 drivers via the ESP32 ADC.
    

## 3. Software Environment & Architecture

- **Framework:** PlatformIO (Arduino framework)
    
- **Platform Version:** `espressif32@6.7.0` (Locked to Arduino Core v2.0.17 to ensure stability with ESPAsyncWebServer and prevent FreeRTOS crashes).
    
- **File System:** LittleFS (Used for serving offline JS libraries and logging CSV data).
    
- **Code Structure:** Modular C++ (`.h` and `.cpp` pairs).
    
    - `Config.h`: Single source of truth for pins, globals, and the State Machine (`SystemState`).
        
    - `MotorControl`: Manages BTS7960 PWM mapping. Uses 22.5kHz frequency to eliminate coil whine. Maps 1000-2000us RC commands to 0-255 PWM. Calculates total system Amperage from ADC pins.
        
    - `MavlinkHandler`: Parses MAVLink `VFR_HUD` packets from the Pixhawk over hardware serial to extract real-time ground speed.
        
    - `WebInterface`: Hosts the ESPAsyncWebServer. Serves an embedded HTML/JS UI. Uses WebSockets for low-latency command parsing.
        
    - `DataLogger`: Mounts LittleFS and handles dynamic CSV file generation and appending for test metrics.
        
    - `main.cpp`: The conductor. Runs the main loop and executes the logic for the current state.
        

## 4. Implemented Testing Protocols & Features

The firmware currently features a state machine (`MANUAL, CRUISE_A-D, TURN_TEST, METER_TEST, METER_JAM, E_STOP`) designed to execute specific agronomic tests:

1. **Manual Control (nipple.js):**
    
    - Dual on-screen joysticks map to tank steering.
        
    - _Crucial Detail:_ `nipplejs.min.js` is hosted locally on LittleFS to allow full offline operation in the field without breaking the UI.
        
2. **Speed Regulation Calibration (Brackets A-D):**
    
    - Automated PWM sweeps. The system holds a PWM, stabilizes for 1.5s, logs `PWM`, `GroundSpeed`, and `Amps` to a CSV, and increments.
        
3. **Autonomous Turn Test (180° Pivot):**
    
    - UI button acts as a Start/Stop toggle.
        
    - Uses a "Pivot Turn" (one wheel locked, one wheel driven) to naturally shift the planter by its track width for the next row.
        
    - Alternates direction (Left Pivot / Right Pivot) automatically on each test. Logs total turn duration to CSV.
        
4. **Telemetry Rate Test:**
    
    - WebSocket ping frequency is calculated client-side in JS to monitor connection health. Can be logged to LittleFS via UI toggle.
        
5. **Seed Metering & Anti-Jam System:**
    
    - UI toggle runs the seed meter at a constant RPM for visual single/double seed accuracy tests.
        
    - _Anti-Jam Logic:_ ESP32 actively monitors the Seed Meter's `IS` (current sense) pin. If current spikes above a threshold (stall/jam), it automatically cuts power, reverses the motor for 500ms to spit the jammed seed out, and resumes forward planting.
        
6. **Power Consumption Logging:**
    
    - System reads the `IS` analog voltages, converts to Amps (Left + Right + Meter + 0.3A overhead), and logs continuously during calibration tests to calculate `Wh/ha`.
        

## 5. Current State of Development

All code modules have been successfully refactored, linked, and compiled in PlatformIO for the ESP32-S3. The HTML/JS UI, WebSockets, LittleFS logging, motor mapping, and State Machine logic are fully written.

**Immediate Next Steps for the IDE:**

1. Review the modular files to ensure all variables (`extern`) and function signatures perfectly align across headers and source files.
    
2. Fine-tune the ADC math conversion for the BTS7960 `IS` pins to accurately calculate real-world Amperage based on multimeter probing.
    
3. Set the specific `JAM_THRESHOLD_RAW` analog value for the seed meter anti-jam sequence.