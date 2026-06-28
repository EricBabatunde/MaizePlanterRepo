#include "MotorControl.h"

void initMotors()
{
  // Drive Motors PWM Setup
  ledcSetup(CH_L_LPWM, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_L_LPWM, CH_L_LPWM);
  ledcSetup(CH_L_RPWM, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_L_RPWM, CH_L_RPWM);

  ledcSetup(CH_R_LPWM, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_R_LPWM, CH_R_LPWM);
  ledcSetup(CH_R_RPWM, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_R_RPWM, CH_R_RPWM);

  // Seed Metering Motor PWM Setup
  ledcSetup(CH_M_R, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_M_RPWM, CH_M_R);
  ledcSetup(CH_M_L, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_M_LPWM, CH_M_L);

  // Set Pixhawk Input Pins as INPUT
  pinMode(PIN_PIX_LEFT, INPUT);
  pinMode(PIN_PIX_RIGHT, INPUT);

  // Ensure meter starts idle
  setMeterMotor(0);

  // Ensure drive motors start idle
  ledcWrite(CH_L_LPWM, 0);
  ledcWrite(CH_L_RPWM, 0);
  ledcWrite(CH_R_LPWM, 0);
  ledcWrite(CH_R_RPWM, 0);
}

void setMeterMotor(int speedPWM)
{
  // Positive for forward (planting), Negative for reverse (clearing jam)
  if (speedPWM > 0)
  {
    ledcWrite(CH_M_R, speedPWM);
    ledcWrite(CH_M_L, 0);
  }
  else if (speedPWM < 0)
  {
    ledcWrite(CH_M_R, 0);
    ledcWrite(CH_M_L, abs(speedPWM));
  }
  else
  {
    ledcWrite(CH_M_R, 0);
    ledcWrite(CH_M_L, 0);
  }
}

float getTotalAmps()
{
  // Passive monitoring of the drive wheels + active monitoring of the meter.
  // Assumes a shared common ground across the mechatronic chassis.
  float vL = (analogRead(PIN_L_IS) / 4095.0) * 3.3;
  float vR = (analogRead(PIN_R_IS) / 4095.0) * 3.3;
  float vM = (analogRead(PIN_M_IS) / 4095.0) * 3.3;

  float currentL = vL * 0.85;
  float currentR = vR * 0.85;
  float currentM = vM * 0.85;

  return currentL + currentR + currentM + 0.30; // 0.3A overhead for logic boards
}

void updateDriveFromPixhawk()
{
  // Read left and right pulses
  unsigned long leftPulse = pulseIn(PIN_PIX_LEFT, HIGH, 25000);
  unsigned long rightPulse = pulseIn(PIN_PIX_RIGHT, HIGH, 25000);

  // Failsafe Implementation
  if (leftPulse < 500 || rightPulse < 500)
  {
    ledcWrite(CH_L_LPWM, 0);
    ledcWrite(CH_L_RPWM, 0);
    ledcWrite(CH_R_LPWM, 0);
    ledcWrite(CH_R_RPWM, 0);
    return;
  }

  // Direct Mapping and Reverse Lockout
  int leftIntent = 0;
  if (leftPulse >= 1500)
  {
    leftIntent = map(constrain(leftPulse, 1500, 2000), 1500, 2000, 0, 204);
    // Apply deadzone around 1500 for forward motion
    if (leftPulse < 1500 + DEADZONE)
    {
      leftIntent = 0;
    }
  }
  else
  {
    leftIntent = 0;
  }
  // CRITICAL AGRONOMIC CONSTRAINT: If drops below 1500 - DEADZONE, must evaluate to 0
  if (leftPulse < (1500 - DEADZONE))
  {
    leftIntent = 0;
  }

  int rightIntent = 0;
  if (rightPulse >= 1500)
  {
    rightIntent = map(constrain(rightPulse, 1500, 2000), 1500, 2000, 0, 255);
    // Apply deadzone around 1500 for forward motion
    if (rightPulse < 1500 + DEADZONE)
    {
      rightIntent = 0;
    }
  }
  else
  {
    rightIntent = 0;
  }
  // CRITICAL AGRONOMIC CONSTRAINT: If drops below 1500 - DEADZONE, must evaluate to 0
  if (rightPulse < (1500 - DEADZONE))
  {
    rightIntent = 0;
  }

  // Write intent to RPWM pins. LPWM (reverse) pins strictly held at 0 at all times.
  ledcWrite(CH_L_RPWM, leftIntent);
  ledcWrite(CH_L_LPWM, 0);
  ledcWrite(CH_R_RPWM, rightIntent);
  ledcWrite(CH_R_LPWM, 0);
}