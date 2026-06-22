/* =============================================================================
   MECHATRONICS MODULE — MechatronicsModule.cpp
   Autonomous Smart Maize Planter — Phase 3 Direct-Drive Architecture

   Responsibilities:
     1. Finite State Machine for furrow actuator sequencing
     2. PID-controlled seeding motor synchronised to ground speed
     3. AS5048A magnetic encoder reading via SPI

   The Pixhawk handles ALL drive-motor logic directly. This module contains
   ABSOLUTELY NO drive-motor passthrough logic.
   ============================================================================= */

#include "MechatronicsModule.h"
#include <SPI.h>

// ---------------------------------------------------------------------------
// 1. INTERNAL STATE
// ---------------------------------------------------------------------------
static PlanterState currentState = PLANTER_IDLE;

// Non-blocking timer for actuator stroke timing
static unsigned long stateEntryTime = 0;

// PID state variables
static float pidIntegral   = 0.0f;
static float pidPrevError  = 0.0f;
static unsigned long pidLastTime = 0;

// Encoder state for RPM calculation
static uint16_t prevAngle       = 0;
static unsigned long prevAngleTime = 0;
static float currentRPM         = 0.0f;

// Custom SPI instance for the AS5048A (avoids conflict with default SPI)
static SPIClass encoderSPI(HSPI);
static const SPISettings encoderSPISettings(1000000, MSBFIRST, SPI_MODE1);

// AS5048A command to read the angle register (address 0x3FFF with parity)
static const uint16_t AS5048A_CMD_ANGLE = 0xFFFF;

// ---------------------------------------------------------------------------
// 2. INTERNAL HELPERS
// ---------------------------------------------------------------------------

/// Set the furrow actuator outputs.
/// direction:  1 = deploy (RPWM HIGH, LPWM LOW)
///            -1 = retract (RPWM LOW, LPWM HIGH)
///             0 = hold / off (both LOW)
static void setActuator(int direction) {
    switch (direction) {
        case 1:  // Deploy
            digitalWrite(ACTUATOR_RPWM, HIGH);
            digitalWrite(ACTUATOR_LPWM, LOW);
            break;
        case -1: // Retract
            digitalWrite(ACTUATOR_RPWM, LOW);
            digitalWrite(ACTUATOR_LPWM, HIGH);
            break;
        default: // Hold / Off
            digitalWrite(ACTUATOR_RPWM, LOW);
            digitalWrite(ACTUATOR_LPWM, LOW);
            break;
    }
}

/// Set the seed motor PWM duty cycle (0–255). Forward rotation only.
static void setSeedMotorPWM(uint8_t duty) {
    ledcWrite(LEDC_CHANNEL_SEED, duty);
    // L_PWM held LOW for unidirectional forward rotation
    digitalWrite(SEED_MOTOR_LPWM, LOW);
}

/// Read the 14-bit absolute angle from the AS5048A via SPI.
/// Returns a value in the range [0, 16383].
static uint16_t readEncoderAngle() {
    encoderSPI.beginTransaction(encoderSPISettings);
    digitalWrite(ENCODER_CS, LOW);
    uint16_t rawData = encoderSPI.transfer16(AS5048A_CMD_ANGLE);
    digitalWrite(ENCODER_CS, HIGH);
    encoderSPI.endTransaction();

    // Mask off the top 2 bits (parity + error flag) to get the 14-bit angle
    return rawData & 0x3FFF;
}

/// Calculate real-time RPM from the AS5048A angle readings.
/// Uses the angular displacement between successive calls and the elapsed time.
static void calculateRPM() {
    unsigned long now = millis();
    uint16_t angle = readEncoderAngle();

    unsigned long dt = now - prevAngleTime;
    if (dt < 10) return; // Avoid division by zero / noisy short intervals

    // Calculate angular displacement (handle wrap-around of 14-bit counter)
    int32_t delta = (int32_t)angle - (int32_t)prevAngle;
    if (delta < -8192) delta += 16384;      // Wrapped forward
    else if (delta > 8192) delta -= 16384;  // Wrapped backward

    // Convert to RPM:
    //   delta / 16384 = fraction of a full revolution
    //   dt / 60000.0  = time in minutes
    //   RPM = (delta / 16384) / (dt / 60000)
    float revolutions = (float)abs(delta) / 16384.0f;
    float minutes     = (float)dt / 60000.0f;
    currentRPM = revolutions / minutes;

    prevAngle     = angle;
    prevAngleTime = now;
}

// ---------------------------------------------------------------------------
// 3. PUBLIC API — INITIALISATION
// ---------------------------------------------------------------------------
void Mechatronics_Init() {
    Serial.println("[Mechatronics] Initialising GPIO, PWM, and SPI...");

    // --- Actuator GPIO (simple digital output, no PWM needed) ---
    pinMode(ACTUATOR_RPWM, OUTPUT);
    pinMode(ACTUATOR_LPWM, OUTPUT);
    setActuator(0); // Start with actuator off

    // --- Seed Motor PWM via LEDC ---
    // ESP32 Arduino Core 2.x API: ledcSetup(channel, freq, resolution) + ledcAttachPin(pin, channel)
    ledcSetup(LEDC_CHANNEL_SEED, LEDC_FREQ_HZ, LEDC_RESOLUTION);
    ledcAttachPin(SEED_MOTOR_RPWM, LEDC_CHANNEL_SEED);
    pinMode(SEED_MOTOR_LPWM, OUTPUT);
    setSeedMotorPWM(0); // Start with motor off

    // --- AS5048A Encoder SPI ---
    pinMode(ENCODER_CS, OUTPUT);
    digitalWrite(ENCODER_CS, HIGH); // Deselect
    encoderSPI.begin(ENCODER_SCK, ENCODER_MISO, ENCODER_MOSI, ENCODER_CS);

    // Prime the encoder state
    prevAngle     = readEncoderAngle();
    prevAngleTime = millis();
    pidLastTime   = millis();

    Serial.println("[Mechatronics] Initialisation complete. State: IDLE.");
    Serial.printf("  Seed Motor: R_PWM=%d, L_PWM=%d\n", SEED_MOTOR_RPWM, SEED_MOTOR_LPWM);
    Serial.printf("  Actuator:   R_PWM=%d, L_PWM=%d\n", ACTUATOR_RPWM, ACTUATOR_LPWM);
    Serial.printf("  Encoder:    CS=%d, MISO=%d, MOSI=%d, SCK=%d\n",
                  ENCODER_CS, ENCODER_MISO, ENCODER_MOSI, ENCODER_SCK);
    Serial.printf("  Deploy: %d ms, Retract: %d ms\n", ACTUATOR_DEPLOY_MS, ACTUATOR_RETRACT_MS);
}

// ---------------------------------------------------------------------------
// 4. PUBLIC API — FINITE STATE MACHINE
// ---------------------------------------------------------------------------
void updateStateMachine(float groundSpeed, float distToWaypoint,
                        bool waypointReached, bool eStopActive) {

    // E-STOP is the highest-priority override — checked FIRST in every state
    if (eStopActive && currentState != PLANTER_E_STOP) {
        Serial.println("[Mechatronics] *** E-STOP ACTIVATED *** Halting all outputs.");
        setActuator(0);
        setSeedMotorPWM(0);
        pidIntegral  = 0.0f;
        pidPrevError = 0.0f;
        currentState = PLANTER_E_STOP;
        stateEntryTime = millis();
        return;
    }

    unsigned long now = millis();

    switch (currentState) {

        case PLANTER_IDLE:
            // Outputs are safe: actuator off, seed motor off
            setActuator(0);
            setSeedMotorPWM(0);
            // Transition to DEPLOYING when the mission is active and we are
            // far enough from the waypoint to begin a new row
            if (!eStopActive && !waypointReached && distToWaypoint > 2.0f) {
                Serial.println("[Mechatronics] State: IDLE -> DEPLOYING");
                setActuator(1); // Begin deploy stroke
                currentState   = PLANTER_DEPLOYING;
                stateEntryTime = now;
            }
            break;

        case PLANTER_DEPLOYING:
            // Actuator is deploying — wait for the timed stroke to complete
            if ((now - stateEntryTime) >= ACTUATOR_DEPLOY_MS) {
                Serial.println("[Mechatronics] State: DEPLOYING -> PLANTING");
                setActuator(0); // Hold in deployed position
                currentState   = PLANTER_PLANTING;
                stateEntryTime = now;
            }
            break;

        case PLANTER_PLANTING:
            // Seed motor is PID-controlled (handled by updateSeedingPID).
            // Transition to RETRACTING when approaching the end of the row.
            if (waypointReached || distToWaypoint < 0.5f) {
                Serial.printf("[Mechatronics] State: PLANTING -> RETRACTING (wp_reached=%d, dist=%.2f m)\n",
                              waypointReached, distToWaypoint);
                setSeedMotorPWM(0);  // Stop seeding immediately
                pidIntegral  = 0.0f; // Reset PID integral to prevent windup
                pidPrevError = 0.0f;
                setActuator(-1);     // Begin retract stroke
                currentState   = PLANTER_RETRACTING;
                stateEntryTime = now;
            }
            break;

        case PLANTER_RETRACTING:
            // Actuator is retracting — wait for the timed stroke to complete
            if ((now - stateEntryTime) >= ACTUATOR_RETRACT_MS) {
                Serial.println("[Mechatronics] State: RETRACTING -> TURNING");
                setActuator(0); // Actuator fully retracted
                currentState   = PLANTER_TURNING;
                stateEntryTime = now;
            }
            break;

        case PLANTER_TURNING:
            // Seed motor off, actuator off. Wait for the Pixhawk to complete
            // its U-turn and begin tracking the next waypoint.
            setSeedMotorPWM(0);
            if (!waypointReached && distToWaypoint > 2.0f) {
                Serial.println("[Mechatronics] State: TURNING -> DEPLOYING");
                setActuator(1); // Begin deploy for the new row
                currentState   = PLANTER_DEPLOYING;
                stateEntryTime = now;
            }
            break;

        case PLANTER_E_STOP:
            // All outputs remain halted. Recovery requires E-STOP to be cleared.
            setActuator(0);
            setSeedMotorPWM(0);
            if (!eStopActive) {
                Serial.println("[Mechatronics] E-STOP cleared. State: E_STOP -> IDLE");
                currentState   = PLANTER_IDLE;
                stateEntryTime = now;
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// 5. PUBLIC API — PID SEEDING CONTROLLER
// ---------------------------------------------------------------------------
void updateSeedingPID(float groundSpeed) {
    // Only run PID when actively planting
    if (currentState != PLANTER_PLANTING) return;

    // Anti-windup: if the vehicle is stationary, force PWM to zero
    if (groundSpeed <= 0.001f) {
        setSeedMotorPWM(0);
        pidIntegral  = 0.0f;
        pidPrevError = 0.0f;
        return;
    }

    // Read the encoder and compute actual RPM
    calculateRPM();

    // Calculate the target RPM proportional to ground speed
    float targetRPM = SEED_RATE_FACTOR * groundSpeed;

    // Time delta in seconds for integral/derivative terms
    unsigned long now = millis();
    float dt = (float)(now - pidLastTime) / 1000.0f;
    pidLastTime = now;
    if (dt <= 0.0f || dt > 1.0f) {
        // Guard against erratic timing (first call, or overflow)
        dt = 0.01f;
    }

    // PID error calculation
    float error = targetRPM - currentRPM;

    // Proportional term
    float pTerm = PID_KP * error;

    // Integral term (with anti-windup clamping)
    pidIntegral += error * dt;
    // Clamp integral to prevent windup — equivalent to ±255 PWM contribution
    float integralMax = 255.0f / PID_KI;
    if (pidIntegral > integralMax) pidIntegral = integralMax;
    if (pidIntegral < -integralMax) pidIntegral = -integralMax;
    float iTerm = PID_KI * pidIntegral;

    // Derivative term
    float dTerm = PID_KD * (error - pidPrevError) / dt;
    pidPrevError = error;

    // Compute raw PID output and clamp to valid PWM range [0, 255]
    float output = pTerm + iTerm + dTerm;
    if (output < 0.0f)   output = 0.0f;
    if (output > 255.0f) output = 255.0f;

    setSeedMotorPWM((uint8_t)output);
}

// ---------------------------------------------------------------------------
// 6. PUBLIC API — STATE GETTERS
// ---------------------------------------------------------------------------
PlanterState Mechatronics_GetState() {
    return currentState;
}

float Mechatronics_GetSeedRPM() {
    return currentRPM;
}

const char* Mechatronics_GetStateString() {
    switch (currentState) {
        case PLANTER_IDLE:       return "IDLE";
        case PLANTER_DEPLOYING:  return "DEPLOYING";
        case PLANTER_PLANTING:   return "PLANTING";
        case PLANTER_RETRACTING: return "RETRACTING";
        case PLANTER_TURNING:    return "TURNING";
        case PLANTER_E_STOP:     return "E-STOP";
        default:                 return "UNKNOWN";
    }
}
