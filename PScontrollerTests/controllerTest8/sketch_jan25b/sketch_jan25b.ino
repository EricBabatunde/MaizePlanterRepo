#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// --- VARIABLES FOR TRACKING CHANGES ---
// We use these to remember what the controller looked like 
// in the LAST loop. If nothing changes, we don't print.
int oldLx = 0, oldLy = 0, oldRx = 0, oldRy = 0;
bool oldCross = 0, oldCircle = 0, oldSquare = 0, oldTri = 0;
bool oldL1 = 0, oldR1 = 0;

// Threshold to ignore tiny stick jitters (Deadzone)
const int NOISE_THRESHOLD = 15; 

void setup() {
  Serial.begin(115200);
  
  // Setup Bluepad32
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys(); // Good for testing, ensures clean pairing
  BP32.enableVirtualDevice(false); // Disable mouse mode

  Serial.println("-----------------------------------------");
  Serial.println("MAIZE PLANTER CONTROL READY");
  Serial.println("Waiting for PS4 Controller (Share + PS)...");
  Serial.println("-----------------------------------------");
}

void loop() {
  // Update Bluetooth data
  bool dataUpdated = BP32.update();
  
  // If data updated, process it
  if (dataUpdated) {
    processControllers();
  }
  
  // Small delay to prevent CPU overload, but fast enough for smooth control
  delay(20); 
}

// --- CONTROLLER PROCESSING ---
void processControllers() {
  for (auto myController : myControllers) {
    if (myController && myController->isConnected() && myController->hasData()) {
      if (myController->isGamepad()) {
        processGamepad(myController);
      }
    }
  }
}

void processGamepad(ControllerPtr ctl) {
  // --- READ CURRENT VALUES ---
  
  // Sticks (Range approx -511 to 512)
  // 0 is Center. Negative is Left/Up, Positive is Right/Down
  int lx = ctl->axisX();
  int ly = ctl->axisY();
  int rx = ctl->axisRX();
  int ry = ctl->axisRY();

  // Buttons (Returns true if pressed)
  // Mappings: A=Cross, B=Circle, X=Square, Y=Triangle
  bool cross = ctl->a();
  bool circle = ctl->b();
  bool square = ctl->x();
  bool triangle = ctl->y();
  bool l1 = ctl->l1();
  bool r1 = ctl->r1();

  // --- CHECK FOR CHANGES (Event-Based Logic) ---
  // We check if ANY value has changed significantly since the last loop.
  
  bool hasChanged = false;

  // Check Buttons
  if (cross != oldCross || circle != oldCircle || square != oldSquare || triangle != oldTri || l1 != oldL1 || r1 != oldR1) {
    hasChanged = true;
  }

  // Check Sticks (Apply Deadzone/Threshold to ignore tiny jitters)
  if (abs(lx - oldLx) > NOISE_THRESHOLD || abs(ly - oldLy) > NOISE_THRESHOLD || 
      abs(rx - oldRx) > NOISE_THRESHOLD || abs(ry - oldRy) > NOISE_THRESHOLD) {
    hasChanged = true;
  }

  // --- PRINT ONLY IF CHANGED ---
  if (hasChanged) {
    String output = "";

    // Format Sticks: "LX: 100  LY: -200 ..."
    output += "LX:" + String(lx) + "\t";
    output += "LY:" + String(ly) + "\t";
    output += "RX:" + String(rx) + "\t";
    output += "RY:" + String(ry) + "\t| "; // Separator

    // Format Buttons: "X:1 O:0 ..."
    output += "X:" + String(cross) + " ";
    output += "O:" + String(circle) + " ";
    output += "Sq:" + String(square) + " ";
    output += "Tri:" + String(triangle) + " ";
    output += "L1:" + String(l1) + " ";
    output += "R1:" + String(r1);

    Serial.println(output);

    // Update "Old" values for the next comparison
    oldLx = lx; oldLy = ly; oldRx = rx; oldRy = ry;
    oldCross = cross; oldCircle = circle; oldSquare = square; oldTri = triangle;
    oldL1 = l1; oldR1 = r1;
  }
  
  // --- COLOR FEEDBACK (Optional) ---
  // Set LED to Green when Cross is pressed, otherwise Blue
  if (cross) ctl->setColorLED(0, 255, 0); // Green
  else ctl->setColorLED(0, 0, 255);       // Blue
}

// --- CONNECTION CALLBACKS (Required by Library) ---

void onConnectedController(ControllerPtr ctl) {
  bool foundEmptySlot = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      Serial.printf("CONTROLLER CONNECTED! (Index %d)\n", i);
      myControllers[i] = ctl;
      foundEmptySlot = true;
      
      // Set initial LED color to Blue
      ctl->setColorLED(0, 0, 255);
      break;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      Serial.printf("CONTROLLER DISCONNECTED (Index %d)\n", i);
      myControllers[i] = nullptr;
      break;
    }
  }
}