#include "NetworkModule.h"
#include "MavlinkModule.h"
#include "MechatronicsModule.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Wi-Fi AP Settings
const char* ssid = "MAIZE_PLANTER_GCS";
const char* password = ""; // Open network for easy field access

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- Flight Log State ---
static bool flightLogActive = false;
static File flightLogFile;

// --- WebSocket Event Handler ---
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
                 
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[Network] WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    } 
    else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[Network] WebSocket client #%u disconnected\n", client->id());
    } 
    else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        
        // Ensure the entire message is contained in one frame
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0; // Null-terminate the string for safety

            // DEBUG: Print the raw incoming payload BEFORE JSON parsing
            Serial.printf("[WS] Raw incoming payload: %s\n", (char*)data);
            
            // Allocate a strict memory limit for the incoming JSON
            // 8192 bytes allows for a substantial number of waypoints
            DynamicJsonDocument doc(8192);
            DeserializationError err = deserializeJson(doc, data);
            
            if (!err) {
                // Check if the payload is an Array (Mission Waypoints)
                if (doc.is<JsonArray>()) {
                    JsonArray array = doc.as<JsonArray>();
                    std::vector<Waypoint> wps;
                    
                    // Reserve vector space to prevent heap fragmentation
                    wps.reserve(array.size());
                    
                    for (JsonObject wpObj : array) {
                        Waypoint w;
                        w.row = wpObj["row"] | 0;
                        w.lat = wpObj["lat"] | 0.0f;
                        w.lon = wpObj["lon"] | 0.0f;
                        w.local_x = wpObj["local_x"] | 0.0f;
                        w.local_y = wpObj["local_y"] | 0.0f;
                        wps.push_back(w);
                    }
                    
                    Serial.printf("[Network] Parsed %d waypoints from WS. Passing to MAVLink...\n", wps.size());
                    Mavlink_UploadWaypoints(wps);
                } 
                // Check if the payload is an Object (Commands like E-STOP)
                else if (doc.is<JsonObject>()) {
                    JsonObject obj = doc.as<JsonObject>();
                    if (obj["command"] == "ESTOP") {
                        Serial.println("[Network] E-STOP COMMAND RECEIVED via WS! Notifying MAVLink Module...");
                        Mavlink_TriggerEStop();
                    }
                    else if (obj["command"] == "MISSION") {
                        // Parse waypoints array from the MISSION payload
                        JsonArray wpArray = obj["waypoints"].as<JsonArray>();
                        std::vector<Waypoint> wps;
                        wps.reserve(wpArray.size());

                        for (JsonObject wpObj : wpArray) {
                            Waypoint w;
                            w.row     = wpObj["row"] | 0;
                            w.lat     = wpObj["lat"] | 0.0f;
                            w.lon     = wpObj["lon"] | 0.0f;
                            w.local_x = wpObj["local_x"] | 0.0f;
                            w.local_y = wpObj["local_y"] | 0.0f;
                            wps.push_back(w);
                        }

                        Serial.printf("[Network] MISSION COMMAND — %d waypoints parsed. Uploading to Pixhawk...\n", wps.size());
                        Mavlink_UploadWaypoints(wps);

                        // Arm the MechatronicsModule FSM so IDLE can transition to DEPLOYING
                        Mechatronics_StartMission();
                        Serial.println("[Network] MechatronicsModule mission flag SET.");
                    }
                    else if (obj["command"] == "START_LOG") {
                        if (!flightLogActive) {
                            flightLogFile = LittleFS.open("/flight_log.csv", FILE_APPEND);
                            if (flightLogFile) {
                                // Write CSV header if file is empty
                                if (flightLogFile.size() == 0) {
                                    flightLogFile.println("Timestamp,Lat,Lon,Heading,Speed,DistToWP,State,Latency");
                                }
                                flightLogActive = true;
                                Serial.println("[Network] Flight log STARTED.");
                            } else {
                                Serial.println("[Network] ERROR: Could not open /flight_log.csv");
                            }
                        }
                    }
                    else if (obj["command"] == "STOP_LOG") {
                        if (flightLogActive) {
                            flightLogFile.close();
                            flightLogActive = false;
                            Serial.println("[Network] Flight log STOPPED.");
                        }
                    }
                    else if (obj["command"] == "PING") {
                        // Extract timestamp as a string to prevent 32-bit overflow
                        const char* ts = obj["timestamp"] | "0";
                        char pong[96];
                        snprintf(pong, sizeof(pong), "{\"type\":\"PONG\",\"timestamp\":\"%s\"}", ts);
                        client->text(pong);
                    }
                }
            } else {
                Serial.printf("[Network] JSON Parse failed: %s\n", err.c_str());
            }
        }
    }
}

// --- Broadcast Telemetry to UI ---
void Network_SendTelemetry(const String& json) {
    // Non-blocking 5Hz throttle — prevents WebSocket TX buffer overflow
    // (Error 1006) when the caller fires faster than the async send can drain.
    static unsigned long lastWsTime = 0;
    unsigned long now = millis();
    if (now - lastWsTime < 200) return; // Discard if < 200ms since last send
    lastWsTime = now;

    // Only broadcast if at least one client is connected
    if (ws.count() > 0) {
        ws.textAll(json);
    }
}

// --- Flight Log Append (called at 1Hz from Mavlink_Task) ---
void Network_AppendFlightLog(float lat, float lon, float heading, float speed,
                             float wpDist, const char* state, int latency) {
    if (!flightLogActive || !flightLogFile) return;
    // Guard: do not write rows with empty state (e.g. from stray PING frames)
    if (state == nullptr || state[0] == '\0') return;

    flightLogFile.printf("%lu,%.6f,%.6f,%.1f,%.2f,%.2f,%s,%d\n",
                         millis(), lat, lon, heading, speed, wpDist, state, latency);
    flightLogFile.flush();
}

// --- Flight Log Status Getter ---
bool Network_IsLogging() {
    return flightLogActive;
}

void Network_Init() {
    Serial.println("[Network] Starting Access Point...");
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("[Network] AP IP Address: ");
    Serial.println(IP);

    if(!LittleFS.begin(true)){
        Serial.println("[Network] LittleFS Mount Failed. Check data upload.");
        // We continue anyway, but UI won't load
    } else {
        Serial.println("[Network] LittleFS Mounted Successfully.");
    }

    // Serve static files from LittleFS — explicit routes to prevent
    // ESPAsyncWebServer from searching for non-existent .gz variants.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/script.js", "application/javascript");
    });
    server.on("/geo_math.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/geo_math.js", "application/javascript");
    });
    server.on("/path_planner.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/path_planner.js", "application/javascript");
    });

    // Setup WebSocket
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    server.begin();
    Serial.println("[Network] AsyncWebServer started on port 80.");
}

// --- Main Network Task (Core 0) ---
void Network_Task(void *pvParameters) {
    while (true) {
        // Handle WebSocket client disconnection cleanup
        ws.cleanupClients();
        
        // Yield to other processes (like WiFi handling) on Core 0
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
