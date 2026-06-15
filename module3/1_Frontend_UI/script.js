document.addEventListener('DOMContentLoaded', () => {
    // 1. UI Elements
    const startBtn = document.getElementById('startBtn');
    const armBtn = document.getElementById('armBtn'); // The new ARM button
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

    const telemetrySpeed = document.getElementById('telemetrySpeed');
    const telemetryHeading = document.getElementById('telemetryHeading');

    // 2. State Variables & Constants
    let isRunning = false;
    let globalWaypoints = [];
    const API_URL = "http://127.0.0.1:8000/api/plan_mission";
    const MQTT_BROKER = "127.0.0.1";
    const MQTT_PORT = 9001;

    // ==========================================
    // 3. CORE INFRASTRUCTURE (Initialised Early)
    // ==========================================

    // Create the MQTT Client instance at the TOP so all functions can see it
    const mqttClient = new Paho.MQTT.Client(MQTT_BROKER, Number(MQTT_PORT), "GCS_Web_Client_" + parseInt(Math.random() * 100, 10));

    function setupCanvas() {
        canvas.width = gridContainer.clientWidth;
        canvas.height = gridContainer.clientHeight;
    }
    window.addEventListener('resize', setupCanvas);
    setupCanvas();

    // ==========================================
    // 4. EVENT LISTENERS (The Buttons)
    // ==========================================

    startBtn.addEventListener('click', () => {
        if (isRunning) return;
        requestMissionFromServer();
    });

    // The ARM & DRIVE Button Logic
    armBtn.addEventListener('click', () => {
        const confirmArm = confirm("WARNING: Ensure the planter is clear. Proceed with ARM and AUTO drive?");
        if (confirmArm) {
            sysState.textContent = 'ARMING...';
            console.log("Sending ARM Command via MQTT...");

            try {
                const payload = JSON.stringify({ action: "ARM_AUTO" });
                const message = new Paho.MQTT.Message(payload);
                message.destinationName = "maizepro/command";
                mqttClient.send(message);
                console.log("Command sent successfully.");
            } catch (err) {
                console.error("Failed to send command:", err);
                alert("MQTT Send Failed. Check Console.");
            }
        }
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
        armBtn.style.display = 'none'; // Hide arm button on reset
    });

    // ==========================================
    // 5. REST API: FETCH MISSION
    // ==========================================
    async function requestMissionFromServer() {
        isRunning = true;
        sysState.textContent = 'CALCULATING...';
        sysState.className = 'badge planning';

        const payload = {
            start_lat: 7.2225,
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
            globalWaypoints = data.waypoints;
            wpCount.textContent = data.total_waypoints;

            sysState.textContent = 'MISSION READY';
            sysState.className = 'badge cruise';

            drawPath(globalWaypoints);

            // Reveal the ARM button now that the mission is ready
            armBtn.style.display = 'block';

        } catch (error) {
            console.error("API Error:", error);
            sysState.textContent = 'ERROR';
            sysState.style.backgroundColor = 'red';
            isRunning = false;
        }
    }

    // ==========================================
    // 6. VISUALISATION: DRAW PATH
    // ==========================================
    function drawPath(points) {
        if (points.length === 0) return;
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        const xs = points.map(p => p.local_x);
        const ys = points.map(p => p.local_y);

        const minX = Math.min(...xs);
        const maxX = Math.max(...xs);
        const minY = Math.min(...ys);
        const maxY = Math.max(...ys);

        const fieldWidth = maxX - minX;
        const fieldHeight = maxY - minY;

        const padding = 0.8;
        const scaleX = (canvas.width * padding) / (fieldWidth || 1);
        const scaleY = (canvas.height * padding) / (fieldHeight || 1);
        const scale = Math.min(scaleX, scaleY);

        const offsetX = (canvas.width / 2) + (Math.abs(minX) * scale / 2);
        const offsetY = canvas.height * 0.9;

        ctx.beginPath();
        ctx.strokeStyle = '#4CAF50';
        ctx.lineWidth = 3;

        points.forEach((p, index) => {
            const drawX = offsetX + (p.local_x * scale);
            const drawY = offsetY - (p.local_y * scale);

            if (index === 0) ctx.moveTo(drawX, drawY);
            else ctx.lineTo(drawX, drawY);
        });
        ctx.stroke();

        points.forEach((p) => {
            const drawX = offsetX + (p.local_x * scale);
            const drawY = offsetY - (p.local_y * scale);
            ctx.fillStyle = '#1e3c72';
            ctx.beginPath();
            ctx.arc(drawX, drawY, 4, 0, Math.PI * 2);
            ctx.fill();
        });

        const startX = offsetX + (points[0].local_x * scale);
        const startY = offsetY - (points[0].local_y * scale);
        ctx.fillStyle = '#00FF00';
        ctx.beginPath();
        ctx.arc(startX, startY, 6, 0, Math.PI * 2);
        ctx.fill();
    }

    // ==========================================
    // 7. MQTT BROKER CONNECTION & CALLBACKS
    // ==========================================
    mqttClient.onConnectionLost = (responseObject) => {
        if (responseObject.errorCode !== 0) {
            console.error("MQTT Connection Lost:", responseObject.errorMessage);
            sysState.textContent = 'LINK LOST';
            sysState.style.backgroundColor = 'red';
        }
    };

    mqttClient.onMessageArrived = (message) => {
        try {
            const telemetry = JSON.parse(message.payloadString);
            if (telemetry.system_state) {
                sysState.textContent = telemetry.system_state;
                if (telemetry.system_state === "E-STOP") sysState.style.backgroundColor = 'red';
            }
            if (telemetry.ground_speed !== undefined) telemetrySpeed.textContent = telemetry.ground_speed.toFixed(2);
            if (telemetry.heading !== undefined) telemetryHeading.textContent = telemetry.heading.toFixed(1);
        } catch (e) {
            console.error("Error parsing telemetry JSON", e);
        }
    };

    // Connect to the broker
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