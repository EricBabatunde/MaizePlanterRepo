#include <Arduino.h>

/* =========================================================================
 * MAIZE PLANTER: FINAL MANUAL PHASE DRIVER
 * ========================================================================= */

// --- PIN DEFINITIONS ---
#define RX_PIN 18 
#define TX_PIN 17

// Motor 1: LEFT DRIVE
#define L_PWM_F 4
#define L_PWM_R 5

// Motor 2: RIGHT DRIVE
#define R_PWM_F 6
#define R_PWM_R 7

// Motor 3: STEERING (Press Wheel)
#define S_PWM_L 15
#define S_PWM_R 16

// Motor 4: METERING
#define M_PWM_F 8
#define M_PWM_R 3

// --- SETTINGS ---
#define DEADZONE 10     // Reduced deadzone for better response
#define CRUISE_SPEED 180
#define TURN_SPEED 150  // Speed for U-Turns

// Global Variables
int inputLY = 128; 
int inputRY = 128; 
int btnCode = 0;
bool cruiseMode = false;

// Serial Parsing
const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;

// LED for Debug
#define DEBUG_LED 2 // Use 48 if your board is different

// Prototypes
void setMotor(int pinF, int pinR, int val);
void recvWithStartEndMarkers();
void parseData();

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  pinMode(DEBUG_LED, OUTPUT);

  // Setup PWM
  ledcAttach(L_PWM_F, 15000, 8); ledcAttach(L_PWM_R, 15000, 8);
  ledcAttach(R_PWM_F, 15000, 8); ledcAttach(R_PWM_R, 15000, 8);
  ledcAttach(S_PWM_L, 15000, 8); ledcAttach(S_PWM_R, 15000, 8);
  ledcAttach(M_PWM_F, 15000, 8); ledcAttach(M_PWM_R, 15000, 8);
}

void loop() {
  // 1. Receive Data
  recvWithStartEndMarkers();
  
  if (newData == true) {
    parseData();
    newData = false;
    // Toggle LED every time we get data to show "Heartbeat"
    digitalWrite(DEBUG_LED, !digitalRead(DEBUG_LED)); 
  }

  // 2. Map Inputs
  // Map 0-255 -> Speed 255 to -255 (Inverted because PS2 0 is UP)
  int leftSpeed = map(inputLY, 0, 255, 255, -255);
  int rightSpeed = map(inputRY, 0, 255, 255, -255);
  
  // 3. Deadzone Filter
  if (abs(leftSpeed) < DEADZONE) leftSpeed = 0;
  if (abs(rightSpeed) < DEADZONE) rightSpeed = 0;

  // 4. Button Logic (Macros)
  // 1=Cruise(X), 2=Left U(L1), 3=Right U(R1), 4=Brake(Tri), 5=Meter(Sq), 6=Act(Cir)

  if (btnCode == 4) { // BRAKE (Triangle)
    cruiseMode = false;
    leftSpeed = 0;
    rightSpeed = 0;
  }
  
  if (btnCode == 1) { // CRUISE TOGGLE (X)
     cruiseMode = !cruiseMode;
     btnCode = 0; // Reset button so it doesn't flicker
  }

  // -- MACRO OVERRIDES --
  if (btnCode == 2) { // U-TURN LEFT (L1)
     leftSpeed = -TURN_SPEED; 
     rightSpeed = TURN_SPEED;
     cruiseMode = false;
  } 
  else if (btnCode == 3) { // U-TURN RIGHT (R1)
     leftSpeed = TURN_SPEED; 
     rightSpeed = -TURN_SPEED;
     cruiseMode = false;
  }
  else if (cruiseMode) {
     leftSpeed = CRUISE_SPEED;
     rightSpeed = CRUISE_SPEED;
  }

  // -- METERING (Square) --
  // Run metering motor while holding Square
  if (btnCode == 5) setMotor(M_PWM_F, M_PWM_R, 200);
  else setMotor(M_PWM_F, M_PWM_R, 0);
  
  // -- ACTUATOR (Circle) --
  // Placeholder logic for now
  // if (btnCode == 6) ...

  // 5. DRIVE MOTORS
  setMotor(L_PWM_F, L_PWM_R, leftSpeed);
  setMotor(R_PWM_F, R_PWM_R, rightSpeed);

  // 6. STEERING ASSIST (Differential)
  // Calculate difference
  int diff = leftSpeed - rightSpeed;
  
  // Steer into the turn
  if (diff > 40) setMotor(S_PWM_L, S_PWM_R, 200);       // Turning Right -> Steer Right
  else if (diff < -40) setMotor(S_PWM_L, S_PWM_R, -200);  // Turning Left -> Steer Left
  else setMotor(S_PWM_L, S_PWM_R, 0);                     // Straight -> Center

  delay(10); // Loop stability
}

// --- HELPERS ---
void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial1.available() > 0 && newData == false) {
    rc = Serial1.read();
    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) ndx = numChars - 1;
      } else {
        receivedChars[ndx] = '\0';
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    } else if (rc == startMarker) recvInProgress = true;
  }
}

void parseData() {
  char * strtokIndx; 
  strtokIndx = strtok(receivedChars, ",");      
  inputLY = atoi(strtokIndx);
  strtokIndx = strtok(NULL, ","); 
  inputRY = atoi(strtokIndx);
  strtokIndx = strtok(NULL, ","); 
  btnCode = atoi(strtokIndx);
}

void setMotor(int pinF, int pinR, int val) {
  if (val > 0) {
    ledcWrite(pinF, val);
    ledcWrite(pinR, 0);
  } else if (val < 0) {
    ledcWrite(pinF, 0);
    ledcWrite(pinR, abs(val));
  } else {
    ledcWrite(pinF, 0);
    ledcWrite(pinR, 0);
  }
}