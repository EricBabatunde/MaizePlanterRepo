#include "MotorControl.h"

void initMotors() {
  // Attach pins to their respective PWM channels
  ledcSetup(CH_L_R, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_L_RPWM, CH_L_R);
  ledcSetup(CH_L_L, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_L_LPWM, CH_L_L);
  ledcSetup(CH_R_R, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_R_RPWM, CH_R_R);
  ledcSetup(CH_R_L, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_R_LPWM, CH_R_L);
  
  // Ensure motors start at zero movement (1500us)
  setMotors(1500, 1500); 
}

void setMotors(int leftRC, int rightRC) {
  // Constrain inputs to standard RC range for safety
  leftRC = constrain(leftRC, 1000, 2000);
  rightRC = constrain(rightRC, 1000, 2000);

  // Deadband to prevent motor jitter when sticks are centered
  int deadband = 20; 

  // --- LEFT MOTOR ---
  if (leftRC > (1500 + deadband)) {
    int pwm = map(leftRC, 1500, 2000, 0, 255);
    ledcWrite(CH_L_R, pwm); 
    ledcWrite(CH_L_L, 0);
  } else if (leftRC < (1500 - deadband)) {
    int pwm = map(leftRC, 1480, 1000, 0, 255);
    ledcWrite(CH_L_R, 0);
    ledcWrite(CH_L_L, pwm); 
  } else {
    ledcWrite(CH_L_R, 0);
    ledcWrite(CH_L_L, 0);
  }

  // --- RIGHT MOTOR ---
  if (rightRC > (1500 + deadband)) {
    int pwm = map(rightRC, 1500, 2000, 0, 255);
    ledcWrite(CH_R_R, pwm);
    ledcWrite(CH_R_L, 0);
  } else if (rightRC < (1500 - deadband)) {
    int pwm = map(rightRC, 1480, 1000, 0, 255);
    ledcWrite(CH_R_R, 0);
    ledcWrite(CH_R_L, pwm);
  } else {
    ledcWrite(CH_R_R, 0);
    ledcWrite(CH_R_L, 0);
  }
}