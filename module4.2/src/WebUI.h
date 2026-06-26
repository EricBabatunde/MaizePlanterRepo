#pragma once
#include "Config.h"

// Starts the Access Point and the Async Web Server
void initWebUI();

// Sends a heartbeat ping and basic telemetry to the UI
void broadcastTelemetry();