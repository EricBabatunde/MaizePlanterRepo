#include "waypoint_manager.h"

// --- Live Telemetry Storage ---
float currentSpeed = 0.0;
float currentHeading = 0.0;
double currentLat = 0.0;
double currentLon = 0.0;

// Hardware Serial 1 dedicated to the Pixhawk
HardwareSerial PixhawkSerial(1);

// Mission State Variables
NavWaypoint missionBuffer[MAX_WAYPOINTS];
uint16_t totalWaypoints = 0;
bool uploadInProgress = false;
unsigned long lastHeartbeatMillis = 0;

// --- Internal Helper: Send MAVLink Message ---
void sendMavlinkMessage(mavlink_message_t *msg)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, msg);
    PixhawkSerial.write(buf, len);
}

// --- 1. The Heartbeat ---
// The Pixhawk ignores commands if it doesn't receive a 1Hz heartbeat from the GCS
void sendHeartbeat()
{
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(ESP32_SYS_ID, ESP32_COMP_ID, &msg,
                               MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID,
                               MAV_MODE_MANUAL_ARMED, 0, MAV_STATE_ACTIVE);
    sendMavlinkMessage(&msg);
}

// --- 2. Start the Handshake ---
void startMissionUpload(NavWaypoint *points, uint16_t count)
{
    if (count > MAX_WAYPOINTS)
        count = MAX_WAYPOINTS;

    // Copy to local buffer
    for (int i = 0; i < count; i++)
    {
        missionBuffer[i] = points[i];
    }

    totalWaypoints = count;
    uploadInProgress = true;

    Serial.println("\n[MAVLINK] Initiating Mission Upload...");
    Serial.print("[MAVLINK] Sending MISSION_COUNT: ");
    Serial.println(totalWaypoints);

    // Step 1: Send MISSION_COUNT
    mavlink_message_t msg;
    mavlink_msg_mission_count_pack(ESP32_SYS_ID, ESP32_COMP_ID, &msg,
                                   TARGET_SYS_ID, TARGET_COMP_ID,
                                   totalWaypoints, MAV_MISSION_TYPE_MISSION, 0);
    sendMavlinkMessage(&msg);
}

// --- 3. Parse Incoming Packets from Pixhawk ---
void handleMavlinkMessage(mavlink_message_t *msg)
{
    switch (msg->msgid)
    {
        // ---> ADD THIS DIAGNOSTIC BLOCK <---
    case MAVLINK_MSG_ID_HEARTBEAT:
    {
        // // Only print occasionally to avoid spamming the terminal
        // static unsigned long lastHbPrint = 0;
        // if (millis() - lastHbPrint > 5000)
        // {
        //     Serial.print("[MAVLINK DIAGNOSTIC] Heard heartbeat! Pixhawk SYS_ID: ");
        //     Serial.print(msg->sysid);
        //     Serial.print(" | COMP_ID: ");
        //     Serial.println(msg->compid);
        //     lastHbPrint = millis();
        // }
        break;
    }
    case MAVLINK_MSG_ID_COMMAND_ACK:
    {
        mavlink_command_ack_t ack;
        mavlink_msg_command_ack_decode(msg, &ack);

        Serial.print("\n[MAVLINK] COMMAND_ACK Received! Command ID: ");
        Serial.print(ack.command);
        Serial.print(" | Result: ");

        // Translate the raw number into a readable aerospace result
        switch (ack.result)
        {
        case MAV_RESULT_ACCEPTED:
            Serial.println("ACCEPTED (0)");
            break;
        case MAV_RESULT_TEMPORARILY_REJECTED:
            Serial.println("TEMPORARILY_REJECTED (1) - Usually a Pre-Arm failure or bad mode.");
            break;
        case MAV_RESULT_DENIED:
            Serial.println("DENIED (2) - Pixhawk refuses this command from this port.");
            break;
        case MAV_RESULT_UNSUPPORTED:
            Serial.println("UNSUPPORTED (3) - Command ID not recognised.");
            break;
        case MAV_RESULT_FAILED:
            Serial.println("FAILED (4) - Command valid, but execution failed.");
            break;
        default:
            Serial.println(ack.result);
            break;
        }
        break;
    }
    // ---> ADD THESE NEW TELEMETRY BLOCKS <---
    case MAVLINK_MSG_ID_VFR_HUD:
    {
        mavlink_vfr_hud_t hud;
        mavlink_msg_vfr_hud_decode(msg, &hud);

        currentSpeed = hud.groundspeed; // Speed in m/s
        currentHeading = hud.heading;   // Compass heading in degrees
        break;
    }

    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
    {
        mavlink_global_position_int_t pos;
        mavlink_msg_global_position_int_decode(msg, &pos);

        // Pixhawk sends Lat/Lon scaled by 10^7. We divide to get standard decimals.
        currentLat = pos.lat / 10000000.0;
        currentLon = pos.lon / 10000000.0;
        break;
    }
    // ----------------------------------------
    // -----------------------------------
    case MAVLINK_MSG_ID_MISSION_REQUEST_INT:
    case MAVLINK_MSG_ID_MISSION_REQUEST:
    {
        // Step 2 & 4: Pixhawk requests a specific waypoint
        mavlink_mission_request_int_t req;
        mavlink_msg_mission_request_int_decode(msg, &req);

        uint16_t seq = req.seq;
        Serial.print("[MAVLINK] Pixhawk requested WP: ");
        Serial.println(seq);

        if (seq < totalWaypoints)
        {
            // Step 3: Send the requested waypoint
            mavlink_message_t item_msg;
            int32_t lat_int = (int32_t)(missionBuffer[seq].lat * 1e7);
            int32_t lon_int = (int32_t)(missionBuffer[seq].lon * 1e7);

            // MAV_CMD_NAV_WAYPOINT is command 16
            mavlink_msg_mission_item_int_pack(ESP32_SYS_ID, ESP32_COMP_ID, &item_msg,
                                              TARGET_SYS_ID, TARGET_COMP_ID, seq,
                                              MAV_FRAME_GLOBAL_INT, MAV_CMD_NAV_WAYPOINT,
                                              0,                   // current
                                              1,                   // autocontinue
                                              0, 0, 0, 0,          // params 1-4 (delay, acceptance radius, pass-through, yaw)
                                              lat_int, lon_int, 0, // x, y, z (altitude 0 for rover)
                                              MAV_MISSION_TYPE_MISSION);

            sendMavlinkMessage(&item_msg);
            Serial.print("[MAVLINK] Sent WP ");
            Serial.println(seq);
        }
        break;
    }

    case MAVLINK_MSG_ID_MISSION_ACK:
    {
        // Step 5: Mission Accepted
        mavlink_mission_ack_t ack;
        mavlink_msg_mission_ack_decode(msg, &ack);

        if (ack.type == MAV_MISSION_ACCEPTED)
        {
            Serial.println("[MAVLINK] *** MISSION UPLOAD SUCCESSFUL ***");
            uploadInProgress = false;
        }
        else
        {
            Serial.print("[MAVLINK] ERROR: Mission rejected. Code: ");
            Serial.println(ack.type);
            uploadInProgress = false;
        }
        break;
    }
    }
}

// --- Public Setup & Loop ---
void setupMavlink()
{
    PixhawkSerial.begin(MAV_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.println("[MAVLINK] Hardware Serial 1 Initialised on RX:18, TX:17.");
}

void loopMavlink()
{
    // 1. Maintain Heartbeat
    unsigned long currentMillis = millis();
    if (currentMillis - lastHeartbeatMillis >= 1000)
    {
        lastHeartbeatMillis = currentMillis;
        sendHeartbeat();
    }

    // 2. Read incoming data from Pixhawk
    mavlink_message_t msg;
    mavlink_status_t status;

    while (PixhawkSerial.available() > 0)
    {
        uint8_t c = PixhawkSerial.read();
        // Parse the byte. If a full packet is formed, process it.
        if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status))
        {
            handleMavlinkMessage(&msg);
        }
    }
}

// --- 4. Command the Rover to Drive ---
void commandArmAndAuto()
{
    Serial.println("\n[MAVLINK] Executing ARM and AUTO sequence...");

    // Step 1: Send the ARM command (Command ID 400)
    mavlink_message_t arm_msg;
    mavlink_msg_command_long_pack(ESP32_SYS_ID, ESP32_COMP_ID, &arm_msg,
                                  TARGET_SYS_ID, TARGET_COMP_ID,
                                  MAV_CMD_COMPONENT_ARM_DISARM, 0,
                                  1.0f, // Param 1: 1 to ARM, 0 to DISARM
                                  0, 0, 0, 0, 0, 0);
    sendMavlinkMessage(&arm_msg);
    Serial.println("[MAVLINK] Sent ARM Command.");

    delay(500); // Give the Pixhawk 500ms to process the arming sequence

    // Step 2: Send the SET_MODE command to enter AUTO mode
    // Note: In ArduRover, AUTO is Custom Mode 10.
    mavlink_message_t mode_msg;
    mavlink_msg_set_mode_pack(ESP32_SYS_ID, ESP32_COMP_ID, &mode_msg,
                              TARGET_SYS_ID,
                              MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
                              10); // 10 = ArduRover AUTO mode
    sendMavlinkMessage(&mode_msg);
    Serial.println("[MAVLINK] Sent AUTO Mode Command.\n");
}