
# Project: Autonomous Smart Maize Planter

## 1. System Architecture Overview

### 1.1 Mechanical & Power Architecture

- **Drive Train:** Front-Wheel Differential Drive utilising high-torque geared wiper motors interfaced with BTS7960 H-Bridge drivers. This configuration provides superior pulling power through loose agricultural soil.

- **Rear Assembly:** Active Presswheel system driven by a dedicated wiper motor and BTS7960. An MPU6050 IMU is mounted on the pivot assembly to provide closed-loop angular feedback, ensuring the wheel tracks correctly during complex manoeuvres.

- **Depth Control:** High-precision linear actuator responsible for the deployment and retraction of the furrow opener. The system is calibrated to maintain a consistent planting depth of 1.5 inches (approx. 38mm) to ensure optimal seed germination.

- **Power Source:** 12V Lithium-ion Battery (568Wh).

    - _Note:_ Requires a dedicated voltage monitoring subsystem and low-voltage disconnect to prevent deep discharge cycles that could permanently damage the high-capacity cells.

- **Communication:** MAVLink protocol over UART for high-level telemetry, heartbeats, and command interfacing with the flight controller, allowing for standardised data exchange between the ESP32 and navigation stack.

### 1.2 Electronic & Control Stack (Iteration 2)

- **Primary Controllers:**

    - _Manual Stage:_ ESP32 (standard) for PS4 controller compatibility and initial drive testing, Bluepad32 library integration for low-latency PS4 Manual Override, enabling real-time chassis validation and manual obstacle avoidance.

    - _Autonomous Stage:_ ESP32-S3 for high-performance logic, hosting the MAVLink bridge.

- **Autonomous Intelligence:** Flight Controller + GPS for waypoint navigation and path correction.

- **Communication Protocol:** **MAVLink over UART**. The ESP32 acts as a companion computer or bridge, sending heartbeat, local position, and mission command messages to the Flight Controller.

- **Motor Drivers:** BTS7960 High-Current H-Bridge drivers for all wiper motors (Drive, Seeding, and Presswheel).

-  **Feedback Sensors:**

    - **AS5048A (Magnetic Encoder):** High-resolution non-contact feedback for the seed metering motor to ensure precise seed singulation and spacing.

    - **MPU6050 (IMU):** Strategic mounting on the presswheel pivot for orientation control and tilt compensation during turning motions.

## 2. Operational Logic and Decision logs
### 2.1 Presswheel Turning Compensation

During a pivot or differential turn, the presswheel must be actively steered to an angle $\theta$ that aligns with the vehicle's instantaneous centre of rotation (ICR). This active steering prevents the "dragging" or "scrubbing" effect typical of fixed-castor designs, which can disrupt the seedbed or increase the load on the drive motors. The control logic calculates the required angle based on the differential speed difference between the front drive wheels.
### 2.2 Linear Actuator Sequencing 

To maintain the integrity of the soil and the precision of the planting depth, the furrow opener follows a strict operational sequence:
- **Deploy:** The linear actuator lowers the opener only when `plantingActive == true` AND `isMoving == true`. This prevents the opener from digging a hole in a single spot while stationary.

- **Retract:** The system initiates an immediate retraction of the opener during U-turns (triggered by L1/R1 buttons) or whenever a `brake` or `stop` command is received. This ensures the opener is clear of the ground during lateral movements to prevent mechanical strain and unnecessary soil disruption.

### 2.3 Autonomous Navigation via MAVLink

The autonomous phase relies on the **MAVLink** protocol to synchronise the ESP32's hardware control with the Flight Controller's navigation decisions:

- **Waypoint Mission:** The Flight Controller calculates the trajectory based on the input field dimensions. The ESP32 receives `MISSION_ITEM` or `SET_POSITION_TARGET_LOCAL_NED` commands via UART to execute movements.

- **Telemetry & Heartbeat:** The ESP32 sends a constant 1Hz `HEARTBEAT` and `SYS_STATUS` message. If the MAVLink stream is interrupted, the planter enters a failsafe state, stopping all motors and retracting the furrow opener.

- **Precision Triggering:** The "Drop Event" for the seed metering system is triggered when the Flight Controller sends a custom MAVLink command or reaches a specific `MISSION_ITEM` coordinate.
## 3. Development Roadmap

### Phase 1: Manual Motion

- **1.1 Basic Tank Drive:** Verification of forward/reverse and stationary pivot movements using the PS4 controller (Completed in Iteration 1).

- **1.2 Presswheel Synchronisation:** Development of Iteration 2 code to control the rear wiper motor, correlating its rotation with the differential drive stick inputs to aid steering.

- **1.3 Actuator Control:** Mapping the PS4 D-Pad to the linear actuator for manual depth adjustment and testing the mechanical response time of the retraction sequence
### Phase 2: Seeding & Autonomous Logic

- **2.1 Seed Metering Calibration:** Bench-testing the AS5048A feedback to map motor rotations to specific seed drop counts.

- **2.2 GPS Waypoint Integration:** Transitioning to MAVLink-based navigation to move between predefined planting coordinates.

## 4. Safety & Failsafes

- **Hardware E-Stop:** A physical, normally-closed (NC) emergency stop switch that cuts the logic-level enable signals to all BTS7960 drivers.

- **Signal-Loss Protocol:** The ESP32 must monitor the Bluetooth connection; if the PS4 controller disconnects, the vehicle must immediately retract the furrow opener and enter a "Safe-Stop" mode.

- **Thermal Management:** Monitoring the BTS7960 temperatures, particularly during the high-load "Deploy" phase of the linear actuator.