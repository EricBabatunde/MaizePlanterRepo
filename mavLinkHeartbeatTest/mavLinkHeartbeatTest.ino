#include <HardwareSerial.h>
#include "C:\Users\ACER\Documents\Arduino\libraries\MAVLink\mavlink\common/mavlink.h" //

// Use Hardware Serial 1 for MAVLink
HardwareSerial MavSerial(1); 

void setup() {
  Serial.begin(115200);
  // RX=4, TX=5 (Match your hardware wiring)
  MavSerial.begin(57600, SERIAL_8N1, 4, 5); 
  Serial.println("MAVLink Layer Initialised...");
}

void loop() {
  // 1. Send Heartbeat at 1Hz
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 1000) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  // 2. Listen for Data
  receiveMavlink();
}

void sendHeartbeat() {
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];
  
  // Define system as a "GCS" or "Companion Computer"
  mavlink_msg_heartbeat_pack(1, 191, &msg, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, 0);
  uint16_t len
   = mavlink_msg_to_send_buffer(buf, &msg);
  MavSerial.write(buf, len);
}

void receiveMavlink() {
  mavlink_message_t msg;
  mavlink_status_t status;

  while (MavSerial.available() > 0) {
    uint8_t c = MavSerial.read();
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      // Decode Ground Speed from VFR_HUD message
      if (msg.msgid == MAVLINK_MSG_ID_VFR_HUD) {
        mavlink_vfr_hud_t hud;
        mavlink_msg_vfr_hud_decode(&msg, &hud);
        Serial.print("Ground Speed (m/s): ");
        Serial.println(hud.groundspeed);
      }
    }
  }
}