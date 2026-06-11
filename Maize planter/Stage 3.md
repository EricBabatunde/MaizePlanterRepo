
This digital link allows the ESP32-S3 to act as the "GCS" (Ground Control Station), commanding the Pixhawk and receiving real-time telemetry like ground speed for your 30cm seeding logic.
### **3.1. Hardware: The Telemetry Link**

The Pixhawk’s **TELEM 2** port uses a 6-pin DF13 or JST-GH connector. You only need three connections to the ESP32-S3.

|**Pixhawk TELEM 2 Pin**|**ESP32-S3 Pin (Example)**|**Description**|
|---|---|---|
|**TX (Pin 2)**|**GPIO 4 (RX)**|Data from Pixhawk to ESP32.|
|**RX (Pin 3)**|**GPIO 5 (TX)**|Data from ESP32 to Pixhawk.|
|**GND (Pin 6)**|**GND**|Mandatory common ground reference.|

- **Warning:** Do **not** connect the Red VCC (5V) wire from the TELEM port to the ESP32-S3 if your ESP32 is already powered via USB or another regulator.
    
### **3.2. Software: Mission Planner Configuration**

You must tell the Pixhawk to "speak" MAVLink on its second telemetry port.

1. Connect Pixhawk to Mission Planner via USB.
    
2. Go to **CONFIG** > **Full Parameter List**.
    
3. Set **`SERIAL2_PROTOCOL`** to **`2`** (MAVLink 2).
    
4. Set **`SERIAL2_BAUD`** to **`57`** (57600 baud).
    
5. Click **Write Params**.
    
### **3.3. Software: ESP32-S3 MAVLink "Heartbeat"**

The ESP32-S3 must send a constant "Heartbeat" to the Pixhawk to maintain the link.
#### **1. Get the Library**

Ensure you have the **MAVLink library** in your Arduino `libraries` folder as discussed earlier: Out of the libraries you provided, the **`okalachev/mavlink-arduino`** version is the most robust and "Arduino-ready". It is structured specifically to work with the Arduino IDE’s library manager logic.

#### **2. Upload the Listener Code**

This code establishes the link and prints the Pixhawk's ground speed—which you will eventually use to calculate your 30cm seed drop intervals.

C++

```
#include <HardwareSerial.h>
#include "common/mavlink.h" //

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
  uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
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
```

---

#### **3. Testing Stage 3**

1. Open the Serial Monitor on the ESP32-S3.
    
2. If the wiring is correct and the parameters are written, you should see **"Ground Speed: 0.00"** appearing.
    
3. **Physical Check:** Pick up the Pixhawk/GPS assembly and walk briskly. You should see the speed values in the Serial Monitor increase.



