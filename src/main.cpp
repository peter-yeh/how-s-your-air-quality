// Board: Espressif ESP32 Dev Module (PlatformIO board: esp32dev)
// Physical board noted in this project: Sunton 3.2-inch ESP32 LCD board

#include <Arduino.h>
#include "display/Display.h"
#include "storage/Storage.h"
#include "sensor/Sensor.h"
#include "wireless/Wireless.h"
#include "wireless/BleServer.h"

DisplayController display;
StorageController storage;
SensorController sensor;
WirelessController wireless;
BleServer ble;

void airQualityTask(void *pvParameters)
{
  uint32_t lastStatusUpdate = 0;
  uint32_t lastBurnInShift = 0;
  uint8_t shiftIndex = 0;
  constexpr int16_t burnInShifts[] = {0, 5, 0, -5};

  while (true)
  {
    float pm1 = 0;
    float pm25 = 0;
    float pm10 = 0;

    if (sensor.read(pm1, pm25, pm10))
    {
      const String readingTime = wireless.currentTime();

      if (readingTime != "time unavailable")
      {
        Reading reading;
        reading.time = readingTime;
        reading.pm1 = pm1;
        reading.pm25 = pm25;
        reading.pm10 = pm10;
        storage.saveReading(reading);
      }

      display.showPM(pm1, pm25, pm10);
    }

    if (millis() - lastStatusUpdate >= 1000)
    {
      display.showStatus(wireless.clockTime().c_str(), wireless.connected(), ble.connected());
      lastStatusUpdate = millis();
    }

    display.update();
    if (millis() - lastBurnInShift >= 30000)
    {
      shiftIndex = (shiftIndex + 1) % 4;
      display.shiftScreen(burnInShifts[shiftIndex], 0);
      lastBurnInShift = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void setup()
{
  Serial.begin(115200);
  display.begin();

  if (!wireless.begin("AnsonGarden", "66485973", 8 * 60 * 60))
  {
    Serial.println("WARNING: Continuing without synchronized time.");
  }

  if (storage.begin())
  {
    storage.testReadWrite();
  }

  ble.begin(&storage);

  Serial.println("\n--- BMV080 Initializing ---");
  while (!sensor.begin())
  {
    Serial.println("Retrying sensor init in 2s...");
    delay(2000);
  }
  Serial.println("SUCCESS: BMV080 Connected.");

  // Launch sensor & display in a dedicated FreeRTOS task with 32KB stack
  xTaskCreatePinnedToCore(
      airQualityTask,
      "AirQualityTask",
      32768,
      NULL,
      1,
      NULL,
      1);
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}
