// Board: Espressif ESP32 Dev Module (PlatformIO board: esp32dev)
// Physical board noted in this project: Sunton 3.2-inch ESP32 LCD board

#include <Arduino.h>
#include "config/BoardConfig.h"
#include "display/Display.h"
#include "storage/Storage.h"
#include "storage/Settings.h"
#include "sensor/BMVSensor.h"
#include "wireless/Wireless.h"
#include "wireless/BleServer.h"
#define LOG_CLASS "Application"
#include "utilities/Logger.h"
#include <esp_task_wdt.h> // Watchdog timer

#include "utilities/secrets.h"

using namespace TimingConfig;

DisplayController display;
StorageController storage;
SettingsController settings;
BMVSensor sensor;
WirelessController wireless;
BleServer ble;

void airQualityTask(void *pvParameters)
{
  constexpr int16_t burnInShiftX[] = {0, 2, 0, -2};
  constexpr int16_t burnInShiftY[] = {2, 0, -2, 0};

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

    if (currentTick - lastSecondTick >= DISPLAY_UPDATE_INTERVAL_MS) // second task
    {
      lastSecondTick = currentTick;

      latestReading = sensor.read();
      APP_LOG("Latest sensor value: PM1=%.2f, PM2.5=%.2f, PM10=%.2f (valid=%s)",
              latestReading.pm1, latestReading.pm25, latestReading.pm10,
              latestReading.valid ? "true" : "false");

      // Only add valid samples to statistics
      if (latestReading.valid)
      {
        stats.addSample(latestReading.pm1, latestReading.pm25, latestReading.pm10);
        minuteStats.addSample(latestReading.pm1, latestReading.pm25, latestReading.pm10);
      }

      const bool redrawGraph = display.addGraphSample(latestReading.pm1, latestReading.pm25, latestReading.pm10);

      AirQualitySummary summary;
      const bool hasNewSummary = stats.getSummary(summary);

      display.renderNow(latestReading.pm1, latestReading.pm25, latestReading.pm10,
                        currentTick / 1000, wireless.clockTime().c_str(),
                        wireless.connected(), ble.connected(),
                        summary, hasNewSummary, redrawGraph);

      // Feed watchdog
      esp_task_wdt_reset();
    }

    if (currentTick - lastMinuteTick >= MINUTE_AGGREGATION_INTERVAL_MS) // minute task
    {
      lastMinuteTick = currentTick;

      AirQualitySummary summary;

      if (minuteStats.getSummary(summary) && minuteStats.getCount() > 0)
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
          APP_LOG("Minute summary saved: %s", summary.toString().c_str());
        }
        else
        {
          APP_LOG("Skipping minute save: time unavailable (WiFi disconnected?)");
        }

        minuteStats.clear();
      }
      else
      {
        APP_LOG("Minute summary skipped: no valid samples collected");
      }

      // Burn-in shift for the display to prevent screen burn-in
      display.shiftScreen(burnInShiftX[shiftIndex], burnInShiftY[shiftIndex]);
      shiftIndex = (shiftIndex + 1) % 4;

      storage.createNextDayFile();
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void setup()
{
  Serial.begin(115200);

  // Connect to WiFi on startup
  APP_LOG("Connecting to WiFi...");
  wireless.begin(WIFI_SSID, WIFI_PASSWORD, 8 * 60 * 60);

  // Initialize watchdog timer (30 second timeout)
  esp_task_wdt_init(30, true);

  display.begin();

  // Initialize storage first
  const bool storageReady = storage.begin();
  // SerialLogger.enableStorage(storageReady);

  // Read brightness from storage and set it
  uint8_t brightness = settings.getBrightness();
  display.setBrightness(brightness);
  display.setGraphMode(settings.getGraphMode());

  ble.begin(&storage, &settings, &display);

  // Initialize sensor with max retries
  uint8_t sensorRetries = 0;
  while (!sensor.begin() && sensorRetries < SensorConfig::MAX_INIT_RETRIES)
  {
    sensorRetries++;
    APP_LOG("Sensor initialization attempt %u/%u failed, retrying in %lums...",
            sensorRetries, SensorConfig::MAX_INIT_RETRIES, SensorConfig::RETRY_DELAY_MS);
    delay(SensorConfig::RETRY_DELAY_MS);
  }

  if (sensorRetries >= SensorConfig::MAX_INIT_RETRIES)
  {
    APP_LOG("CRITICAL: BMV080 sensor failed to initialize after %u attempts. Starting in degraded mode.",
            SensorConfig::MAX_INIT_RETRIES);
  }
  else
  {
    APP_LOG("BMV080 connected after %u attempt(s)", sensorRetries);
  }

  // Subscribe this task to watchdog
  esp_task_wdt_add(NULL);

  // Launch sensor & display in a dedicated FreeRTOS task with 32KB stack
  TaskHandle_t airQualityTaskHandle = NULL;
  xTaskCreatePinnedToCore(
      airQualityTask,
      "AirQualityTask",
      32768,
      NULL,
      1,
      &airQualityTaskHandle,
      1);

  if (airQualityTaskHandle != NULL)
  {
    esp_task_wdt_add(airQualityTaskHandle);
  }
}

void loop() // useless loop just to follow the framework
{
  esp_task_wdt_reset(); // Feed watchdog from main loop
  vTaskDelay(pdMS_TO_TICKS(1000));
}
