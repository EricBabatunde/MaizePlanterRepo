#include "MotorControl.h"

void initMotors()
{
  // Attach pins to their respective PWM channels
  ledcSetup(CH_L_R, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_L_RPWM, CH_L_R);
  ledcSetup(CH_L_L, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_L_LPWM, CH_L_L);
  ledcSetup(CH_R_R, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_R_RPWM, CH_R_R);
  ledcSetup(CH_R_L, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_R_LPWM, CH_R_L);

  // Seed Metering Motor
  ledcSetup(CH_M_R, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_M_RPWM, CH_M_R);
  ledcSetup(CH_M_L, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_M_LPWM, CH_M_L);

  // Ensure motors start at zero movement (1500us)
  setMotors(1500, 1500);
  setMeterMotor(0);
}

void setMotors(int leftRC, int rightRC)
{
  // Constrain inputs to standard RC range for safety
  leftRC = constrain(leftRC, 1000, 2000);
  rightRC = constrain(rightRC, 1000, 2000);

  // Deadband to prevent motor jitter when sticks are centered
  int deadband = 20;

  // --- LEFT MOTOR ---
  if (leftRC > (1500 + deadband))
  {
    int pwm = map(leftRC, 1500, 2000, 0, 255);
    ledcWrite(CH_L_R, pwm);
    ledcWrite(CH_L_L, 0);
  }
  else if (leftRC < (1500 - deadband))
  {
    int pwm = map(leftRC, 1480, 1000, 0, 255);
    ledcWrite(CH_L_R, 0);
    ledcWrite(CH_L_L, pwm);
  }
  else
  {
    ledcWrite(CH_L_R, 0);
    ledcWrite(CH_L_L, 0);
  }

  // --- RIGHT MOTOR ---
  if (rightRC > (1500 + deadband))
  {
    int pwm = map(rightRC, 1500, 2000, 0, 255);
    ledcWrite(CH_R_R, pwm);
    ledcWrite(CH_R_L, 0);
  }
  else if (rightRC < (1500 - deadband))
  {
    int pwm = map(rightRC, 1480, 1000, 0, 255);
    ledcWrite(CH_R_R, 0);
    ledcWrite(CH_R_L, pwm);
  }
  else
  {
    ledcWrite(CH_R_R, 0);
    ledcWrite(CH_R_L, 0);
  }
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
  // ESP32 ADC is 12-bit (0-4095).
  // You will adjust the 0.85 multiplier after probing the IS pin with your multimeter.
  float vL = (analogRead(PIN_L_IS) / 4095.0) * 3.3;
  float vR = (analogRead(PIN_R_IS) / 4095.0) * 3.3;
  float vM = (analogRead(PIN_M_IS) / 4095.0) * 3.3;

  float currentL = vL * 0.85;
  float currentR = vR * 0.85;
  float currentM = vM * 0.85;

  return currentL + currentR + currentM + 0.30; // 0.3A overhead for Pixhawk/ESP32
}