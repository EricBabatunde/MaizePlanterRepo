import asyncio
import websockets
import json
import math

# Differential Drive Kinematic Constants
V_G = 1.0        # Constant ground speed (m/s)
YAW_RATE = 0.2   # Constant turning rate (rad/s)
UPDATE_HZ = 10   # 10Hz MAVLink telemetry rate

async def send_telemetry(websocket):
    """Continuously calculates and sends simulated telemetry to the GCS."""
    x, y = 10.0, 10.0 # Start near the middle of a 50x50 field
    heading_rad = 0.0
    
    try:
        while True:
            # 1. Update Kinematics (Circular Trajectory)
            dt = 1.0 / UPDATE_HZ
            heading_rad += YAW_RATE * dt
            x += V_G * math.cos(heading_rad) * dt
            y += V_G * math.sin(heading_rad) * dt
            
            # Convert to degrees for the GCS UI (0-360)
            heading_deg = (math.degrees(heading_rad)) % 360
            
            # 2. Construct JSON Payload
            payload = {
                "state": "AUTO",
                "v_g": round(V_G, 2),
                "heading": round(heading_deg, 1),
                "x": round(x, 2),
                "y": round(y, 2),
                "wp_dist": 5.4,
                "batt": 12.2,
                "ping": 12
            }
            
            # 3. Transmit
            await websocket.send(json.dumps(payload))
            await asyncio.sleep(dt)
    except websockets.exceptions.ConnectionClosed:
        pass # Handle disconnection silently, main handler will catch it

async def receive_commands(websocket):
    """Listens for incoming commands from the GCS (e.g., E-STOP)."""
    try:
        async for message in websocket:
            data = json.loads(message)
            print(f"[RECEIVED FROM GCS]: {data}")
            
            # Simulate E-STOP action
            if data.get("command") == "ESTOP":
                print("!!! EMERGENCY STOP TRIGGERED. KILLING MOTORS !!!")
                # In a real scenario, this is where you'd break the telemetry loop 
                # or set V_G to 0.0
                
    except websockets.exceptions.ConnectionClosed:
        pass

async def handle_client(websocket):
    """Handles both sending and receiving concurrently for a connected client."""
    print("[SERVER] GCS Connected. Initiating Data Streams...")
    try:
        # Run both the sender and receiver tasks at the same time
        await asyncio.gather(
            send_telemetry(websocket),
            receive_commands(websocket)
        )
    except Exception as e:
        print(f"[SERVER] Error: {e}")
    finally:
        print("[SERVER] GCS Disconnected.")

async def main():
    print("--- MECHATRONICS TEST-BENCH: GCS MOCK SERVER V2 ---")
    print("Broadcasting on ws://localhost:8080/ws")
    async with websockets.serve(handle_client, "localhost", 8080):
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    asyncio.run(main())