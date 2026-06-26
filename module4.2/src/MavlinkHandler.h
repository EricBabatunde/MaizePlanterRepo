#pragma once
#include "Config.h"

// Initialise the serial port to the Pixhawk
void initMavlink();

// Listen for incoming packets (VFR_HUD for speed, HEARTBEAT for mode)
void receiveMavlink();

// Sends joystick intent to Pixhawk (Range: 1000 - 2000)
// Channel 1 = Steering, Channel 3 = Throttle
void sendRCOverride(uint16_t steerRC, uint16_t throttleRC);

// Commands the Pixhawk to change its internal state
// 0 = MANUAL, 4 = HOLD, 10 = AUTO
void requestPixhawkMode(uint32_t custom_mode);