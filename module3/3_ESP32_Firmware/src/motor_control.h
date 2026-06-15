#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

// --- Pixhawk PWM Input Pins ---
#define PIN_IN_LEFT 4
#define PIN_IN_RIGHT 5

// --- BTS7960 Driver Output Pins ---
#define PIN_EN 21    // Shared Enable Pin for both drivers
#define PIN_L_FWD 10 // Left Motor Forward (RPWM)
#define PIN_L_REV 11 // Left Motor Reverse (LPWM)
#define PIN_R_FWD 12 // Right Motor Forward (RPWM)
#define PIN_R_REV 13 // Right Motor Reverse (LPWM)

// --- PWM Constants ---
#define PWM_NEUTRAL 1500 // 1500 microseconds is STOP
#define PWM_DEADBAND 50  // Ignore signals between 1450 and 1550 to prevent jitter
#define PWM_MIN 1000     // Full Reverse
#define PWM_MAX 2000     // Full Forward

// --- Public Functions ---
void setupMotors();
void loopMotors();
void triggerEStop();

#endif // MOTOR_CONTROL_H