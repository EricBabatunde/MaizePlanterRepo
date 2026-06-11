#include "WebUI.h"
#include "DataLogger.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* ssid = "MaizePlanter_Alpha";
const char* password = "mechatronics";

// --- HTML / UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Planter Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <script src="/nipplejs.min.js"></script>
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
    <button class="btn btn-bracket" onclick="sendCmd('BD')">D<br>(1850-1950)</button>
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

    function initWebSocket() {
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

    function sendCmd(cmd) { 
      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("CMD:" + cmd); 
      } else {
        alert("Cannot send command: ESP32 disconnected!");
      }
    }

    initWebSocket();

    window.addEventListener('load', function() {
      try {
        if (typeof nipplejs === 'undefined') throw new Error("nipplejs not loaded");
        
        document.getElementById('left-joy').innerText = ""; 
        document.getElementById('right-joy').innerText = "";
        
        var joyL = nipplejs.create({zone: document.getElementById('left-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'cyan', lockY: true});
        var joyR = nipplejs.create({zone: document.getElementById('right-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'magenta', lockY: true});

        var joyLY = 0.00;
        var joyRY = 0.00;

        joyL.on('move', function (evt, data) { joyLY = data.vector.y; });
        joyR.on('move', function (evt, data) { joyRY = data.vector.y; });

        joyL.on('end', function () { 
          joyLY = 0.00; 
          if(websocket && websocket.readyState === WebSocket.OPEN) websocket.send("JL:0.00"); 
        });
        joyR.on('end', function () { 
          joyRY = 0.00; 
          if(websocket && websocket.readyState === WebSocket.OPEN) websocket.send("JR:0.00"); 
        });

        setInterval(function() {
          if(websocket && websocket.readyState === WebSocket.OPEN) {
            if (joyLY !== 0) websocket.send("JL:" + joyLY.toFixed(2));
            if (joyRY !== 0) websocket.send("JR:" + joyRY.toFixed(2));
          }
        }, 100); 

      } catch (error) {
        console.error("Joystick Error:", error);
        document.getElementById('left-joy').innerText = "JS Load Failed";
        document.getElementById('right-joy').innerText = "JS Load Failed";
      }
    });
  </script>
</body></html>
)rawliteral";

// --- Logic ---

void startBracket(SystemState newState, int startPWM, int limitPWM, String bracketName) {
  currentState = newState;
  currentSweepPWM = startPWM;
  sweepUpperLimit = limitPWM;
  lastStepTime = millis();
  
  startNewLogFile(bracketName);

  Serial.println("[CRUISE] State: AUTO");
  Serial.printf("[CRUISE] Target: %dus. Stabilising...\n", currentSweepPWM);
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
        if (cmd == "BD") startBracket(CRUISE_D, 1850, 1950, "D"); 
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

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

void initWebUI() {
  WiFi.softAP(ssid, password);
  Serial.println("\n--- Alpha Planter Boot Sequence ---");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  
  // Serve the local joystick file
  server.serveStatic("/nipplejs.min.js", LittleFS, "/nipplejs.min.js");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  server.begin();
  Serial.println("[SYSTEM] Web server started.");
}