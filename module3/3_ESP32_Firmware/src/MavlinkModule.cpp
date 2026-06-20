#include "MavlinkModule.h"
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

// E-STOP Flag
static bool estopPending = false;

// Mission Upload State
static std::vector<Waypoint> pendingMission;
static bool uploadRequested = false;
static uint16_t currentMissionSeq = 0;
enum MissionState { IDLE, SENDING_COUNT, WAITING_REQUEST, SENDING_ITEMS, ACKNOWLEDGED, ERROR };
static MissionState missionState = IDLE;
static unsigned long lastMissionStateChange = 0;
#define MISSION_TIMEOUT_MS 3000

// Telemetry State
static int32_t current_lat_1e7 = 0;
static int32_t current_lon_1e7 = 0;
static uint16_t current_heading_cd = 0; // centidegrees
static float current_speed_ms = 0.0f;
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
    portEXIT_CRITICAL(&stateMux);
}

// --- Upload Mission (called from Network Core) ---
void Mavlink_UploadWaypoints(const std::vector<Waypoint>& wps) {
    portENTER_CRITICAL(&stateMux);
    pendingMission = wps;
    uploadRequested = true;
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
            current_speed_ms = hud.groundspeed;
            // DEBUG: Print raw ground speed for engineering verification
            Serial.printf("[MAVLink] Raw Ground Speed: %.2f m/s\n", hud.groundspeed);
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
                missionState = ERROR;
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

        // 3. Handle E-STOP (RC Override)
        bool performEstop = false;
        portENTER_CRITICAL(&stateMux);
        if (estopPending) {
            performEstop = true;
            estopPending = false; // Reset after reading
        }
        portEXIT_CRITICAL(&stateMux);

        if (performEstop) {
            Serial.println("[MAVLink] Executing E-STOP. Overriding RC 1 (Steering) & 3 (Throttle) to 1500us neutral.");
            // RC Channels Override: Channel 1 and 3 to 1500us. Others to UINT16_MAX (ignore).
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
            missionState = ERROR;
        }

        // 5. Package Telemetry for Network (at ~5Hz)
        if (now - lastTelemetrySent >= 200) {
            lastTelemetrySent = now;
            
            // Create minimal JSON
            StaticJsonDocument<256> doc;
            doc["lat"] = (float)current_lat_1e7 / 1e7;
            doc["lon"] = (float)current_lon_1e7 / 1e7;
            doc["heading"] = (float)current_heading_cd / 100.0f;
            doc["v_g"] = current_speed_ms;
            
            // Add pseudo-state based on E-STOP
            // Note: A true system state would come from MAVLink HEARTBEAT base_mode/custom_mode
            doc["state"] = performEstop ? "E-STOP" : "AUTO"; 

            String json;
            serializeJson(doc, json);
            Network_SendTelemetry(json);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to watchdog / other tasks
    }
}
