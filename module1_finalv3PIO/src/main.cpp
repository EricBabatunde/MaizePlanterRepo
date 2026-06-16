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
int turnPhase = 0;
unsigned long phaseStartTime = 0;
unsigned long lastTelemetryBroadcast = 0;
bool isNextTurnLeft = true;

unsigned long turnStartTime = 0;

int jamPhase = 0;
unsigned long jamStartTime = 0;
// Set this threshold after probing the IS pin with a stalled motor
const int JAM_THRESHOLD_RAW = 2000;

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
  // else if (currentState == TURN_TEST)
  // {
  //   // --- Autonomous 180° Alternating Pivot Turn ---
  //   unsigned long currentDuration = millis() - phaseStartTime;

  //   if (turnPhase == 0)
  //   { // Decelerate
  //     setMotors(1500, 1500);
  //     if (currentDuration > 1400)
  //     {
  //       logTurnEvent("Deceleration", currentDuration);
  //       turnPhase = 1;
  //       phaseStartTime = millis();
  //     }
  //   }
  //   else if (turnPhase == 1)
  //   { // Pivot Turn

  //     // Execute the alternating pivot
  //     if (isNextTurnLeft)
  //     {
  //       setMotors(1500, 1850); // Lock Left, Drive Right
  //     }
  //     else
  //     {
  //       setMotors(1850, 1500); // Drive Left, Lock Right
  //     }

  //     // Doubled the target time from 4.2s to 8.4s for single-wheel drive
  //     if (currentDuration > 8400)
  //     {
  //       logTurnEvent(isNextTurnLeft ? "Pivot_Left" : "Pivot_Right", currentDuration);
  //       turnPhase = 2;
  //       phaseStartTime = millis();
  //     }
  //   }
  //   else if (turnPhase == 2)
  //   { // Re-align
  //     setMotors(1500, 1500);
  //     if (currentDuration > 1800)
  //     {
  //       logTurnEvent("Re-alignment", currentDuration);
  //       turnPhase = 3;
  //       phaseStartTime = millis();
  //     }
  //   }
  //   else if (turnPhase == 3)
  //   { // Accelerate & Reset
  //     setMotors(1650, 1650);
  //     if (currentDuration > 800)
  //     {
  //       logTurnEvent("Acceleration", currentDuration);

  //       // Reset state and toggle the direction for the next button press
  //       currentState = MANUAL;
  //       setMotors(1500, 1500);
  //       isNextTurnLeft = !isNextTurnLeft;

  //       Serial.println("[TURN] Pivot Turn Complete. Direction toggled. Returned to MANUAL.");
  //     }
  //   }
  // }
  else if (currentState == METER_TEST)
  {
    // 1. Actively monitor for a physical jam
    if (analogRead(PIN_M_IS) > JAM_THRESHOLD_RAW)
    {
      currentState = METER_JAM;
      jamPhase = 0;
      jamStartTime = millis();
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