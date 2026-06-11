#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// This callback gets called when a new controller connects
void onConnectedController(ControllerPtr ctl) {
  bool foundEmptySlot = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      Serial.printf("CALLBACK: Controller is connected, index=%d\n", i);
      ControllerProperties properties = ctl->getProperties();
      Serial.printf("Controller model: %s, VID=0x%04x PID=0x%04x\n", 
                    ctl->getModelName().c_str(), properties.vendor_id, properties.product_id);
      myControllers[i] = ctl;
      foundEmptySlot = true;
      break;
    }
  }
  if (!foundEmptySlot) {
    Serial.println("CALLBACK: Controller connected, but no empty slot found");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  bool foundController = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
      myControllers[i] = nullptr;
      foundController = true;
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t* addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  // Setup the Bluepad32 callbacks
  BP32.setup(&onConnectedController, &onDisconnectedController);
  
  // "forgetBluetoothKeys()" prevents issues if you switch controllers often.
  // You can remove this line once everything is working perfectly.
  // BP32.forgetBluetoothKeys(); 
  
  Serial.println("Waiting for PS4 Controller... (Hold SHARE + PS Button)");
}

void loop() {
  // FIXED: calling update() without assigning it to a variable
  // This resolves the "void value not ignored" error
  BP32.update();
  
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    ControllerPtr ctl = myControllers[i];
    if (ctl && ctl->isConnected()) {
      processController(ctl);
    }
  }
}

void processController(ControllerPtr ctl) {
  // Get Stick Values (Range: -511 to 512)
  int axisLY = ctl->axisY(); // Left Stick Y (Speed)
  int axisRX = ctl->axisRX(); // Right Stick X (Steering)
  
  // Get Button States
  // 1 = Pressed, 0 = Released
  bool btnCross = ctl->a(); // On PS4, Cross is mapped to 'a' generic button
  bool btnCircle = ctl->b();
  
  // --- PRINT DEBUG INFO ---
  // Only print every 200ms to avoid spamming
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    Serial.printf("idx=%d, LY=%4d, RX=%4d, Cross=%d\n", 
                  ctl->index(), axisLY, axisRX, btnCross);
                  
    // Add your Motor Logic Here later!
    // Example: if (axisLY < -200) { DriveForward(); }
  }
}