#pragma once
/* =============================================================================
   MECHATRONICS MODULE — MechatronicsModule.h
   Autonomous Smart Maize Planter — Phase 3 Direct-Drive Architecture
   Controls: Seeding Mechanism (BTS7960), Furrow Actuator (BTS7960),
             AS5048A Magnetic Encoder (PWM output mode)
   The Pixhawk handles all drive-motor logic directly via ArduRover.
   ============================================================================= */

#include <Arduino.h>

// ---------------------------------------------------------------------------
// 1. HARDWARE PIN DEFINITIONS
// ---------------------------------------------------------------------------

// Seed Motor — BTS7960 H-Bridge
#define SEED_MOTOR_RPWM     1       // Forward rotation PWM
#define SEED_MOTOR_LPWM     2       // Reverse rotation PWM (held LOW)

// Furrow Actuator — BTS7960 H-Bridge
#define ACTUATOR_RPWM       47      // Deploy direction
#define ACTUATOR_LPWM       48      // Retract direction

// AS5048A Magnetic Encoder — PWM Output Mode
// The encoder outputs a PWM signal whose duty cycle maps to 0–359°.
// Read via a non-blocking ISR on CHANGE to avoid blocking FreeRTOS tasks.
#define ENCODER_PWM_PIN     9

// MAVLink UART (defined in MavlinkModule — listed here for reference only)
// #define PIXHAWK_RX_PIN   4
// #define PIXHAWK_TX_PIN   5

// ---------------------------------------------------------------------------
// 2. ACTUATOR TIMING CONSTANTS (tunable during field calibration)
// ---------------------------------------------------------------------------
// Deploy encounters more mechanical resistance (furrow opener into soil),
// so it is given a longer stroke time than retract.
#define ACTUATOR_DEPLOY_MS  3000    // Milliseconds to fully deploy
#define ACTUATOR_RETRACT_MS 2500    // Milliseconds to fully retract

// ---------------------------------------------------------------------------
// 3. PID TUNING CONSTANTS
// ---------------------------------------------------------------------------
#define PID_KP              2.0f
#define PID_KI              0.5f
#define PID_KD              0.1f

// Seeding rate — Target RPM = SEED_RATE_FACTOR × groundSpeed (m/s)
#define SEED_RATE_FACTOR    100.0f

// ---------------------------------------------------------------------------
// 3b. EMA LOW-PASS FILTER
// ---------------------------------------------------------------------------
// Exponential Moving Average smoothing factor for encoder RPM.
// Lower values = heavier smoothing (less responsive, less jitter).
// Higher values = lighter smoothing (more responsive, more jitter).
// Tuneable during field calibration.
#define EMA_ALPHA           0.2f

// ---------------------------------------------------------------------------
// 4. LEDC PWM CONFIGURATION
// ---------------------------------------------------------------------------
#define LEDC_CHANNEL_SEED   0       // LEDC channel for seed motor
#define LEDC_FREQ_HZ        20000   // 20 kHz — inaudible for BTS7960
#define LEDC_RESOLUTION      8      // 8-bit duty cycle (0–255)

// ---------------------------------------------------------------------------
// 5. FSM STATE ENUMERATION
// ---------------------------------------------------------------------------
enum PlanterState {
    PLANTER_IDLE,           // Actuator retracted, seed motor off
    PLANTER_DEPLOYING,      // Actuator deploying for ACTUATOR_DEPLOY_MS
    PLANTER_PLANTING,       // Actuator held, seed motor PID-active
    PLANTER_RETRACTING,     // Actuator retracting for ACTUATOR_RETRACT_MS
    PLANTER_TURNING,        // Seed motor off, waiting for next row
    PLANTER_E_STOP          // Highest priority — all outputs halted
};

// ---------------------------------------------------------------------------
// 6. PUBLIC API
// ---------------------------------------------------------------------------

/// Initialise GPIO pins, LEDC PWM channels, and encoder PWM interrupt.
void Mechatronics_Init();

/// Run the finite state machine. Must be called rapidly (every ~10ms).
/// All timing is non-blocking via millis().
void updateStateMachine(float groundSpeed, float distToWaypoint,
                        bool waypointReached, bool eStopActive, float currentHeading);

/// Run the seeding PID controller. Reads encoder RPM and adjusts PWM.
/// Must be called rapidly alongside the state machine.
void updateSeedingPID(float groundSpeed);

/// Return the current FSM state for telemetry reporting.
PlanterState Mechatronics_GetState();

/// Return the current measured seed plate RPM (for telemetry).
float Mechatronics_GetSeedRPM();

/// Return a human-readable string for the current planter state.
const char* Mechatronics_GetStateString();

/// Arm the FSM — the IDLE state will only transition to DEPLOYING
/// after this function has been called (i.e. a MISSION command received).
void Mechatronics_StartMission();
