#pragma once
#include <Arduino.h>

// --- State Machine Enum ---
enum SystemState
{
    PIX_MANUAL, // Passing RC override from UI to Pixhawk
    PIX_AUTO,   // Pixhawk executing the L1 Weave Test
    PIX_DISARM, // Software E-Stop (Sending 0 to RC overrides)
    METER_TEST, // Testing the Seed Metering Motor
    METER_JAM,  // Active Anti-Jam sequence
    E_STOP      // Hard kill state
};

// --- Global Shared Variables (Defined later in main.cpp) ---
extern SystemState currentState;
extern float currentGroundSpeed;
extern String currentPixhawkMode; // Used to update the UI status

// --- RC Override Variables (Mapped to 1000 - 2000us) ---
extern int targetThrottleRC; // Maps to MAVLink Channel 3
extern int targetSteerRC;    // Maps to MAVLink Channel 1

// --- Test & Anti-Jam Variables ---
extern unsigned long meterStartTime;
extern int jamDebounceCounter;
extern int jamPhase;
extern unsigned long jamStartTime;
extern const int JAM_THRESHOLD_RAW;

// --- Hardware Pins (ESP32-S3) ---
// (Drive motor pins bypassed in Phase 4.2, but left defined to prevent compile errors)
const int PIN_L_LPWM = 11;
const int PIN_L_RPWM = 12;
const int PIN_R_LPWM = 13;
const int PIN_R_RPWM = 14;

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

// --- PWM Configuration ---
const int CH_M_R = 4;
const int CH_M_L = 5;
const int PWM_FREQ = 22500; // 22.5 kHz (Silent operation)
const int PWM_RES = 8;      // 8-bit resolution (0-255)