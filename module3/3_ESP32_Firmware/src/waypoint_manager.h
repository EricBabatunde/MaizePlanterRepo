#ifndef WAYPOINT_MANAGER_H
#define WAYPOINT_MANAGER_H

#include <Arduino.h>
#include <../lib/MAVLink/mavlink/common/mavlink.h>

// --- Configuration Constants ---
#define RX_PIN 18 // ESP32 RX <- Pixhawk TX
#define TX_PIN 17 // ESP32 TX -> Pixhawk RX
#define MAV_BAUD 57600
#define MAX_WAYPOINTS 100

// Define our ESP32 identity on the MAVLink network (Ground Control Station)
#define ESP32_SYS_ID 255
#define ESP32_COMP_ID 190

// Define the Pixhawk identity (Autopilot)
#define TARGET_SYS_ID 1
#define TARGET_COMP_ID 1

// --- Data Structures ---
struct NavWaypoint
{
    int row;
    double lat;
    double lon;
};

// --- Public Functions ---
void setupMavlink();
void loopMavlink();
void startMissionUpload(NavWaypoint *points, uint16_t count);
void commandArmAndAuto();

#endif // WAYPOINT_MANAGER_H