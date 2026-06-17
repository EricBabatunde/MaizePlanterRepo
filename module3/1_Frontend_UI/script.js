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
    ledConnection:  document.getElementById('ledConnection'),
    valConnection:  document.getElementById('valConnection'),
    valLatency:     document.getElementById('valLatency'),
    valBattery:     document.getElementById('valBattery'),
    ledGPS:         document.getElementById('ledGPS'),
    valGPS:         document.getElementById('valGPS'),
    // Telemetry cards
    valState:       document.getElementById('valState'),
    valSpeed:       document.getElementById('valSpeed'),
    valHeading:     document.getElementById('valHeading'),
    valWpDist:      document.getElementById('valWpDist'),
    valPosition:    document.getElementById('valPosition'),
    // Mission control
    inputRowSpacing:    document.getElementById('inputRowSpacing'),
    inputWaypoints:     document.getElementById('inputWaypoints'),
    btnUploadWaypoints: document.getElementById('btnUploadWaypoints'),
    btnStartMission:    document.getElementById('btnStartMission'),
    // Map canvas
    mapCanvas:      document.getElementById('mapCanvas'),
    canvasWrapper:  document.getElementById('canvasWrapper'),
    // Diagnostics
    btnUTurnTest:   document.getElementById('btnUTurnTest'),
    toggleLogging:  document.getElementById('toggleLogging'),
    loggingStatus:  document.getElementById('loggingStatus'),
    btnExportCSV:   document.getElementById('btnExportCSV'),
    valLogCount:    document.getElementById('valLogCount'),
    // E-STOP
    btnEstop:       document.getElementById('btnEstop'),
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
    // Canvas transform cache
    canvasScale: 1,
    canvasOffsetX: 0,
    canvasOffsetY: 0,
    canvasFieldMinX: 0,
    canvasFieldMinY: 0,
  };

  // =========================================================================
  // 3. WEBSOCKET CLIENT MODULE
  // =========================================================================
  const WS_URL = `ws://${window.location.hostname || '192.168.4.1'}/ws`;
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
    if (d.state !== undefined)   state.systemState = d.state;
    if (d.v_g !== undefined)     state.groundSpeed  = d.v_g;
    if (d.heading !== undefined) state.heading       = d.heading;
    if (d.x !== undefined)       state.posX          = d.x;
    if (d.y !== undefined)       state.posY          = d.y;
    if (d.wp_dist !== undefined) state.wpDist        = d.wp_dist;
    if (d.batt !== undefined)    state.battery       = d.batt;
    if (d.ping !== undefined)    state.ping          = d.ping;

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
    DOM.valSpeed.innerHTML   = `${state.groundSpeed.toFixed(2)} <small>m/s</small>`;
    DOM.valHeading.innerHTML = `${state.heading.toFixed(1)}<small>°</small>`;
    DOM.valWpDist.innerHTML  = `${state.wpDist.toFixed(1)} <small>m</small>`;
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
  // 6. CANVAS RENDERER MODULE
  // =========================================================================
  const GRID_COLOUR    = 'rgba(255,255,255,0.06)';
  const AXIS_COLOUR    = 'rgba(255,255,255,0.15)';
  const WP_COLOUR      = '#58a6ff';
  const WP_LINE_COLOUR = 'rgba(88,166,255,0.3)';
  const PLANTER_COLOUR = '#3fb950';
  const TRAIL_COLOUR   = 'rgba(63,185,80,0.25)';
  const CANVAS_PAD     = 50; // pixels of padding around field extents

  /** Resize the canvas buffer to match its CSS size (prevents blurriness). */
  function resizeCanvas() {
    const rect = DOM.canvasWrapper.getBoundingClientRect();
    DOM.mapCanvas.width  = rect.width;
    DOM.mapCanvas.height = rect.height;
    renderCanvas();
  }

  /** Compute scale and offset to fit waypoints + planter position into the canvas. */
  function computeTransform() {
    const allX = state.waypoints.map(w => w.x).concat([state.posX]);
    const allY = state.waypoints.map(w => w.y).concat([state.posY]);
    if (allX.length === 0) { state.canvasScale = 1; return; }

    const minX = Math.min(...allX, 0);
    const maxX = Math.max(...allX, 1);
    const minY = Math.min(...allY, 0);
    const maxY = Math.max(...allY, 1);

    const fieldW = maxX - minX || 1;
    const fieldH = maxY - minY || 1;
    const cw = DOM.mapCanvas.width  - CANVAS_PAD * 2;
    const ch = DOM.mapCanvas.height - CANVAS_PAD * 2;

    state.canvasScale   = Math.min(cw / fieldW, ch / fieldH);
    state.canvasOffsetX = CANVAS_PAD - minX * state.canvasScale + (cw - fieldW * state.canvasScale) / 2;
    // Y-axis is inverted on canvas (top = 0), so we flip
    state.canvasOffsetY = DOM.mapCanvas.height - CANVAS_PAD + minY * state.canvasScale - (ch - fieldH * state.canvasScale) / 2;
    state.canvasFieldMinX = minX;
    state.canvasFieldMinY = minY;
  }

  /** Convert field coordinates (metres) to canvas pixel coordinates. */
  function toCanvas(x, y) {
    return [
      state.canvasOffsetX + x * state.canvasScale,
      state.canvasOffsetY - y * state.canvasScale,
    ];
  }

  /** Draw a background reference grid with axis labels. */
  function drawGrid() {
    const w = DOM.mapCanvas.width;
    const h = DOM.mapCanvas.height;
    const step = state.canvasScale >= 10 ? 1 : state.canvasScale >= 4 ? 5 : 10; // metres

    ctx.strokeStyle = GRID_COLOUR;
    ctx.lineWidth   = 1;
    ctx.font        = '10px sans-serif';
    ctx.fillStyle   = AXIS_COLOUR;

    // Vertical grid lines
    const startX = Math.floor(state.canvasFieldMinX / step) * step;
    for (let gx = startX; ; gx += step) {
      const [px] = toCanvas(gx, 0);
      if (px > w + 10) break;
      if (px < -10) continue;
      ctx.beginPath(); ctx.moveTo(px, 0); ctx.lineTo(px, h); ctx.stroke();
      ctx.fillText(`${gx}m`, px + 3, h - 4);
    }
    // Horizontal grid lines
    const startY = Math.floor(state.canvasFieldMinY / step) * step;
    for (let gy = startY; ; gy += step) {
      const [, py] = toCanvas(0, gy);
      if (py < -10) break;
      if (py > h + 10) continue;
      ctx.beginPath(); ctx.moveTo(0, py); ctx.lineTo(w, py); ctx.stroke();
      ctx.fillText(`${gy}m`, 4, py - 3);
    }

    // Draw origin axes slightly brighter
    ctx.strokeStyle = AXIS_COLOUR;
    ctx.lineWidth = 1.5;
    const [ox, oy] = toCanvas(0, 0);
    ctx.beginPath(); ctx.moveTo(ox, 0); ctx.lineTo(ox, h); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(0, oy); ctx.lineTo(w, oy); ctx.stroke();
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
   * Draw the planter as a directional triangle that rotates to
   * match its current heading.
   */
  function drawPlanter() {
    const [px, py] = toCanvas(state.posX, state.posY);
    const rad = (state.heading - 90) * (Math.PI / 180); // -90 because 0° = North (up)
    const size = 12;

    ctx.save();
    ctx.translate(px, py);
    ctx.rotate(rad);

    // Triangle (points right in local coords, rotated by heading)
    ctx.beginPath();
    ctx.moveTo(size, 0);
    ctx.lineTo(-size * 0.6, -size * 0.5);
    ctx.lineTo(-size * 0.6, size * 0.5);
    ctx.closePath();
    ctx.fillStyle = PLANTER_COLOUR;
    ctx.fill();
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    ctx.restore();

    // Glow effect
    ctx.beginPath();
    ctx.arc(px, py, 16, 0, Math.PI * 2);
    const grad = ctx.createRadialGradient(px, py, 2, px, py, 16);
    grad.addColorStop(0, 'rgba(63,185,80,0.35)');
    grad.addColorStop(1, 'rgba(63,185,80,0)');
    ctx.fillStyle = grad;
    ctx.fill();
  }

  /** Master render — clears canvas and draws all layers. */
  function renderCanvas() {
    const w = DOM.mapCanvas.width;
    const h = DOM.mapCanvas.height;
    if (w === 0 || h === 0) return;

    ctx.clearRect(0, 0, w, h);
    // Dark background fill
    ctx.fillStyle = '#0a0c10';
    ctx.fillRect(0, 0, w, h);

    computeTransform();
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
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href     = url;
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

  /* --- E-STOP (highest priority) --- */
  DOM.btnEstop.addEventListener('click', () => {
    const payload = { command: 'ESTOP', pwm: 1500 };
    wsSend(payload);
    // Also update UI immediately for responsiveness
    state.systemState = 'E-STOP';
    updateTelemetryUI();
    console.warn('[E-STOP] Emergency stop command sent!');
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

  /* --- Canvas resize observer --- */
  const resizeObserver = new ResizeObserver(() => { resizeCanvas(); });
  resizeObserver.observe(DOM.canvasWrapper);

  // =========================================================================
  // 10. INITIALISATION
  // =========================================================================
  resizeCanvas();
  wsConnect();
  console.log('[GCS] Ground Control Station initialised.');
});