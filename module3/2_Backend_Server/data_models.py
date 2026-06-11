from pydantic import BaseModel, Field
from typing import List

# --- INBOUND: What the Frontend sends to the Backend ---
class MissionRequest(BaseModel):
    start_lat: float = Field(..., description="Starting Latitude from Pixhawk")
    start_lon: float = Field(..., description="Starting Longitude from Pixhawk")
    initial_heading: float = Field(..., description="Current compass heading in degrees")
    length_m: float = Field(..., description="Length of the field in metres")
    width_m: float = Field(..., description="Width of the field in metres")
    row_spacing_m: float = Field(0.6, description="Cross-track row spacing (default 0.6m)")

# --- OUTBOUND: What the Backend sends back to the Frontend ---
class Waypoint(BaseModel):
    row: int
    lat: float
    lon: float
    

class MissionResponse(BaseModel):
    status: str
    message: str
    total_waypoints: int
    waypoints: List[Waypoint]

# --- TELEMETRY: What the ESP32 publishes to MQTT (For reference) ---
class TelemetryData(BaseModel):
    lat: float
    lon: float
    heading: float
    ground_speed: float
    system_state: str  # e.g., "CRUISE", "TURNING", "E-STOP"