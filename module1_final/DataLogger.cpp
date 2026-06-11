#include "DataLogger.h"
#include <LittleFS.h>

// Local variables hidden from the rest of the system
int runCountA = 0;
int runCountB = 0;
int runCountC = 0;
int runCountD = 0;
String currentLogFile = "/log.csv";

void initLogger() {
  if (!LittleFS.begin(true)) {
    Serial.println("[ERROR] LittleFS Mount Failed!");
  } else {
    Serial.println("[SYSTEM] LittleFS Mounted Successfully.");
  }
}

void startNewLogFile(String bracketName) {
  // Generate a unique filename based on the bracket
  if (bracketName == "A") { runCountA++; currentLogFile = "/BracketA_Run" + String(runCountA) + ".csv"; }
  else if (bracketName == "B") { runCountB++; currentLogFile = "/BracketB_Run" + String(runCountB) + ".csv"; }
  else if (bracketName == "C") { runCountC++; currentLogFile = "/BracketC_Run" + String(runCountC) + ".csv"; }
  else if (bracketName == "D") { runCountD++; currentLogFile = "/BracketD_Run" + String(runCountD) + ".csv"; }

  // Create the file and write the CSV Header
  File file = LittleFS.open(currentLogFile, FILE_WRITE);
  if(file) {
    file.println("PWM_us,GroundSpeed_ms");
    file.close();
    Serial.printf("[LOGGING] New file created: %s\n", currentLogFile.c_str());
  } else {
    Serial.println("[ERROR] LittleFS file creation failed!");
  }
}

void logToFS(int pwm, float speed) {
  File file = LittleFS.open(currentLogFile, FILE_APPEND);
  if(file) {
    file.printf("%d,%.3f\n", pwm, speed);
    file.close();
  } else {
    Serial.println("[ERROR] Failed to open LittleFS for appending!");
  }
}