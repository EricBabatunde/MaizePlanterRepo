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
