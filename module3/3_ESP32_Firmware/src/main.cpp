#include <Arduino.h>
#include "telemetry_mqtt.h"
#include "waypoint_manager.h"

unsigned long lastTelemetryMillis = 0;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n====================================");
  Serial.println(" MAIZE-PRO GCS FIRMWARE: V-MODEL INTEGRATION ");
  Serial.println("====================================\n");

  // Initialize Modules
  setupNetwork();
  setupMavlink();
}

void loop()
{
  // 1. Maintain Wi-Fi and MQTT
  loopNetwork();

  // 2. Maintain MAVLink connection to Pixhawk
  loopMavlink();

  // 3. Publish Dummy Telemetry to Web UI
  unsigned long currentMillis = millis();
  if (currentMillis - lastTelemetryMillis >= 1000)
  {
    lastTelemetryMillis = currentMillis;
    publishDummyTelemetry(); // Note: We will replace this with real Pixhawk telemetry later
  }
}