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