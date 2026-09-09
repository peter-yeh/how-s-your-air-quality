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
  AirQualityStats stats;
  uint32_t lastStatusUpdate = 0;
  uint32_t lastBurnInShift = 0;
  uint32_t lastMinuteTick = millis();
  uint8_t shiftIndex = 0;
  constexpr int16_t burnInShifts[] = {0, 5, 0, -5};

  while (true)
  {
    float pm1 = 0;
    float pm25 = 0;
    float pm10 = 0;

    if (sensor.read(pm1, pm25, pm10))
    {
      // Real-time PM readings on the display update every second
      stats.addSample(pm1, pm25, pm10);

      AirQualitySummary summary;
      if (stats.getSummary(summary))
      {
        display.showStats(summary);
      }
    }

    // Every minute: compute average readings, update graph, and save to CSV
    if (millis() - lastMinuteTick >= 60000)
    {
      lastMinuteTick = millis();

      AirQualitySummary summary;

      if (stats.getSummary(summary))
      {
        const String readingTime = wireless.currentTime();
        if (readingTime != "time unavailable")
        {
          Reading reading;
          reading.time = readingTime;
          reading.pm1 = summary.averagePm1;
          reading.pm25 = summary.averagePm25;
          reading.pm10 = summary.averagePm10;
          storage.saveReading(reading);
        }

        display.addGraphSample(summary.averagePm1, summary.averagePm25, summary.averagePm10);
        stats.clear();
      }
    }

    if (millis() - lastStatusUpdate >= 1000)
    {
      display.showStatus(wireless.clockTime().c_str(), wireless.connected(), ble.connected(), millis() / 1000);
      lastStatusUpdate = millis();
    }

    display.update();
    if (millis() - lastBurnInShift >= 60000)
    {
      shiftIndex = (shiftIndex + 1) % 4;
      display.shiftScreen(burnInShifts[shiftIndex], 0);
      lastBurnInShift = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup()
{
  Serial.begin(115200);
  display.begin();
  storage.begin();
  storage.testReadWrite();
  wireless.begin("AnsonGarden", "66485973", 8 * 60 * 60);
  ble.begin(&storage);

  Serial.println("\n--- BMV080 Initializing ---");
  while (!sensor.begin())
  {
    Serial.println("Retrying sensor init in 2s...");
    delay(2000);
  }
  Serial.println("\n--- BMV080 Connected ---");

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

void loop() // useless loop just to follow the framework
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}
