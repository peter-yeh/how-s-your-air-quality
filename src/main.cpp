#include <Arduino.h>
#include "display/Display.h"
#include "storage/Storage.h"

DisplayController display;
StorageController storage;

void setup()
{
  Serial.begin(115200);
  display.begin();
  if (storage.begin())
  {
    storage.testReadWrite();
  }
}

void loop()
{
  float pm1 = random(1, 101);
  float pm25 = random(1, 101);
  float pm10 = random(1, 101);

  display.showPM(pm1, pm25, pm10);
  display.update();
}
