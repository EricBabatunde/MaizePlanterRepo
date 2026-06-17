/* =============================================================================
   GEO_MATH.JS — Geodesy Module
   Autonomous Smart Maize Planter — Ground Control Station
   ============================================================================
   Converts local Cartesian coordinates (X, Y in metres) into Global Earth
   Coordinates (Latitude, Longitude) using a flat-earth approximation.

   Ported from:  2_Backend_Server/path_planner.py  (lines 41–48)
   ============================================================================ */

'use strict';

/**
 * WGS-84 semi-major axis (equatorial radius of the Earth) in metres.
 * This is the standard value used in GPS and all major geodetic systems.
 * @constant {number}
 */
const EARTH_RADIUS_M = 6378137.0;

/**
 * Convert an angle from degrees to radians.
 *
 * The conversion formula is:
 *     radians = degrees × (π / 180)
 *
 * This is required because JavaScript's Math.sin(), Math.cos(), etc.
 * all expect their arguments in radians, not compass degrees.
 *
 * @param   {number} degrees — Angle in degrees (0–360 or arbitrary).
 * @returns {number}         — Equivalent angle in radians.
 */
function degreesToRadians(degrees) {
  return degrees * (Math.PI / 180.0);
}

/**
 * Convert an angle from radians to degrees.
 *
 * The conversion formula is:
 *     degrees = radians × (180 / π)
 *
 * @param   {number} radians — Angle in radians.
 * @returns {number}         — Equivalent angle in degrees.
 */
function radiansToDegrees(radians) {
  return radians * (180.0 / Math.PI);
}

/**
 * Calculate a global (Lat, Lon) coordinate from a local Cartesian offset.
 *
 * This function applies the same two-stage transformation used in the
 * Python path_planner.py:
 *
 *   STAGE 1 — Rotation Matrix  (align local grid to True North)
 *   ─────────────────────────────────────────────────────────────
 *   The planter's local coordinate frame (X = cross-track, Y = along-track)
 *   is rotated by the compass heading so that the Y-axis aligns with the
 *   initial heading direction.  This is a standard 2D clockwise rotation
 *   for geographic compass bearings:
 *
 *       deltaNorth = (localY × cos(heading)) − (localX × sin(heading))
 *       deltaEast  = (localY × sin(heading)) + (localX × cos(heading))
 *
 *   STAGE 2 — Geographic Translation  (metres → decimal degrees)
 *   ─────────────────────────────────────────────────────────────
 *   The metre offsets are converted to latitude/longitude deltas using
 *   the flat-earth approximation (valid for small displacements):
 *
 *       newLat = originLat + (deltaNorth / R) × (180 / π)
 *       newLon = originLon + (deltaEast  / (R × cos(originLat))) × (180 / π)
 *
 *   where R = 6 378 137 m (WGS-84 equatorial radius).
 *
 * @param {number} originLat  — Origin latitude in decimal degrees.
 * @param {number} originLon  — Origin longitude in decimal degrees.
 * @param {number} localX     — Cross-track offset in metres (negative = left).
 * @param {number} localY     — Along-track offset in metres.
 * @param {number} headingDeg — Compass heading in degrees (0 = North, 90 = East).
 *
 * @returns {{ lat: number, lon: number }}
 *   An object containing the computed global latitude and longitude.
 */
function calculateGlobalCoordinate(originLat, originLon, localX, localY, headingDeg) {

  /* ── Convert compass heading from degrees to radians ──────────────── */
  const headingRad = degreesToRadians(headingDeg);

  /* ── Stage 1: Rotation Matrix ─────────────────────────────────────── *
   * Rotate the local (X, Y) offsets into North/East displacements
   * using the planter's compass heading.
   *
   * This is a standard clockwise rotation for geographic bearings:
   *   deltaNorth =  Y·cos(θ) − X·sin(θ)
   *   deltaEast  =  Y·sin(θ) + X·cos(θ)                              */
  const deltaNorth = (localY * Math.cos(headingRad)) - (localX * Math.sin(headingRad));
  const deltaEast  = (localY * Math.sin(headingRad)) + (localX * Math.cos(headingRad));

  /* ── Stage 2: Geographic Translation (metres → decimal degrees) ──── *
   * Apply the flat-earth approximation.  The longitude correction
   * includes cos(originLat) to account for meridian convergence
   * (lines of longitude get closer together as you move towards
   * the poles).                                                        */
  const latNew = originLat + (deltaNorth / EARTH_RADIUS_M) * (180.0 / Math.PI);
  const lonNew = originLon + (deltaEast / (EARTH_RADIUS_M * Math.cos(degreesToRadians(originLat)))) * (180.0 / Math.PI);

  return { lat: latNew, lon: lonNew };
}
