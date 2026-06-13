### Step 1: The Local MQTT Broker (Eclipse Mosquitto)

Mosquitto is exceptionally lightweight. However, for security reasons, version 2.0 and above default to only allowing connections from the laptop itself (localhost). We must configure it to accept the ESP32's connection over your Wi-Fi.

**1. Installation:**

- **Windows:** Download the installer from the official Mosquitto website.
    
- **macOS:** Use Homebrew: `brew install mosquitto`
    
- **Linux/Ubuntu:** `sudo apt install mosquitto mosquitto-clients`
    

**2. The Critical Configuration (`mosquitto.conf`):** Navigate to the directory where Mosquitto is installed. Open `mosquitto.conf` in a text editor (as Administrator/sudo) and add these two lines at the very bottom:

Plaintext

```
listener 1883 0.0.0.0
allow_anonymous true
```

_Engineering Note:_ `0.0.0.0` tells the broker to listen to all network interfaces (your Wi-Fi), and `allow_anonymous true` disables password checks for now. We can add authentication later during the final security audit.
 
In Ubuntu, the master /etc/mosquitto/mosquitto.conf file is typically locked by the system, and it is standard Linux practice to leave it untouched. Instead, the master file is programmed to automatically load any .conf files placed inside the /etc/mosquitto/conf.d/ directory.

Here is the precise way to implement your local bridge:

- **Open your terminal and create a new configuration file:**
    
    Bash
    
    ```
    sudo nano /etc/mosquitto/conf.d/esp32_bridge.conf
    ```
    
- **Paste our required network rules:**
    
    Plaintext
    
    ```
    listener 1883 0.0.0.0
    allow_anonymous true
    ```
    
- **Save and Exit:** Press `Ctrl+O`, `Enter`, then `Ctrl+X`.
    
- **Restart the Mosquitto Service:** Since we are running it as a background service on Ubuntu, apply the changes by restarting it:
    
    Bash
    
    ```
    sudo systemctl restart mosquitto
    ```
    
- **Verify it is running:**
    
    Bash
    
    ```
    sudo systemctl status mosquitto
    ```
    
    _(You should see an "active (running)" status in green)._

### Step 2: The Python Backend Environment

We must isolate our Python dependencies using a Virtual Environment so they do not conflict with your system.

**1. Create the `requirements.txt` File:** In your `2_Backend_Server` folder, create a file named `requirements.txt` and paste the following:

Plaintext

```
fastapi==0.103.1
uvicorn==0.23.2
pydantic==2.3.0
paho-mqtt==1.6.1
```

**2. Terminal Setup Commands:** Open your terminal, navigate to your `2_Backend_Server` folder, and execute the following:

- - **Before you create the environment,** ensure you have the `venv` package installed (Ubuntu strips this out by default to save space):
    
    Bash
    
    ```
    sudo apt update
    sudo apt install python3-venv
    ```
    
- **Now, create your isolated environment:**
    
    Bash
    
    ```
    python3 -m venv venv
    ```
    
- **Activate it:**
    
    Bash
    
    ```
    source venv/bin/activate
    ```
        
- **Install the libraries:** `pip install -r requirements.txt`
    

### Step 3: The C++ Environment (`platformio.ini`)

Since you are using an ESP32-S3, we must configure PlatformIO to pull the correct libraries and set the architecture.

In your `3_ESP32_Firmware` folder, replace the contents of your `platformio.ini` with this configuration:

Ini, TOML

```
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

; Serial Monitor Options
monitor_speed = 115200
monitor_filters = time, esp32_exception_decoder

; Library Dependencies
lib_deps =
    ; MQTT Communication (Highly stable, non-blocking)
    knolleary/PubSubClient @ ^2.8
    
    ; JSON Parsing (Crucial for reading waypoints)
    bblanchon/ArduinoJson @ ^6.21.3
    

```

Just ensure you copy your existing `mavlink` folder directly into `Maize_Planter_GCS/3_ESP32_Firmware/lib/`. PlatformIO is smart enough to automatically compile any source code it finds in that directory.
### Step 4: The Crucial Pre-requisite (The Network Bridge)

Before we write code for the individual modules, you must prepare the physical network bridge.

1. **The Shared Network:** Your laptop (running Mosquitto and FastAPI) and the ESP32 must be connected to the **exact same Wi-Fi network** (e.g., your mobile hotspot).
    
2. **The Static IP:** The ESP32 needs to know where to send its data. Find your laptop's IPv4 address on this Wi-Fi network (e.g., `192.168.1.100`). **Write this down.** This IP address will be hardcoded into both the Frontend UI (to fetch waypoints) and the ESP32 (to publish telemetry to Mosquitto).
    
3. **Firewall Rules:** Your laptop's firewall will naturally block incoming connections. You must explicitly allow inbound traffic on **Port 1883 (MQTT)** and **Port 8000 (FastAPI)**. If you skip this, the ESP32 will repeatedly report a "Connection Refused" error.





### Unit Logic Verification (How to test this - path_planner)

FastAPI comes with a brilliant feature: it auto-generates a testing interface for you. You don't even need the frontend to test it!

1. **Start the Server:** Ensure your virtual environment is activated (`source venv/bin/activate`). Then run this command:
    
    Bash
    
    ```
    uvicorn main:app --reload
    ```
    
    _You should see text saying the application startup is complete and that MQTT is connected._
    
2. **Open the Testing Portal:** Open a web browser on your laptop and go to: **[http://127.0.0.1:8000/docs](http://127.0.0.1:8000/docs)**
    
3. **Execute a Test Request:**
    
    - Click on the green **`POST /api/plan_mission`** box to expand it.
        
    - Click the **"Try it out"** button on the right.
        
    - The portal will provide a pre-filled JSON box based on our `data_models.py`. Change the values to match our earlier test (Length: 20, Width: 5).
        
    - Click the blue **"Execute"** button.