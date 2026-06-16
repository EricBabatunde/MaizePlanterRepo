#include "MavlinkHandler.h"
#include <HardwareSerial.h>

// If this include throws an error, change it back to your absolute path:
#include "../lib/MAVLink/mavlink/common/mavlink.h"

// #include <mavlink.h>

// Create the hardware serial instance for the Pixhawk
HardwareSerial MavSerial(1);

void initMavlink()
{
  // Start serial on the pins defined in Config.h
  MavSerial.begin(57600, SERIAL_8N1, PIN_MAV_RX, PIN_MAV_TX);
}

void receiveMavlink()
{
  mavlink_message_t msg;
  mavlink_status_t status;

  // Process all available bytes from the Pixhawk
  while (MavSerial.available())
  {
    uint8_t c = MavSerial.read();

    // Parse the byte into a MAVLink message
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status))
    {

      // We only care about the HUD packet right now (for velocity)
      if (msg.msgid == MAVLINK_MSG_ID_VFR_HUD)
      {
        mavlink_vfr_hud_t hud;
        mavlink_msg_vfr_hud_decode(&msg, &hud);

        // Update the global shared variable
        currentGroundSpeed = hud.groundspeed;
      }
    }
  }
}