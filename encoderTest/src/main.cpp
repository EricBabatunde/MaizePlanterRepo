#include <Arduino.h>
/*
 * PROJECT: Autonomous Smart Maize Planter
 * MODULE: Stage 2 - AS5048A Encoder Verification
 * GOAL: Read the 14-bit raw angle via SPI and output to Serial Plotter.
 */

#include <SPI.h>

// Define SPI Pins matching the hardware table
const int CS_PIN = 10;
const int SCK_PIN = 12;
const int MISO_PIN = 13;
const int MOSI_PIN = 11;

// AS5048A Register Address for Angle (with Parity and Read bit)
const uint16_t ANGLE_REGISTER = 0xFFFF;

void setup()
{
  Serial.begin(115200);

  // Initialise SPI Bus
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  Serial.println("--- MECHATRONICS TEST-BENCH: AS5048A ---");
}

uint16_t readEncoderAngle()
{
  uint16_t rawAngle;

  // SPI Transaction
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_PIN, LOW);

  // Send the read command
  SPI.transfer16(ANGLE_REGISTER);
  digitalWrite(CS_PIN, HIGH);
  delayMicroseconds(10); // Brief pause to allow IC to process

  // Read the actual data
  digitalWrite(CS_PIN, LOW);
  rawAngle = SPI.transfer16(0x0000);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();

  // Mask out the parity and error bits (keep only the 14 bits of data)
  return (rawAngle & 0x3FFF);
}

void loop()
{
  uint16_t currentAngle = readEncoderAngle();

  // Convert the 14-bit value to degrees
  float degrees = (currentAngle / 16384.0) * 360.0;

  Serial.print("Raw_Value:");
  Serial.print(currentAngle);
  Serial.print("\tDegrees:");
  Serial.println(degrees);

  delay(50); // 20Hz Refresh Rate
}