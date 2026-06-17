/* =============================================================================
   GROUND CONTROL STATION — script.js
   Autonomous Smart Maize Planter
   Modules: WebSocket Client | Canvas Renderer | Telemetry UI | Data Logger
   ============================================================================= */

document.addEventListener('DOMContentLoaded', () => {
  'use strict';

  // =========================================================================
  // 1. DOM REFERENCES
  // =========================================================================
  const DOM = {
    // Header indicators
    ledConnection: document.getElementById('ledConnection'),
    valConnection: document.getElementById('valConnection'),
    valLatency: document.getElementById('valLatency'),
    valBattery: document.getElementById('valBattery'),
    ledGPS: document.getElementById('ledGPS'),
    valGPS: document.getElementById('valGPS'),
    // Telemetry cards
    valState: document.getElementById('valState'),
    valSpeed: document.getElementById('valSpeed'),
    valHeading: document.getElementById('valHeading'),
    valWpDist: document.getElementById('valWpDist'),
    valPosition: document.getElementById('valPosition'),
    // Mission control
    inputRowSpacing: document.getElementById('inputRowSpacing'),
    inputFieldWidth: document.getElementById('inputFieldWidth'),
    inputFieldLength: document.getElementById('inputFieldLength'),
    btnApplyMapScale: document.getElementById('btnApplyMapScale'),
    inputWaypoints: document.getElementById('inputWaypoints'),
    btnUploadWaypoints: document.getElementById('btnUploadWaypoints'),
    btnStartMission: document.getElementById('btnStartMission'),
    // Map canvas
    mapCanvas: document.getElementById('mapCanvas'),
    canvasWrapper: document.getElementById('canvasWrapper'),
    // Diagnostics
    btnUTurnTest: document.getElementById('btnUTurnTest'),
    toggleLogging: document.getElementById('toggleLogging'),
    loggingStatus: document.getElementById('loggingStatus'),
    btnExportCSV: document.getElementById('btnExportCSV'),
    valLogCount: document.getElementById('valLogCount'),
    // E-STOP
    btnEstop: document.getElementById('btnEstop'),
  };

  const ctx = DOM.mapCanvas.getContext('2d');

  // =========================================================================
  // 2. APPLICATION STATE
  // =========================================================================
  const state = {
    // Connection
    wsConnected: false,
    // Telemetry (mirrors ESP32 JSON payload)
    systemState: 'IDLE',
    groundSpeed: 0,
    heading: 0,
    posX: 0,
    posY: 0,
    wpDist: 0,
    battery: 0,
    ping: 0,
    // Mission waypoints [{x, y}, ...]
    waypoints: [],
    // Data logging
    isLogging: false,
    logBuffer: [],
    // Static farmland scaling (Pixels-Per-Metre)
    ppm: 1,
    fieldWidth: 50,   // metres — default
    fieldLength: 50,   // metres — default
  };

  // =========================================================================
  // 3. WEBSOCKET CLIENT MODULE
  // =========================================================================
  const WS_URL = `ws://localhost:8080/ws`;
  let ws = null;
  let reconnectTimer = null;
  const RECONNECT_DELAY_MS = 3000;

  /** Establish WebSocket connection to the ESP32. */
  function wsConnect() {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;
    try { ws = new WebSocket(WS_URL); } catch (e) { console.error('[WS] Creation failed:', e); return; }
    ws.onopen = () => {
      state.wsConnected = true;
      clearTimeout(reconnectTimer);
      updateConnectionUI(true);
      console.log('[WS] Connected to', WS_URL);
    };
    ws.onclose = (ev) => {
      state.wsConnected = false;
      updateConnectionUI(false);
      console.warn('[WS] Closed — code:', ev.code, 'reason:', ev.reason);
      scheduleReconnect();
    };
    ws.onerror = (err) => {
      console.error('[WS] Error:', err);
      // onclose will fire after onerror, so reconnect is handled there.
    };
    ws.onmessage = (ev) => {
      try {
        const data = JSON.parse(ev.data);
        handleTelemetry(data);
      } catch (e) {
        console.warn('[WS] Non-JSON message received:', ev.data);
      }
    };
  }

  /** Schedule an automatic reconnection attempt. */
  function scheduleReconnect() {
    clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(() => { wsConnect(); }, RECONNECT_DELAY_MS);
  }

  /** Send a JSON command to the ESP32 via WebSocket. */
  function wsSend(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
    } else {
      console.warn('[WS] Cannot send — not connected.');
    }
  }

  // =========================================================================
  // 4. TELEMETRY HANDLER
  // =========================================================================
  /**
   * Process an incoming telemetry JSON packet from the ESP32.
   * Expected: { state, v_g, heading, x, y, wp_dist, batt, ping }
   */
  function handleTelemetry(d) {
    // Update internal state
    if (d.state !== undefined) state.systemState = d.state;
    if (d.v_g !== undefined) state.groundSpeed = d.v_g;
    if (d.heading !== undefined) {
      /* SPINNING-HEADING FIX — Normalise to 0–360°.
         If the ESP32 sends cumulative gyroscope deltas or values
         outside the expected range, this clamp prevents the canvas
         rotation from spiralling out of control. */
      state.heading = ((d.heading % 360) + 360) % 360;
    }
    if (d.x !== undefined) state.posX = d.x;
    if (d.y !== undefined) state.posY = d.y;
    if (d.wp_dist !== undefined) state.wpDist = d.wp_dist;
    if (d.batt !== undefined) state.battery = d.batt;
    if (d.ping !== undefined) state.ping = d.ping;

    // Push to UI
    updateTelemetryUI();
    // Redraw canvas with new position
    renderCanvas();
    // Data logging
    if (state.isLogging) {
      state.logBuffer.push({ timestamp: Date.now(), ...d });
      DOM.valLogCount.textContent = state.logBuffer.length;
    }
  }

  // =========================================================================
  // 5. UI UPDATE FUNCTIONS
  // =========================================================================

  /** Update header connection indicator. */
  function updateConnectionUI(connected) {
    DOM.ledConnection.className = connected ? 'led led--connected' : 'led led--disconnected';
    DOM.valConnection.textContent = connected ? 'Connected' : 'Disconnected';
  }

  /** Push latest telemetry values into the DOM. */
  function updateTelemetryUI() {
    // System state badge
    const s = state.systemState;
    DOM.valState.textContent = s;
    DOM.valState.className = 'telemetry-card__value badge badge--' +
      (s === 'AUTO' ? 'auto' : s === 'MANUAL' ? 'manual' : s === 'E-STOP' ? 'estop' : 'idle');

    // Numerical readouts
    DOM.valSpeed.innerHTML = `${state.groundSpeed.toFixed(2)} <small>m/s</small>`;
    DOM.valHeading.innerHTML = `${state.heading.toFixed(1)}<small>°</small>`;
    DOM.valWpDist.innerHTML = `${state.wpDist.toFixed(1)} <small>m</small>`;
    DOM.valPosition.textContent = `${state.posX.toFixed(1)} , ${state.posY.toFixed(1)}`;

    // Header indicators
    DOM.valLatency.textContent = `${state.ping} ms`;
    DOM.valBattery.textContent = `${state.battery.toFixed(1)} V`;

    // GPS indicator — simple heuristic: fix OK if battery > 0 (i.e. data arriving)
    const hasFix = state.wsConnected && state.battery > 0;
    DOM.ledGPS.className = hasFix ? 'led led--fix-ok' : 'led led--no-fix';
    DOM.valGPS.textContent = hasFix ? 'Fix OK' : 'No Fix';
  }

  // =========================================================================
  // 6. CANVAS RENDERER MODULE — Fixed PPM with Bottom-Left Origin
  // =========================================================================
  const GRID_COLOUR    = 'rgba(255,255,255,0.06)';
  const AXIS_COLOUR    = 'rgba(255,255,255,0.15)';
  const WP_COLOUR      = '#58a6ff';
  const WP_LINE_COLOUR = 'rgba(88,166,255,0.3)';
  const PLANTER_COLOUR = '#3fb950';
  const TRAIL_COLOUR   = 'rgba(63,185,80,0.25)';

  /**
   * Apply the user-defined farmland dimensions to the canvas.
   * Calculates a fixed Pixels-Per-Metre (PPM) scale and sets the
   * canvas buffer size to maintain the physical aspect ratio of
   * the field.  This replaces all previous dynamic auto-zoom logic.
   */
  function applyMapScale() {
    const fieldW = parseFloat(DOM.inputFieldWidth.value)  || 50;
    const fieldL = parseFloat(DOM.inputFieldLength.value) || 50;

    state.fieldWidth  = fieldW;
    state.fieldLength = fieldL;

    /* PPM is derived from the container's CSS width and the field width */
    const containerWidth = DOM.canvasWrapper.clientWidth;
    if (containerWidth < 1) return;

    state.ppm = containerWidth / fieldW;

    /* Set canvas buffer dimensions — width fills the container;
       height is proportional to the field length in metres */
    DOM.mapCanvas.width  = containerWidth;
    DOM.mapCanvas.height = Math.round(fieldL * state.ppm);

    console.log(`[Map] Scale applied — Field: ${fieldW}×${fieldL} m, PPM: ${state.ppm.toFixed(2)}, Canvas: ${DOM.mapCanvas.width}×${DOM.mapCanvas.height} px`);
    renderCanvas();
  }

  /**
   * Convert field coordinates (metres) to canvas pixel coordinates
   * using the fixed PPM scale with a bottom-left origin.
   *   PixelX = TelemetryX × PPM
   *   PixelY = canvas.height − (TelemetryY × PPM)
   */
  function toCanvas(x, y) {
    return [
      x * state.ppm,
      DOM.mapCanvas.height - (y * state.ppm),
    ];
  }

  /**
   * Draw a static reference grid at fixed metre increments.
   * Uses 1 m lines when PPM ≥ 10, otherwise 5 m lines.
   */
  function drawGrid() {
    const w    = DOM.mapCanvas.width;
    const h    = DOM.mapCanvas.height;
    const step = state.ppm >= 10 ? 1 : 5; // metres per grid line

    ctx.strokeStyle = GRID_COLOUR;
    ctx.lineWidth   = 1;
    ctx.font        = '10px sans-serif';
    ctx.fillStyle   = AXIS_COLOUR;

    // Vertical grid lines (X-axis, left to right)
    for (let gx = 0; gx <= state.fieldWidth; gx += step) {
      const px = gx * state.ppm;
      ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, h); ctx.stroke();
      ctx.fillText(`${gx}m`, px + 3, h - 4);
    }

    // Horizontal grid lines (Y-axis, bottom to top)
    for (let gy = 0; gy <= state.fieldLength; gy += step) {
      const py = h - (gy * state.ppm);
      ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(w, py); ctx.stroke();
      ctx.fillText(`${gy}m`, 4, py - 3);
    }

    // Draw origin axes (X=0 and Y=0) slightly brighter
    ctx.strokeStyle = AXIS_COLOUR;
    ctx.lineWidth   = 1.5;
    // X=0 vertical axis
    ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(0, h); ctx.stroke();
    // Y=0 horizontal axis (bottom of canvas)
    ctx.beginPath(); ctx.moveTo(0, h); ctx.lineTo(w, h); ctx.stroke();
  }

  /** Draw waypoint markers and connecting lines. */
  function drawWaypoints() {
    const wps = state.waypoints;
    if (wps.length === 0) return;

    // Connecting lines
    ctx.strokeStyle = WP_LINE_COLOUR;
    ctx.lineWidth   = 2;
    ctx.setLineDash([6, 4]);
    ctx.beginPath();
    wps.forEach((wp, i) => {
      const [px, py] = toCanvas(wp.x, wp.y);
      i === 0 ? ctx.moveTo(px, py) : ctx.lineTo(px, py);
    });
    ctx.stroke();
    ctx.setLineDash([]);

    // Waypoint circles
    wps.forEach((wp, i) => {
      const [px, py] = toCanvas(wp.x, wp.y);
      ctx.beginPath();
      ctx.arc(px, py, 5, 0, Math.PI * 2);
      ctx.fillStyle = i === 0 ? '#3fb950' : WP_COLOUR;
      ctx.fill();
      ctx.strokeStyle = '#fff';
      ctx.lineWidth = 1;
      ctx.stroke();
      // Label
      ctx.fillStyle = '#aaa';
      ctx.font = '10px sans-serif';
      ctx.fillText(`WP${i}`, px + 8, py - 4);
    });
  }

  /**
   * Draw the planter as a directional triangle.
   * The incoming telemetry heading is in DEGREES (0–360), already
   * normalised by handleTelemetry().  Canvas rotate() expects
   * RADIANS, so we convert here:
   *   radians = degrees × (Math.PI / 180)
   * The −90° offset ensures 0° heading points North (up on screen).
   *
   * ctx.save() / ctx.restore() bracket ALL translate/rotate calls
   * so the transform matrix is cleanly reset for the next frame.
   */
  function drawPlanter() {
    const [px, py] = toCanvas(state.posX, state.posY);

    /* Convert heading from degrees to radians for canvas rotation.
       Subtract 90° so that 0° = North (canvas 0 rad = East).
       state.heading is already normalised to 0–360 by handleTelemetry(). */
    const headingRad = (state.heading - 90) * (Math.PI / 180);
    const size = 12;

    ctx.save();
    ctx.translate(px, py);
    ctx.rotate(headingRad);

    // Triangle (points right in local coords, rotated by heading)
    ctx.beginPath();
    ctx.moveTo(size, 0);
    ctx.lineTo(-size * 0.6, -size * 0.5);
    ctx.lineTo(-size * 0.6,  size * 0.5);
    ctx.closePath();
    ctx.fillStyle = PLANTER_COLOUR;
    ctx.fill();
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    ctx.restore();

    // Glow effect (drawn in absolute coordinates — no rotation needed)
    ctx.beginPath();
    ctx.arc(px, py, 16, 0, Math.PI * 2);
    const grad = ctx.createRadialGradient(px, py, 2, px, py, 16);
    grad.addColorStop(0, 'rgba(63,185,80,0.35)');
    grad.addColorStop(1, 'rgba(63,185,80,0)');
    ctx.fillStyle = grad;
    ctx.fill();
  }

  /** Master render — clears canvas and draws all layers.
   *  IMPORTANT: This function NEVER modifies canvas.width or
   *  canvas.height.  Those are set only by applyMapScale(). */
  function renderCanvas() {
    const w = DOM.mapCanvas.width;
    const h = DOM.mapCanvas.height;
    if (w < 1 || h < 1) return;

    /* SAFETY: Force-reset the transform matrix to the identity (1:1)
       at the start of every frame.  This guarantees no stale rotation
       or translation leaks from a previous render cycle, even if a
       drawing function failed to call ctx.restore(). */
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    ctx.clearRect(0, 0, w, h);
    // Dark background fill
    ctx.fillStyle = '#0a0c10';
    ctx.fillRect(0, 0, w, h);

    drawGrid();
    drawWaypoints();
    drawPlanter();
  }

  // =========================================================================
  // 7. WAYPOINT PARSING
  // =========================================================================
  /** Parse the waypoints textarea (one "X, Y" pair per line). */
  function parseWaypointsFromText(text) {
    const lines = text.trim().split('\n');
    const wps = [];
    for (const line of lines) {
      const cleaned = line.trim();
      if (!cleaned || cleaned.startsWith('#')) continue; // skip blanks & comments
      const parts = cleaned.split(/[,\s]+/).map(Number);
      if (parts.length >= 2 && !isNaN(parts[0]) && !isNaN(parts[1])) {
        wps.push({ x: parts[0], y: parts[1] });
      }
    }
    return wps;
  }

  // =========================================================================
  // 8. DATA LOGGING & CSV EXPORT
  // =========================================================================

  /** Convert the log buffer array into a CSV string. */
  function logBufferToCSV() {
    if (state.logBuffer.length === 0) return '';
    const headers = Object.keys(state.logBuffer[0]);
    const rows = state.logBuffer.map(entry =>
      headers.map(h => (entry[h] !== undefined ? entry[h] : '')).join(',')
    );
    return [headers.join(','), ...rows].join('\n');
  }

  /** Trigger a browser download of the CSV data. */
  function downloadCSV() {
    const csv = logBufferToCSV();
    if (!csv) { alert('No logged data to export.'); return; }
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `gcs_telemetry_${new Date().toISOString().slice(0, 19).replace(/:/g, '-')}.csv`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  // =========================================================================
  // 9. EVENT LISTENERS
  // =========================================================================

  /* --- Upload Waypoints --- */
  DOM.btnUploadWaypoints.addEventListener('click', () => {
    const wps = parseWaypointsFromText(DOM.inputWaypoints.value);
    if (wps.length < 2) {
      alert('Please enter at least 2 waypoints (one X, Y pair per line).');
      return;
    }
    state.waypoints = wps;
    renderCanvas();
    console.log(`[Mission] ${wps.length} waypoints uploaded.`);
  });

  /* --- Start Mission --- */
  DOM.btnStartMission.addEventListener('click', () => {
    if (state.waypoints.length < 2) {
      alert('Upload waypoints before starting a mission.');
      return;
    }
    const rowSpacing = parseFloat(DOM.inputRowSpacing.value);
    const payload = {
      command: 'START_MISSION',
      row_spacing: rowSpacing,
      waypoints: state.waypoints,
    };
    wsSend(payload);
    console.log('[Mission] START_MISSION sent:', payload);
  });

  /* --- E-STOP (highest priority) ---
     BUG FIX #3: Strict, dedicated click listener that bypasses the
     generic wsSend() wrapper.  Directly verifies the WebSocket state
     and forcefully transmits the exact E-STOP payload. */
  DOM.btnEstop.addEventListener('click', () => {
    const payload = { command: 'ESTOP', pwm: 1500 };

    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(payload));
      console.log('[E-STOP] Emergency command transmitted: ', payload);
    } else {
      console.error('[E-STOP] FAILED — WebSocket is not open. readyState:',
        ws ? ws.readyState : 'null');
    }

    // Update UI immediately for responsiveness regardless of send result
    state.systemState = 'E-STOP';
    updateTelemetryUI();
  });

  /* --- U-Turn Test --- */
  DOM.btnUTurnTest.addEventListener('click', () => {
    wsSend({ command: 'UTURN_TEST' });
    console.log('[Diagnostics] U-Turn test triggered.');
  });

  /* --- Data Logging Toggle --- */
  DOM.toggleLogging.addEventListener('change', () => {
    state.isLogging = DOM.toggleLogging.checked;
    DOM.loggingStatus.textContent = state.isLogging ? 'ON' : 'OFF';
    DOM.loggingStatus.style.color = state.isLogging ? '#3fb950' : '';
    if (state.isLogging) {
      state.logBuffer = [];
      DOM.valLogCount.textContent = '0';
      console.log('[Logger] Data logging started — buffer cleared.');
    } else {
      console.log(`[Logger] Data logging stopped — ${state.logBuffer.length} samples captured.`);
    }
  });

  /* --- Export CSV --- */
  DOM.btnExportCSV.addEventListener('click', downloadCSV);

  /* --- Apply Map Scale --- */
  DOM.btnApplyMapScale.addEventListener('click', () => {
    applyMapScale();
    console.log('[Mission] Map scale updated by operator.');
  });

  // =========================================================================
  // 11. INITIALISATION
  // =========================================================================
  /* Apply the default field dimensions (50 × 50 m) on first load */
  applyMapScale();
  wsConnect();
  console.log('[GCS] Ground Control Station initialised.');
});