#include "WebUI.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char *ssid = "MaizePlanter_Alpha";
const char *password = "mechatronics";

// --- HTML / UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Phase 4.2 GCS</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <script src="/nipplejs.min.js"></script>
  <style>
    body { text-align: center; background: #1e1e1e; color: white; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 10px; transition: none; }
    body.latency-flash { background: #2c3e50 !important; }
    .status-bar { display: flex; justify-content: space-between; margin-bottom: 15px; }
    .status-box { flex: 1; padding: 5px; font-weight: bold; border-radius: 5px; font-size: 14px; }
    #status { background: #e67e22; margin-right: 5px; }
    #pix-mode { background: #34495e; margin-left: 5px; }
    .btn { padding: 15px 10px; border-radius: 8px; border: none; margin: 5px; font-weight: bold; font-size: 14px; cursor: pointer; color: white; }
    .btn-manual { background: #3498db; width: 45%; }
    .btn-auto { background: #9b59b6; width: 45%; }
    .btn-estop { background: #e74c3c; width: 95%; height: 60px; font-size: 18px; margin-top: 15px; box-shadow: 0px 4px 10px rgba(231, 76, 60, 0.5); }
    #joystick-container { display: flex; justify-content: space-between; height: 180px; margin-top: 20px; padding: 0 10px; }
    .joy-zone { position: relative; width: 45%; height: 100%; background: #2c3e50; border-radius: 15px; display: flex; align-items: center; justify-content: center; font-size: 12px; color: #7f8c8d; text-align: center; }
    .panel { background: #34495e; padding: 10px; border-radius: 10px; margin-bottom: 10px; }
  </style>
</head>
<body>
  <h2>Pixhawk Direct-Drive</h2>
  <div class="status-bar">
    <div id="status" class="status-box">Connecting...</div>
    <div id="pix-mode" class="status-box">Mode: -- | 0.0 m/s</div>
  </div>
  
  <div class="panel">
    <h3>Flight Controller Mode</h3>
    <button class="btn btn-manual" onclick="sendCmd('MANUAL')">MODE: MANUAL</button>
    <button class="btn btn-auto" onclick="sendCmd('AUTO')">MODE: AUTO (Weave)</button>
    <br>
    <button id="meter-btn" class="btn" style="background:#27ae60; width:95%; margin-top:10px;" onclick="sendCmd('METER')">Run Seed Meter</button>
  </div>

  <button class="btn btn-estop" onclick="sendCmd('DISARM')">DISARM / E-STOP</button>

  <div id="joystick-container">
    <div id="left-joy" class="joy-zone">Throttle<br>(Fwd/Rev)</div>
    <div id="right-joy" class="joy-zone">Steering<br>(L/R)</div>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = function(event) {
        document.getElementById('status').innerText = "LINK ACTIVE";
        document.getElementById('status').style.background = "#2ecc71";
      };
      
      websocket.onmessage = function(event) {
        // Simple telemetry string from ESP32: e.g. "MANUAL | 1.2 m/s"
        if(event.data.startsWith("T:")) {
            document.getElementById('pix-mode').innerText = event.data.substring(2);
        }
      };

      websocket.onclose = function(event) {
        document.getElementById('status').innerText = "LINK LOST";
        document.getElementById('status').style.background = "#e74c3c";
        setTimeout(initWebSocket, 1000);
      };
    }

    var isMeterRunning = false;

    function sendCmd(cmd) { 
      if (cmd === 'METER') {
         isMeterRunning = !isMeterRunning;
         document.getElementById('meter-btn').innerText = isMeterRunning ? "Stop Seed Meter" : "Run Seed Meter";
         document.getElementById('meter-btn').style.background = isMeterRunning ? "#c0392b" : "#27ae60";
         cmd = isMeterRunning ? 'METER_START' : 'METER_STOP';
      }
      
      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("CMD:" + cmd); 
      }
    }

    initWebSocket();

    window.addEventListener('load', function() {
      try {
        // Left Joy: Locked to Y-axis (Throttle)
        var joyL = nipplejs.create({zone: document.getElementById('left-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'cyan', lockX: true});
        // Right Joy: Locked to X-axis (Steering)
        var joyR = nipplejs.create({zone: document.getElementById('right-joy'), mode: 'static', position: {left: '50%', top: '50%'}, color: 'magenta', lockY: true});
        
        var throttleY = 0.00; var steerX = 0.00;

        joyL.on('move', function (evt, data) { throttleY = data.vector.y; document.body.classList.add('latency-flash'); });
        joyR.on('move', function (evt, data) { steerX = data.vector.x; document.body.classList.add('latency-flash'); });
        
        joyL.on('end', function () { throttleY = 0.00; document.body.classList.remove('latency-flash'); if(websocket.readyState === WebSocket.OPEN) websocket.send("TH:0.00"); });
        joyR.on('end', function () { steerX = 0.00; document.body.classList.remove('latency-flash'); if(websocket.readyState === WebSocket.OPEN) websocket.send("ST:0.00"); });

        setInterval(function() {
          if(websocket && websocket.readyState === WebSocket.OPEN) {
            if (throttleY !== 0) websocket.send("TH:" + throttleY.toFixed(2));
            if (steerX !== 0) websocket.send("ST:" + steerX.toFixed(2));
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

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    String msg = (char *)data;

    // Process UI Commands
    if (msg.startsWith("CMD:"))
    {
      String cmd = msg.substring(4);

      if (cmd == "DISARM")
      {
        currentState = PIX_DISARM;
        // Zero out intents immediately
        targetThrottleRC = 1500;
        targetSteerRC = 1500;
        Serial.println("\n[UI] *** SOFTWARE E-STOP / DISARM TRIGGERED ***");
      }
      else if (cmd == "MANUAL")
      {
        currentState = PIX_MANUAL;
        Serial.println("\n[UI] Requesting MAVLink Mode: MANUAL");
      }
      else if (cmd == "AUTO")
      {
        currentState = PIX_AUTO;
        Serial.println("\n[UI] Requesting MAVLink Mode: AUTO (Weave Test)");
      }
      else if (cmd == "METER_START")
      {
        currentState = METER_TEST;
        // Motor logic stays the same; this is purely ESP32 side
        meterStartTime = millis();
        jamDebounceCounter = 0;
        Serial.println("\n[METER] Seed Plate Running.");
      }
      else if (cmd == "METER_STOP")
      {
        currentState = PIX_MANUAL; // Revert to idle manual state
        Serial.println("[METER] Seed Plate Stopped.");
      }
    }
    // Joystick Parsing
    else if (msg.startsWith("TH:") && currentState == PIX_MANUAL)
    {
      // NippleJS Y is positive UP. We map -1.0 to 1.0 -> 1000 to 2000
      float val = msg.substring(3).toFloat();
      targetThrottleRC = 1500 + (int)(val * 500);
    }
    else if (msg.startsWith("ST:") && currentState == PIX_MANUAL)
    {
      // NippleJS X is positive RIGHT. Map -1.0 to 1.0 -> 1000 to 2000
      float val = msg.substring(3).toFloat();
      targetSteerRC = 1500 + (int)(val * 500);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_DATA)
  {
    handleWebSocketMessage(arg, data, len);
  }
}

void initWebUI()
{
  WiFi.softAP(ssid, password);
  Serial.println("\n--- Phase 4.2 UI Boot Sequence ---");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Serve the local joystick file
  server.serveStatic("/nipplejs.min.js", LittleFS, "/nipplejs.min.js");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", index_html); });

  server.begin();
  Serial.println("[SYSTEM] Web server started.");
}

void broadcastTelemetry()
{
  // Send a simple formatted string to update the UI status bar
  String payload = "T:" + currentPixhawkMode + " | " + String(currentGroundSpeed, 2) + " m/s";
  ws.textAll(payload);
}