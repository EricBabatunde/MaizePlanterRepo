#include <Arduino.h>

// ================= PIN DEFINITIONS =================
// Front LEFT Motor
const int FL_RPWM = 4;  // Forward
const int FL_LPWM = 5;  // Backward

// Front RIGHT Motor
const int FR_RPWM = 6;  // Forward
const int FR_LPWM = 7;  // Backward

// REAR Assist Motor
const int BK_RPWM = 15; // Forward
const int BK_LPWM = 16; // Backward

// SEED Metering Motor
const int SD_RPWM = 17; // Dispense
const int SD_LPWM = 18; // Reverse
// ===================================================

String inputString = "";

// Function to drive a single motor driver
void driveMotor(int pinFwd, int pinBwd, int speed) {
  speed = constrain(speed, -255, 255); // Safety limit

  if (speed > 0) {
    // Forward
    analogWrite(pinFwd, speed);
    analogWrite(pinBwd, 0);
  } else if (speed < 0) {
    // Backward
    analogWrite(pinFwd, 0);
    analogWrite(pinBwd, abs(speed));
  } else {
    // Stop / Brake
    analogWrite(pinFwd, 0);
    analogWrite(pinBwd, 0);
  }
}

void parseCommand(String data) {
  // Expected Format: <L,R,B,S> -> <255,255,0,0>
  
  // Clean markers
  data.replace("<", "");
  data.replace(">", "");

  int idx1 = data.indexOf(',');
  int idx2 = data.indexOf(',', idx1 + 1);
  int idx3 = data.indexOf(',', idx2 + 1);

  if (idx1 > 0 && idx2 > 0 && idx3 > 0) {
    int l_val = data.substring(0, idx1).toInt();
    int r_val = data.substring(idx1 + 1, idx2).toInt();
    int b_val = data.substring(idx2 + 1, idx3).toInt();
    int s_val = data.substring(idx3 + 1).toInt();

    driveMotor(FL_RPWM, FL_LPWM, l_val); // Left
    driveMotor(FR_RPWM, FR_LPWM, r_val); // Right
    driveMotor(BK_RPWM, BK_LPWM, b_val); // Rear (Only active if turning)
    driveMotor(SD_RPWM, SD_LPWM, s_val); // Seed
  }
}/////

void setup() {
  Serial.begin(115200);
  
  pinMode(FL_RPWM, OUTPUT); pinMode(FL_LPWM, OUTPUT);
  pinMode(FR_RPWM, OUTPUT); pinMode(FR_LPWM, OUTPUT);
  pinMode(BK_RPWM, OUTPUT); pinMode(BK_LPWM, OUTPUT);
  pinMode(SD_RPWM, OUTPUT); pinMode(SD_LPWM, OUTPUT);
}

void loop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    
    if (inChar == '<') {
      inputString = "<";
    } else if (inChar == '>') {
      inputString += ">";
      parseCommand(inputString);
      inputString = "";
    } else {
      inputString += inChar;
    }
  }
}