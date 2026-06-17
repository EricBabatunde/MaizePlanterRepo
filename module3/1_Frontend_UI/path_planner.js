/* =============================================================================
   PATH_PLANNER.JS — Boustrophedon Route Generator
   Autonomous Smart Maize Planter — Ground Control Station
   ============================================================================
   Generates a serpentine (boustrophedon) waypoint coverage path for the
   autonomous maize planter.

   Ported from:  2_Backend_Server/path_planner.py  (generate_waypoints)
   Depends on:   geo_math.js  (calculateGlobalCoordinate)
   ============================================================================ */

'use strict';

/**
 * Generate a complete serpentine (boustrophedon) mission plan.
 *
 * The algorithm produces a coverage pattern where the planter:
 *   1. Starts at the bottom-right of the field (local origin 0, 0).
 *   2. Drives along the Y-axis for the full field length (forward pass).
 *   3. Steps LEFT in the negative X direction by one row spacing.
 *   4. Drives back along the Y-axis (return pass — reversed Y coords).
 *   5. Repeats until the full field width is covered.
 *
 * This is the "Long Vector" strategy — only two waypoints per row
 * (start and end), which is sufficient for straight-line autonomous
 * navigation between headlands.
 *
 * For each generated local point, the function calls
 * calculateGlobalCoordinate() from geo_math.js to compute the
 * corresponding WGS-84 latitude and longitude.
 *
 * ── Boustrophedon Pattern (top-down view, 3 rows shown) ──
 *
 *       Row 2         Row 1         Row 0
 *     x=-1.2        x=-0.6         x=0
 *       │             │              │
 *       ▼  ◄──────────┘   ┌─────────┘
 *       │                 │
 *       └──────────────►  ▼
 *
 * @param {number} startLat          — Origin latitude (decimal degrees).
 * @param {number} startLon          — Origin longitude (decimal degrees).
 * @param {number} initialHeadingDeg — Compass heading (degrees, 0 = North).
 * @param {number} fieldLength       — Along-track length of the field (metres).
 * @param {number} fieldWidth        — Cross-track width of the field (metres).
 * @param {number} [rowSpacing=0.6]  — Distance between rows (metres).
 *                                     Defaults to 0.6 m per the planter's
 *                                     mechanical seed spacing.
 *
 * @returns {Array<Object>} An array of waypoint objects, each containing:
 *   {
 *     row:     number,  // Zero-based row index
 *     lat:     number,  // WGS-84 latitude (decimal degrees)
 *     lon:     number,  // WGS-84 longitude (decimal degrees)
 *     local_x: number,  // Cross-track offset (metres, negative = left)
 *     local_y: number   // Along-track offset (metres)
 *   }
 */
function generateWaypoints(startLat, startLon, initialHeadingDeg, fieldLength, fieldWidth, rowSpacing = 0.6) {

  const waypoints = [];

  /* ── Calculate the number of rows ──────────────────────────────────
   * Math.ceil ensures we cover the full field width even when it is
   * not perfectly divisible by the row spacing.
   * The +1 accounts for the first row at x = 0 (fencepost counting). */
  const numRows = Math.ceil(fieldWidth / rowSpacing) + 1;

  console.log(`[PathPlanner] Generating mission — ${numRows} rows, ${fieldLength} m long, ${rowSpacing} m spacing.`);

  for (let i = 0; i < numRows; i++) {

    /* ── 1. Local Cartesian Grid (x, y) ──────────────────────────────
     * The planter starts at the bottom-right (0, 0).
     * Turning LEFT means stepping in the NEGATIVE X direction.
     * Each subsequent row shifts left by one row spacing. */
    const xLocal = -i * rowSpacing;

    /* ── 2. Boustrophedon alternation ────────────────────────────────
     * Even-indexed rows (0, 2, 4 …) → forward pass:  Y goes 0 → length
     * Odd-indexed rows  (1, 3, 5 …) → return pass:   Y goes length → 0
     * This creates the serpentine (back-and-forth) pattern. */
    let yStart, yEnd;
    if (i % 2 === 0) {
      yStart = 0.0;
      yEnd   = fieldLength;
    } else {
      yStart = fieldLength;
      yEnd   = 0.0;
    }

    /* ── 3. Generate two waypoints per row ("Long Vector" strategy) ── */
    const rowPoints = [
      { lx: xLocal, ly: yStart },
      { lx: xLocal, ly: yEnd },
    ];

    for (const point of rowPoints) {
      /* ── 4. Convert local (x, y) → global (lat, lon) ──────────────
       * Delegates to geo_math.js which applies the rotation matrix
       * and flat-earth translation identically to the Python version. */
      const global = calculateGlobalCoordinate(
        startLat,
        startLon,
        point.lx,
        point.ly,
        initialHeadingDeg
      );

      /* ── 5. Assemble the waypoint object ───────────────────────────
       * Structure matches the Pydantic Waypoint model from
       * data_models.py:  { row, lat, lon, local_x, local_y }         */
      waypoints.push({
        row:     i,
        lat:     global.lat,
        lon:     global.lon,
        local_x: point.lx,
        local_y: point.ly,
      });
    }
  }

  console.log(`[PathPlanner] Generated ${waypoints.length} waypoints across ${numRows} rows.`);
  return waypoints;
}
