#include <PS2X_lib.h>  //for v1.6

/* =========================================================================
 * MAIZE PLANTER: PS2 CARTESIAN READER (ARDUINO UNO)
 * PURPOSE: Read Joystick as X/Y coordinates (-128 to 127)
 * UP = Positive Y, RIGHT = Positive X
 * ========================================================================= */

PS2X ps2x; 
int error = 0; 
byte type = 0;
byte vibrate = 0;

// Store previous values to detect change
int oldLX = 0, oldLY = 0, oldRX = 0, oldRY = 0;

// --- PINOUT (ARDUINO UNO) ---
#define PS2_DAT  13  
#define PS2_CMD  11  
#define PS2_SEL  10  
#define PS2_CLK  12  

void setup(){
  Serial.begin(57600); 
  delay(300); 
  
  Serial.println("Maize Planter: Initializing Cartesian Mode...");

  // RETRY LOOP
  while(true) {
    error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
    
    if(error == 0){
      Serial.println("SUCCESS: Controller Found!");
      Serial.println("Output Format: [X, Y] where Center is 0,0");
      break;
    }
    else {
      Serial.print("Error: "); Serial.println(error);
      delay(1000);
    }
  }
}

void loop() {
  // 1. Read Controller
  // If reading fails, skip this loop (Helps with random disconnects)
  bool success = ps2x.read_gamepad(false, 0); 
  
  // You mentioned the dongle disconnects sometimes. 
  // If read_gamepad() fails, we shouldn't print garbage data.
  if (!success) {
    // Optional: You could try re-config here, but usually it reconnects itself
    delay(50);
    return; 
  }

  // --- RAW READ (0-255) ---
  int rawLY = ps2x.Analog(PSS_LY);
  int rawLX = ps2x.Analog(PSS_LX);
  int rawRY = ps2x.Analog(PSS_RY); 
  int rawRX = ps2x.Analog(PSS_RX); 

  // --- CARTESIAN MAPPING ---
  // Map 0..255 to -128..127
  // INVERT Y: Standard PS2 is 0=Up. We want 0=Down (-128), 255=Up (127)
  // Actually, standard is 0=Up. So:
  // Map(0, 255) -> (127, -128)  [Up is Positive]
  
  int cartLY = map(rawLY, 0, 255, 127, -128);
  int cartLX = map(rawLX, 0, 255, -128, 127);
  
  int cartRY = map(rawRY, 0, 255, 127, -128);
  int cartRX = map(rawRX, 0, 255, -128, 127);

  // Deadzone filter (ignore tiny jitter around 0)
  if (abs(cartLX) < 10) cartLX = 0;
  if (abs(cartLY) < 10) cartLY = 0;
  if (abs(cartRX) < 10) cartRX = 0;
  if (abs(cartRY) < 10) cartRY = 0;

  // --- DETECT CHANGE ---
  // Check against old values (using a small threshold of 2 to avoid spam)
  if (abs(cartLX - oldLX) > 2 || abs(cartLY - oldLY) > 2 || 
      abs(cartRX - oldRX) > 2 || abs(cartRY - oldRY) > 2) {
    
    Serial.print("Joy Move -> L:[");
    Serial.print(cartLX); Serial.print(", "); Serial.print(cartLY);
    Serial.print("] R:[");
    Serial.print(cartRX); Serial.print(", "); Serial.print(cartRY);
    Serial.println("]");
    
    oldLX = cartLX; oldLY = cartLY; oldRX = cartRX; oldRY = cartRY;
  }

  // --- BUTTONS (Event Based) ---
  if (ps2x.NewButtonState()) {
    if(ps2x.ButtonPressed(PSB_GREEN))      Serial.println("BTN: TRIANGLE");
    if(ps2x.ButtonPressed(PSB_RED))        Serial.println("BTN: CIRCLE");
    if(ps2x.ButtonPressed(PSB_BLUE))       Serial.println("BTN: X");
    if(ps2x.ButtonPressed(PSB_PINK))       Serial.println("BTN: SQUARE");
    if(ps2x.ButtonPressed(PSB_L1))         Serial.println("BTN: L1");
    if(ps2x.ButtonPressed(PSB_R1))         Serial.println("BTN: R1");
    if(ps2x.ButtonPressed(PSB_L2))         Serial.println("BTN: L2");
    if(ps2x.ButtonPressed(PSB_R2))         Serial.println("BTN: R2");
    if(ps2x.ButtonPressed(PSB_SELECT))     Serial.println("BTN: SELECT");
    if(ps2x.ButtonPressed(PSB_START))      Serial.println("BTN: START");
    if(ps2x.ButtonPressed(PSB_PAD_UP))     Serial.println("BTN: UP");
    if(ps2x.ButtonPressed(PSB_PAD_RIGHT))  Serial.println("BTN: RIGHT");
    if(ps2x.ButtonPressed(PSB_PAD_DOWN))   Serial.println("BTN: DOWN");
    if(ps2x.ButtonPressed(PSB_PAD_LEFT))   Serial.println("BTN: LEFT");
  }
  
  delay(50); 
}