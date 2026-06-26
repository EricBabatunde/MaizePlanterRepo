#pragma once
#include "Config.h"

// Initialise ONLY the auxiliary motor pins (Seed Meter)
void initMotors();

// Controls the seed metering disc (Range: -255 to 255)
void setMeterMotor(int speedPWM);

// Returns total system current draw
float getTotalAmps();