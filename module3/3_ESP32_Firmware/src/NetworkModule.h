#pragma once
#include <Arduino.h>

// Initialise Wi-Fi AP, LittleFS, WebServer, and WebSockets
void Network_Init();

// FreeRTOS Task to handle Network cleanup routines (runs on Core 0)
void Network_Task(void *pvParameters);

// Send a JSON telemetry string to all connected WebSocket clients
// Called internally by the MavlinkModule
void Network_SendTelemetry(const String& json);
