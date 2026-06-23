/* =============================================================================
   MAVLINK MODULE — MavlinkModule.cpp
   Autonomous Smart Maize Planter — Phase 3 Direct-Drive Architecture

   Responsibilities:
     1. Parse incoming MAVLink packets from the Pixhawk via HardwareSerial
     2. Extract telemetry: position, heading, ground speed, waypoint distance
     3. Detect MISSION_ITEM_REACHED events for FSM synchronisation
     4. Send 1Hz GCS heartbeat
     5. Execute E-STOP via RC_CHANNELS_OVERRIDE (Ch 1 & 3 to 1500µs neutral)
     6. Manage mission upload state machine (MISSION_COUNT/MISSION_ITEM_INT)
     7. Package telemetry JSON for the WebSocket UI at ~5Hz
     8. Call MechatronicsModule update functions on every loop iteration

   This module contains ABSOLUTELY NO drive-motor passthrough logic.
   ============================================================================= */

#include "MavlinkModule.h"
#include "MechatronicsModule.h"
#include "NetworkModule.h"
#include <HardwareSerial.h>
#include <MAVLink_ardupilotmega.h>
#include <ArduinoJson.h>

// User-specified Pixhawk TELEM 2 pins
#define PIXHAWK_RX_PIN 4
#define PIXHAWK_TX_PIN 5
#define MAVLINK_BAUD 57600

// Target system and component ID for ArduPilot
#define TARGET_SYSTEM 1
#define TARGET_COMPONENT 1

// GCS system and component ID (This ESP32)
#define GCS_SYSTEM 255
#define GCS_COMPONENT 190

HardwareSerial PixhawkSerial(1); // Use UART1

// --- Thread-Safe Shared State ---
static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

// E-STOP Flags
static bool estopPending = false;    // One-shot trigger for RC override
static bool eStopActive  = false;    // Latching flag for MechatronicsModule

// Mission Upload State
static std::vector<Waypoint> pendingMission;
static bool uploadRequested = false;
static uint16_t currentMissionSeq = 0;
enum MissionState { MS_IDLE, SENDING_COUNT, WAITING_REQUEST, SENDING_ITEMS, ACKNOWLEDGED, MS_ERROR };
static MissionState missionState = MS_IDLE;
static unsigned long lastMissionStateChange = 0;
#define MISSION_TIMEOUT_MS 3000

// Telemetry State
static int32_t current_lat_1e7    = 0;
static int32_t current_lon_1e7    = 0;
static uint16_t current_heading_cd = 0;   // centidegrees
static float current_speed_ms     = 0.0f;
static float current_wp_dist      = 0.0f; // metres — from NAV_CONTROLLER_OUTPUT
static bool missionItemReached    = false; // set by MISSION_ITEM_REACHED msg
static unsigned long lastTelemetrySent = 0;

// --- Helper function to send a mavlink message ---
void sendMavlinkMessage(mavlink_message_t* msg) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, msg);
    PixhawkSerial.write(buf, len);
}

// --- Trigger E-STOP (called from Network Core) ---
void Mavlink_TriggerEStop() {
    portENTER_CRITICAL(&stateMux);
    estopPending = true;
    eStopActive  = true;   // Latches until manually cleared
    portEXIT_CRITICAL(&stateMux);
}

// --- Upload Mission (called from Network Core) ---
void Mavlink_UploadWaypoints(const std::vector<Waypoint>& wps) {
    portENTER_CRITICAL(&stateMux);
    pendingMission = wps;
    uploadRequested = true;
    portEXIT_CRITICAL(&stateMux);
}

// --- Telemetry Getter Functions (thread-safe reads) ---
float Mavlink_GetGroundSpeed() {
    portENTER_CRITICAL(&stateMux);
    float val = current_speed_ms;
    portEXIT_CRITICAL(&stateMux);
    return val;
}

float Mavlink_GetDistToWaypoint() {
    portENTER_CRITICAL(&stateMux);
    float val = current_wp_dist;
    portEXIT_CRITICAL(&stateMux);
    return val;
}

bool Mavlink_GetWaypointReached() {
    portENTER_CRITICAL(&stateMux);
    bool val = missionItemReached;
    portEXIT_CRITICAL(&stateMux);
    return val;
}

bool Mavlink_GetEStopActive() {
    portENTER_CRITICAL(&stateMux);
    bool val = eStopActive;
    portEXIT_CRITICAL(&stateMux);
    return val;
}

void Mavlink_ClearWaypointReached() {
    portENTER_CRITICAL(&stateMux);
    missionItemReached = false;
    portEXIT_CRITICAL(&stateMux);
}

void Mavlink_Init() {
    PixhawkSerial.begin(MAVLINK_BAUD, SERIAL_8N1, PIXHAWK_RX_PIN, PIXHAWK_TX_PIN);
    Serial.println("[MAVLink] Initialised on UART1 (RX:4, TX:5)");
}

// --- Process Incoming MAVLink Packets ---
void processMavlinkMessage(mavlink_message_t* msg) {
    switch (msg->msgid) {
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
            mavlink_global_position_int_t pos;
            mavlink_msg_global_position_int_decode(msg, &pos);
            current_lat_1e7 = pos.lat;
            current_lon_1e7 = pos.lon;
            current_heading_cd = pos.hdg; // centidegrees (0-35999)
            break;
        }
        case MAVLINK_MSG_ID_VFR_HUD: {
            mavlink_vfr_hud_t hud;
            mavlink_msg_vfr_hud_decode(msg, &hud);
            portENTER_CRITICAL(&stateMux);
            current_speed_ms = hud.groundspeed;
            portEXIT_CRITICAL(&stateMux);
            // DEBUG: Print raw ground speed for engineering verification
            Serial.printf("[MAVLink] Raw Ground Speed: %.2f m/s\n", hud.groundspeed);
            break;
        }
        case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT: {
            mavlink_nav_controller_output_t nav;
            mavlink_msg_nav_controller_output_decode(msg, &nav);
            portENTER_CRITICAL(&stateMux);
            current_wp_dist = nav.wp_dist;
            portEXIT_CRITICAL(&stateMux);
            Serial.printf("[MAVLink] WP Distance: %.1f m\n", nav.wp_dist);
            break;
        }
        case MAVLINK_MSG_ID_MISSION_ITEM_REACHED: {
            mavlink_mission_item_reached_t reached;
            mavlink_msg_mission_item_reached_decode(msg, &reached);
            portENTER_CRITICAL(&stateMux);
            missionItemReached = true;
            portEXIT_CRITICAL(&stateMux);
            Serial.printf("[MAVLink] MISSION_ITEM_REACHED — seq: %d\n", reached.seq);
            break;
        }
        case MAVLINK_MSG_ID_MISSION_REQUEST: {
            mavlink_mission_request_t req;
            mavlink_msg_mission_request_decode(msg, &req);
            
            if (missionState == WAITING_REQUEST || missionState == SENDING_ITEMS) {
                if (req.seq < pendingMission.size()) {
                    currentMissionSeq = req.seq;
                    missionState = SENDING_ITEMS;
                }
            }
            break;
        }
        case MAVLINK_MSG_ID_MISSION_ACK: {
            mavlink_mission_ack_t ack;
            mavlink_msg_mission_ack_decode(msg, &ack);
            if (ack.type == MAV_MISSION_ACCEPTED) {
                Serial.println("[MAVLink] Mission upload SUCCESS!");
                missionState = ACKNOWLEDGED;
            } else {
                Serial.printf("[MAVLink] Mission upload FAILED, code: %d\n", ack.type);
                missionState = MS_ERROR;
            }
            break;
        }
    }
}

// --- Main MAVLink Task (Core 1) ---
void Mavlink_Task(void *pvParameters) {
    mavlink_message_t msg;
    mavlink_status_t status;
    unsigned long lastHeartbeat = 0;

    while (true) {
        unsigned long now = millis();

        // 1. Send 1Hz Heartbeat
        if (now - lastHeartbeat >= 1000) {
            lastHeartbeat = now;
            mavlink_msg_heartbeat_pack(GCS_SYSTEM, GCS_COMPONENT, &msg, 
                MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, MAV_MODE_FLAG_SAFETY_ARMED, 0, MAV_STATE_ACTIVE);
            sendMavlinkMessage(&msg);
        }

        // 2. Parse incoming serial bytes
        while (PixhawkSerial.available() > 0) {
            uint8_t c = PixhawkSerial.read();
            if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
                processMavlinkMessage(&msg);
            }
        }

        // 3. Handle E-STOP (RC Override to Pixhawk)
        bool performEstop = false;
        portENTER_CRITICAL(&stateMux);
        if (estopPending) {
            performEstop = true;
            estopPending = false; // Reset after reading
        }
        portEXIT_CRITICAL(&stateMux);

        if (performEstop) {
            Serial.println("[MAVLink] Executing E-STOP. Overriding RC 1 (Steering) & 3 (Throttle) to 1500µs neutral.");
            // RC Channels Override: Channel 1 and 3 to 1500µs. Others to UINT16_MAX (ignore).
            mavlink_msg_rc_channels_override_pack(GCS_SYSTEM, GCS_COMPONENT, &msg,
                TARGET_SYSTEM, TARGET_COMPONENT,
                1500, UINT16_MAX, 1500, UINT16_MAX, 
                UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
                UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
                UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
                UINT16_MAX, UINT16_MAX);
            sendMavlinkMessage(&msg);
        }

        // 4. Handle Mission Upload State Machine
        bool handleUpload = false;
        portENTER_CRITICAL(&stateMux);
        if (uploadRequested) {
            handleUpload = true;
            uploadRequested = false;
            missionState = SENDING_COUNT;
            lastMissionStateChange = now;
        }
        portEXIT_CRITICAL(&stateMux);

        if (missionState == SENDING_COUNT) {
            Serial.printf("[MAVLink] Starting mission upload. Total points: %d\n", pendingMission.size());
            mavlink_msg_mission_count_pack(GCS_SYSTEM, GCS_COMPONENT, &msg,
                TARGET_SYSTEM, TARGET_COMPONENT, pendingMission.size(), MAV_MISSION_TYPE_MISSION, 0);
            sendMavlinkMessage(&msg);
            missionState = WAITING_REQUEST;
            lastMissionStateChange = now;
        } 
        else if (missionState == SENDING_ITEMS) {
            if (currentMissionSeq < pendingMission.size()) {
                Waypoint wp = pendingMission[currentMissionSeq];
                
                mavlink_msg_mission_item_int_pack(GCS_SYSTEM, GCS_COMPONENT, &msg,
                    TARGET_SYSTEM, TARGET_COMPONENT,
                    currentMissionSeq,
                    MAV_FRAME_GLOBAL_RELATIVE_ALT, // Standard for ArduRover
                    MAV_CMD_NAV_WAYPOINT,
                    (currentMissionSeq == 0) ? 1 : 0, // current
                    1, // autocontinue
                    0, 0, 0, 0, // params 1-4
                    (int32_t)(wp.lat * 1e7), // Lat
                    (int32_t)(wp.lon * 1e7), // Lon
                    0, // Alt
                    MAV_MISSION_TYPE_MISSION);
                
                sendMavlinkMessage(&msg);
                missionState = WAITING_REQUEST; // Wait for next request or ack
                lastMissionStateChange = now;
            }
        }
        else if ((missionState == WAITING_REQUEST || missionState == SENDING_ITEMS) && 
                 (now - lastMissionStateChange > MISSION_TIMEOUT_MS)) {
            Serial.println("[MAVLink] Mission upload TIMEOUT.");
            missionState = MS_ERROR;
        }

        // 5. Run MechatronicsModule (same core — no cross-core overhead)
        float gs   = Mavlink_GetGroundSpeed();
        float dist = Mavlink_GetDistToWaypoint();
        bool  wpR  = Mavlink_GetWaypointReached();
        bool  eS   = Mavlink_GetEStopActive();

        updateStateMachine(gs, dist, wpR, eS);
        updateSeedingPID(gs);

        // 6. Package Telemetry for Network (at ~5Hz)
        if (now - lastTelemetrySent >= 200) {
            lastTelemetrySent = now;
            
            // Create minimal JSON
            StaticJsonDocument<256> doc;
            doc["lat"]     = (double)current_lat_1e7 / 10000000.0;
            doc["lon"]     = (double)current_lon_1e7 / 10000000.0;
            doc["heading"] = (float)current_heading_cd / 100.0f;
            doc["v_g"]     = current_speed_ms;
            doc["wp_dist"] = current_wp_dist;

            // Planter state from MechatronicsModule
            doc["state"]      = Mechatronics_GetStateString();
            doc["seed_rpm"]   = Mechatronics_GetSeedRPM();

            String json;
            serializeJson(doc, json);
            Network_SendTelemetry(json);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to watchdog / other tasks
    }
}
