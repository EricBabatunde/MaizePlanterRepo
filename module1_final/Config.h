#pragma once
#include <Arduino.h>

// --- State Machine Enum ---
enum SystemState { MANUAL, CRUISE_A, CRUISE_B, CRUISE_C, CRUISE_D, E_STOP };

// --- Global Shared Variables (Defined later in main.ino) ---
extern SystemState currentState;
extern float currentGroundSpeed;
extern int targetLeftRC;
extern int targetRightRC;

// --- Cruise Variables (Shared) ---
extern int currentSweepPWM;
extern int sweepUpperLimit;
extern unsigned long lastStepTime;
const int STEP_DELAY_MS = 2000; 
const int PWM_INCREMENT = 10;

// --- Hardware Pins (ESP32-S3) ---
// Motor Driver Pins
const int PIN_L_RPWM = 11; 
const int PIN_L_LPWM = 12; 
const int PIN_R_RPWM = 13; 
const int PIN_R_LPWM = 14; // S3 Safe Pin

// MAVLink Pins
const int PIN_MAV_RX = 4;
const int PIN_MAV_TX = 5;

// --- PWM Configuration (v2.0.17 Core) ---
const int CH_L_R = 0;
const int CH_L_L = 1;
const int CH_R_R = 2;
const int CH_R_L = 3;

const int PWM_FREQ = 22500; // 22.5 kHz (Silent operation)
const int PWM_RES = 8;      // 8-bit resolution (0-255)