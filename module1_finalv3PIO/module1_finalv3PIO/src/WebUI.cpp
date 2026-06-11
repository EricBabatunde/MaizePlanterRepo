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
    .status-bar { display: flex; justify-content: space-between; margin-bottom: 15px; }
    .status-box { flex: 1; padding: 5px; font-weight: bold; border-radius: 5px; font-size: 14px; }
    #status { background: #e67e22; margin-right: 5px; }
    #tel-display { background: #34495e; margin-left: 5px; }
    .btn { padding: 15px 10px; border-radius: 8px; border: none; margin: 5px; font-weight: bold; font-size: 14px; cursor: pointer; color: white; }
    .btn-bracket { background: #3498db; width: 30%; }
    .btn-test { background: #8e44ad; width: 45%; }
    .btn-tel { background: #f39c12; width: 45%; }
    .btn-estop { background: #e74c3c; width: 95%; height: 60px; font-size: 20px; margin-top: 15px; }
    .btn-manual { background: #95a5a6; width: 95%; margin-top: 10px; padding: 10px; }
    #joystick-container { display: flex; justify-content: space-between; height: 180px; margin-top: 20px; padding: 0 10px; }
    .joy-zone { position: relative; width: 45%; height: 100%; background: #2c3e50; border-radius: 15px; display: flex; align-items: center; justify-content: center; font-size: 12px; color: #7f8c8d; text-align: center; }
    .panel { background: #2c3e50; padding: 10px; border-radius: 10px; margin-bottom: 10px; }
  </style>
</head>
<body>
  <h2>Alpha Planter UI</h2>
  <div class="status-bar">
    <div id="status" class="status-box">Connecting...</div>
    <div id="tel-display" class="status-box">Telemetry: -- Hz</div>
  </div>
  
  <div class="panel">
    <h3>Calibration & Tests</h3>
    <button class="btn btn-bracket" onclick="sendCmd('BA')">A (1550-1650)</button>
    <button class="btn btn-bracket" onclick="sendCmd('BB')">B (1650-1750)</button>
    <button class="btn btn-bracket" onclick="sendCmd('BC')">C (1750-1850)</button>
    <button class="btn btn-bracket" onclick="sendCmd('BD')">D (1850-1950)</button>
    
    <button class="btn btn-test" onclick="sendCmd('TURN')">Test 180° Turn</button>
    <button id="tel-btn" class="btn btn-tel" onclick="sendCmd('TEL')">Log Telemetry</button>
    
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
    var isTelLogging = false;
    var lastMsgTime = 0;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = function(event) {
        document.getElementById('status').innerText = "CONNECTED";
        document.getElementById('status').style.background = "#2ecc71";
      };
      
      // Calculate Telemetry Rate on incoming messages
      websocket.onmessage = function(event) {
        var now = Date.now();
        if (lastMsgTime !== 0) {
          var interval = now - lastMsgTime;
          var hz = 1000 / interval;
          document.getElementById('tel-display').innerText = "Telemetry: " + hz.toFixed(1) + " Hz";
          
          // Only bounce the interval back to the ESP32 if logging is active
          if (isTelLogging) {
            websocket.send("TEL_LOG:" + interval);
          }
        }
        lastMsgTime = now;
      };

      websocket.onclose = function(event) {
        document.getElementById('status').innerText = "DISCONNECTED";
        document.getElementById('status').style.background = "#e74c3c";
        document.getElementById('tel-display').innerText = "Telemetry: -- Hz";
        setTimeout(initWebSocket, 2000);
      };
    }

    function sendCmd(cmd) { 
      if (cmd === 'TEL') {
         isTelLogging = !isTelLogging;
         document.getElementById('tel-btn').innerText = isTelLogging ? "Stop Telemetry" : "Log Telemetry";
         document.getElementById('tel-btn').style.background = isTelLogging ? "#c0392b" : "#f39c12";
      }
      
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
        var joyLY = 0.00; var joyRY = 0.00;

        joyL.on('move', function (evt, data) { joyLY = data.vector.y; });
        joyR.on('move', function (evt, data) { joyRY = data.vector.y; });
        joyL.on('end', function () { joyLY = 0.00; if(websocket && websocket.readyState === WebSocket.OPEN) websocket.send("JL:0.00"); });
        joyR.on('end', function () { joyRY = 0.00; if(websocket && websocket.readyState === WebSocket.OPEN) websocket.send("JR:0.00"); });

        setInterval(function() {
          if(websocket && websocket.readyState === WebSocket.OPEN) {
            if (joyLY !== 0) websocket.send("JL:" + joyLY.toFixed(2));
            if (joyRY !== 0) websocket.send("JR:" + joyRY.toFixed(2));
          }
        }, 100); 
      } catch (error) {
        console.error("Joystick Error:", error);
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
    
    // Process Telemetry Interval Logs
    if (msg.startsWith("TEL_LOG:")) {
       if (isLoggingTelemetry) {
         int interval = msg.substring(8).toInt();
         logTelemetry(interval);
       }
       return; // Exit early to save processing time
    }
    
    // Process UI Commands
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
      else if (cmd == "TEL") {
        isLoggingTelemetry = !isLoggingTelemetry;
        if (isLoggingTelemetry) {
          startTelemetryLog();
        } else {
          Serial.println("[TELEMETRY] Logging stopped.");
        }
      }
      else if (currentState == MANUAL) { 
        if (cmd == "BA") startBracket(CRUISE_A, 1550, 1650, "A"); 
        if (cmd == "BB") startBracket(CRUISE_B, 1650, 1750, "B"); 
        if (cmd == "BC") startBracket(CRUISE_C, 1750, 1850, "C"); 
        if (cmd == "BD") startBracket(CRUISE_D, 1850, 1950, "D"); 
        
        if (cmd == "TURN") {
          currentState = TURN_TEST;
          turnPhase = 0;
          phaseStartTime = millis();
          startTurnLog();
          Serial.println("\n[TURN] Executing 180° Autonomous Turn...");
        }
      }
    } 
    else if (msg.startsWith("JL:") && currentState == MANUAL) {
      targetLeftRC = 1500 + (int)(msg.substring(3).toFloat() * 500); 
    }
    else if (msg.startsWith("JR:") && currentState == MANUAL) {
      targetRightRC = 1500 + (int)(msg.substring(3).toFloat() * 500); 
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
    request->send(200, "text/html", index_html);
  });
  
  server.begin();
  Serial.println("[SYSTEM] Web server started.");
}

void broadcastTelemetry() {
  // Pushes a packet to the UI so JS can calculate the arrival frequency
  ws.textAll("P"); 
}