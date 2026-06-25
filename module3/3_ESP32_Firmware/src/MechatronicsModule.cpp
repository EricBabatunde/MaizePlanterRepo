/* =============================================================================
   MECHATRONICS MODULE — MechatronicsModule.cpp
   Autonomous Smart Maize Planter — Phase 3 Direct-Drive Architecture

   Responsibilities:
     1. Finite State Machine for furrow actuator sequencing
     2. PID-controlled seeding motor synchronised to ground speed
     3. AS5048A magnetic encoder reading via PWM output (non-blocking ISR)

   The Pixhawk handles ALL drive-motor logic directly. This module contains
   ABSOLUTELY NO drive-motor passthrough logic.
   ============================================================================= */

#include "MechatronicsModule.h"
#include "MavlinkModule.h"

// ---------------------------------------------------------------------------
// 1. INTERNAL STATE
// ---------------------------------------------------------------------------
static PlanterState currentState = PLANTER_IDLE;

// Mission-started flag — IDLE only transitions to DEPLOYING when true.
// Set by Mechatronics_StartMission() (called from NetworkModule on MISSION command).
// Reset on E-STOP so the operator must re-send a MISSION to resume.
static bool missionStarted = false;

// Row-established safety latch — prevents the PLANTING→RETRACTING transition
// from firing prematurely when distToWaypoint initialises at 0.00 m.
// Set true once distToWaypoint exceeds 1.0 m (i.e. the planter is
// confirmed to be tracking along the row). Reset on each deployment.
static bool rowEstablished = false;

// Non-blocking timer for actuator stroke timing
static unsigned long stateEntryTime = 0;

// PID state variables
static float pidIntegral   = 0.0f;
static float pidPrevError  = 0.0f;
static unsigned long pidLastTime = 0;

// Encoder state for RPM calculation
static float prevAngleDeg       = 0.0f;
static unsigned long prevAngleTime = 0;
static float currentRPM         = 0.0f;
static float smoothedRPM        = 0.0f;  // EMA-filtered RPM for PID input

// ---------------------------------------------------------------------------
// 2. AS5048A PWM ENCODER — NON-BLOCKING ISR
// ---------------------------------------------------------------------------
// The AS5048A PWM output has a period of approximately 4096µs (244 Hz).
// Duty cycle maps linearly to 0–359°.
// We use an ISR on CHANGE to capture RISING and FALLING timestamps without
// ever blocking a FreeRTOS task (pulseIn() is forbidden).

static volatile unsigned long isrRisingTime  = 0;  // µs timestamp of last RISING edge
static volatile unsigned long isrHighTime    = 0;  // µs duration of the HIGH pulse
static volatile unsigned long isrPeriod      = 0;  // µs duration of the full PWM period
static volatile bool          isrNewData     = false;

/// ISR — must be in IRAM to avoid flash-cache misses during execution.
static void IRAM_ATTR encoderPWM_ISR() {
    unsigned long now = esp_timer_get_time(); // Microseconds, 64-bit safe on ESP32

    if (digitalRead(ENCODER_PWM_PIN) == HIGH) {
        // RISING edge — record the timestamp and compute the period from
        // the previous RISING edge (if available)
        if (isrRisingTime > 0) {
            isrPeriod = now - isrRisingTime;
        }
        isrRisingTime = now;
    } else {
        // FALLING edge — compute the HIGH pulse width
        if (isrRisingTime > 0) {
            isrHighTime = now - isrRisingTime;
            isrNewData  = true;
        }
    }
}

/// Convert the ISR's pulse-width data into an angle in degrees (0.0–359.0).
/// Returns the angle, or -1.0 if no new data is available.
static float readEncoderAngleDeg() {
    // Atomically snapshot the volatile ISR variables
    noInterrupts();
    unsigned long highTime = isrHighTime;
    unsigned long period   = isrPeriod;
    bool          newData  = isrNewData;
    isrNewData = false;
    interrupts();

    if (!newData || period == 0) return -1.0f;

    // Duty cycle → angle: (highTime / period) × 360°
    float dutyCycle = (float)highTime / (float)period;

    // Clamp duty cycle to [0.0, 1.0] to guard against ISR timing artefacts
    if (dutyCycle < 0.0f) dutyCycle = 0.0f;
    if (dutyCycle > 1.0f) dutyCycle = 1.0f;

    return dutyCycle * 360.0f;
}

/// Calculate real-time RPM from the PWM angle readings.
/// Handles the 359°→0° wrap-around gracefully and applies the EMA filter.
static void calculateRPM() {
    float angleDeg = readEncoderAngleDeg();
    if (angleDeg < 0.0f) return; // No new encoder data available

    unsigned long now = millis();
    unsigned long dt = now - prevAngleTime;
    if (dt < 10) return; // Avoid division by zero / noisy short intervals

    // --- Wrap-Around Filter ---
    // Calculate the shortest angular distance, accounting for the fact that
    // the seed plate continuously rotates and the angle jumps 359°→0°.
    float delta = angleDeg - prevAngleDeg;

    // If the absolute delta exceeds 180°, we have crossed the wrap boundary.
    // Correct by adding/subtracting a full revolution.
    if (delta > 180.0f) {
        delta -= 360.0f;   // e.g. 350→10 gives delta = -340 → corrected to +20
    } else if (delta < -180.0f) {
        delta += 360.0f;   // e.g. 10→350 gives delta = +340 → corrected to -20
    }

    // Convert to RPM:
    //   |delta| / 360.0  = fraction of a full revolution
    //   dt / 60000.0     = time in minutes
    //   RPM = (|delta| / 360) / (dt / 60000)
    float revolutions = fabsf(delta) / 360.0f;
    float minutes     = (float)dt / 60000.0f;
    float rawRPM      = revolutions / minutes;

    // --- EMA Low-Pass Filter ---
    // Smooth out the microsecond jitter inherent in PWM timing measurement.
    // The PID controller uses smoothedRPM, never the raw reading.
    smoothedRPM = (EMA_ALPHA * rawRPM) + ((1.0f - EMA_ALPHA) * smoothedRPM);
    currentRPM  = smoothedRPM;

    prevAngleDeg  = angleDeg;
    prevAngleTime = now;
}

// ---------------------------------------------------------------------------
// 3. INTERNAL HELPERS — ACTUATOR & MOTOR
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

// ---------------------------------------------------------------------------
// 4. PUBLIC API — INITIALISATION
// ---------------------------------------------------------------------------
void Mechatronics_Init() {
    Serial.println("[Mechatronics] Initialising GPIO, PWM, and encoder ISR...");

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

    // --- AS5048A Encoder PWM Input ---
    pinMode(ENCODER_PWM_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PWM_PIN), encoderPWM_ISR, CHANGE);

    // Prime the encoder state
    prevAngleDeg  = 0.0f;
    prevAngleTime = millis();
    pidLastTime   = millis();

    Serial.println("[Mechatronics] Initialisation complete. State: IDLE.");
    Serial.printf("  Seed Motor: R_PWM=%d, L_PWM=%d\n", SEED_MOTOR_RPWM, SEED_MOTOR_LPWM);
    Serial.printf("  Actuator:   R_PWM=%d, L_PWM=%d\n", ACTUATOR_RPWM, ACTUATOR_LPWM);
    Serial.printf("  Encoder:    PWM_PIN=%d (ISR on CHANGE)\n", ENCODER_PWM_PIN);
    Serial.printf("  Deploy: %d ms, Retract: %d ms\n", ACTUATOR_DEPLOY_MS, ACTUATOR_RETRACT_MS);
    Serial.printf("  EMA Alpha: %.2f\n", EMA_ALPHA);
}

// ---------------------------------------------------------------------------
// 5. PUBLIC API — FINITE STATE MACHINE
// ---------------------------------------------------------------------------
void updateStateMachine(float groundSpeed, float distToWaypoint,
                        bool waypointReached, bool eStopActive, float currentHeading) {

    static float turnStartHeading = 0.0f;

    // E-STOP is the highest-priority override — checked FIRST in every state
    if (eStopActive && currentState != PLANTER_E_STOP) {
        Serial.println("[Mechatronics] *** E-STOP ACTIVATED *** Halting all outputs.");
        setActuator(0);
        setSeedMotorPWM(0);
        pidIntegral  = 0.0f;
        pidPrevError = 0.0f;
        missionStarted = false; // Require a new MISSION command after E-STOP recovery
        currentState = PLANTER_E_STOP;
        stateEntryTime = millis();
        return;
    }

    unsigned long now = millis();

    switch (currentState) {

        case PLANTER_IDLE:
            setActuator(0);
            setSeedMotorPWM(0);
            if (missionStarted && !eStopActive) {
                Serial.println("[Mechatronics] State: IDLE -> DEPLOYING");
                setActuator(1); // Deploy
                currentState   = PLANTER_DEPLOYING;
                stateEntryTime = now;
            }
            break;

        case PLANTER_DEPLOYING:
            if ((now - stateEntryTime) > ACTUATOR_DEPLOY_MS) {
                Serial.println("[Mechatronics] State: DEPLOYING -> PLANTING");
                setActuator(0);            // Stop actuator
                rowEstablished = false;    // RESET THE LATCH
                currentState   = PLANTER_PLANTING;
                stateEntryTime = now;
            }
            break;

        case PLANTER_PLANTING:
            // Safety Latch: Wait for planter to leave the start zone
            if (!rowEstablished && distToWaypoint > 1.0f) {
                rowEstablished = true;
                Serial.println("[Mechatronics] Row established. Monitoring for end of row.");
            }

            // End of Row Trigger
            if (rowEstablished && (waypointReached || distToWaypoint < 0.5f)) {
                Serial.printf("[Mechatronics] State: PLANTING -> RETRACTING (wp_reached=%d, dist=%.2f m)\n",
                              waypointReached, distToWaypoint);
                setSeedMotorPWM(0);
                pidIntegral  = 0.0f;
                pidPrevError = 0.0f;
                Mavlink_ClearWaypointReached(); // CLEAR THE STICKY FLAG
                setActuator(-1);               // Retract
                currentState   = PLANTER_RETRACTING;
                stateEntryTime = now;
            }
            break;

        case PLANTER_RETRACTING:
            if ((now - stateEntryTime) > ACTUATOR_RETRACT_MS) {
                Serial.println("[Mechatronics] State: RETRACTING -> TURNING");
                setActuator(0);
                turnStartHeading = currentHeading; // Capture heading for U-turn
                currentState   = PLANTER_TURNING;  // DO NOT SET THIS TO DEPLOYING
                stateEntryTime = now;
            }
            break;

        case PLANTER_TURNING: {
            setSeedMotorPWM(0);

            float headingDiff = abs(currentHeading - turnStartHeading);
            if (headingDiff > 180.0f) {
                headingDiff = 360.0f - headingDiff;
            }

            if (headingDiff > 150.0f && distToWaypoint > 2.0f) {
                Serial.println("[Mechatronics] State: TURNING -> DEPLOYING");
                setActuator(1); // Deploy for new row
                currentState   = PLANTER_DEPLOYING;
                stateEntryTime = now;
            }
            break;
        }

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
// 6. PUBLIC API — PID SEEDING CONTROLLER
// ---------------------------------------------------------------------------
void updateSeedingPID(float groundSpeed) {
    // Only run PID when actively planting
    if (currentState != PLANTER_PLANTING) return;

    // Anti-windup: if the vehicle is stationary, force PWM to zero
    if (groundSpeed <= 0.001f) {
        setSeedMotorPWM(0);
        pidIntegral  = 0.0f;
        pidPrevError = 0.0f;
        smoothedRPM  = 0.0f;
        return;
    }

    // Read the encoder and compute actual RPM (with wrap-around + EMA)
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

    // PID error calculation — uses EMA-smoothed RPM, not raw
    float error = targetRPM - smoothedRPM;

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
// 7. PUBLIC API — STATE GETTERS
// ---------------------------------------------------------------------------
PlanterState Mechatronics_GetState() {
    return currentState;
}

float Mechatronics_GetSeedRPM() {
    return smoothedRPM;
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

void Mechatronics_StartMission() {
    missionStarted = true;
    Serial.println("[Mechatronics] Mission ARMED — IDLE will transition to DEPLOYING when conditions are met.");
}
