### **Stage 2: The Nervous System (The Passthrough Wiring)**

Once Stage 1 is verified (the HUD moves when you move the PixHawk), we connect the ESP32 to "hear" the PixHawk's steering intent.

#### **2.1 Hardware: Servo Rail to ESP32**

The PixHawk output pins on the "Servo Rail" (the 3-pin headers at the back).

- **Steering (Output 1):** Signal pin (bottom row) to **ESP32 GPIO 18**.
    
- **Throttle (Output 3):** Signal pin (bottom row) to **ESP32 GPIO 19**.
    
- **Ground:** Connect any GND pin on the PixHawk Servo Rail to an **ESP32 GND** pin.
    

#### **2.2 Software: Skid-Steer Configuration**

In Mission Planner, we need to tell the PixHawk that it is driving a "Tank" (Skid-Steer) even though it is outputting standard steering/throttle signals.

1. Go to `CONFIG` > `Full Parameter List`.
    
2. Search for **`SERVO1_FUNCTION`**: Set to `73` (Throttle Left).
    
3. Search for **`SERVO3_FUNCTION`**: Set to `74` (Throttle Right).
    
4. Search for **`PILOT_STEER_TYPE`**: Set to `0` (Default) or `1` depending on your preference.
    
5. Click **Write Params**.
    

### **2.3. Hardware: The PixHawk Servo Rail Pinout**

The "Servo Rail" is the block of 3-row pins at the back of the PixHawk. Think of it as the vehicle's output interface.

#### **The 3-Row Architecture**

Regardless of whether a pin is labelled 1–8 (MAIN) or 1–6 (AUX), the vertical orientation is always the same:

- **Top Row (-):** Common Ground (GND).
    
- **Middle Row (+):** Power Rail (typically 5V). **Note:** This rail is usually _unpowered_ unless you provide 5V to it separately; however, for our setup, we will leave this row **empty**.
    
- **Bottom Row (S):** Signal (PWM). This is where the actual "commands" live.
    

#### **The Labels: MAIN vs AUX**

- **MAIN OUT (1–8):** These are the primary outputs controlled by the ArduRover firmware. We will use **Pin 1** and **Pin 3**.
    
- **AUX OUT (1–6):** Usually reserved for secondary functions (relays, camera triggers). We will ignore these for now.
    
- **RCIN / SBUS:** These are _inputs_ for traditional RC receivers. Since the ESP32 is our "receiver", we won't plug anything here yet.
    

#### **The Connection Protocol**

You will connect **three** wires, not two. Signal integrity is impossible without a common reference.

1. **Wire 1:** MAIN OUT **Pin 1 (Bottom/Signal)** $\rightarrow$ ESP32 **GPIO 18** (Steering Intent).
    
2. **Wire 2:** MAIN OUT **Pin 3 (Bottom/Signal)** $\rightarrow$ ESP32 **GPIO 19** (Throttle Intent).
    
3. **Wire 3:** Any Top Row (-) Pin** $\rightarrow$ ESP32 **GND**.
    

### **2.4. Software Logic: Throttle and Steering Integration**

To make the planter move smoothly, we have to decide _where_ the "mixing" happens. ArduRover can output pre-mixed "Left Motor/Right Motor" signals, but for a Smart Planter with a PS4 override, it is much better to receive **Raw Intent** (Throttle and Steering) and let the ESP32 do the mixing.

#### **The Configuration (Mission Planner)**

We will set the PixHawk to output "Ground Steering" and "Throttle" as independent channels:

- **`SERVO1_FUNCTION = 26` (GroundSteering):** This tells the PixHawk that Pin 1 is for turning.
    
- **`SERVO3_FUNCTION = 70` (Throttle):** This tells the PixHawk that Pin 3 is for forward/backward speed.
    
- **`PILOT_STEER_TYPE = 0`:** This ensures the PixHawk treats the vehicle as a standard coordinated-steering rover for its internal calculations, leaving the differential "Skid-Steer" logic to our ESP32.
    

### **2.5. Passthrough Verification Code**

This version adds a "Deadzone" and "Failsafe" check. If the Pixhawk is disconnected or disarmed, the ESP32 will report $0$ to prevent the planter from crawling due to signal noise.

C++

```
/* * PROJECT: Autonomous Smart Maize Planter
 * MODULE: Stage 2 - Navigation Passthrough Logic
 * GOAL: Verify Pixhawk PWM inputs and map to Normalised Intent (-100 to 100)
 */

// Hardware Pin Definitions
const int PIN_STEER    = 18; // Pixhawk Main Out 1
const int PIN_THROTTLE = 19; // Pixhawk Main Out 3

// Calibration Constants (Standard ArduPilot PWM)
const int PWM_MIN     = 1000;
const int PWM_NEUTRAL = 1500;
const int PWM_MAX     = 2000;
const int DEADZONE    = 25;  // Ignore minor fluctuations around 1500

void setup() {
  Serial.begin(115200);
  pinMode(PIN_STEER, INPUT);
  pinMode(PIN_THROTTLE, INPUT);
  
  Serial.println("--- MECHATRONICS TEST-BENCH: STAGE 2 ---");
  Serial.println("Initialising PWM Passthrough Listener...");
}

void loop() {
  // 1. Capture Raw Microseconds
  long rawSteer = pulseIn(PIN_STEER, HIGH, 25000);    // 25ms timeout
  long rawThrottle = pulseIn(PIN_THROTTLE, HIGH, 25000);

  // 2. Failsafe: Check if signal exists
  if (rawSteer == 0 || rawThrottle == 0) {
    Serial.println("CRITICAL: No PWM Signal Detected. Check Wiring/Arming Status.");
  } else {
    // 3. Normalise values to -100 (Full Left/Rev) to 100 (Full Right/Fwd)
    int normSteer = map(rawSteer, PWM_MIN, PWM_MAX, -100, 100);
    int normThrottle = map(rawThrottle, PWM_MIN, PWM_MAX, -100, 100);

    // 4. Apply Deadzone Logic
    if (abs(normSteer) < (DEADZONE / 5)) normSteer = 0;
    if (abs(normThrottle) < (DEADZONE / 5)) normThrottle = 0;

    // 5. Output for Engineering Verification
    Serial.print("RAW [S:"); Serial.print(rawSteer);
    Serial.print(" T:"); Serial.print(rawThrottle);
    Serial.print("] | INTENT [Steer:"); Serial.print(normSteer);
    Serial.print("% Throttle:"); Serial.print(normThrottle);
    Serial.println("%]");
  }

  delay(100); // 10Hz Refresh Rate for Serial Debugging
}
```

