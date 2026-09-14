// Board: Espressif ESP32 Dev Module (PlatformIO board: esp32dev)
// Physical board noted in this project: Sunton 3.2-inch ESP32 LCD board

#include <Arduino.h>
#include "display/Display.h"
#include "storage/Storage.h"
#include "sensor/Sensor.h"
#include "wireless/Wireless.h"
#include "wireless/BleServer.h"
#include "Logger.h"

DisplayController display;
StorageController storage;
SensorController sensor;
WirelessController wireless;
BleServer ble;

void airQualityTask(void *pvParameters)
{
  constexpr int16_t burnInShiftX[] = {0, 2, 0, -2};
  constexpr int16_t burnInShiftY[] = {2, 0, -2, 0};
  constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 1000;

  AirQualityStats stats;
  LatestReading latestReading;

  uint32_t currentTick = millis();
  uint8_t shiftIndex = 0;
  uint32_t lastDisplayUpdate = 0;
  uint32_t lastBurnInShift = 0;

  uint32_t lastSecondTick = currentTick;
  uint32_t lastMinuteTick = currentTick;
  uint32_t lastHourTick = currentTick;

  while (true)
  {
    currentTick = millis();
    latestReading = sensor.read();
    stats.addSample(latestReading.pm1, latestReading.pm25, latestReading.pm10);

    if (currentTick - lastMinuteTick >= 60000) // minute task
    {
      lastMinuteTick = currentTick;

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

        stats.clear();
      }

      // Burn-in shift for the display to prevent screen burn-in
      display.shiftScreen(burnInShiftX[shiftIndex], burnInShiftY[shiftIndex]);
      shiftIndex = (shiftIndex + 1) % 4;
    }

    if (currentTick - lastSecondTick >= 1000) // second task
    {
      lastSecondTick = currentTick;

      const bool redrawGraph = display.addGraphSample(latestReading.pm1, latestReading.pm25, latestReading.pm10);

      AirQualitySummary summary;
      const bool hasNewSummary = stats.getSummary(summary);

      display.renderNow(latestReading.pm1, latestReading.pm25, latestReading.pm10,
                        currentTick / 1000, wireless.clockTime().c_str(),
                        wireless.connected(), ble.connected(),
                        summary, hasNewSummary, redrawGraph);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup()
{
  Serial.begin(115200);
  display.begin();
  const bool storageReady = storage.begin();
  SerialLogger.enableStorage(storageReady);
  storage.testReadWrite();

  // Read brightness from storage and set it
  uint8_t brightness = storage.getBrightness();
  display.setBrightness(brightness);

  wireless.begin("AnsonGarden", "66485973", 8 * 60 * 60);
  ble.begin(&storage, &display);

  Serial.println("--- BMV080 Initializing ---");
  while (!sensor.begin())
  {
    Serial.println("Retrying sensor init in 2s...");
    delay(2000);
  }
  Serial.println("--- BMV080 Connected ---");

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
