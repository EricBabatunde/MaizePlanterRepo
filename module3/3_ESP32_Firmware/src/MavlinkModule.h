#pragma once
#include <Arduino.h>
#include <vector>

// Waypoint structure passed from NetworkModule to MavlinkModule
struct Waypoint {
    uint16_t row;
    float lat;
    float lon;
    float local_x;
    float local_y;
};

// Initialise the MAVLink module and serial connection
void Mavlink_Init();

// FreeRTOS Task to handle MAVLink parsing and state machines
void Mavlink_Task(void *pvParameters);

// Trigger an immediate E-STOP (RC Override on Channel 1 and 3)
void Mavlink_TriggerEStop();

// Queue a new mission for upload to the Pixhawk
void Mavlink_UploadWaypoints(const std::vector<Waypoint>& wps);
