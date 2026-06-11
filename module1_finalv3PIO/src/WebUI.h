#pragma once
#include "Config.h"

// Starts the Access Point and the Async Web Server
void initWebUI();

// Sends a heartbeat ping to the UI to calculate telemetry rate
void broadcastTelemetry();