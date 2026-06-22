#pragma once
/* =============================================================================
   MAVLINK MODULE — MavlinkModule.h
   Autonomous Smart Maize Planter — Phase 3 Direct-Drive Architecture
   Handles: Pixhawk serial comms, MAVLink parsing, telemetry extraction,
            mission upload state machine, E-STOP RC override.
   ============================================================================= */

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

// FreeRTOS Task to handle MAVLink parsing, state machines, and mechatronics
void Mavlink_Task(void *pvParameters);

// Trigger an immediate E-STOP (RC Override on Channel 1 and 3, + mechatronics halt)
void Mavlink_TriggerEStop();

// Queue a new mission for upload to the Pixhawk
void Mavlink_UploadWaypoints(const std::vector<Waypoint>& wps);

// ---------------------------------------------------------------------------
// Telemetry Getter Functions — Thread-safe access to MAVLink-derived data
// Used by main.cpp to feed values into the MechatronicsModule.
// ---------------------------------------------------------------------------

/// Ground speed in m/s from VFR_HUD
float Mavlink_GetGroundSpeed();

/// Distance to current waypoint in metres from NAV_CONTROLLER_OUTPUT
float Mavlink_GetDistToWaypoint();

/// True when the Pixhawk reports a MISSION_ITEM_REACHED event
bool Mavlink_GetWaypointReached();

/// True when the E-STOP is active (latches until manually cleared)
bool Mavlink_GetEStopActive();

/// Clear the waypoint-reached flag (called after state machine processes it)
void Mavlink_ClearWaypointReached();
