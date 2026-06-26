#include <Arduino.h>
#include "Config.h"
#include "MotorControl.h"
#include "MavlinkHandler.h"
#include "WebUI.h"
#include "DataLogger.h"

// --- Global Shared Variables ---
SystemState currentState = PIX_MANUAL;
float currentGroundSpeed = 0.0;
String currentPixhawkMode = "UNKNOWN";

int targetThrottleRC = 1500;
int targetSteerRC = 1500;

// --- Timers ---
unsigned long lastTelemetryBroadcast = 0;
unsigned long lastRCOverride = 0;
unsigned long lastDataLog = 0;

// --- Meter & Anti-Jam Variables ---
unsigned long meterStartTime = 0;
int jamDebounceCounter = 0;
int jamPhase = 0;
unsigned long jamStartTime = 0;
const int JAM_THRESHOLD_RAW = 3600;

void setup()
{
  Serial.begin(115200);
  initLogger();
  initMotors(); // Now ONLY initializes the Seed Metering Motor
  initMavlink();
  initWebUI();

  startValidationLog(); // Start a fresh log file on boot
}

void loop()
{
  // 1. Ingest incoming MAVLink packets (Fast as possible)
  receiveMavlink();

  // 2. Broadcast UI Telemetry (5Hz)
  if (millis() - lastTelemetryBroadcast > 200)
  {
    broadcastTelemetry();
    lastTelemetryBroadcast = millis();
  }

  // 3. Log Performance Data to Flash (2Hz)
  if (millis() - lastDataLog > 500)
  {
    float currentAmps = getTotalAmps();
    logValidationData(currentPixhawkMode, currentGroundSpeed, currentAmps);
    lastDataLog = millis();
  }

  // 4. Pixhawk RC Override Heartbeat (10Hz)
  // We MUST continuously stream RC commands, otherwise Pixhawk enters failsafe
  if (millis() - lastRCOverride > 100)
  {
    if (currentState == PIX_MANUAL || currentState == METER_TEST)
    {
      // Pass UI joysticks directly to Pixhawk
      sendRCOverride(targetSteerRC, targetThrottleRC);
    }
    else if (currentState == PIX_AUTO)
    {
      // Let Pixhawk navigate; release RC overrides
      sendRCOverride(UINT16_MAX, UINT16_MAX);

      // If we haven't entered AUTO yet, keep requesting it
      if (currentPixhawkMode != "AUTO")
      {
        requestPixhawkMode(10); // 10 = ArduRover AUTO mode
      }
    }
    else if (currentState == PIX_DISARM || currentState == E_STOP)
    {
      // Software kill: Force sticks to dead neutral
      sendRCOverride(1500, 1500);

      // Optionally force it back to manual mode to abort any running missions
      if (currentPixhawkMode != "MANUAL")
      {
        requestPixhawkMode(0); // 0 = ArduRover MANUAL mode
      }
    }

    lastRCOverride = millis();
  }

  // 5. Independent Seed Meter State Machine (Unchanged, local to ESP32)
  if (currentState == METER_TEST)
  {
    if (millis() - meterStartTime > 500) // Ignore startup inrush spike
    {
      if (analogRead(PIN_M_IS) > JAM_THRESHOLD_RAW)
      {
        jamDebounceCounter++;
        if (jamDebounceCounter > 5)
        {
          currentState = METER_JAM;
          jamPhase = 0;
          jamStartTime = millis();
          logJamEvent(millis());
          Serial.println("[WARN] Jam Detected! Executing clear sequence.");
        }
      }
      else
      {
        jamDebounceCounter = 0;
      }
    }
  }
  else if (currentState == METER_JAM)
  {
    unsigned long jamElapsed = millis() - jamStartTime;

    if (jamPhase == 0)
    { // Stop
      setMeterMotor(0);
      if (jamElapsed > 200)
      {
        jamPhase = 1;
        jamStartTime = millis();
      }
    }
    else if (jamPhase == 1)
    { // Reverse to clear
      setMeterMotor(-200);
      if (jamElapsed > 500)
      {
        jamPhase = 2;
        jamStartTime = millis();
      }
    }
    else if (jamPhase == 2)
    { // Pause
      setMeterMotor(0);
      if (jamElapsed > 200)
      {
        currentState = METER_TEST;
        setMeterMotor(225);
        meterStartTime = millis();
        jamDebounceCounter = 0;
        Serial.println("[INFO] Jam cleared. Resuming.");
      }
    }
  }
}