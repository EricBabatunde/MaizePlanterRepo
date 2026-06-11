#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <HardwareSerial.h>
#include "C:\Users\ACER\Documents\Arduino\libraries\MAVLink\mavlink\common/mavlink.h"

// --- Configuration ---
const char* ssid = "MaizePlanter_Alpha";
const char* password = "mechatronics";

// Pins
const int PIN_STEER = 18;
const int PIN_THROTTLE = 19;
const int PIN_PRESSWHEEL = 21;

// State Machine
enum SystemState { MANUAL, CRUISE, E_STOP };
SystemState currentState = MANUAL;

// Cruise Variables
int runCount = 0;
int currentSweepPWM = 1550;
unsigned long lastStepTime = 0;
float currentGroundSpeed = 0.0;

// Web Server
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

HardwareSerial MavSerial(1);

// --- HTML / UI (Simplified for space, focus on nipple.js) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Planter C2</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script src="https://cdnjs.cloudflare.com/ajax/libs/nipplejs/0.9.0/nipplejs.min.js"></script>
  <style>
    body { text-align: center; background: #222; color: white; font-family: Arial; }
    .btn { width: 80px; height: 80px; border-radius: 50%; border: none; margin: 10px; font-weight: bold; }
    #joystick-container { display: flex; justify-content: space-around; height: 250px; margin-top: 20px; }
    .x { background: #3498db; } .tri { background: #2ecc71; } .sq { background: #e74c3c; } .cir { background: #f1c40f; }
  </style>
</head>
<body>
  <h2>Autonomous Planter Control</h2>
  <div id="joystick-container">
    <div id="left-joy" style="position: relative; width: 150px;"></div>
    <div id="right-joy" style="position: relative; width: 150px;"></div>
  </div>
  <button class="btn x" onclick="sendCmd('X')">X (LOG)</button>
  <button class="btn tri" onclick="sendCmd('TRI')">TRI (STOP)</button>
  <button class="btn sq" onclick="sendCmd('SQ')">SQ (E-STOP)</button>
  <button class="btn cir" onclick="sendCmd('CIR')">CIR (PRESS)</button>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket = new WebSocket(gateway);
    function sendCmd(c) { websocket.send("BTN:"+c); }

    var joyL = nipplejs.create({zone: document.getElementById('left-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'blue'});
    var joyR = nipplejs.create({zone: document.getElementById('right-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'red'});

    joyL.on('move', function (evt, data) { websocket.send("JL:" + data.vector.y); });
    joyR.on('move', function (evt, data) { websocket.send("JR:" + data.vector.y); });
  </script>
</body></html>
)rawliteral";

// --- Logic Functions ---

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    
    if (message.startsWith("BTN:")) {
      char btn = message.charAt(4);
      if (btn == 'X' && currentState == MANUAL) {
        if(runCount < 2) {
          runCount++;
          currentState = CRUISE;
          currentSweepPWM = 1550;
          Serial.println("Starting Cruise Run " + String(runCount));
        }
      } 
      else if (btn == 'T') currentState = MANUAL; // Triangle
      else if (btn == 'S') currentState = E_STOP; // Square
      else if (btn == 'C') analogWrite(PIN_PRESSWHEEL, 150); // Circle test
    }
  }
}

void setup() {
  Serial.begin(115200);
  MavSerial.begin(57600, SERIAL_8N1, 4, 5);
  
  if(!LittleFS.begin(true)) Serial.println("LFS Error");

  WiFi.softAP(ssid, password);
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.begin();

  pinMode(PIN_PRESSWHEEL, OUTPUT);
  // Initialise Pixhawk Outputs to Neutral
  ledcAttach(PIN_THROTTLE, 50, 16); 
  ledcAttach(PIN_STEER, 50, 16);
}

void loop() {
  receiveMavlink();
  
  if (currentState == CRUISE) {
    if (millis() - lastStepTime > 3000) { // 3s Stabilisation
      logToFS(currentSweepPWM, currentGroundSpeed);
      currentSweepPWM += 10;
      if (currentSweepPWM > 1800) currentState = MANUAL;
      lastStepTime = millis();
    }
    // Set motors to current sweep speed
    setMotors(currentSweepPWM, currentSweepPWM);
  }
  
  // Heartbeat logic here...
}

void logToFS(int pwm, float speed) {
  String filename = "/run_" + String(runCount) + ".csv";
  File file = LittleFS.open(filename, FILE_APPEND);
  if(file) {
    file.printf("%d, %.3f\n", pwm, speed);
    file.close();
  }
}