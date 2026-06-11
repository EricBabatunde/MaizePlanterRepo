#include <PS2X_lib.h>

/* =========================================================================
 * MAIZE PLANTER: UNO TRANSMITTER (SAFE MODE)
 * ROLE: Robustly send data to ESP32, even if Controller is missing
 * ========================================================================= */

PS2X ps2x;
int error = 1; // Start with error state

// PS2 Pinout (Uno)
#define PS2_DAT 13
#define PS2_CMD 11
#define PS2_SEL 10
#define PS2_CLK 12

// LED for Status
#define STATUS_LED 8 // Using Pin 8 for LED since 13 is used by SPI/PS2

unsigned long lastAttempt = 0;

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  delay(1000); // Wait for ESP32 to boot first
}

void loop() {
  // 1. Try to Connect/Read Controller
  // If we had an error previously, try to re-config every 1 second
  if (error != 0) {
    if (millis() - lastAttempt > 1000) {
      lastAttempt = millis();
      // Try to Config
      error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
      
      if (error == 0) {
        // Success!
        digitalWrite(STATUS_LED, HIGH); 
      } else {
        digitalWrite(STATUS_LED, LOW);
      }
    }
    // While disconnected, send "Safe Center" values to ESP32
    // This keeps the link alive!
    Serial.println("<128,128,0>");
    delay(50);
    return;
  }

  // 2. Read Controller (If connected)
  bool success = ps2x.read_gamepad(false, 0);
  
  // If read fails, force a re-config next loop
  if (!success) {
    error = 1;
    return;
  }

  // 3. Get Values
  int LY = ps2x.Analog(PSS_LY);
  int RY = ps2x.Analog(PSS_RY);
  int btnCode = 0;

  if(ps2x.Button(PSB_BLUE))       btnCode = 1; // Cruise
  else if(ps2x.Button(PSB_L1))    btnCode = 2; // Left U
  else if(ps2x.Button(PSB_R1))    btnCode = 3; // Right U
  else if(ps2x.Button(PSB_GREEN)) btnCode = 4; // Brake
  else if(ps2x.Button(PSB_PINK))  btnCode = 5; // Actuator Up
  else if(ps2x.Button(PSB_RED))   btnCode = 6; // Actuator Down

  // 4. Send Packet
  Serial.print("<");
  Serial.print(LY); Serial.print(",");
  Serial.print(RY); Serial.print(",");
  Serial.print(btnCode);
  Serial.println(">");

  delay(20); // 50Hz Update
}