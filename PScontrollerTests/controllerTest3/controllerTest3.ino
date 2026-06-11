#include <PS2X_lib.h>

/******************************************************************
 * MAIZE PLANTER - PS2 CONTROLLER (Safe Pin Config)
 * * WIRING:
 * DATA (Pin 1) -> GPIO 4   <-- NEW SAFE PIN
 * CMD  (Pin 2) -> GPIO 13
 * ATT  (Pin 6) -> GPIO 5
 * CLK  (Pin 7) -> GPIO 18
 * Power -> External 3.3V (Ensure Common Ground!)
 * GND   -> ESP32 GND
 ******************************************************************/

PS2X ps2x; 

// --- UPDATED PIN DEFINITIONS ---
#define PS2_DAT 4   // Moved from 19 to 4 to avoid USB conflict
#define PS2_CMD 13  
#define PS2_SEL 5
#define PS2_CLK 18

#define pressures   false
#define rumble      false

int error = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println("Maize Planter: Connecting to PS2 Controller...");

  // Try to configure the controller
  // The 'false, false' disables pressure sensitivity and rumble to save power/data
  error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);

  if(error == 0){
    Serial.println("SUCCESS: Found Controller!");
  }  
  else if(error == 1)
    Serial.println("ERROR: No controller found. Check GROUND connection and WIRING.");
  else if(error == 2)
    Serial.println("ERROR: Controller found but not accepting commands.");
  else if(error == 3)
    Serial.println("ERROR: Controller refusing to enter Pressures mode.");
}

void loop() {
  if(error == 1) { 
    delay(500); // Wait and try again if disconnected
    ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);
    return; 
  }

  // Read the controller
  ps2x.read_gamepad(false, 0);

  // --- READ JOYSTICKS ---
  // Left Stick: Y (Up/Down), X (Left/Right)
  // Right Stick: Y (Up/Down), X (Left/Right)
  int LY = ps2x.Analog(PSS_LY); 
  int LX = ps2x.Analog(PSS_LX); 
  int RY = ps2x.Analog(PSS_RY); 
  int RX = ps2x.Analog(PSS_RX); 

  // --- READ BUTTONS ---
  // Note: 0 = Released, 1 = Pressed
  bool btnX = ps2x.Button(PSB_BLUE);      // X Button (Cross)
  bool btnCircle = ps2x.Button(PSB_RED);  // Circle
  bool btnTri = ps2x.Button(PSB_GREEN);   // Triangle
  bool btnSquare = ps2x.Button(PSB_PINK); // Square
  bool btnL1 = ps2x.Button(PSB_L1);
  bool btnR1 = ps2x.Button(PSB_R1);

  // --- PRINT DATA ---
  // Format: LY, LX, RY, RX | Buttons
  Serial.print("LeftStick Y:"); Serial.print(LY);
  Serial.print(" X:"); Serial.print(LX);
  Serial.print(" | RightStick Y:"); Serial.print(RY);
  Serial.print(" X:"); Serial.print(RX);
  
  Serial.print(" | BTN: ");
  if(btnSquare) Serial.print("[SQ] ");
  if(btnTri)    Serial.print("[TRI] ");
  if(btnCircle) Serial.print("[CIR] ");
  if(btnX)      Serial.print("[X] ");
  if(btnL1)     Serial.print("[L1] ");
  if(btnR1)     Serial.print("[R1] ");

  Serial.println(""); 
  
  delay(50); 
}