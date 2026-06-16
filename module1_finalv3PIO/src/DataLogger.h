#pragma once
#include "Config.h"
#include <Arduino.h>

// Mount LittleFS
void initLogger();

// Generates a new file name and writes the CSV header
void startNewLogFile(String bracketName);

// Appends a new data point to the current run file
void logToFS(int pwm, float speed, float amps);

void startTurnLog();
void logTurnEvent(String phase, unsigned long duration);
void startTelemetryLog();
void logTelemetry(int interval);