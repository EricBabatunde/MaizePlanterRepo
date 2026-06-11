#include <WiFi.h>
#include <ESPAsyncWebServer.h>
// #include <LittleFS.h> // Uncomment when ready to log
// #include <HardwareSerial.h> // Uncomment when ready for MAVLink

// --- Configuration ---
const char* ssid = "MaizePlanter_Alpha";
const char* password = "mechatronics";

// Pins
const int PIN_STEER = 18;
const int PIN_THROTTLE = 19;
const int PIN_PRESSWHEEL = 21;

// PWM Channels (Required for ESP32 Core v2.x)
const int THROTTLE_CHANNEL = 0;
const int STEER_CHANNEL = 1;

// State Machine
enum SystemState { MANUAL, CRUISE_A, CRUISE_B, CRUISE_C, E_STOP };
SystemState currentState = MANUAL;

// Cruise Variables
int currentSweepPWM = 1500;
int sweepUpperLimit = 1500;
unsigned long lastStepTime = 0;
const int STEP_DELAY_MS = 2000; 
const int PWM_INCREMENT = 10;   

// Web Server
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- HTML / UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Planter Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <script src="https://cdn.jsdelivr.net/npm/nipplejs@0.9.0/dist/nipplejs.min.js"></script>
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
        
        document.getElementById('left-joy').innerText = ""; 
        document.getElementById('right-joy').innerText = "";
        
        var joyL = nipplejs.create({zone: document.getElementById('left-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'cyan', lockY: true});
        var joyR = nipplejs.create({zone: document.getElementById('right-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'magenta', lockY: true});

        joyL.on('move', function (evt, data) { if(websocket.readyState === WebSocket.OPEN) websocket.send("JL:" + data.vector.y.toFixed(2)); });
        joyL.on('end', function () { if(websocket.readyState === WebSocket.OPEN) websocket.send("JL:0.00"); });
        
        joyR.on('move', function (evt, data) { if(websocket.readyState === WebSocket.OPEN) websocket.send("JR:" + data.vector.y.toFixed(2)); });
        joyR.on('end', function () { if(websocket.readyState === WebSocket.OPEN) websocket.send("JR:0.00"); });
      } catch (error) {
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
      int pwmOut = 1500 + (int)(yVal * 500); 
      Serial.printf("[MANUAL] Left Joy: %.2f -> Commanded PWM: %dus\n", yVal, pwmOut);
    }
    else if (msg.startsWith("JR:") && currentState == MANUAL) {
      float yVal = msg.substring(3).toFloat();
      int pwmOut = 1500 + (int)(yVal * 500); 
      Serial.printf("[MANUAL] Right Joy: %.2f -> Commanded PWM: %dus\n", yVal, pwmOut);
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
  ledcSetup(THROTTLE_CHANNEL, 50, 16); // Channel 0, 50Hz, 16-bit
  ledcAttachPin(PIN_THROTTLE, THROTTLE_CHANNEL); 
  
  ledcSetup(STEER_CHANNEL, 50, 16);    // Channel 1, 50Hz, 16-bit
  ledcAttachPin(PIN_STEER, STEER_CHANNEL);
  
  pinMode(PIN_PRESSWHEEL, OUTPUT);

  // Set up Access Point
  // WiFi.softAP(ssid, password);
  // Serial.println("\n--- Alpha Planter Boot Sequence ---");
  // Serial.print("AP IP Address: ");
  // Serial.println(WiFi.softAPIP());

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
  
  server.begin();
  Serial.println("Web server started. Ready for bench test.");
}

void loop() {
  // --- Automated Cruise Ramping Logic ---
  if (currentState == CRUISE_A || currentState == CRUISE_B || currentState == CRUISE_C) {
    if (millis() - lastStepTime > STEP_DELAY_MS) {
      Serial.printf("[LOGGING] %dus | Simulating Data Write to LittleFS...\n", currentSweepPWM);
      
      currentSweepPWM += PWM_INCREMENT;
      
      if (currentSweepPWM > sweepUpperLimit) {
        Serial.println("[CRUISE] Bracket complete. Returning to MANUAL mode (1500us).");
        currentState = MANUAL;
        currentSweepPWM = 1500;
      } else {
        Serial.printf("[CRUISE] Target: %dus. Stabilising (%dms)....\n", currentSweepPWM, STEP_DELAY_MS);
      }
      lastStepTime = millis(); 
    }
  }
}

// --- HARDWARE STUBS (Empty for bench testing UI) ---
void receiveMavlink() {
  // Will be implemented in hardware phase
}

void logToFS(int pwm, float speed) {
  // Will be implemented in hardware phase
}

void setMotors(int leftPWM, int rightPWM) {
  // Will be implemented in hardware phase
}