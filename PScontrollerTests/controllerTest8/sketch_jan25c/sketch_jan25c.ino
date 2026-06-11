#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// --- PIN DEFINITIONS (BTS7960 Setup) ---
// Note: Connect R_EN and L_EN on BTS7960 to +5V

// Left Motor (BTS7960)
const int L_FWD_PIN = 25; // Connect to RPWM
const int L_REV_PIN = 26; // Connect to LPWM

// Right Motor (BTS7960)
const int R_FWD_PIN = 27; // Connect to RPWM
const int R_REV_PIN = 14; // Connect to LPWM

// Steering Motor (BTS7960) - Unused in Iteration 1 Logic
const int S_FWD_PIN = 12; 
const int S_REV_PIN = 13;

// Seed Metering Motor (MOSFET/Relay)
const int SEED_PIN = 2; 

// --- CONSTANTS ---
const int DEADBAND = 30;       // Ignore small stick movements
const int TURN_SPEED = 180;    // Speed of the active wheel during U-Turn
const int CRUISE_SPEED = 150;  // Fixed speed for Cruise Control

// --- STATE VARIABLES ---
bool cruiseControlActive = false;
bool plantingActive = false;

// PWM Properties for ESP32
const int freq = 5000;
const int resolution = 8; // 8-bit resolution (0-255)

// PWM Channels (ESP32 ledc)
const int ch_L_FWD = 0;
const int ch_L_REV = 1;
const int ch_R_FWD = 2;
const int ch_R_REV = 3;

void setup() {
  Serial.begin(115200);

  // Setup Seed Pin
  pinMode(SEED_PIN, OUTPUT);
  digitalWrite(SEED_PIN, LOW);

  // Setup PWM for Left Motor
  ledcSetup(ch_L_FWD, freq, resolution);
  ledcAttachPin(L_FWD_PIN, ch_L_FWD);
  ledcSetup(ch_L_REV, freq, resolution);
  ledcAttachPin(L_REV_PIN, ch_L_REV);
  
  // Setup PWM for Right Motor
  ledcSetup(ch_R_FWD, freq, resolution);
  ledcAttachPin(R_FWD_PIN, ch_R_FWD);
  ledcSetup(ch_R_REV, freq, resolution);
  ledcAttachPin(R_REV_PIN, ch_R_REV);

  // Initialize Bluepad32
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys(); 
  BP32.enableVirtualDevice(false);
  
  Serial.println("--- MAIZE PLANTER ITERATION 1 (MANUAL + LOCKING) ---");
}

void loop() {
  BP32.update();
  
  // Process controllers if connected
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    ControllerPtr ctl = myControllers[i];
    if (ctl && ctl->isConnected() && ctl->hasData()) {
      processGameplay(ctl);
    }
  }
  delay(10);
}

void processGameplay(ControllerPtr ctl) {
  // --- INPUTS ---
  // PS4 Sticks are -511 to 512. We map them to -255 to 255.
  int rawLy = ctl->axisY();  // Left Stick Y
  int rawRy = ctl->axisRY(); // Right Stick Y
  
  // Invert Stick Y because usually Up is Negative on gamepads
  int stickL = map(rawLy, -512, 512, 255, -255); 
  int stickR = map(rawRy, -512, 512, 255, -255);

  bool btnL1 = ctl->l1();
  bool btnR1 = ctl->r1();
  
  // Control Buttons
  bool btnCross = ctl->a();    // Cruise Activate
  bool btnCircle = ctl->b();   // Planting Activate
  
  // STOP / BRAKE Button (Square "Box" Button)
  // This button cancels all active modes (Cruise, Planting, etc.)
  bool btnBox = ctl->x();      

  // --- 1. BRAKE / RESET LOGIC (Highest Priority) ---
  if (btnBox) {
    stopAllMotors();
    cruiseControlActive = false;
    plantingActive = false;
    digitalWrite(SEED_PIN, LOW);
    Serial.println(">> BRAKE PRESSED: ALL SYSTEMS STOP <<");
    return; // Skip the rest of the loop
  }

  // --- 2. MODE ACTIVATION (Latch Logic) ---
  
  // Cruise Control (Press Cross to Start, Box to Stop)
  if (btnCross && !cruiseControlActive) {
    cruiseControlActive = true;
    Serial.println(">> CRUISE CONTROL: ON <<");
  }
  
  // Planting Mechanism (Press Circle to Start, Box to Stop)
  if (btnCircle && !plantingActive) {
    plantingActive = true;
    digitalWrite(SEED_PIN, HIGH);
    Serial.println(">> PLANTING MECHANISM: ON <<");
  }

  // --- 3. DRIVE LOGIC (Mutually Exclusive) ---
  
  if (btnL1) {
    // --- LEFT U-TURN MODE (Iteration 1) ---
    // 1. (Iteration 2: Move Presswheel to 53 deg here)
    
    // 2. Lock Left Wheel, Drive Right Wheel
    driveRaw(ch_L_FWD, ch_L_REV, 0);          // Left STOP
    driveRaw(ch_R_FWD, ch_R_REV, TURN_SPEED); // Right FORWARD
    Serial.println("Turning LEFT (Left Locked)");
    
  } else if (btnR1) {
    // --- RIGHT U-TURN MODE (Iteration 1) ---
    // 1. (Iteration 2: Move Presswheel to 53 deg here)
    
    // 2. Lock Right Wheel, Drive Left Wheel
    driveRaw(ch_L_FWD, ch_L_REV, TURN_SPEED); // Left FORWARD
    driveRaw(ch_R_FWD, ch_R_REV, 0);          // Right STOP
    Serial.println("Turning RIGHT (Right Locked)");
    
  } else if (cruiseControlActive) {
    // --- CRUISE CONTROL MODE ---
    // Drives forward blindly.
    // Safety override: If sticks move significantly, cancel cruise? 
    // For now, per your request, it stays until Brake (Box) is pressed.
    driveRaw(ch_L_FWD, ch_L_REV, CRUISE_SPEED);
    driveRaw(ch_R_FWD, ch_R_REV, CRUISE_SPEED);
    
  } else {
    // --- MANUAL TANK DRIVE ---
    // Left Motor
    if (abs(stickL) < DEADBAND) driveRaw(ch_L_FWD, ch_L_REV, 0);
    else driveRaw(ch_L_FWD, ch_L_REV, stickL);

    // Right Motor
    if (abs(stickR) < DEADBAND) driveRaw(ch_R_FWD, ch_R_REV, 0);
    else driveRaw(ch_R_FWD, ch_R_REV, stickR);
  }
}

// --- HELPER: Drive BTS7960 ---
// Uses 2 PWM channels per motor.
// Speed: -255 (Max Reverse) to 255 (Max Forward)
void driveRaw(int ch_FWD, int ch_REV, int speed) {
  if (speed == 0) {
    ledcWrite(ch_FWD, 0);
    ledcWrite(ch_REV, 0);
  } else if (speed > 0) {
    ledcWrite(ch_FWD, speed);
    ledcWrite(ch_REV, 0);
  } else {
    ledcWrite(ch_FWD, 0);
    ledcWrite(ch_REV, abs(speed));
  }
}

void stopAllMotors() {
  driveRaw(ch_L_FWD, ch_L_REV, 0);
  driveRaw(ch_R_FWD, ch_R_REV, 0);
  // Steering motor stop (if connected)
  // digitalWrite(S_FWD_PIN, LOW); 
  // digitalWrite(S_REV_PIN, LOW);
}

// --- CONTROLLER CALLBACKS ---
void onConnectedController(ControllerPtr ctl) {
  bool found = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      myControllers[i] = ctl;
      found = true;
      Serial.println("Controller Connected");
      break;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      myControllers[i] = nullptr;
      stopAllMotors();
      Serial.println("Controller Disconnected - STOPPING");
      break;
    }
  }
}