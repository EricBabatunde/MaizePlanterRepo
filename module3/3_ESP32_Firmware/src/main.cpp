#include <Arduino.h>
#include "telemetry_mqtt.h"
#include "waypoint_manager.h"
#include "motor_control.h" // NEW INCLUSION

unsigned long lastTelemetryMillis = 0;

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(0);
  delay(1000);

  Serial.println("\n====================================");
  Serial.println(" MAIZE-PRO GCS FIRMWARE: STAGE 3 FINAL ");
  Serial.println("====================================\n");

  // Initialize Modules
  setupNetwork();
  setupMavlink();
  setupMotors(); // NEW: Initialise the interrupts and drivers

  // Enable the drivers after setup is complete
  digitalWrite(PIN_EN, HIGH);
}

void loop()
{
  // 1. Maintain Wi-Fi and MQTT
  loopNetwork();

  // 2. Maintain MAVLink connection to Pixhawk
  loopMavlink();

  // 3. Update Motor PWMs (Lightning Fast, Non-Blocking)
  loopMotors();

  // 4. Publish LIVE Telemetry to Web UI (1 Hz)
  unsigned long currentMillis = millis();
  if (currentMillis - lastTelemetryMillis >= 1000)
  {
    lastTelemetryMillis = currentMillis;

    // Pass the live variables extracted from MAVLink straight to MQTT
    publishLiveTelemetry("AUTO DRIVING", currentSpeed, currentHeading, currentLat, currentLon);
  }
}