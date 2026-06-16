#pragma once
#include <Arduino.h>

// --- State Machine Enum ---
enum SystemState
{
    MANUAL,
    CRUISE_A,
    CRUISE_B,
    CRUISE_C,
    CRUISE_D,
    TURN_TEST,
    METER_TEST,
    METER_JAM,
    E_STOP
};

// --- Global Shared Variables (Defined later in main.ino) ---
extern SystemState currentState;
extern float currentGroundSpeed;
extern int targetLeftRC;
extern int targetRightRC;

// --- Cruise Variables (Shared) ---
extern int currentSweepPWM;
extern int sweepUpperLimit;
extern unsigned long lastStepTime;
const int STEP_DELAY_MS = 1500; // Updated to 1.5s for shorter field
const int PWM_INCREMENT = 10;

// --- Test State Variables (Shared) ---
extern bool isLoggingTelemetry;
extern int turnPhase;
extern unsigned long phaseStartTime;

// --- Test & Anti-Jam Variables ---
extern bool isNextTurnLeft;
extern unsigned long turnStartTime;

extern int jamPhase;
extern unsigned long jamStartTime;
extern const int JAM_THRESHOLD_RAW;

// --- Hardware Pins (ESP32-S3) ---
// Motor Driver Pins
const int PIN_L_LPWM = 11;
const int PIN_L_RPWM = 12;
const int PIN_R_LPWM = 13;
const int PIN_R_RPWM = 14; // S3 Safe Pin

// Seed Metering Motor
const int PIN_M_RPWM = 15;
const int PIN_M_LPWM = 16;

// Current Sensing (Analog ADC Pins)
const int PIN_L_IS = 6;
const int PIN_R_IS = 7;
const int PIN_M_IS = 8;

// MAVLink Pins
const int PIN_MAV_RX = 4;
const int PIN_MAV_TX = 5;

// --- PWM Configuration (v2.0.17 Core) ---
const int CH_L_R = 0;
const int CH_L_L = 1;
const int CH_R_R = 2;
const int CH_R_L = 3;
const int CH_M_R = 4;
const int CH_M_L = 5;

const int PWM_FREQ = 22500; // 22.5 kHz (Silent operation)
const int PWM_RES = 8;      // 8-bit resolution (0-255)