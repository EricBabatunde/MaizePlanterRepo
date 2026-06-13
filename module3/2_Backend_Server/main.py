from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from data_models import MissionRequest, MissionResponse, Waypoint
from path_planner import generate_waypoints
import paho.mqtt.client as mqtt
import json
import threading

# 1. Initialise the Web API
app = FastAPI(title="MaizePro Ground Control Station")

# Allow the frontend (which might run on a different port/file) to talk to this API
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # In production, we restrict this to your specific UI IP
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 2. Initialise the MQTT Broker Connection (Background Thread)
MQTT_BROKER = "127.0.0.1" # Localhost (Eclipse Mosquitto)
MQTT_PORT = 1883

def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] Connected to Mosquitto Broker with result code {rc}")
    # Subscribe to the ESP32's telemetry channel
    client.subscribe("maizepro/telemetry")

def on_message(client, userdata, msg):
    # This will trigger every time the ESP32 sends an update
    payload = msg.payload.decode()
    print(f"[TELEMETRY RECEIVED] {msg.topic}: {payload}")
    # Note: Later, we will forward this to the frontend via WebSockets

mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

def start_mqtt():
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        print(f"[MQTT ERROR] Could not connect to broker: {e}")

# Run MQTT in a background thread so it doesn't block the API
threading.Thread(target=start_mqtt, daemon=True).start()

# 3. Define the API Endpoint
@app.post("/api/plan_mission", response_model=MissionResponse)
async def plan_mission(request: MissionRequest):
    print(f"[API] Received mission request: {request.length_m}m x {request.width_m}m")
    
    try:
        # Call our verified mathematical logic
        raw_waypoints = generate_waypoints(
            lat0=request.start_lat,
            lon0=request.start_lon,
            heading_deg=request.initial_heading,
            length=request.length_m,
            width=request.width_m,
            row_spacing=request.row_spacing_m
        )
        
        # Package the raw maths into our strict JSON models
        # formatted_waypoints = [
        #     Waypoint(row=wp["row"], lat=wp["lat"], lon=wp["lon"]) 
        #     for wp in raw_waypoints
        # ]

        formatted_waypoints = [
            Waypoint(row=wp["row"], lat=wp["lat"], lon=wp["lon"], local_x=wp["local_x"], local_y=wp["local_y"]) 
            for wp in raw_waypoints
        ]
        
        # Optional: Publish the new mission to the ESP32 via MQTT instantly
        mission_payload = json.dumps([wp.dict() for wp in formatted_waypoints])
        mqtt_client.publish("maizepro/mission", mission_payload)
        
        return MissionResponse(
            status="success",
            message="Serpentine route generated and published to queue.",
            total_waypoints=len(formatted_waypoints),
            waypoints=formatted_waypoints
        )
        
    except Exception as e:
        print(f"[API ERROR] {str(e)}")
        raise HTTPException(status_code=500, detail="Failed to generate mission geometry.")