#include "MavlinkHandler.h"
#include <HardwareSerial.h>

// Ensure this path matches your local library structure
#include "../lib/MAVLink/mavlink/common/mavlink.h"

// Create the hardware serial instance for the Pixhawk (TELEM 2)
HardwareSerial MavSerial(1);

// Standard ArduPilot System IDs
const uint8_t SYS_ID = 255; // GCS ID
const uint8_t COMP_ID = 1;  // Primary Component
const uint8_t TARG_SYS = 1; // Pixhawk ID

void initMavlink()
{
  MavSerial.begin(57600, SERIAL_8N1, PIN_MAV_RX, PIN_MAV_TX);
}

void receiveMavlink()
{
  mavlink_message_t msg;
  mavlink_status_t status;

  while (MavSerial.available())
  {
    uint8_t c = MavSerial.read();

    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status))
    {
      // Extract Ground Speed
      if (msg.msgid == MAVLINK_MSG_ID_VFR_HUD)
      {
        mavlink_vfr_hud_t hud;
        mavlink_msg_vfr_hud_decode(&msg, &hud);
        currentGroundSpeed = hud.groundspeed;
      }
      // Extract Flight Mode
      else if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT)
      {
        mavlink_heartbeat_t hb;
        mavlink_msg_heartbeat_decode(&msg, &hb);

        // Decode ArduRover specific modes
        if (hb.custom_mode == 0)
          currentPixhawkMode = "MANUAL";
        else if (hb.custom_mode == 1)
          currentPixhawkMode = "ACRO";
        else if (hb.custom_mode == 3)
          currentPixhawkMode = "STEERING";
        else if (hb.custom_mode == 4)
          currentPixhawkMode = "HOLD";
        else if (hb.custom_mode == 10)
          currentPixhawkMode = "AUTO";
        else
          currentPixhawkMode = "MODE_" + String(hb.custom_mode);
      }
    }
  }
}

void sendRCOverride(uint16_t steerRC, uint16_t throttleRC)
{
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];

  // MAVLink v2 requires 18 channel arguments.
  // UINT16_MAX (65535) means "Release control / Do not override this channel".
  mavlink_msg_rc_channels_override_pack(SYS_ID, COMP_ID, &msg, TARG_SYS, 1,
                                        steerRC,                                                     // CH1: Steering
                                        UINT16_MAX,                                                  // CH2: (Ignored)
                                        throttleRC,                                                  // CH3: Throttle
                                        UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,  // CH4 - CH8
                                        UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,  // CH9 - CH13
                                        UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX); // CH14 - CH18

  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  MavSerial.write(buf, len);
}

void requestPixhawkMode(uint32_t custom_mode)
{
  mavlink_message_t msg;
  uint8_t buf[MAVLINK_MAX_PACKET_LEN];

  // Base mode 1 = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
  mavlink_msg_set_mode_pack(SYS_ID, COMP_ID, &msg, TARG_SYS, 1, custom_mode);

  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
  MavSerial.write(buf, len);
}