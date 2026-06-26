#pragma once
#include "Config.h"
#include <Arduino.h>

// Mount LittleFS
void initLogger();

// Generates a new file for a Pixhawk Validation Run
void startValidationLog();

// Appends Telemetry (Mode, Speed, Current) to the active file
void logValidationData(String mode, float speed, float amps);

// Logs seed meter jam events
void logJamEvent(unsigned long timestamp);