#include "telemetry_mqtt.h"
#include "waypoint_manager.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- Internal Objects ---
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// --- MQTT Callback (Fires when a message arrives) ---
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.println("\n====================================");
    Serial.print("[MQTT] Message arrived on topic: ");
    Serial.println(topic);

    // If it is our mission payload, parse it!
    if (String(topic) == "maizepro/mission")
    {
        Serial.println("[MQTT] Parsing Mission JSON...");

        // Allocate a JSON document. Size depends on array length.
        // 4096 bytes is plenty for our bench-test (about 40-50 waypoints).
        DynamicJsonDocument doc(32768);

        DeserializationError error = deserializeJson(doc, payload, length);
        if (error)
        {
            Serial.print("[ERROR] JSON Deserialization failed: ");
            Serial.println(error.c_str());
            return;
        }

        // Iterate through the JSON array
        JsonArray array = doc.as<JsonArray>();

        // --- NEW INTEGRATION CODE ---
        NavWaypoint parsedPoints[MAX_WAYPOINTS];
        uint16_t waypointCount = 0;

        for (JsonObject wp : array)
        {
            if (waypointCount >= MAX_WAYPOINTS)
                break; // Prevent buffer overflow

            parsedPoints[waypointCount].row = wp["row"];
            parsedPoints[waypointCount].lat = wp["lat"];
            parsedPoints[waypointCount].lon = wp["lon"];

            waypointCount++;
        }

        Serial.print("[SYSTEM] Passing ");
        Serial.print(waypointCount);
        Serial.println(" waypoints to MAVLink Manager...");

        // Trigger the Pixhawk Handshake!
        startMissionUpload(parsedPoints, waypointCount);
        // ----------------------------

        Serial.println("====================================\n");
    }
    // --- NEW: Handle Live Commands ---
    else if (String(topic) == "maizepro/command")
    {

        // We only need a tiny buffer for a simple command
        DynamicJsonDocument doc(256);
        deserializeJson(doc, payload, length);

        String command = doc["action"];
        Serial.print("[MQTT] Received Command: ");
        Serial.println(command);

        if (command == "ARM_AUTO")
        {
            commandArmAndAuto(); // Call the MAVLink function we just wrote!
        }
    }
}

// --- Network Reconnection Logic ---
void reconnectMQTT()
{
    // Loop until we're reconnected
    while (!mqttClient.connected())
    {
        Serial.print("[MQTT] Attempting connection to Broker...");
        // Create a random client ID
        String clientId = "ESP32_Planter_";
        clientId += String(random(0xffff), HEX);

        // Attempt to connect
        if (mqttClient.connect(clientId.c_str()))
        {
            Serial.println(" CONNECTED!");
            // Subscribe to the mission topic immediately upon connection
            mqttClient.subscribe("maizepro/mission");
            // NEW: Subscribe to live commands (Arm/E-Stop)
            mqttClient.subscribe("maizepro/command");
        }
        else
        {
            Serial.print(" FAILED, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" -> Retrying in 3 seconds...");
            delay(3000); // Only place we use delay, as it's a blocking failure state
        }
    }
}

// --- Public Setup Function ---
void setupNetwork()
{
    Serial.print("[WIFI] Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n[WIFI] Connected! ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    // Configure MQTT
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

    // --> THE FIX: Increase the MQTT buffer size to handle massive JSON payloads <--
    mqttClient.setBufferSize(8192);

    mqttClient.setCallback(mqttCallback);
}

// --- Public Loop Function ---
void loopNetwork()
{
    if (!mqttClient.connected())
    {
        reconnectMQTT();
    }
    // Critical: this keeps the MQTT connection alive and polls for incoming data
    mqttClient.loop();
}

// --- Dummy Telemetry Publisher ---
// void publishDummyTelemetry()
// {
//     if (mqttClient.connected())
//     {
//         // Create a tiny JSON string simulating Pixhawk telemetry
//         String payload = "{\"system_state\":\"CRUISE\", \"ground_speed\": 1.05, \"heading\": 45.2}";
//         mqttClient.publish("maizepro/telemetry", payload.c_str());
//     }
// }

// --- Live Telemetry Publisher ---
void publishLiveTelemetry(String systemState, float speed, float heading, double lat, double lon)
{
    if (mqttClient.connected())
    {
        // Allocate a small document for outgoing telemetry
        StaticJsonDocument<256> doc;

        doc["system_state"] = systemState;
        doc["ground_speed"] = speed;
        doc["heading"] = heading;

        // Only publish GPS if we have a valid reading (not exactly 0.0)
        if (lat != 0.0 && lon != 0.0)
        {
            doc["lat"] = lat;
            doc["lon"] = lon;
        }

        char buffer[256];
        serializeJson(doc, buffer);
        mqttClient.publish("maizepro/telemetry", buffer);
    }
}
