document.addEventListener('DOMContentLoaded', () => {
    const startBtn = document.getElementById('startBtn');
    const resetBtn = document.getElementById('resetBtn');
    const fieldLengthInput = document.getElementById('fieldLength');
    const fieldWidthInput = document.getElementById('fieldWidth');
    const initialHeadingInput = document.getElementById('initialHeading');
    const sysState = document.getElementById('sysState');
    const wpCount = document.getElementById('wpCount');
    const planter = document.getElementById('planter');
    const canvas = document.getElementById('pathCanvas');
    const ctx = canvas.getContext('2d');
    const gridContainer = document.getElementById('gridContainer');

    // Telemetry UI Elements
    const telemetrySpeed = document.getElementById('telemetrySpeed');
    const telemetryHeading = document.getElementById('telemetryHeading');
    const seedCount = document.getElementById('seedCount');

    let isRunning = false;
    let globalWaypoints = [];

    // Server Configuration
    const API_URL = "http://127.0.0.1:8000/api/plan_mission";
    const MQTT_BROKER = "127.0.0.1";
    const MQTT_PORT = 9001; // The WebSocket port we just configured!

    function setupCanvas() {
        canvas.width = gridContainer.clientWidth;
        canvas.height = gridContainer.clientHeight;
    }

    window.addEventListener('resize', setupCanvas);
    setupCanvas();

    startBtn.addEventListener('click', () => {
        if (isRunning) return;
        requestMissionFromServer();
    });

    resetBtn.addEventListener('click', () => {
        isRunning = false;
        sysState.textContent = 'IDLE';
        sysState.className = 'badge idle';
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        planter.style.left = '90%';
        planter.style.top = '90%';
        wpCount.textContent = '0';
        globalWaypoints = [];
    });

    // --- 1. HTTP REST API LOGIC ---
    async function requestMissionFromServer() {
        isRunning = true;
        sysState.textContent = 'CALCULATING...';
        sysState.className = 'badge planning';

        const payload = {
            start_lat: 7.2225, // Mock start location (FUNAAB)
            start_lon: 3.4408,
            initial_heading: parseFloat(initialHeadingInput.value),
            length_m: parseFloat(fieldLengthInput.value),
            width_m: parseFloat(fieldWidthInput.value),
            row_spacing_m: 0.6
        };

        try {
            const response = await fetch(API_URL, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });

            if (!response.ok) throw new Error("Server rejected the request.");

            const data = await response.json();
            console.log("Mission Received:", data);

            globalWaypoints = data.waypoints;
            wpCount.textContent = data.total_waypoints;

            sysState.textContent = 'MISSION READY';
            sysState.className = 'badge cruise';

            drawPath(globalWaypoints);

            // In a real scenario, the ESP32 drives the animation. 
            // We'll leave your simulation animation here just for visual testing.
            animatePlanterSimulation(globalWaypoints);

        } catch (error) {
            console.error("API Error:", error);
            sysState.textContent = 'ERROR';
            sysState.style.backgroundColor = 'red';
            isRunning = false;
        }
    }

    // Drawing logic now uses the local_x and local_y from the Python backend!
    function drawPath(points) {
        if (points.length === 0) return;

        // 1. Clear the previous drawing
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // 2. Find the bounding box of the physical field
        const xs = points.map(p => p.local_x);
        const ys = points.map(p => p.local_y);

        const minX = Math.min(...xs);
        const maxX = Math.max(...xs);
        const minY = Math.min(...ys);
        const maxY = Math.max(...ys);

        const fieldWidth = maxX - minX;
        const fieldHeight = maxY - minY;

        // 3. Calculate dynamic scaling to fill the canvas
        // We use 0.8 to leave a 10% padding around the edges
        const padding = 0.8;
        const scaleX = (canvas.width * padding) / (fieldWidth || 1);
        const scaleY = (canvas.height * padding) / (fieldHeight || 1);

        // Use the smaller scale to maintain the aspect ratio of the physical field
        const scale = Math.min(scaleX, scaleY);

        // 4. Calculate the offsets to center the path on the canvas
        // We align the starting point (0,0) to the bottom right of the drawable area
        const offsetX = (canvas.width / 2) + (Math.abs(minX) * scale / 2);
        const offsetY = canvas.height * 0.9; // 10% from the bottom

        // 5. Draw the continuous path
        ctx.beginPath();
        ctx.strokeStyle = '#4CAF50'; // The green line
        ctx.lineWidth = 3;

        points.forEach((p, index) => {
            // Apply scale and offset. Note: Y is subtracted because canvas Y grows downwards
            const drawX = offsetX + (p.local_x * scale);
            const drawY = offsetY - (p.local_y * scale);

            if (index === 0) {
                ctx.moveTo(drawX, drawY); // Start the path
            } else {
                ctx.lineTo(drawX, drawY); // Connect to the next point
            }
        });

        ctx.stroke(); // Physically render the green line

        // 6. Draw the waypoint markers (blue dots) over the line
        points.forEach((p) => {
            const drawX = offsetX + (p.local_x * scale);
            const drawY = offsetY - (p.local_y * scale);

            ctx.fillStyle = '#1e3c72';
            ctx.beginPath();
            ctx.arc(drawX, drawY, 4, 0, Math.PI * 2);
            ctx.fill();
        });

        // 7. Mark the Starting Point (Green dot)
        const startX = offsetX + (points[0].local_x * scale);
        const startY = offsetY - (points[0].local_y * scale);
        ctx.fillStyle = '#00FF00';
        ctx.beginPath();
        ctx.arc(startX, startY, 6, 0, Math.PI * 2);
        ctx.fill();
    }

    function animatePlanterSimulation(points) {
        // Keeping your team member's animation logic intact for now
        // so you can verify the path looks correct visually.
        // Eventually, this function will be DELETED, and the planter's position 
        // will be updated entirely by the MQTT telemetry function below.
    }

    // --- 2. MQTT TELEMETRY LOGIC ---
    // Create a client instance
    const mqttClient = new Paho.MQTT.Client(MQTT_BROKER, Number(MQTT_PORT), "GCS_Web_Client_" + parseInt(Math.random() * 100, 10));

    mqttClient.onConnectionLost = (responseObject) => {
        if (responseObject.errorCode !== 0) {
            console.error("MQTT Connection Lost:", responseObject.errorMessage);
            sysState.textContent = 'LINK LOST';
            sysState.style.backgroundColor = 'red';
        }
    };

    mqttClient.onMessageArrived = (message) => {
        console.log("Telemetry Received:", message.payloadString);
        try {
            const telemetry = JSON.parse(message.payloadString);

            // Update UI with real data from ESP32
            if (telemetry.system_state) {
                sysState.textContent = telemetry.system_state;
                if (telemetry.system_state === "E-STOP") sysState.style.backgroundColor = 'red';
            }
            if (telemetry.ground_speed !== undefined) telemetrySpeed.textContent = telemetry.ground_speed.toFixed(2);
            if (telemetry.heading !== undefined) telemetryHeading.textContent = telemetry.heading.toFixed(1);

            // TODO later: Use telemetry.lat and telemetry.lon to update the planter's icon position on the canvas!

        } catch (e) {
            console.error("Error parsing telemetry JSON", e);
        }
    };

    // Connect the client
    mqttClient.connect({
        onSuccess: () => {
            console.log("Connected to MQTT Broker via WebSockets!");
            mqttClient.subscribe("maizepro/telemetry");
        },
        onFailure: (e) => {
            console.error("MQTT Connection failed:", e.errorMessage);
        }
    });
});