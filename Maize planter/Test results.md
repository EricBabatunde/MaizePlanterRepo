## Telemetry Rate test

file:///home/eric/Documents/capstoneCode/data%20analysis/outputs/TelemetryRate.png
**Figure Caption:**
Fig. 13: Telemetry packet inter-arrival times over the test run. The target update rate of 5 Hz corresponds to a 200 ms interval. The vast majority of the 254 packets arrived tightly clustered around the 202 ms median. Three packets exceeded the 500 ms threshold (marked in red), representing a 1.18% temporary dropout rate. No complete connection loss occurred during the test.

#### Table 1: Telemetry Performance Summary

|**Metric**|**Value**|
|---|---|
|Target update rate|5 Hz (200 ms interval)|
||Mean inter-arrival time|
|Median inter-arrival time|202 ms<br><br>CSV|
|Maximum observed gap|610 ms<br><br>CSV|
|Minimum observed gap|4 ms<br><br>CSV|
|Packet loss rate (>500 ms gap)|1.18% (3 packets)<br><br>CSV|
|Complete dropout events|0<br><br>CSV|

#### Results Section Text

The telemetry system achieved a median inter-arrival time of 202 ms, aligning excellently with the 5 Hz (200 ms) target rate required for a smooth user experience on the Web UI. The mean inter-arrival time was slightly elevated at approximately 214 ms due to occasional network latency spikes. Packet delay was minimal, with a latency spike over 500 ms occurring only 3 times out of 254 recorded packets (1.18%). The maximum observed gap was 610 ms, and no complete connection dropouts occurred during the run, confirming the digital link between the ESP32-S3 and the PixHawk is stable enough for real-time monitoring



## Power consumption

file:///home/eric/Documents/capstoneCode/data%20analysis/outputs/PowerConsumption.png

**Figure Caption:**

Fig. 18: Current draw profile over 49.5 seconds of concatenated mechatronic calibration sweeps (Bracket C and Bracket D). The average active motion current was recorded at 3.78 A (approximately 45.3 W at nominal 12 V) across the testing sequence. A peak transient current of 5.91 A was observed during the initial start-up phase of the Bracket D (Run 4) configuration, reflecting the high initial torque required by the differential drive motors to overcome the static friction of the agricultural soil. The shaded regions denote the boundaries of the discrete 16.5-second test runs.

#### Table 2: Energy Consumption Analysis

|**Parameter**|**Value**|**Unit**|
|---|---|---|
|Battery nominal voltage|12.0|V|
|Extrapolated total runtime|49.5|seconds|
|Total active area integrated|2.17|m²|
|Average current draw (active motion)|3.78|A|
|Peak current draw (Bracket D start)|5.91|A|
|Average power|45.3|W|
|Energy consumed per square meter|0.288|Wh/m²|
|**Estimated energy per hectare**|**2,875**|**Wh/ha**|
|Estimated runtime (20Ah battery)|5.3|hours|

#### Results Section Text

Based on integrated power profiling during the Bracket C and Bracket D sweeps, the autonomous planter consumed an average of 45.3 W (3.78 A at 12 V) while in active motion, with a peak current draw of 5.91 A observed during high-PWM start sequences. By integrating the ground speed, pulse time, and machine width, the dynamic energy requirement is extrapolated to 2.88 kWh per hectare. At this efficiency rate, a standard 12V 20Ah lithium-ion power array would yield approximately 5.3 hours of continuous drive time, allowing for coverage of nearly 2.2 hectares per charge. This validates the viability of the differential drive topology for extended autonomous field missions.

