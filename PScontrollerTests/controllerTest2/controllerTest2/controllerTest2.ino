#include <PS2X_lib.h>  //for v1.6

/******************************************************************
 * MAIZE PLANTER - PS2 CONTROLLER TEST
 * * Wiring (ESP32-S3):
 * DATA (Pin 1) -> GPIO 19
 * CMD  (Pin 2) -> GPIO 13
 * ATT  (Pin 6) -> GPIO 5
 * CLK  (Pin 7) -> GPIO 18
 * Power -> 3.3V
 * GND   -> GND
 ******************************************************************/

PS2X ps2x; // create PS2 Controller Class

// --- PIN DEFINITIONS ---
#define PS2_DAT 19
#define PS2_CMD 13
#define PS2_SEL 5
#define PS2_CLK 18

// --- MODES ---
#define pressures   false
#define rumble      false

int error = 0;
byte type = 0;
byte vibrate = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);  // Give the receiver time to wake up

  Serial.println("Connecting to PS2 Controller...");

  // setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
  error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);

  if(error == 0){
    Serial.println("Found Controller, configured successful");
  }  
  else if(error == 1)
    Serial.println("No controller found, check wiring, see readme.txt to enable debug. visit www.billporter.info for troubleshooting tips");
  else if(error == 2)
    Serial.println("Controller found but not accepting commands. see readme.txt to enable debug. Visit www.billporter.info for troubleshooting tips");
  else if(error == 3)
    Serial.println("Controller refusing to enter Pressures mode, may not support it. ");

  type = ps2x.readType(); 
  switch(type) {
    case 0: Serial.println("Unknown Controller type found "); break;
    case 1: Serial.println("DualShock Controller found "); break;
    case 2: Serial.println("GuitarHero Controller found "); break;
	case 3: Serial.println("Wireless Sony DualShock Controller found "); break;
   }
}

void loop() {
  /* You must Read Gamepad to get new values and set vibration values
     ps2x.read_gamepad(small motor on/off, larger motor strenght from 0-255)
     if you don't enable the rumble, use ps2x.read_gamepad(false, 0);
  */
  
  if(error == 1) { // Skip loop if no controller found
    delay(500);
    ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble); // Try to reconnect
    return; 
  }

  ps2x.read_gamepad(false, 0); // Read controller

  // --- READ JOYSTICKS ---
  // Values range from 0 to 255. Center is approx 128.
  int LY = ps2x.Analog(PSS_LY); // Left Stick Y (Up/Down)
  int LX = ps2x.Analog(PSS_LX); // Left Stick X (Left/Right)
  int RY = ps2x.Analog(PSS_RY); // Right Stick Y
  int RX = ps2x.Analog(PSS_RX); // Right Stick X

  // --- READ BUTTONS ---
  bool btnX = ps2x.Button(PSB_BLUE);      // X Button (Cross)
  bool btnCircle = ps2x.Button(PSB_RED);  // Circle
  bool btnTri = ps2x.Button(PSB_GREEN);   // Triangle
  bool btnSquare = ps2x.Button(PSB_PINK); // Square

  // --- PRINT DATA TO SERIAL MONITOR ---
  Serial.print("L_Stick Y: "); Serial.print(LY);
  Serial.print(" | L_Stick X: "); Serial.print(LX);
  Serial.print(" || R_Stick Y: "); Serial.print(RY);
  Serial.print(" | R_Stick X: "); Serial.print(RX);
  
  Serial.print(" || Buttons: ");
  if(btnSquare) Serial.print("[SQUARE] ");
  if(btnTri)    Serial.print("[TRIANGLE] ");
  if(btnCircle) Serial.print("[CIRCLE] ");
  if(btnX)      Serial.print("[X] ");

  Serial.println(""); // New line
  
  delay(50); // Small delay to keep serial readable
}