#include "DataLogger.h"
#include <LittleFS.h>

// Local variables hidden from the rest of the system
int runCountA = 0, runCountB = 0, runCountC = 0, runCountD = 0;
int runCountTel = 0;
int jamEventCount = 0;
String currentLogFile = "/log.csv";
String telLogFile = "/tel.csv"; // Dedicated string so it doesn't overwrite cruise logs

void initLogger()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("[ERROR] LittleFS Mount Failed!");
    return;
  }
  Serial.println("[SYSTEM] LittleFS Mounted Successfully.");

  // Create persistent cumulative log files if they don't already exist
  if (!LittleFS.exists("/TurnTest_Log.csv"))
  {
    File f = LittleFS.open("/TurnTest_Log.csv", FILE_WRITE);
    if (f) { f.println("Direction,Duration_ms"); f.close(); }
  }
  if (!LittleFS.exists("/Jam_Events.csv"))
  {
    File f = LittleFS.open("/Jam_Events.csv", FILE_WRITE);
    if (f) { f.println("Event_Num,Timestamp_ms"); f.close(); }
  }
}

void startNewLogFile(String bracketName)
{
  if (bracketName == "A")
  {
    runCountA++;
    currentLogFile = "/BracketA_Run" + String(runCountA) + ".csv";
  }
  else if (bracketName == "B")
  {
    runCountB++;
    currentLogFile = "/BracketB_Run" + String(runCountB) + ".csv";
  }
  else if (bracketName == "C")
  {
    runCountC++;
    currentLogFile = "/BracketC_Run" + String(runCountC) + ".csv";
  }
  else if (bracketName == "D")
  {
    runCountD++;
    currentLogFile = "/BracketD_Run" + String(runCountD) + ".csv";
  }

  File file = LittleFS.open(currentLogFile, FILE_WRITE);
  if (file)
  {
    file.println("PWM_us,GroundSpeed_ms,Current_A"); // Added Current
    file.close();
  }
}

// Ensure you update the definition in DataLogger.h to match this new signature!
void logToFS(int pwm, float speed, float amps)
{
  File file = LittleFS.open(currentLogFile, FILE_APPEND);
  if (file)
  {
    file.printf("%d,%.3f,%.2f\n", pwm, speed, amps);
    file.close();
  }
}

void logTurnEvent(String direction, unsigned long duration)
{
  File file = LittleFS.open("/TurnTest_Log.csv", FILE_APPEND);
  if (file)
  {
    file.printf("%s,%lu\n", direction.c_str(), duration);
    file.close();
  }
}

void startTelemetryLog()
{
  runCountTel++;
  telLogFile = "/Telemetry_Run" + String(runCountTel) + ".csv";
  File file = LittleFS.open(telLogFile, FILE_WRITE);
  if (file)
  {
    file.println("Interval_ms");
    file.close();
    Serial.printf("[LOGGING] Telemetry log created: %s\n", telLogFile.c_str());
  }
}

void logTelemetry(int interval)
{
  File file = LittleFS.open(telLogFile, FILE_APPEND);
  if (file)
  {
    file.printf("%d\n", interval);
    file.close();
  }
}

void logJamEvent(unsigned long timestamp)
{
  jamEventCount++;
  File file = LittleFS.open("/Jam_Events.csv", FILE_APPEND);
  if (file)
  {
    file.printf("%d,%lu\n", jamEventCount, timestamp);
    file.close();
    Serial.printf("[LOGGING] Jam event #%d logged at %lums\n", jamEventCount, timestamp);
  }
}