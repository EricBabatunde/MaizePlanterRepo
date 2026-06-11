#include "EspUsbHost.h"

// Check your board specs. Usually 2 or 48.
#define LED_BUILTIN 2
#define LED_PIN_2 48 

class MyEspUsbHost : public EspUsbHost {
  void onConfig(uint8_t addr, const uint8_t* config_desc) {
    // SUCCESS! A device was physically detected.
    digitalWrite(LED_BUILTIN, HIGH); 
    digitalWrite(LED_PIN_2, HIGH);
  }

  void onReady(uint8_t addr) {
    // SUCCESS! Device is ready to talk.
    // Blink fast to show "Ready"
    for(int i=0; i<5; i++) {
        digitalWrite(LED_BUILTIN, LOW); digitalWrite(LED_PIN_2, LOW);
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH); digitalWrite(LED_PIN_2, HIGH);
        delay(100);
    }
  }

  void onHID(uint8_t addr, const uint8_t* data, uint16_t len) {
    // Data is flowing!
  }
};

MyEspUsbHost usbHost;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  
  // Turn OFF initially
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_PIN_2, LOW);

  usbHost.begin();
}

void loop() {
  usbHost.task();
}