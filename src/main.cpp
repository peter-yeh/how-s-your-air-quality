// Board: Espressif ESP32 Dev Module (PlatformIO board: esp32dev)
// Physical board noted in this project: Sunton 3.2-inch ESP32 LCD board

#include <Arduino.h>
#include "display/Display.h"
#include "storage/Storage.h"
#include "storage/Settings.h"
#include "sensor/Sensor.h"
#include "wireless/Wireless.h"
#include "wireless/BleServer.h"
#define LOG_CLASS "Application"
#include "utilities/Logger.h"

#include "utilities/secrets.h"

DisplayController display;
StorageController storage;
SettingsController settings;
SensorController sensor;
WirelessController wireless;
BleServer ble;

void airQualityTask(void *pvParameters)
{
  constexpr int16_t burnInShiftX[] = {0, 2, 0, -2};
  constexpr int16_t burnInShiftY[] = {2, 0, -2, 0};
  constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 1000;

  AirQualityStats stats;
  AirQualityStats minuteStats;
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

    if (currentTick - lastMinuteTick >= 60000) // minute task
    {
      APP_LOG("Executing the tasks every minute");

      lastMinuteTick = currentTick;

      AirQualitySummary summary;

      if (minuteStats.getSummary(summary))
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

        minuteStats.clear();
      }

      // Burn-in shift for the display to prevent screen burn-in
      display.shiftScreen(burnInShiftX[shiftIndex], burnInShiftY[shiftIndex]);
      shiftIndex = (shiftIndex + 1) % 4;

      APP_LOG("Minute summary: %s", summary.toString().c_str());
    }

    if (currentTick - lastSecondTick >= 1000) // second task
    {
      APP_LOG("Executing the tasks every second");
      lastSecondTick = currentTick;

      latestReading = sensor.read();
      stats.addSample(latestReading.pm1, latestReading.pm25, latestReading.pm10);
      minuteStats.addSample(latestReading.pm1, latestReading.pm25, latestReading.pm10);

      const bool redrawGraph = display.addGraphSample(latestReading.pm1, latestReading.pm25, latestReading.pm10);

      AirQualitySummary summary;
      const bool hasNewSummary = stats.getSummary(summary);

      display.renderNow(latestReading.pm1, latestReading.pm25, latestReading.pm10,
                        currentTick / 1000, wireless.clockTime().c_str(),
                        wireless.connected(), ble.connected(),
                        summary, hasNewSummary, redrawGraph);
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void setup()
{

  Serial.begin(115200);
  display.begin();

  wireless.begin(WIFI_SSID, WIFI_PASSWORD, 8 * 60 * 60);

  const bool storageReady = storage.begin();
  SerialLogger.enableStorage(storageReady);
  storage.testReadWrite();

  // Read brightness from storage and set it
  uint8_t brightness = settings.getBrightness();
  display.setBrightness(brightness);
  display.setGraphMode(settings.getGraphMode());

  ble.begin(&storage, &settings, &display);

  while (!sensor.begin())
  {
    APP_LOG("Retrying sensor initialization in 2s...");
    delay(2000);
  }
  APP_LOG("BMV080 connected");

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
