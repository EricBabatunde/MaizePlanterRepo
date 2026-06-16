#include "motor_control.h"

// Volatile variables are required because they are modified inside an Interrupt
volatile unsigned long leftStartTime = 0;
volatile unsigned int leftPulseWidth = 1500;

volatile unsigned long rightStartTime = 0;
volatile unsigned int rightPulseWidth = 1500;

// --- Interrupt Service Routines (ISRs) ---
// These trigger instantly in the background whenever the Pixhawk pin changes state.
void IRAM_ATTR leftInterrupt()
{
    if (digitalRead(PIN_IN_LEFT) == HIGH)
    {
        leftStartTime = micros(); // Pin went high, start the stopwatch
    }
    else
    {
        leftPulseWidth = micros() - leftStartTime; // Pin went low, record the width
    }
}

void IRAM_ATTR rightInterrupt()
{
    if (digitalRead(PIN_IN_RIGHT) == HIGH)
    {
        rightStartTime = micros();
    }
    else
    {
        rightPulseWidth = micros() - rightStartTime;
    }
}

// --- Internal Helper: Map and Drive ---
void driveMotor(unsigned int pulseWidth, int pinFwd, int pinRev)
{
    if (pulseWidth > (PWM_NEUTRAL + PWM_DEADBAND))
    {
        // Forward Mapping
        int speed = map(pulseWidth, PWM_NEUTRAL + PWM_DEADBAND, PWM_MAX, 0, 255);
        speed = constrain(speed, 0, 255);
        analogWrite(pinFwd, speed);
        analogWrite(pinRev, 0);
    }
    else if (pulseWidth < (PWM_NEUTRAL - PWM_DEADBAND))
    {
        // Reverse Mapping
        int speed = map(pulseWidth, PWM_NEUTRAL - PWM_DEADBAND, PWM_MIN, 0, 255);
        speed = constrain(speed, 0, 255);
        analogWrite(pinFwd, 0);
        analogWrite(pinRev, speed);
    }
    else
    {
        // Neutral / Deadband - Stop the motor
        analogWrite(pinFwd, 0);
        analogWrite(pinRev, 0);
    }
}

// --- Public Setup ---
void setupMotors()
{
    // 1. Initialise BTS7960 Output Pins
    pinMode(PIN_EN, OUTPUT);
    pinMode(PIN_L_FWD, OUTPUT);
    pinMode(PIN_L_REV, OUTPUT);
    pinMode(PIN_R_FWD, OUTPUT);
    pinMode(PIN_R_REV, OUTPUT);

    // Turn off motors initially for safety
    triggerEStop();

    // 2. Initialise Pixhawk Input Pins
    pinMode(PIN_IN_LEFT, INPUT_PULLDOWN);
    pinMode(PIN_IN_RIGHT, INPUT_PULLDOWN);

    // 3. Attach Hardware Interrupts
    attachInterrupt(digitalPinToInterrupt(PIN_IN_LEFT), leftInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_IN_RIGHT), rightInterrupt, CHANGE);

    Serial.println("[MOTORS] Non-blocking PWM Motor Control Initialised.");
}

// --- Public Loop ---
// --- Public Loop ---
void loopMotors()
{
    // Briefly disable interrupts to safely copy the volatile variables
    noInterrupts();
    unsigned int currentLeft = leftPulseWidth;
    unsigned int currentRight = rightPulseWidth;
    interrupts();

    // --- NEW: Diagnostic Serial Print (Every 1 second) ---
    // static unsigned long lastMotorPrint = 0;
    // if (millis() - lastMotorPrint > 1000)
    // {
    //     Serial.print("[MOTORS] Pixhawk Input -> Left: ");
    //     Serial.print(currentLeft);
    //     Serial.print(" us | Right: ");
    //     Serial.print(currentRight);
    //     Serial.println(" us");
    //     lastMotorPrint = millis();
    // }
    // -----------------------------------------------------

    // Send the calculated speeds to the motor drivers
    driveMotor(currentLeft, PIN_L_FWD, PIN_L_REV);
    driveMotor(currentRight, PIN_R_FWD, PIN_R_REV);
}

// --- Failsafe: Emergency Stop ---
void triggerEStop()
{
    digitalWrite(PIN_EN, LOW); // Instantly cuts the logic gate inside the BTS7960
    analogWrite(PIN_L_FWD, 0);
    analogWrite(PIN_L_REV, 0);
    analogWrite(PIN_R_FWD, 0);
    analogWrite(PIN_R_REV, 0);
}