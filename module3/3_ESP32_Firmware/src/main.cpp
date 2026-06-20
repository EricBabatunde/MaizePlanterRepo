#include <Arduino.h>
#include "NetworkModule.h"
#include "MavlinkModule.h"

// Task Handles
TaskHandle_t NetworkTaskHandle = NULL;
TaskHandle_t MavlinkTaskHandle = NULL;

void setup() {
    // Initialize standard Serial for debugging
    Serial.begin(115200);
    while (!Serial); // Wait for native USB serial (if applicable)
    
    Serial.println("\n\n========================================================");
    Serial.println("   MAIZE PLANTER — EMBEDDED GCS FIRMWARE (ESP32-S3)");
    Serial.println("========================================================\n");

    // 1. Initialize Subsystems
    Network_Init();
    Mavlink_Init();

    // 2. Spawn FreeRTOS Tasks
    // Map Wi-Fi, Web Server, and WebSocket Handling to Core 0
    xTaskCreatePinnedToCore(
        Network_Task,
        "NetworkTask",
        8192,           // Stack size (words)
        NULL,           // Parameter
        1,              // Priority
        &NetworkTaskHandle,
        0               // Core ID
    );

    // Map HardwareSerial parsing and MAVLink State Machine to Core 1
    // Given priority 2 to ensure we never drop incoming serial bytes
    xTaskCreatePinnedToCore(
        Mavlink_Task,
        "MavlinkTask",
        8192,           // Stack size (words)
        NULL,           // Parameter
        2,              // Priority
        &MavlinkTaskHandle,
        1               // Core ID
    );

    Serial.println("[System] FreeRTOS Tasks spawned successfully. Entering OS loop.");
}

void loop() {
    // The main loop is unused. We delete this task to free up memory
    // as all processing is handled by our two pinned FreeRTOS tasks.
    vTaskDelete(NULL);
}
