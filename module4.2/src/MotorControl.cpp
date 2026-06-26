#include "MotorControl.h"

void initMotors()
{
  // Seed Metering Motor PWM Setup
  ledcSetup(CH_M_R, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_M_RPWM, CH_M_R);
  ledcSetup(CH_M_L, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_M_LPWM, CH_M_L);

  // Ensure meter starts idle
  setMeterMotor(0);
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