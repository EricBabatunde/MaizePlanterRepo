#pragma once
#include "Config.h"

// Initialize motor pins and PWM channels
void initMotors();

// Update motor speeds (Takes standard 1000-2000 RC values)
void setMotors(int leftRC, int rightRC);

void setMeterMotor(int speedPWM); // Range: -255 to 255
float getTotalAmps();