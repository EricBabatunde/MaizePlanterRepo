#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h> // Uncomment when ready to log
#include <HardwareSerial.h> // Uncomment when ready for MAVLink
#include "C:\Users\ACER\Documents\Arduino\libraries\MAVLink\mavlink\common/mavlink.h" //


// --- Configuration ---

// Pins
// --- Motor Pins (Standard ESP32 Safe Pins) ---
const int PIN_L_RPWM = 11; // Left Motor Forward
const int PIN_L_LPWM = 12; // Left Motor Reverse
const int PIN_R_RPWM = 13; // Right Motor Forward
const int PIN_R_LPWM = 14; // Right Motor Reverse

// PWM Channels (v2.0.17 Core)
const int CH_L_R = 0;
const int CH_L_L = 1;
const int CH_R_R = 2;
const int CH_R_L = 3;

// State Machine
enum SystemState { MANUAL, CRUISE_A, CRUISE_B, CRUISE_C, E_STOP };
SystemState currentState = MANUAL;

// Cruise Variables
int currentSweepPWM = 1500;
int sweepUpperLimit = 1500;
unsigned long lastStepTime = 0;
const int STEP_DELAY_MS = 2000; 
const int PWM_INCREMENT = 10;   

// Global Target Variables
int targetLeftRC = 1500;
int targetRightRC = 1500;
float currentGroundSpeed = 0.0;

// MAVLink Serial
HardwareSerial MavSerial(1);

// Web Server
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- HTML / UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Planter Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <script src="nipplejs.min.js"></script>
  <style>
    body { text-align: center; background: #1e1e1e; color: white; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 10px; }
    #status { background: #e67e22; padding: 5px; font-weight: bold; border-radius: 5px; margin-bottom: 15px; }
    .btn { padding: 15px 10px; border-radius: 8px; border: none; margin: 5px; font-weight: bold; font-size: 14px; cursor: pointer; color: white; }
    .btn-bracket { background: #3498db; width: 30%; }
    .btn-estop { background: #e74c3c; width: 95%; height: 60px; font-size: 20px; margin-top: 15px; }
    .btn-manual { background: #95a5a6; width: 95%; margin-top: 10px; padding: 10px; }
    #joystick-container { display: flex; justify-content: space-between; height: 180px; margin-top: 20px; padding: 0 10px; }
    .joy-zone { position: relative; width: 45%; height: 100%; background: #2c3e50; border-radius: 15px; display: flex; align-items: center; justify-content: center; font-size: 12px; color: #7f8c8d; text-align: center; }
    .panel { background: #2c3e50; padding: 10px; border-radius: 10px; margin-bottom: 10px; }
  </style>
</head>
<body>
  <h2>Alpha Planter UI</h2>
  <div id="status">Connecting to ESP32...</div>
  
  <div class="panel">
    <h3>Calibration Brackets</h3>
    <button class="btn btn-bracket" onclick="sendCmd('BA')">A<br>(1550-1650)</button>
    <button class="btn btn-bracket" onclick="sendCmd('BB')">B<br>(1650-1750)</button>
    <button class="btn btn-bracket" onclick="sendCmd('BC')">C<br>(1750-1850)</button>
    <button class="btn btn-manual" onclick="sendCmd('MAN')">Return to MANUAL</button>
  </div>

  <button class="btn btn-estop" onclick="sendCmd('ESTOP')">SOFTWARE E-STOP</button>

  <div id="joystick-container">
    <div id="left-joy" class="joy-zone">Left Track</div>
    <div id="right-joy" class="joy-zone">Right Track</div>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    // 1. Robust WebSocket Initialization
    function initWebSocket() {
      console.log('Trying to open a WebSocket connection...');
      websocket = new WebSocket(gateway);
      
      websocket.onopen = function(event) {
        document.getElementById('status').innerText = "🟢 SYSTEM CONNECTED";
        document.getElementById('status').style.background = "#2ecc71";
      };
      
      websocket.onclose = function(event) {
        document.getElementById('status').innerText = "🔴 DISCONNECTED. Retrying...";
        document.getElementById('status').style.background = "#e74c3c";
        setTimeout(initWebSocket, 2000);
      };
    }

    // 2. Safe Command Sender
    function sendCmd(cmd) { 
      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("CMD:" + cmd); 
      } else {
        alert("Cannot send command: ESP32 disconnected!");
      }
    }

    // START CONNECTION IMMEDIATELY (Don't wait for joysticks)
    initWebSocket();

    // 3. Wait for external files before trying joysticks
    window.addEventListener('load', function() {
      try {
        if (typeof nipplejs === 'undefined') throw new Error("nipplejs not loaded");
        
        // Clear the fallback text
        document.getElementById('left-joy').innerText = ""; 
        document.getElementById('right-joy').innerText = "";
        
        // Initialize the joysticks
        var joyL = nipplejs.create({zone: document.getElementById('left-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'cyan', lockY: true});
        var joyR = nipplejs.create({zone: document.getElementById('right-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'magenta', lockY: true});

        // --- THE TRANSMITTER LOGIC ---
        
        // 1. Variables to hold the current position
        var joyLY = 0.00;
        var joyRY = 0.00;

        // 2. Update the variables only when the stick moves
        joyL.on('move', function (evt, data) { joyLY = data.vector.y; });
        joyR.on('move', function (evt, data) { joyRY = data.vector.y; });

        // 3. Reset to zero and immediately send the stop command when released
        joyL.on('end', function () { 
          joyLY = 0.00; 
          if(websocket && websocket.readyState === WebSocket.OPEN) websocket.send("JL:0.00"); 
        });
        joyR.on('end', function () { 
          joyRY = 0.00; 
          if(websocket && websocket.readyState === WebSocket.OPEN) websocket.send("JR:0.00"); 
        });

        // 4. The "Transmitter Loop": Broadcasts current state every 100ms (10Hz)
        setInterval(function() {
          if(websocket && websocket.readyState === WebSocket.OPEN) {
            // Only broadcast if the stick is actively being held away from center
            if (joyLY !== 0) websocket.send("JL:" + joyLY.toFixed(2));
            if (joyRY !== 0) websocket.send("JR:" + joyRY.toFixed(2));
          }
        }, 100); 

      } catch (error) {
        // Failsafe if the local file didn't load
        console.error("Joystick Error:", error);
        document.getElementById('left-joy').innerText = "JS Load Failed";
        document.getElementById('right-joy').innerText = "JS Load Failed";
      }
    });
  </script>
</body></html>
)rawliteral";

// --- Logic Functions ---

void startBracket(SystemState newState, int startPWM, int limitPWM, String bracketName) {
  currentState = newState;
  currentSweepPWM = startPWM;
  sweepUpperLimit = limitPWM;
  lastStepTime = millis();
  
  Serial.printf("\n[UI] Bracket %s Triggered\n", bracketName.c_str());
  Serial.println("[CRUISE] State: AUTO");
  Serial.printf("[CRUISE] Target: %dus. Stabilising (%dms)...\n", currentSweepPWM, STEP_DELAY_MS);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;
    
    if (msg.startsWith("CMD:")) {
      String cmd = msg.substring(4);
      
      if (cmd == "ESTOP") {
        currentState = E_STOP;
        currentSweepPWM = 1500;
        Serial.println("\n[UI] *** SOFTWARE E-STOP TRIGGERED ***");
      } 
      else if (cmd == "MAN") {
        currentState = MANUAL;
        currentSweepPWM = 1500;
        Serial.println("\n[UI] Switched to MANUAL mode.");
      }
      else if (currentState == MANUAL) { 
        if (cmd == "BA") startBracket(CRUISE_A, 1550, 1650, "A"); 
        if (cmd == "BB") startBracket(CRUISE_B, 1650, 1750, "B"); 
        if (cmd == "BC") startBracket(CRUISE_C, 1750, 1850, "C"); 
      } else {
        Serial.println("[UI] Cannot start bracket. Not in MANUAL mode.");
      }
    } 
    else if (msg.startsWith("JL:") && currentState == MANUAL) {
      float yVal = msg.substring(3).toFloat();
      targetLeftRC = 1500 + (int)(yVal * 500); 
    }
    else if (msg.startsWith("JR:") && currentState == MANUAL) {
      float yVal = msg.substring(3).toFloat();
      targetRightRC = 1500 + (int)(yVal * 500); 
    }
  }
}

// Declare onEvent BEFORE setup()
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

void setup() {
  Serial.begin(115200);
  
  // --- v2.0.17 PWM Setup ---

  // --- MAVLink Setup ---
  MavSerial.begin(57600, SERIAL_8N1, 4, 5); // RX=4, TX=5

  // --- BTS7960 PWM Setup (8-bit, 5kHz) ---
  ledcSetup(CH_L_R, 5000, 8); ledcAttachPin(PIN_L_RPWM, CH_L_R);
  ledcSetup(CH_L_L, 5000, 8); ledcAttachPin(PIN_L_LPWM, CH_L_L);
  ledcSetup(CH_R_R, 5000, 8); ledcAttachPin(PIN_R_RPWM, CH_R_R);
  ledcSetup(CH_R_L, 5000, 8); ledcAttachPin(PIN_R_LPWM, CH_R_L);
  
  // Ensure motors start at 0
  setMotors(1500, 1500);

  // --- ADD THIS STA BLOCK ---
  Serial.println("\n--- Alpha Planter Boot Sequence ---");
  WiFi.mode(WIFI_STA);
  WiFi.begin("MTN_4G_D874BD", "2GA32EU42B"); // <-- CHANGE THESE
  
  Serial.print("Connecting to Home Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("New ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Set up WebSocket and Web Server
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
  }

  // Serve the local joystick file
  server.serveStatic("/nipplejs.min.js", LittleFS, "/nipplejs.min.js");

  // Keep your existing handlers
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  server.begin();
  Serial.println("Web server started. Ready for bench test.");
}

void loop() {
  receiveMavlink();
  
  if (currentState == MANUAL) {
    setMotors(targetLeftRC, targetRightRC);
  } 
  else if (currentState == E_STOP) {
    setMotors(1500, 1500); // Hard kill
  }
  else {
    // --- Automated Cruise Ramping Logic ---
    if (millis() - lastStepTime > STEP_DELAY_MS) {
      Serial.printf("[LOGGING] %dus | Speed: %.2f m/s\n", currentSweepPWM, currentGroundSpeed);
      
      currentSweepPWM += PWM_INCREMENT;
      
      if (currentSweepPWM > sweepUpperLimit) {
        Serial.println("[CRUISE] Bracket complete. Returning to MANUAL mode.");
        currentState = MANUAL;
        currentSweepPWM = 1500;
        targetLeftRC = 1500;
        targetRightRC = 1500;
      } else {
        Serial.printf("[CRUISE] Target: %dus. Stabilising...\n", currentSweepPWM);
      }
      lastStepTime = millis(); 
    }
    
    // Apply automated sweep values equally to both front tracks
    setMotors(currentSweepPWM, currentSweepPWM);
  }
}

// --- HARDWARE STUBS (Empty for bench testing UI) ---
// --- HARDWARE FUNCTIONS ---

void setMotors(int leftRC, int rightRC) {
  // Constrain inputs to standard RC range
  leftRC = constrain(leftRC, 1000, 2000);
  rightRC = constrain(rightRC, 1000, 2000);

  // Deadband to prevent motor jitter when sticks are centered
  int deadband = 20; 

  // --- LEFT MOTOR ---
  if (leftRC > (1500 + deadband)) {
    int pwm = map(leftRC, 1500, 2000, 0, 255);
    ledcWrite(CH_L_R, pwm); // Forward
    ledcWrite(CH_L_L, 0);
  } else if (leftRC < (1500 - deadband)) {
    int pwm = map(leftRC, 1480, 1000, 0, 255);
    ledcWrite(CH_L_R, 0);
    ledcWrite(CH_L_L, pwm); // Reverse
  } else {
    ledcWrite(CH_L_R, 0);
    ledcWrite(CH_L_L, 0);
  }

  // --- RIGHT MOTOR ---
  if (rightRC > (1500 + deadband)) {
    int pwm = map(rightRC, 1500, 2000, 0, 255);
    ledcWrite(CH_R_R, pwm);
    ledcWrite(CH_R_L, 0);
  } else if (rightRC < (1500 - deadband)) {
    int pwm = map(rightRC, 1480, 1000, 0, 255);
    ledcWrite(CH_R_R, 0);
    ledcWrite(CH_R_L, pwm);
  } else {
    ledcWrite(CH_R_R, 0);
    ledcWrite(CH_R_L, 0);
  }
}

void receiveMavlink() {
  mavlink_message_t msg;
  mavlink_status_t status;

  while (MavSerial.available()) {
    uint8_t c = MavSerial.read();
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      
      // Look for the Heads Up Display packet which contains ground speed
      if (msg.msgid == MAVLINK_MSG_ID_VFR_HUD) {
        mavlink_vfr_hud_t hud;
        mavlink_msg_vfr_hud_decode(&msg, &hud);
        currentGroundSpeed = hud.groundspeed; 
      }
    }
  }
}
void logToFS(int pwm, float speed) {
  // Will be implemented in hardware phase
}
