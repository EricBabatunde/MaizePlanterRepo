#include <Arduino.h>
#include "NetworkModule.h"
#include "MavlinkModule.h"
#include "MechatronicsModule.h"

// Task Handles
TaskHandle_t NetworkTaskHandle = NULL;
TaskHandle_t MavlinkTaskHandle = NULL;

void setup() {
    // Initialise standard Serial for debugging
    Serial.begin(115200);
    while (!Serial); // Wait for native USB serial (if applicable)
    
    Serial.println("\n\n========================================================");
    Serial.println("   MAIZE PLANTER — PHASE 3 DIRECT-DRIVE FIRMWARE");
    Serial.println("   ESP32-S3: Seeding + Actuator + MAVLink Routing");
    Serial.println("   Pixhawk: Direct drive-motor control via ArduRover");
    Serial.println("========================================================\n");

    // 1. Initialise Subsystems
    Network_Init();
    Mavlink_Init();
    Mechatronics_Init();

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

    // Map HardwareSerial parsing, MAVLink State Machine, and
    // MechatronicsModule (FSM + PID) to Core 1.
    // Priority 2 ensures we never drop incoming serial bytes.
    // Stack increased to 12288 for SPI + PID computation overhead.
    xTaskCreatePinnedToCore(
        Mavlink_Task,
        "MavlinkTask",
        12288,          // Stack size (words) — increased for Phase 3
        NULL,           // Parameter
        2,              // Priority
        &MavlinkTaskHandle,
        1               // Core ID
    );

    Serial.println("[System] FreeRTOS Tasks spawned successfully. Entering OS loop.");
    Serial.println("[System] NOTE: Drive motors are controlled DIRECTLY by the Pixhawk.");
    Serial.println("[System]       This ESP32 manages seeding, actuator, and telemetry ONLY.\n");
}

void loop() {
    // The main loop is unused. We delete this task to free up memory
    // as all processing is handled by our two pinned FreeRTOS tasks.
    vTaskDelete(NULL);
}
