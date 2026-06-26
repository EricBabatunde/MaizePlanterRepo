#include "DataLogger.h"
#include <LittleFS.h>

int validationRunCount = 0;
int jamEventCount = 0;
String currentLogFile = "/Validation_Log.csv";

void initLogger()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("[ERROR] LittleFS Mount Failed!");
    return;
  }
  Serial.println("[SYSTEM] LittleFS Mounted Successfully.");

  if (!LittleFS.exists("/Jam_Events.csv"))
  {
    File f = LittleFS.open("/Jam_Events.csv", FILE_WRITE);
    if (f)
    {
      f.println("Event_Num,Timestamp_ms");
      f.close();
    }
  }
}

void startValidationLog()
{
  validationRunCount++;
  currentLogFile = "/Pixhawk_Val_" + String(validationRunCount) + ".csv";

  File file = LittleFS.open(currentLogFile, FILE_WRITE);
  if (file)
  {
    file.println("Time_ms,PixhawkMode,GroundSpeed_ms,Current_A");
    file.close();
    Serial.printf("[LOGGING] Started new validation log: %s\n", currentLogFile.c_str());
  }
}

void logValidationData(String mode, float speed, float amps)
{
  File file = LittleFS.open(currentLogFile, FILE_APPEND);
  if (file)
  {
    file.printf("%lu,%s,%.3f,%.2f\n", millis(), mode.c_str(), speed, amps);
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
  }
}