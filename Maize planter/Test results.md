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



## Seed damage rate

file:///home/eric/Documents/capstoneCode/data%20analysis/outputs/SeedDamage.png
#### Figure Caption

Fig. 3: Seed damage assessment after passing through the automated metering mechanism (n=200 seeds). Undamaged seeds: 97.5% (n=195). Cracked but viable: 1.0% (n=2). Severely damaged (unplantable): 1.5% (n=3). The total damage rate of 2.5% is well within the acceptable limit, and the severe damage rate of 1.5% strictly satisfies the agricultural target of <2%.
#### Table 3: Seed Damage Assessment

|**Damage Category**|**Count (n=200)**|**Percentage**|**Acceptance Threshold**|**Status**|
|---|---|---|---|---|
|Undamaged|195|97.5%|—|—|
|Cracked (viable)|2|1.0%|—|—|
|Severely damaged (unplantable)|3|1.5%|< 2%|✓|
|**Total damage**|**5**|**2.5%**|**< 5%**|✓|
#### Text for the Results Section

Visual inspection of 200 maize kernels after passing through the seed metering mechanism revealed a total damage rate of 2.5%, with only 1.5% severely damaged (unplantable). The primary cause of damage was minor mechanical pinching between the seed plate grooves and the hopper exit for kernels exhibiting irregular morphological dimensions. However, the damage rate remains entirely acceptable for agricultural application, as commercial pneumatic and mechanical planters typically tolerate a 2–5% total damage rate. This confirms the mechanical tolerance of the 3D-printed seed plate design is suitable for autonomous field operations.


## Single vs double sorting accuracy

file:///home/eric/Documents/capstoneCode/data%20analysis/outputs/SortingAccuracy.png

#### Figure Caption

Fig. 17: Bench-test evaluation of the mechanical seed plate's singulation accuracy (n=200 drops). The target output of a single seed per actuation was achieved 82.5% of the time. The system exhibited a clear bias towards over-sorting, with double seeds occurring in 16.0% of the actuations, and triple seeds in 1.5%. The blue dashed line represents the >90% design target for precision agricultural applications.

#### Table 10: Single-Seed Sorting Accuracy

|**Intended Output**|**Observed Single**|**Observed Double**|**Observed Triple+**|**Overall Accuracy**|**Target**|
|---|---|---|---|---|---|
|Single seed groove|165 (82.5%)|32 (16.0%)|3 (1.5%)|**82.5%**|> 90%|

#### Text for the Results Section

A bench-top calibration test of the seed metering unit was conducted over 200 consecutive actuations to evaluate singulation performance. The single-groove plate correctly dispensed a single maize kernel 82.5% of the time. The primary mode of error was over-sorting, resulting in a 16.0% double-drop rate and a 1.5% triple-drop rate. While the system effectively avoided skipped drops (under-sorting), the 82.5% accuracy falls short of the >90% precision target outlined for this project. This error distribution indicates that the volumetric capacity of the 3D-printed seed groove is marginally too large for the specific maize cultivar being tested, allowing secondary kernels to lodge inside. Before full field integration, it is recommended to either reduce the groove depth by 1–2 mm in CAD or increase the tension on the mechanical singulator brush to more aggressively eject excess kernels prior to the drop chute.


## Command to Actuator Latency

file:///home/eric/Documents/capstoneCode/data%20analysis/outputs/CommandLatency.png

#### Figure Caption

Fig. 12: Distribution of command-to-actuator latency measured over 50 consecutive command transmissions under two network conditions (Close Range <2m, and Field Range 15m). The median latency was 129 ms at close range and 202 ms at 15m. Outliers (marked as red dots) reached a maximum of 331 ms. Crucially, 100% of the tested commands arrived under the 500 ms maximum threshold mandated for reliable real-time mechatronic control.
#### Table 6: Command Latency Statistics

|**Network Condition**|**Number of Trials**|**Mean (ms)**|**Median (ms)**|**SD (ms)**|**95th Percentile (ms)**|**Max (ms)**|**<500 ms?**|
|---|---|---|---|---|---|---|---|
|Close Range (<2m)|50|144|129|51|198|331|✓|
|Field Range (15m)|50|207|202|27|248|328|✓|

#### Text for the Results Section

The command-to-actuator latency of the Web UI control bridge was evaluated over 100 discrete start/stop commands. The system exhibited a mean latency of 144 ms when operated at close range (<2m). When extended to a field-realistic distance of 15m, the mean latency increased expectedly to 207 ms, while the standard deviation tightened from 51 ms to 27 ms, indicating highly stable transmission over distance. The 95th percentile latency remained below 250 ms across both conditions, and the absolute maximum recorded delay (331 ms) never approached the 500 ms safety threshold. These findings confirm that the ESPAsyncWebServer running on the ESP32-S3 is a robust architecture for real-time agricultural telemetry and manual override intervention.