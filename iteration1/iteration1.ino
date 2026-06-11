#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// --- PIN DEFINITIONS (Based on ESP32 DEVKIT V1) ---

// Left Motor (BTS7960 #1)
const int L_FWD_PIN = 26; // D26
const int L_REV_PIN = 25; // D25

// Right Motor (BTS7960 #2)
const int R_FWD_PIN = 33; // D33
const int R_REV_PIN = 32; // D32

// Seed Metering Mechanism
const int SEED_PIN = 2;   // D2 (Also Onboard LED)

// --- CONSTANTS ---
const int DEADBAND = 30;       // Ignore small stick movements
const int TURN_SPEED = 180;    // Speed of the active wheel during U-Turn (0-255)
const int CRUISE_SPEED = 150;  // Fixed speed for Cruise Control

// --- STATE VARIABLES ---
bool cruiseControlActive = false;
bool plantingActive = false;

// PWM Properties
const int freq = 5000;
const int resolution = 8; // 8-bit resolution (0-255)

// PWM Channels
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
  
  Serial.println("--- MAIZE PLANTER ITERATION 1 READY ---");
  Serial.println("Left Motor: D26/D25 | Right Motor: D33/D32");
}

void loop() {
  BP32.update();
  
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
  // PS4 Sticks are -511 to 512. Map to -255 to 255.
  int rawLy = ctl->axisY();  // Left Stick
  int rawRy = ctl->axisRY(); // Right Stick
  
  // Invert Stick Y because Up is negative on gamepads
  int stickL = map(rawLy, -512, 512, 255, -255); 
  int stickR = map(rawRy, -512, 512, 255, -255);

  bool btnL1 = ctl->l1();
  bool btnR1 = ctl->r1();
  bool btnCross = ctl->a();    // Cruise
  bool btnCircle = ctl->b();   // Planting
  bool btnBox = ctl->x();      // Brake/Stop

  // --- 1. BRAKE / RESET LOGIC (Highest Priority) ---
  if (btnBox) {
    stopAllMotors();
    cruiseControlActive = false;
    plantingActive = false;
    digitalWrite(SEED_PIN, LOW);
    Serial.println(">> BRAKE PRESSED: ALL STOP <<");
    return; 
  }

  // --- 2. MODE ACTIVATION ---
  if (btnCross && !cruiseControlActive) {
    cruiseControlActive = true;
    Serial.println(">> CRUISE CONTROL: ON <<");
  }
  
  if (btnCircle && !plantingActive) {
    plantingActive = true;
    digitalWrite(SEED_PIN, HIGH);
    Serial.println(">> PLANTING MECHANISM: ON <<");
  }

  // --- 3. DRIVE LOGIC ---
  if (btnL1) {
    // Left U-Turn: Lock Left, Drive Right
    driveRaw(ch_L_FWD, ch_L_REV, 0);          
    driveRaw(ch_R_FWD, ch_R_REV, TURN_SPEED); 
    Serial.println("Turning LEFT (Left Locked)");
    
  } else if (btnR1) {
    // Right U-Turn: Lock Right, Drive Left
    driveRaw(ch_L_FWD, ch_L_REV, TURN_SPEED); 
    driveRaw(ch_R_FWD, ch_R_REV, 0);          
    Serial.println("Turning RIGHT (Right Locked)");
    
  } else if (cruiseControlActive) {
    // Cruise Control Mode
    driveRaw(ch_L_FWD, ch_L_REV, CRUISE_SPEED);
    driveRaw(ch_R_FWD, ch_R_REV, CRUISE_SPEED);
    
  } else {
    // Manual Tank Drive
    if (abs(stickL) < DEADBAND) driveRaw(ch_L_FWD, ch_L_REV, 0);
    else driveRaw(ch_L_FWD, ch_L_REV, stickL);

    if (abs(stickR) < DEADBAND) driveRaw(ch_R_FWD, ch_R_REV, 0);
    else driveRaw(ch_R_FWD, ch_R_REV, stickR);
  }
}

// --- HELPER: Drive Motor ---
// Speed: -255 (Reverse) to 255 (Forward)
void driveRaw(int ch_FWD, int ch_REV, int speed) {
  if (speed == 0) {
    ledcWrite(ch_FWD, 0);
    ledcWrite(ch_REV, 0);
  } else if (speed > 0) {
    ledcWrite(ch_FWD, speed); // Forward PWM
    ledcWrite(ch_REV, 0);     // Reverse LOW
  } else {
    ledcWrite(ch_FWD, 0);     // Forward LOW
    ledcWrite(ch_REV, abs(speed)); // Reverse PWM
  }
}

void stopAllMotors() {
  driveRaw(ch_L_FWD, ch_L_REV, 0);
  driveRaw(ch_R_FWD, ch_R_REV, 0);
}

// --- CONTROLLER CALLBACKS ---
void onConnectedController(ControllerPtr ctl) {
  bool found = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      myControllers[i] = ctl;
      found = true;
      Serial.println("Controller Connected");
      ctl->setColorLED(0, 0, 255); // Blue Light
      break;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      myControllers[i] = nullptr;
      stopAllMotors();
      Serial.println("Controller Disconnected");
      break;
    }
  }
}