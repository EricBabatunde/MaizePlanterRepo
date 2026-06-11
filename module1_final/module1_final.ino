#include "Config.h"
#include "MotorControl.h"
#include "MavlinkHandler.h"
#include "WebUI.h"
#include "DataLogger.h"

// --- Global Variable Definitions (Declared in Config.h) ---
SystemState currentState = MANUAL;
float currentGroundSpeed = 0.0;
int targetLeftRC = 1500;
int targetRightRC = 1500;

int currentSweepPWM = 1500;
int sweepUpperLimit = 1500;
unsigned long lastStepTime = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize Subsystems
  initLogger();
  initMotors();
  initMavlink();
  initWebUI(); // Starts Wi-Fi and Web Server last
}

void loop() {
  receiveMavlink(); // Continuously update currentGroundSpeed
  
  if (currentState == MANUAL) {
    setMotors(targetLeftRC, targetRightRC);
  } 
  else if (currentState == E_STOP) {
    setMotors(1500, 1500); // Hard kill
  }
  else {
    // --- Automated Cruise Ramping Logic ---
    if (currentState == CRUISE_A || currentState == CRUISE_B || currentState == CRUISE_C || currentState == CRUISE_D) {
      if (millis() - lastStepTime > STEP_DELAY_MS) {
        
        Serial.printf("[LOGGING] %dus | Speed: %.2f m/s\n", currentSweepPWM, currentGroundSpeed);
        logToFS(currentSweepPWM, currentGroundSpeed);
        
        currentSweepPWM += PWM_INCREMENT;
        
        if (currentSweepPWM > sweepUpperLimit) {
          Serial.println("[CRUISE] Bracket complete. Returning to MANUAL mode.");
          currentState = MANUAL;
          currentSweepPWM = 1500;
          targetLeftRC = 1500;
          targetRightRC = 1500;
          setMotors(1500, 1500); 
        } else {
          Serial.printf("[CRUISE] Target: %dus. Stabilising...\n", currentSweepPWM);
        }
        
        lastStepTime = millis(); 
      }
      
      // Apply automated sweep values equally to both front tracks
      setMotors(currentSweepPWM, currentSweepPWM);
    }
  }
}