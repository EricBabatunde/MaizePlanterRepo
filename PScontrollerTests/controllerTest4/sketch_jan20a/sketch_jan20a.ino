#include <PS2X_lib.h>  //for v1.6

/* =========================================================================
 * MAIZE PLANTER: PS2 CONTROLLER DIAGNOSTIC (ARDUINO UNO VERSION)
 * PLATFORM: Arduino Uno
 * PURPOSE: Read all Analog Sticks and Buttons (Event-Based / No Spam).
 * ========================================================================= */

PS2X ps2x; // create PS2 Controller Class

int error = 0; 
byte type = 0;
byte vibrate = 0;

// Variables to store previous stick values for change detection
int oldLX = 0, oldLY = 0, oldRX = 0, oldRY = 0;

// --- PINOUT CONFIGURATION (ARDUINO UNO) ---
#define PS2_DAT  13  // Brown
#define PS2_CMD  11  // Orange
#define PS2_SEL  10  // Yellow (Attention)
#define PS2_CLK  12  // Blue

void setup(){
  Serial.begin(57600); 
  delay(300); 
   
  Serial.println("Maize Planter: Initializing Controller (Event Mode)...");

  // RETRY LOOP
  while(true) {
    error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
    
    if(error == 0){
      Serial.println("SUCCESS: Controller Found!");
      Serial.println("Waiting for input (Press any button or move sticks)...");
      break;
    }
    else {
      Serial.print("Error: "); Serial.println(error);
      Serial.println("Retrying... (Check Wiring)");
      delay(1000);
    }
  }
}

void loop() {
  // 1. Read Controller
  ps2x.read_gamepad(false, 0); 

  // --- HANDLE ANALOG STICKS (Print only on change) ---
  int LY = ps2x.Analog(PSS_LY);
  int LX = ps2x.Analog(PSS_LX);
  int RY = ps2x.Analog(PSS_RY); 
  int RX = ps2x.Analog(PSS_RX); 

  // Check if values changed by more than 2 (Jitter threshold)
  if (abs(LX - oldLX) > 2 || abs(LY - oldLY) > 2 || 
      abs(RX - oldRX) > 2 || abs(RY - oldRY) > 2) {
    
    Serial.print("Sticks Moved -> L:[");
    Serial.print(LX); Serial.print(","); Serial.print(LY);
    Serial.print("] R:[");
    Serial.print(RX); Serial.print(","); Serial.print(RY);
    Serial.println("]");
    
    // Update old values
    oldLX = LX; oldLY = LY; oldRX = RX; oldRY = RY;
  }

  // --- HANDLE BUTTONS (Print only on Press) ---
  // NewButtonState() is true if ANY button changed state
  if (ps2x.NewButtonState()) {
    
    // Shapes
    if(ps2x.ButtonPressed(PSB_GREEN))      Serial.println("Pressed: TRIANGLE");
    if(ps2x.ButtonPressed(PSB_RED))        Serial.println("Pressed: CIRCLE");
    if(ps2x.ButtonPressed(PSB_BLUE))       Serial.println("Pressed: X");
    if(ps2x.ButtonPressed(PSB_PINK))       Serial.println("Pressed: SQUARE");

    // Shoulders
    if(ps2x.ButtonPressed(PSB_L1))         Serial.println("Pressed: L1");
    if(ps2x.ButtonPressed(PSB_R1))         Serial.println("Pressed: R1");
    if(ps2x.ButtonPressed(PSB_L2))         Serial.println("Pressed: L2");
    if(ps2x.ButtonPressed(PSB_R2))         Serial.println("Pressed: R2");

    // Central
    if(ps2x.ButtonPressed(PSB_SELECT))     Serial.println("Pressed: SELECT");
    if(ps2x.ButtonPressed(PSB_START))      Serial.println("Pressed: START");

    // D-Pad
    if(ps2x.ButtonPressed(PSB_PAD_UP))     Serial.println("Pressed: UP");
    if(ps2x.ButtonPressed(PSB_PAD_RIGHT))  Serial.println("Pressed: RIGHT");
    if(ps2x.ButtonPressed(PSB_PAD_DOWN))   Serial.println("Pressed: DOWN");
    if(ps2x.ButtonPressed(PSB_PAD_LEFT))   Serial.println("Pressed: LEFT");

    // Stick Buttons
    if(ps2x.ButtonPressed(PSB_L3))         Serial.println("Pressed: L3");
    if(ps2x.ButtonPressed(PSB_R3))         Serial.println("Pressed: R3");
  }
  
  delay(50); 
}