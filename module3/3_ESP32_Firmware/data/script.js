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
    btnStartMission: document.getElementById('btnStartMission'),
    // Map canvas
    mapCanvas: document.getElementById('mapCanvas'),
    canvasWrapper: document.getElementById('canvasWrapper'),
    // Diagnostics
    btnUTurnTest: document.getElementById('btnUTurnTest'),
    toggleLogging: document.getElementById('data-log-switch'),
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
    lat: 0,        // Live GPS latitude (decimal degrees) from Pixhawk
    lon: 0,        // Live GPS longitude (decimal degrees) from Pixhawk
    posX: 0,
    posY: 0,
    wpDist: 0,
    battery: 0,
    ping: 0,
    seedRPM: 0,
    gpsFix: 'No Fix',
    // Mission waypoints [{x, y}, ...] for canvas rendering
    waypoints: [],
    currentWaypointIndex: 0,
    // Data logging
    isLogging: false,
    logBuffer: [],
    // Static farmland scaling (Pixels-Per-Metre)
    ppm: 1,
    fieldWidth: 50,   // metres — default
    fieldLength: 50,   // metres — default
  };

  // =========================================================================
  // 2b. CONSOLE DEBUG STATE TRACKING
  // =========================================================================
  // State change tracker — logs highly visible transition messages to the
  // mobile browser console for engineering field diagnostics.
  let previousState = '';

  // 1Hz console telemetry throttle — prevents flooding the mobile browser
  let lastConsoleLogTime = 0;
  const CONSOLE_LOG_INTERVAL_MS = 1000;

  // =========================================================================
  // 3. WEBSOCKET CLIENT MODULE
  // =========================================================================
  const WS_URL = `ws://192.168.4.1/ws`;
  let ws = null;
  let reconnectTimer = null;
  const RECONNECT_DELAY_MS = 3000;
  let pingIntervalId = null;

  /** Establish WebSocket connection to the ESP32. */
  function wsConnect() {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;
    try { ws = new WebSocket(WS_URL); } catch (e) { console.error('[WS] Creation failed:', e); return; }
    ws.onopen = () => {
      state.wsConnected = true;
      clearTimeout(reconnectTimer);
      updateConnectionUI(true);
      console.log('[WS] Connected to', WS_URL);

      // Start 1Hz PING for latency measurement
      clearInterval(pingIntervalId);
      pingIntervalId = setInterval(() => {
        wsSend({ command: 'PING', timestamp: Date.now() });
      }, 1000);
    };
    ws.onclose = (ev) => {
      state.wsConnected = false;
      updateConnectionUI(false);
      clearInterval(pingIntervalId);
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

        // Intercept PONG responses for latency calculation
        if (data.type === 'PONG' && data.timestamp) {
          state.ping = Date.now() - data.timestamp;
          DOM.valLatency.textContent = `${state.ping} ms`;
          return; // Do not pass PONG into the telemetry handler
        }

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
    // Update internal state from primary keys
    if (d.state !== undefined) state.systemState = d.state;
    if (d.v_g !== undefined) state.groundSpeed = d.v_g;
    if (d.heading !== undefined) {
      /* SPINNING-HEADING FIX — Normalise to 0–360°.
         If the ESP32 sends cumulative gyroscope deltas or values
         outside the expected range, this clamp prevents the canvas
         rotation from spiralling out of control. */
      state.heading = ((d.heading % 360) + 360) % 360;
    }
    // Live GPS coordinates from Pixhawk (decimal degrees)
    if (d.lat !== undefined) state.lat = d.lat;
    if (d.lon !== undefined) state.lon = d.lon;
    if (d.x !== undefined) state.posX = d.x;
    if (d.y !== undefined) state.posY = d.y;
    if (d.wp_dist !== undefined) state.wpDist = d.wp_dist;
    if (d.batt !== undefined) state.battery = d.batt;
    if (d.ping !== undefined) state.ping = d.ping;
    if (d.seed_rpm !== undefined) state.seedRPM = d.seed_rpm;
    if (d.gps_fix !== undefined) state.gpsFix = d.gps_fix;

    // Parse explicit debug keys (duplicated in C++ for clarity)
    if (d.system_state !== undefined) state.systemState = d.system_state;
    if (d.wp_distance !== undefined) state.wpDist = d.wp_distance;
    if (d.ground_speed !== undefined) state.groundSpeed = d.ground_speed;

    // ── Console State Change Tracker ──────────────────────────────────────────
    if (state.systemState !== previousState) {
      console.log('====== STATE TRANSITION: ' + previousState + ' -> ' + state.systemState + ' ======');

      // Advance waypoint index when the FSM transitions to RETRACTING
      // (indicates the planter has reached the end of a row)
      if (state.systemState === 'RETRACTING' && state.waypoints.length > 0) {
        // Each row has 2 waypoints (start + end), so advance by 2
        state.currentWaypointIndex = Math.min(
          state.currentWaypointIndex + 2,
          state.waypoints.length - 1
        );
      }

      previousState = state.systemState;
    }

    // ── 1Hz Throttled Navigation Telemetry Log ────────────────────────
    const now = Date.now();
    if (now - lastConsoleLogTime >= CONSOLE_LOG_INTERVAL_MS) {
      lastConsoleLogTime = now;
      console.log(
        '[Telemetry] Dist to WP: ' + state.wpDist.toFixed(2) + 'm | ' +
        'Speed: ' + state.groundSpeed.toFixed(2) + 'm/s | ' +
        'Heading: ' + state.heading.toFixed(1) + '°'
      );
    }

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
    // System state badge — map FSM states to CSS badge classes
    const s = state.systemState;
    DOM.valState.textContent = s;
    let badgeClass = 'idle';
    if (s === 'PLANTING')       badgeClass = 'auto';
    else if (s === 'DEPLOYING' || s === 'RETRACTING' || s === 'TURNING') badgeClass = 'manual';
    else if (s === 'E-STOP')    badgeClass = 'estop';
    DOM.valState.className = 'telemetry-card__value badge badge--' + badgeClass;

    // Numerical readouts
    DOM.valSpeed.innerHTML = `${state.groundSpeed.toFixed(2)} <small>m/s</small>`;
    DOM.valHeading.innerHTML = `${state.heading.toFixed(1)}<small>°</small>`;
    DOM.valWpDist.innerHTML = `${state.wpDist.toFixed(2)} <small>m</small>`;
    DOM.valPosition.textContent = `${state.posX.toFixed(1)} , ${state.posY.toFixed(1)}`;

    // Header indicators
    DOM.valLatency.textContent = `${state.ping} ms`;
    DOM.valBattery.textContent = `${state.battery.toFixed(1)} V`;

    // GPS Fix indicator — colour-coded LED and label from telemetry
    const fix = state.gpsFix;
    DOM.valGPS.textContent = fix;
    if (fix === '3D Fix' || fix === 'DGPS' || fix === 'RTK Float' || fix === 'RTK Fixed') {
      DOM.ledGPS.className = 'led led--fix-ok';
    } else if (fix === '2D Fix') {
      DOM.ledGPS.className = 'led led--fix-2d';
    } else {
      DOM.ledGPS.className = 'led led--no-fix';
    }
  }

  // =========================================================================
  // 6. CANVAS RENDERER MODULE — Padded PPM with Bottom-Left Origin
  // =========================================================================

  /* Global canvas padding in pixels — ensures the field boundary never
     touches the edge of the canvas on any side.  All coordinate transforms
     account for this offset so that field origin (0, 0) sits exactly at
     (PADDING, canvas.height − PADDING) on screen. */
  const PADDING = 50;

  const GRID_COLOUR    = 'rgba(255,255,255,0.06)';
  const AXIS_COLOUR    = 'rgba(255,255,255,0.15)';
  const WP_COLOUR      = '#58a6ff';
  const WP_LINE_COLOUR = 'rgba(88,166,255,0.3)';
  const PLANTER_COLOUR = '#3fb950';
  const TRAIL_COLOUR   = 'rgba(63,185,80,0.25)';
  const FIELD_BORDER_COLOUR    = 'rgba(63,185,80,0.7)';
  const FIELD_FILL_COLOUR      = 'rgba(63,185,80,0.04)';

  /**
   * Apply the user-defined farmland dimensions to the canvas.
   * Calculates a fixed Pixels-Per-Metre (PPM) scale that fits
   * the entire field WITHIN the padded drawable area, maintaining
   * the physical aspect ratio.
   *
   * The drawable area is:
   *   drawableWidth  = containerWidth  − (2 × PADDING)
   *   drawableHeight = containerHeight − (2 × PADDING)
   *
   * PPM is the smaller of (drawableWidth / fieldWidth) and
   * (drawableHeight / fieldLength) so the field always fits
   * without distortion.
   */
  function applyMapScale() {
    const fieldW = parseFloat(DOM.inputFieldWidth.value)  || 50;
    const fieldL = parseFloat(DOM.inputFieldLength.value) || 50;

    state.fieldWidth  = fieldW;
    state.fieldLength = fieldL;

    /* Use the wrapper's CSS dimensions as the canvas buffer size */
    const containerWidth  = DOM.canvasWrapper.clientWidth;
    const containerHeight = DOM.canvasWrapper.clientHeight;
    if (containerWidth < 1 || containerHeight < 1) return;

    /* Set canvas buffer to exactly fill the wrapper */
    DOM.mapCanvas.width  = containerWidth;
    DOM.mapCanvas.height = containerHeight;

    /* Calculate the drawable area after removing padding from all sides */
    const drawableW = containerWidth  - (2 * PADDING);
    const drawableH = containerHeight - (2 * PADDING);
    if (drawableW < 1 || drawableH < 1) return;

    /* PPM = whichever axis is tighter, so the field fits entirely */
    state.ppm = Math.min(drawableW / fieldW, drawableH / fieldL);

    console.log(`[Map] Scale applied — Field: ${fieldW}×${fieldL} m, PPM: ${state.ppm.toFixed(2)}, Canvas: ${DOM.mapCanvas.width}×${DOM.mapCanvas.height} px, Padding: ${PADDING}px`);
    renderCanvas();
  }

  /**
   * Convert field coordinates (metres) to canvas pixel coordinates.
   *
   * The origin (0, 0) of the mathematical field is placed at:
   *   screenX = PADDING
   *   screenY = canvas.height − PADDING
   *
   * So the full transform is:
   *   PixelX = PADDING + (fieldX × PPM)
   *   PixelY = (canvas.height − PADDING) − (fieldY × PPM)
   *
   * This guarantees PADDING pixels of clearance on all four sides.
   */
  function toCanvas(x, y) {
    return [
      PADDING + (x * state.ppm),
      (DOM.mapCanvas.height - PADDING) - (y * state.ppm),
    ];
  }

  /**
   * Draw a highly visible rectangular boundary box representing the
   * exact edges of the farmland (0,0) to (fieldWidth, fieldLength).
   * Rendered as a green border with a subtle translucent fill so the
   * operator can clearly see where the field begins and ends.
   */
  function drawFieldBoundary() {
    const [x0, y0] = toCanvas(0, 0);                           // bottom-left
    const [x1, y1] = toCanvas(state.fieldWidth, state.fieldLength); // top-right

    const rectX = x0;
    const rectY = y1;    // y1 is higher on screen (smaller pixel value)
    const rectW = x1 - x0;
    const rectH = y0 - y1;

    // Translucent fill
    ctx.fillStyle = FIELD_FILL_COLOUR;
    ctx.fillRect(rectX, rectY, rectW, rectH);

    // Visible green border
    ctx.strokeStyle = FIELD_BORDER_COLOUR;
    ctx.lineWidth   = 2;
    ctx.setLineDash([]);
    ctx.strokeRect(rectX, rectY, rectW, rectH);

    // Corner labels for field extents
    ctx.fillStyle = 'rgba(63,185,80,0.6)';
    ctx.font = '10px sans-serif';
    ctx.fillText('(0, 0)', x0 + 4, y0 - 4);
    ctx.fillText(`(${state.fieldWidth}, ${state.fieldLength})`, x1 - 60, y1 + 14);
  }

  /**
   * Draw a static reference grid at fixed metre increments within
   * the padded field area.  Uses 1 m lines when PPM ≥ 10, otherwise
   * 5 m lines to avoid visual clutter on large fields.
   */
  function drawGrid() {
    const step = state.ppm >= 10 ? 1 : 5; // metres per grid line

    ctx.strokeStyle = GRID_COLOUR;
    ctx.lineWidth   = 1;
    ctx.font        = '10px sans-serif';
    ctx.fillStyle   = AXIS_COLOUR;

    /* Grid boundaries in pixel space */
    const [xMin, yMax] = toCanvas(0, 0);                             // bottom-left
    const [xMax, yMin] = toCanvas(state.fieldWidth, state.fieldLength); // top-right

    // Vertical grid lines (X-axis, left to right)
    for (let gx = 0; gx <= state.fieldWidth; gx += step) {
      const [px] = toCanvas(gx, 0);
      ctx.beginPath(); ctx.moveTo(px, yMin); ctx.lineTo(px, yMax); ctx.stroke();
      ctx.fillText(`${gx}m`, px + 3, yMax - 4);
    }

    // Horizontal grid lines (Y-axis, bottom to top)
    for (let gy = 0; gy <= state.fieldLength; gy += step) {
      const [, py] = toCanvas(0, gy);
      ctx.beginPath(); ctx.moveTo(xMin, py); ctx.lineTo(xMax, py); ctx.stroke();
      ctx.fillText(`${gy}m`, xMin + 4, py - 3);
    }

    // Draw origin axes (X=0 and Y=0) slightly brighter
    ctx.strokeStyle = AXIS_COLOUR;
    ctx.lineWidth   = 1.5;
    // X=0 vertical axis
    ctx.beginPath(); ctx.moveTo(xMin, yMin); ctx.lineTo(xMin, yMax); ctx.stroke();
    // Y=0 horizontal axis (bottom of field)
    ctx.beginPath(); ctx.moveTo(xMin, yMax); ctx.lineTo(xMax, yMax); ctx.stroke();
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
   * Draw the planter as a directional triangle using kinematic
   * interpolation.  If waypoints exist, the icon position is
   * calculated by interpolating between the previous and current
   * waypoints using distToWaypoint telemetry.  During the TURNING
   * state, the icon snaps to the start of the next row.
   *
   * Fallback: if no waypoints are loaded, uses raw (posX, posY).
   */
  function drawPlanter() {
    let px, py;
    const wps = state.waypoints;
    const idx = state.currentWaypointIndex;

    if (wps.length >= 2 && idx > 0 && idx < wps.length) {
      if (state.systemState === 'TURNING') {
        // Snap to the start of the next row
        const nextIdx = Math.min(idx + 1, wps.length - 1);
        [px, py] = toCanvas(wps[nextIdx].x, wps[nextIdx].y);
      } else {
        // Kinematic interpolation along the current segment
        const wpPrev = wps[idx - 1];
        const wpCurr = wps[idx];
        const segDx = wpCurr.x - wpPrev.x;
        const segDy = wpCurr.y - wpPrev.y;
        const segLen = Math.sqrt(segDx * segDx + segDy * segDy);

        if (segLen > 0.01) {
          // distToWaypoint is distance REMAINING to wpCurr
          const travelled = Math.max(0, segLen - state.wpDist);
          const t = Math.min(1, Math.max(0, travelled / segLen));
          const interpX = wpPrev.x + segDx * t;
          const interpY = wpPrev.y + segDy * t;
          [px, py] = toCanvas(interpX, interpY);
        } else {
          [px, py] = toCanvas(wpCurr.x, wpCurr.y);
        }
      }
    } else {
      // Fallback: use raw telemetry position
      [px, py] = toCanvas(state.posX, state.posY);
    }

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

    drawFieldBoundary();
    drawGrid();
    drawWaypoints();
    drawPlanter();
  }

  // =========================================================================
  // 7. DATA LOGGING & CSV EXPORT
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
  // 8. EVENT LISTENERS
  // =========================================================================

  /* --- Start Mission ---
     Reads the live GPS origin from telemetry, generates a boustrophedon
     path via path_planner.js, renders it on the canvas, and sends the
     full mission payload to the ESP32 over WebSocket. */
  DOM.btnStartMission.addEventListener('click', () => {
    // 1. Read the live GPS origin from current telemetry
    const originLat = state.lat;
    const originLon = state.lon;
    const originHeading = state.heading;

    // 2. Read field dimensions from the HTML inputs
    const fieldLength = parseFloat(DOM.inputFieldLength.value) || 50;
    const fieldWidth  = parseFloat(DOM.inputFieldWidth.value)  || 50;
    const rowSpacing  = parseFloat(DOM.inputRowSpacing.value)  || 0.6;

    // 3. Generate the boustrophedon waypoint path
    const generatedWPs = generateWaypoints(
      originLat, originLon, originHeading,
      fieldLength, fieldWidth, rowSpacing
    );

    if (generatedWPs.length < 2) {
      console.error('[Mission] Path planner generated fewer than 2 waypoints. Aborting.');
      return;
    }

    // 4. Render the generated waypoints on the 2D canvas
    //    Map the path planner's local_x/local_y to the canvas coordinate system
    state.waypoints = generatedWPs.map(wp => ({
      x: wp.local_x,
      y: wp.local_y,
    }));
    state.currentWaypointIndex = 1; // Start interpolating toward WP1
    renderCanvas();

    // 5. Package and send the mission to the ESP32
    const payload = {
      command: 'MISSION',
      waypoints: generatedWPs,
    };
    wsSend(payload);

    console.log(`[Mission] MISSION sent — ${generatedWPs.length} waypoints, ` +
                `origin: (${originLat.toFixed(7)}, ${originLon.toFixed(7)}), ` +
                `heading: ${originHeading.toFixed(1)}°, ` +
                `field: ${fieldLength}×${fieldWidth} m, row spacing: ${rowSpacing} m`);
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
      wsSend({ command: 'START_LOG' });
      console.log('[Logger] Data logging started — buffer cleared, ESP32 logging enabled.');
    } else {
      wsSend({ command: 'STOP_LOG' });
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