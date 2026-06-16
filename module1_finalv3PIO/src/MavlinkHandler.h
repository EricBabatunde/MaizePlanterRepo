#pragma once
#include "Config.h"

// Initialize the serial port to the Pixhawk
void initMavlink();

// Listen for incoming packets and update currentGroundSpeed
void receiveMavlink();