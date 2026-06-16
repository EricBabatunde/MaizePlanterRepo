#include <Arduino.h>
#include "Config.h"
#include "MotorControl.h"
#include "MavlinkHandler.h"
#include "WebUI.h"
#include "DataLogger.h"

// --- Global Shared Variables ---
SystemState currentState = MANUAL;
float currentGroundSpeed = 0.0;
int targetLeftRC = 1500;
int targetRightRC = 1500;

int currentSweepPWM = 1500;
int sweepUpperLimit = 1500;
unsigned long lastStepTime = 0;

// --- Test State Variables ---
bool isLoggingTelemetry = false;
unsigned long lastTelemetryBroadcast = 0;
bool isNextTurnLeft = true;

unsigned long turnStartTime = 0;

int jamPhase = 0;
unsigned long jamStartTime = 0;
// Set this threshold after probing the IS pin with a stalled motor
const int JAM_THRESHOLD_RAW = 3200;

void setup()
{
  Serial.begin(115200);
  initLogger();
  initMotors();
  initMavlink();
  initWebUI();
}

void loop()
{
  receiveMavlink();

  // 1. Maintain 5Hz Telemetry Broadcast to UI
  if (millis() - lastTelemetryBroadcast > 200)
  {
    broadcastTelemetry();
    lastTelemetryBroadcast = millis();
  }

  // 2. Main State Machine
  if (currentState == MANUAL)
  {
    setMotors(targetLeftRC, targetRightRC);
  }
  else if (currentState == E_STOP)
  {
    setMotors(1500, 1500); // Hard kill
  }
  else if (currentState == METER_TEST)
  {
    // Actively monitor for a physical jam
    if (analogRead(PIN_M_IS) > JAM_THRESHOLD_RAW)
    {
      currentState = METER_JAM;
      jamPhase = 0;
      jamStartTime = millis();
      logJamEvent(millis()); // Log the jam occurrence to Jam_Events.csv
      Serial.println("[WARN] Jam Detected! Executing clear sequence.");
    }
  }
  else if (currentState == METER_JAM)
  {
    // 2. Anti-Jam Clearance Sequence
    unsigned long jamElapsed = millis() - jamStartTime;

    if (jamPhase == 0)
    { // Stop to let magnetic field collapse
      setMeterMotor(0);
      if (jamElapsed > 200)
      {
        jamPhase = 1;
        jamStartTime = millis();
      }
    }
    else if (jamPhase == 1)
    { // Reverse to spit the seed back
      setMeterMotor(-200);
      if (jamElapsed > 500)
      {
        jamPhase = 2;
        jamStartTime = millis();
      }
    }
    else if (jamPhase == 2)
    { // Pause before resuming
      setMeterMotor(0);
      if (jamElapsed > 200)
      {
        currentState = METER_TEST;
        setMeterMotor(180); // Resume forward RPM
        Serial.println("[INFO] Jam cleared. Resuming.");
      }
    }
  }
  else
  {
    // --- Automated Cruise Ramping Logic ---
    if (currentState == CRUISE_A || currentState == CRUISE_B || currentState == CRUISE_C || currentState == CRUISE_D)
    {
      if (millis() - lastStepTime > STEP_DELAY_MS)
      {

        float currentAmps = getTotalAmps(); // Grab the live power draw

        Serial.printf("[LOGGING] %dus | %.2fm/s | %.2fA\n", currentSweepPWM, currentGroundSpeed, currentAmps);
        logToFS(currentSweepPWM, currentGroundSpeed, currentAmps);

        currentSweepPWM += PWM_INCREMENT;

        if (currentSweepPWM > sweepUpperLimit)
        {
          currentState = MANUAL;
          currentSweepPWM = 1500;
          targetLeftRC = 1500;
          targetRightRC = 1500;
          setMotors(1500, 1500);
        }
        lastStepTime = millis();
      }
      setMotors(currentSweepPWM, currentSweepPWM);
    }
  }
}