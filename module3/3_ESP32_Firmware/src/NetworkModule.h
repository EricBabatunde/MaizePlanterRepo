#pragma once
#include <Arduino.h>

// Initialise Wi-Fi AP, LittleFS, WebServer, and WebSockets
void Network_Init();

// FreeRTOS Task to handle Network cleanup routines (runs on Core 0)
void Network_Task(void *pvParameters);

// Send a JSON telemetry string to all connected WebSocket clients
// Called internally by the MavlinkModule
void Network_SendTelemetry(const String& json);

// Append a row to the LittleFS flight log CSV (called at 1Hz from Mavlink_Task)
void Network_AppendFlightLog(float lat, float lon, float heading, float speed,
                             float wpDist, const char* state, int latency);

// Returns true if the operator has toggled data logging ON via the GCS
bool Network_IsLogging();
