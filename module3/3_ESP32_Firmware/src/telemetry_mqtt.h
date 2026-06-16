#ifndef TELEMETRY_MQTT_H
#define TELEMETRY_MQTT_H

#include <Arduino.h>

// --- Configuration Constants ---
// YOU MUST CHANGE THESE TO MATCH YOUR SETUP
const char *const WIFI_SSID = "MTN_4G_D874BD";
const char *const WIFI_PASSWORD = "2GA32EU42B";
const char *const MQTT_SERVER = "192.168.0.2"; // Your Ubuntu Laptop's IPv4 Address
const int MQTT_PORT = 1883;

// --- Public Functions ---
void setupNetwork();
void loopNetwork();
// void publishDummyTelemetry(); // Used for our bench-test heartbeat

void publishLiveTelemetry(String systemState, float speed, float heading, double lat, double lon);

#endif // TELEMETRY_MQTT_H