#include <PS2X_lib.h>

/* =========================================================================
 * MAIZE PLANTER: CONTROLLER SPY TEST
 * PURPOSE: Light up LED 13 when 'X' is pressed to prove connection.
 * ========================================================================= */

PS2X ps2x;
int error = 0;

// PS2 Pins
#define PS2_DAT 13  
#define PS2_CMD 11
#define PS2_SEL 10
#define PS2_CLK 12

// Debug LED (Uno Built-in is Pin 13, but PS2 uses 13 for Data!)
// CRITICAL CONFLICT: PS2 uses Pin 13. We CANNOT use the built-in LED (13).
// We must use an external LED or a different Pin.
// DO YOU HAVE AN EXTERNAL LED? If not, we will use Pin 8.
#define DEBUG_LED 8 

void setup() {
  Serial.begin(115200);
  pinMode(DEBUG_LED, OUTPUT);
  delay(300);

  // Setup Controller
  while(true) {
    error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
    if(error == 0) {
      // Flash LED 3 times to say "Connected"
      for(int i=0; i<3; i++) {
        digitalWrite(DEBUG_LED, HIGH); delay(200);
        digitalWrite(DEBUG_LED, LOW); delay(200);
      }
      break;
    }
    delay(500);
  }
}

void loop() {
  ps2x.read_gamepad(false, 0);

  // --- SPY TEST ---
  // If X button is held, turn on LED 8
  if(ps2x.Button(PSB_BLUE)) {
    digitalWrite(DEBUG_LED, HIGH);
  } else {
    digitalWrite(DEBUG_LED, LOW);
  }

  // Still send data so we don't break the system
  int LY = ps2x.Analog(PSS_LY);
  int RY = ps2x.Analog(PSS_RY);
  int btnCode = 0;
  if(ps2x.Button(PSB_BLUE)) btnCode = 1;
  
  Serial.print("<");
  Serial.print(LY); Serial.print(",");
  Serial.print(RY); Serial.print(",");
  Serial.print(btnCode);
  Serial.println(">");
  
  delay(20);
}