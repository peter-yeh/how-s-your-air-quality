#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BMV080.h"

// Instantiate sensor object using the I2C subclass
DFRobot_BMV080_I2C bmv(&Wire);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  Serial.println("\n--- BMV080 Initializing ---");

  Wire.begin();

  // Initialize sensor hardware over I2C
  while (bmv.begin() != 0)
  {
    Serial.println("ERROR: Sensor not detected over I2C!");
    Serial.println("Check wiring (3V3, GND, SDA/MOSI, SCL/SCK) and solder joints.");
    delay(2000);
  }

  Serial.println("SUCCESS: BMV080 Connected.");

  while (bmv.openBmv080() != 0)
  {
    Serial.println("ERROR: Could not open BMV080.");
    delay(2000);
  }

  // Set measurement mode to continuous reading
  bmv.setBmv080Mode(CONTINUOUS_MODE);
}

void loop()
{
  float pm1;
  float pm25;
  float pm10;

  if (bmv.getBmv080Data(&pm1, &pm25, &pm10))
  {
    Serial.print("PM1: ");
    Serial.print(pm1);
    Serial.print(" ug/m3, PM2.5: ");
    Serial.print(pm25);
    Serial.print(" ug/m3, PM10: ");
    Serial.print(pm10);
    Serial.println(" ug/m3");
  }

  delay(100);
}