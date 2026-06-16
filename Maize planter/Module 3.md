### 1. System Architecture: The Data Pipeline

Currently, your group member's site is a visual simulation. To transform this into a functional GCS, we must define a strict, modular pipeline. I propose splitting this into three distinct sub-modules:

- **The Frontend (UI/UX):** The existing Vercel deployment. It will handle farmer inputs (field boundaries, row spacing, seed distance) and visually map the rover's telemetry on the grid.
    
- **The Backend Server:** This is what we will build. It will receive the parameters from the frontend, execute the path-planning mathematics to generate a list of GPS waypoints, and serve as the traffic controller.
    
- **The Telemetry Link (The Bridge):** The bi-directional communication channel between the Backend Server and the ESP32 mounted on the planter.
    

### 2. The V-Model Development Strategy (Software Adaptation)

To adhere to our "Test-Before-Integrate" philosophy, we will tackle this in phases:

#### Stage 1: The "Data Contract" (Bench-Testing)

Before building the backend, we must define exactly what data is being passed around. We need to agree on standard JSON payloads.

- _Command Payload:_ What does the frontend send to the backend? (e.g., `{"mode": "auto", "row_spacing_cm": 75, "coordinates": [...]}`).
    
- _Telemetry Payload:_ What does the ESP32 send back to the server to update the simulation grid in real-time? (e.g., `{"gps_lat": X, "gps_lon": Y, "heading": H, "speed": V, "seeds_dropped": N}`).
    

#### Stage 2: Path Planning Logic (Unit Verification)

The core intelligence of this server will be transforming a farmer's drawn field boundary into a precise "lawnmower" path of GPS coordinates for the Pixhawk to follow.

- We will write and test this mathematical logic in pure isolation. We will input dummy boundary coordinates and verify that the outputted array of waypoints accurately reflects your required 75 cm row spacing.
    

#### Stage 3: Communication & Integration

Once the logic is verified, we will establish the real-time link.

- The ESP32 will be connected via Wi-Fi/Cellular. We must choose the right protocol. REST APIs are fine for sending the initial mission, but for real-time tracking on your grid, we will need **WebSockets** or an **MQTT Broker**.
    

#### Stage 4: Failsafes & Safety Protocols

We are controlling a heavy, potentially dangerous agricultural machine. The server architecture must include failsafes:

- **Heartbeat Monitor:** The server must expect a "heartbeat" ping from the ESP32 every second.
    
- **Signal-Loss Protocol:** If the heartbeat is lost (e.g., the planter drives out of network range), the server must log the exact last known coordinate, and the ESP32 must be programmed to automatically trigger an E-Stop, halting the BTS7960 motor drivers immediately.


### Stage 1

#### A. Command Payload (Frontend → Backend via HTTP/REST)

When the farmer draws the field and clicks "Start Planning", the frontend sends:

- `field_corners`: Array of 4 (Latitude, Longitude) pairs.
    
- `row_spacing_cm`: 60.
    
- `planter_speed_ms`: Desired ground speed (e.g., 1.0 m/s).
    

#### B. Waypoint Payload (Backend → Frontend via HTTP/REST)

The backend calculates the route and replies with:

- `status`: "success" or "error" (e.g., if the field is too small).
    
- `total_distance_m`: For battery estimation.
    
- `waypoints`: Array of (Latitude, Longitude) pairs in the exact order the machine must drive them.
    

#### C. Telemetry Payload (ESP32 → Backend → Frontend via MQTT)

Once the machine starts moving, the ESP32 publishes this to the MQTT broker at 1 Hz (once per second):

- `current_lat`: From Pixhawk GPS.
    
- `current_lon`: From Pixhawk GPS.
    
- `heading`: From IMU/Pixhawk.
    
- `ground_speed`: From Pixhawk telemetry.
    
- `seeds_dropped`: Estimated from the seed motor encoder (from Module 2).
    
- `system_status`: "CRUISE", "TURNING", "MANUAL", or "E-STOP".



### **Stage 2: The Waypoint Generation Mathematics**

To generate our serpentine ("lawnmower") path, our backend must perform three mathematical steps. We will calculate everything in a local, flat 2D Cartesian grid first (in metres), and then translate those points back into global GPS coordinates (Latitude/Longitude).

#### **1. The Inputs (The "Data Contract" Revision)**

When the farmer hits "Start Planning", the backend requires:

- **Origin Coordinates:** (Lat0​,Lon0​) — Pulled directly from the Pixhawk's current position.
    
- **Initial Heading:** ψ0​ — The True North bearing the planter is currently facing.
    
- **Dimensions:** Length (L) and Width (W) in metres.
    
- **Row Spacing:** d=0.6 m.
    

#### **2. Local Cartesian Generation (The Grid)**

We assume the planter starts at (0,0) on our local grid. Since you specified it starts at the bottom-right and must turn _left_ for the next row, our sequence of local waypoints (x,y) will look like this:

- **Row 0 (Forward):** Start (0,0)→ End (0,L)
    
- **Row 1 (Return):** Start (−d,L)→ End (−d,0)
    
- **Row 2 (Forward):** Start (−2d,0)→ End (−2d,L)
    

**The General Formula:** For any given row index i (where i=0,1,2...up to ⌊W/d⌋):

- The x-coordinate (Cross-Track) is always: xi​=−i⋅d
    
- The y-coordinates (Along-Track) alternate based on whether the row index is even or odd:
    
    - If i is **Even**: Drive from y=0 to y=L
        
    - If i is **Odd**: Drive from y=L to y=0
        

#### **3. Geographic Translation (The Matrix Rotation)**

Once we have our array of local waypoints, we cannot just add them to the GPS coordinates. The local grid must be rotated to match the planter's initial compass heading (ψ0​), aligning it with True North.

We apply a standard 2D Rotation Matrix to every (x,y) point to get the North/East offsets:

ΔNorth=x⋅cos(ψ0​)−y⋅sin(ψ0​)

ΔEast=x⋅sin(ψ0​)+y⋅cos(ψ0​)

Finally, we translate these metric offsets into actual Latitude and Longitude. Using the Earth's radius (R≈6,378,137 m):

Latnew​=Lat0​+(RΔNorth​)⋅(π180​)

Lonnew​=Lon0​+(R⋅cos(Lat0​⋅180π​)ΔEast​)⋅(π180​)

### **Unit Logic Verification (The Bench-Test Plan)**

Before integrating this mathematics into the main server API, we will write a standalone Python script to "Test-Before-Integrate".

**Our Verification Bisection Checklist:**

1. **Logic Test:** We will input a dummy field (L=50m, W=10m, ψ0​=45∘).
    
2. **Visualisation:** We will use a library like `matplotlib` to plot the generated GPS coordinates on a graph.
    
3. **Validation:** We must visually confirm that:
    
    - The path does not cross over itself.
        
    - The spacing is exactly 0.6 m.
        
    - The final waypoint correctly terminates the mission.


### The Master File Structure (Module 3)

We will organise your project directory into three distinct workspaces. This ensures maximum modularity.

Plaintext

```
Maize_Planter_GCS/
│
├── 1_Frontend_UI/                  # The farmer's visual interface
│   ├── index.html                  # The layout (Canvas for grid, input forms)
│   ├── style.css                   # The visual styling
│   └── script.js                   # Handles button clicks, HTTP requests, and MQTT grid updates
│
├── 2_Backend_Server/               # The "Brain" (Python)
│   ├── main.py                     # The FastAPI server & MQTT broker connection
│   ├── path_planner.py             # Pure mathematics: the 2D matrix rotation logic we discussed
│   ├── data_models.py              # Defines the exact structure of our JSON payloads
│   └── requirements.txt            # List of Python dependencies to install
│
└── 3_ESP32_Firmware/               # The Planter's Code (PlatformIO / C++)
    ├── platformio.ini              # Project configuration and library dependencies
    ├── src/
    │   ├── main.cpp                # The main RTOS loop (State Machine)
    │   ├── telemetry_mqtt.cpp      # Handles Wi-Fi connection and MQTT publish/subscribe
    │   ├── telemetry_mqtt.h
    │   ├── waypoint_manager.cpp    # Receives HTTP waypoints and pushes them to Pixhawk
    │   ├── waypoint_manager.h
    │   └── failsafe.h              # Signal-loss and E-Stop logic
    └── lib/                        # For custom local libraries (Module 1 & 2 integration)
```

### Required Toolchain & Libraries

To implement this structure, you will need to install specific libraries in both environments. I have curated the most reliable, industry-standard options for agricultural robotics.

#### 1. The Python Backend Environment

You will need to install Python (3.9+) on your laptop/server. Open your terminal in the `2_Backend_Server` folder and we will eventually run `pip install -r requirements.txt`. The requirements will include:

- **`fastapi` & `uvicorn`**: For building a lightning-fast, lightweight HTTP API to receive the field dimensions from the frontend and send back the calculated waypoints.
    
- **`paho-mqtt`**: The official Eclipse library allowing Python to act as an MQTT client (listening to the ESP32's telemetry).
    
- **`math` & `json`**: Built into Python, no installation required. Used for our matrix rotations and data formatting.
    

#### 2. The ESP32 Environment (PlatformIO)

In your `platformio.ini` file, under `lib_deps`, we will add the following libraries. These are strictly vetted for memory efficiency on the ESP32:

- **`knolleary/PubSubClient`**: The absolute gold standard for MQTT communication in C++. It is non-blocking, meaning it won't interrupt your motor control loops.
    
- **`bblanchon/ArduinoJson`**: Required to parse the JSON waypoint data sent from the Python server into usable C++ arrays.
    
- _Note on MAVLink:_ I assume you already have a MAVLink library integrated for Module 1/2 to talk to the Pixhawk.
    

#### 3. The MQTT Broker (The Traffic Cop)

As discussed, we need a broker. For the bench-testing stage, we won't install anything; we will just use a free public broker string in our code (e.g., `broker.hivemq.com`). Later, for field validation (when there is no internet), you will simply download and run **Eclipse Mosquitto** on your laptop. It runs silently in the background and requires practically zero configuration.






### Stage 3: Integration

Brilliant. That visual confirmation means our core mathematics (Stage 2) have passed unit verification. We can confidently move to Stage 3: **Integration**, where we wrap this logic in a networked server so the frontend UI can actually talk to it.

As per our architecture, we will split this into two files:

1. **`data_models.py`:** This enforces our JSON "Data Contract". It acts as a strict bouncer, ensuring the frontend sends exactly the right data types before the maths engine tries to process them.
    
2. **`main.py`:** This is the FastAPI application. It opens an HTTP endpoint for the frontend, calls your `path_planner.py` script, and initialises the MQTT connection to your local Mosquitto broker.

We are now ready to connect the farmer's interface to our new brain. We will be working inside the `1_Frontend_UI` folder.

To make the UI fully functional with our backend, we need to modify the `script.js` file to do two things:

1. **HTTP POST Request:** When the user clicks "Start", instead of generating fake points locally, JavaScript must send the field dimensions to our FastAPI endpoint (`http://127.0.0.1:8000/api/plan_mission`) and receive the array of coordinates.
    
2. **WebSockets / MQTT:** We need to connect the UI to the live telemetry stream so the planter's icon moves on the screen as it drives. (The easiest way to do this in a browser is to use standard WebSockets or an MQTT-over-WebSockets library like Paho).


### ESP32  Firmware development (The telemetry link)

Because the ESP32 is responsible for the hard real-time safety of a heavy agricultural machine, we cannot write this as one giant, procedural Arduino sketch. We must strictly adhere to our modular philosophy. We will architect this firmware as a **Non-Blocking State Machine**.

Here is the proposed architectural breakdown for the `3_ESP32_Firmware/src/` directory. Let us discuss the specific responsibilities of each file before we write the code.

### 1. The Core Architecture: File by File

#### `main.cpp` (The Conductor)

- **Role:** This is the heart of the State Machine. It does not handle complex logic directly; rather, it coordinates the other modules.
    
- **Behaviour:** In its `setup()`, it initialises the Serial ports and calls the setup functions of the other modules. In its `loop()`, it simply calls `MQTT.update()`, `MAVLink.update()`, and `Failsafe.check()` extremely rapidly without ever using a `delay()`.
    
- **States:** It holds the global system state (e.g., `STATE_IDLE`, `STATE_MISSION_RCV`, `STATE_CRUISE`, `STATE_ESTOP`).
    

#### `telemetry_mqtt.h` & `telemetry_mqtt.cpp` (The Network Bridge)

- **Role:** Handles all wireless communication between the ESP32 and your Ubuntu laptop.
    
- **Behaviour:** * Maintains the Wi-Fi connection to your hotspot.
    
    - Connects to your local Mosquitto broker (Port 1883).
        
    - **Subscribe:** Listens to the `maizepro/mission` topic. When your Python backend publishes the JSON array of waypoints, this module catches it, parses it using `ArduinoJson`, and alerts `main.cpp` that a mission has arrived.
        
    - **Publish:** Gathers speed, heading, and GPS data (from the MAVLink module) and publishes it to `maizepro/telemetry` at a strict 1 Hz to update your web UI.
        

#### `waypoint_manager.h` & `waypoint_manager.cpp` (The Translator)

- **Role:** The bridge between our custom JSON waypoints and the Pixhawk's aerospace protocol.
    
- **Behaviour:** * Takes the parsed Latitude/Longitude array from the MQTT module.
    
    - Initiates the strict MAVLink Mission Upload Protocol (Count → Request Item → Send Item → Ack).
        
    - Formats our waypoints into `MISSION_ITEM_INT` MAVLink packets and pushes them over the Hardware Serial (UART) to the Pixhawk.
        

#### `failsafe.h` (The Safety Net)

- **Role:** Ensures the machine fails safely if _anything_ goes wrong.
    
- **Behaviour:** Constantly monitors the time since the last MQTT ping and the time since the last MAVLink heartbeat. If the Wi-Fi drops (laptop dies) or the Pixhawk disconnects (wire comes loose), this module overrides `main.cpp`, immediately forces the motor PWMs to neutral, and commands the Pixhawk to halt.
    

### 2. The Data Flow (How it all links together)

Let us walk through the exact sequence of events when you click "Generate & Start Mission" on your web UI:

1. **Web UI (JS)** sends dimensions to **FastAPI (Python)**.
    
2. **FastAPI** calculates the math and publishes a JSON array to Mosquitto (`maizepro/mission`).
    
3. The **ESP32 (`telemetry_mqtt.cpp`)** receives the JSON, parses the coordinates, and stores them in a local C++ array.
    
4. The **ESP32 (`main.cpp`)** changes state to `STATE_MISSION_RCV` and tells `waypoint_manager.cpp` to start talking to the Pixhawk.
    
5. The **ESP32 (`waypoint_manager.cpp`)** sends the MAVLink waypoint items over the UART wires to the Pixhawk.
    
6. The **Pixhawk** confirms receipt, arms itself, and begins driving the motors.
    
7. As it drives, the **Pixhawk** streams Ground Speed and Heading back to the ESP32 via MAVLink.
    
8. The **ESP32 (`telemetry_mqtt.cpp`)** packages this into JSON and publishes it to Mosquitto (`maizepro/telemetry`), which visually updates your web UI.


### What is left in Module 3? (Closing the Loop)

Currently, the ESP32 gives the Pixhawk the map, but it doesn't give it permission to drive, nor does it tell the farmer where the machine actually is. We have three final steps to complete Module 3:

1. **Commanding the "Start" (Arming & Auto Mode):** Right now, the Pixhawk has the 20 waypoints, but it is sitting in "Manual" mode with the motors disarmed. We need to map a command so that when the web UI sends a specific signal, the ESP32 sends a `MAV_CMD_COMPONENT_ARM_DISARM` command to arm the motors, followed by a `MAV_CMD_DO_SET_MODE` command to switch the Pixhawk into "Auto" mode so it begins driving the route.
    
2. **Live Telemetry Extraction:** If you look at `main.cpp`, we are still calling `publishDummyTelemetry()`. We need to delete this. We must update `waypoint_manager.cpp` to listen for the Pixhawk's `VFR_HUD` message (which contains actual Ground Speed and Heading) and `GLOBAL_POSITION_INT` (which contains real-time Latitude and Longitude), and push those real numbers to our MQTT broker.
    
3. **UI Animation (The Canvas):** Once the real telemetry is flowing into the MQTT broker, we need to add a few lines of JavaScript to your frontend so the little planter icon physically moves along the green path on your screen as the real rover drives in the field.
    

### What is Module 4? (The Waypoint-to-Seed Sequence)

Once Module 3 is closed, the rover will be able to drive the serpentine path autonomously. **Module 4** is where the rover actually becomes a _planter_.

This is the ultimate Mechatronic Integration phase. We will marry your Seeding Unit (Module 2) with the Navigation Brain (Module 3).

- The ESP32 will constantly read the real-time Ground Speed (vg​) coming from the Pixhawk.
    
- Based on our 60 cm row spacing and your required seed spacing (e.g., 30 cm along the row), the ESP32 will calculate the exact RPM required for the seed metering motor.
    
- If the rover slows down (e.g., hits a muddy patch), the ESP32 will instantly slow down the seed motor to guarantee perfectly even seed distribution.
    
- We will also implement the "Actuator Deploy" logic here, ensuring the furrow opener is lowered at the start of a row and raised during the U-turns.


#### **Step 1: Arming and Auto Mode**.

#### The Mechatronics Safety Briefing (CRITICAL)

Up until this point, our code has been completely passive—just moving data around. The code we are about to write will physically engage the BTS7960 motor drivers.

- **Safety Mandate:** Before you upload this code or press the button, you **MUST** place your planter chassis on blocks so that the drive wheels are suspended in the air. We are entering the "Live Fire" stage of testing.
    

#### The MAVLink Theory: Arming & Modes

To make the Pixhawk drive the waypoints, we must send it two specific MAVLink commands in rapid succession:

1. **`MAV_CMD_COMPONENT_ARM_DISARM`:** This tells the Pixhawk to remove the software lock on the PWM outputs. (Parameter 1 must be set to `1.0` to ARM).
    
2. **`MAV_CMD_DO_SET_MODE` (or `SET_MODE`):** ArduRover has several modes (Manual, Acro, Steering, Hold, Auto). We must command it to enter **Mode 10 (AUTO)**, which explicitly tells the flight controller: _"Look at the mission buffer and start driving to waypoint 0."_