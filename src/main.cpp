// Board: Espressif ESP32 Dev Module (PlatformIO board: esp32dev)
// Physical board noted in this project: Sunton 3.2-inch ESP32 LCD board

#include <Arduino.h>
#include "display/Display.h"
#include "storage/Storage.h"
#include "sensor/Sensor.h"
#include "sensor/RollingWindow.h"
#include "wireless/Wireless.h"
#include "wireless/BleServer.h"

DisplayController display;
StorageController storage;
SensorController sensor;
WirelessController wireless;
BleServer ble;

class RollingWindow
{
public:
  static constexpr size_t CAPACITY = 60;

  void addSample(float pm1, float pm25, float pm10)
  {
    samples[head].pm1 = pm1;
    samples[head].pm25 = pm25;
    samples[head].pm10 = pm10;
    samples[head].timestamp = millis();
    head = (head + 1) % CAPACITY;
    if (count < CAPACITY)
    {
      count++;
    }
  }

  bool getAverage(float &avgPm1, float &avgPm25, float &avgPm10) const
  {
    if (count == 0)
    {
      return false;
    }

    const uint32_t now = millis();
    float sum1 = 0;
    float sum25 = 0;
    float sum10 = 0;
    size_t validCount = 0;

    for (size_t i = 0; i < count; ++i)
    {
      // Include samples collected within the 1-minute window
      if (now - samples[i].timestamp <= 61000)
      {
        sum1 += samples[i].pm1;
        sum25 += samples[i].pm25;
        sum10 += samples[i].pm10;
        validCount++;
      }
    }

    if (validCount == 0)
    {
      return false;
    }

    avgPm1 = sum1 / (float)validCount;
    avgPm25 = sum25 / (float)validCount;
    avgPm10 = sum10 / (float)validCount;
    return true;
  }

  void clear()
  {
    head = 0;
    count = 0;
  }

  size_t getCount() const
  {
    return count;
  }

private:
  struct Sample
  {
    float pm1 = 0;
    float pm25 = 0;
    float pm10 = 0;
    uint32_t timestamp = 0;
  } samples[CAPACITY];

  size_t head = 0;
  size_t count = 0;
};

void airQualityTask(void *pvParameters)
{
  RollingWindow window;
  uint32_t lastStatusUpdate = 0;
  AirQualityStats stats;
  uint32_t lastDisplayUpdate = 0;
  uint32_t lastGraphUpdate = 0;
  uint32_t lastBurnInShift = 0;
  uint32_t lastMinuteTick = millis();
  uint8_t shiftIndex = 0;
  constexpr int16_t burnInShifts[] = {0, 5, 0, -5};
  constexpr uint8_t DISPLAY_FPS = 5;
  constexpr uint32_t DISPLAY_INTERVAL_MS = 1000 / DISPLAY_FPS;
  constexpr uint32_t GRAPH_UPDATE_INTERVAL_MS = 2000; // Add graph sample every 2 seconds

  float lastPm1 = 0;
  float lastPm25 = 0;
  float lastPm10 = 0;

  while (true)
  {
    float pm1 = 0;
    float pm25 = 0;
    float pm10 = 0;

    if (sensor.read(pm1, pm25, pm10))
    {
      // Real-time PM readings on the display update every second
      display.showPM(pm1, pm25, pm10);
      window.addSample(pm1, pm25, pm10);
      lastPm1 = pm1;
      lastPm25 = pm25;
      lastPm10 = pm10;
      stats.addSample(pm1, pm25, pm10);
    }

    // Every minute: compute average readings, update graph, and save to CSV
    // Add graph sample every 2 seconds - starts immediately when data is available
    if (millis() - lastGraphUpdate >= GRAPH_UPDATE_INTERVAL_MS)
    {
      lastGraphUpdate = millis();
      display.addGraphSample(lastPm1, lastPm25, lastPm10);
    }

    // Every minute: save averaged readings to CSV for historical data
    if (millis() - lastMinuteTick >= 60000)
    {
      lastMinuteTick = millis();

      float avgPm1 = 0;
      float avgPm25 = 0;
      float avgPm10 = 0;
      AirQualitySummary summary;

      if (window.getAverage(avgPm1, avgPm25, avgPm10))
        if (stats.getSummary(summary))
        {
          const String readingTime = wireless.currentTime();
          if (readingTime != "time unavailable")
          {
            Reading reading;
            reading.time = readingTime;
            reading.pm1 = avgPm1;
            reading.pm25 = avgPm25;
            reading.pm10 = avgPm10;
            reading.pm1 = summary.averagePm1;
            reading.pm25 = summary.averagePm25;
            reading.pm10 = summary.averagePm10;
            storage.saveReading(reading);
          }

          display.addGraphSample(avgPm1, avgPm25, avgPm10);
          window.clear();
          stats.clear();
        }
    }

    if (millis() - lastStatusUpdate >= 1000)
      // Push clock and PM readings to the display together so they refresh in sync.
      if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL_MS)
      {
        display.showStatus(wireless.clockTime().c_str(), wireless.connected(), ble.connected());
        lastStatusUpdate = millis();
        lastDisplayUpdate = millis();

        display.showCurrent(lastPm1, lastPm25, lastPm10);

        AirQualitySummary summary;
        if (stats.getSummary(summary))
        {
          display.showStats(summary);
        }

        display.showStatus(wireless.clockTime().c_str(), wireless.connected(), ble.connected(), millis() / 1000);
      }

    display.update();
    if (millis() - lastBurnInShift >= 30000)
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

  // Read brightness from storage and set it
  uint8_t brightness = storage.getBrightness();
  display.setBrightness(brightness);

  wireless.begin("AnsonGarden", "66485973", 8 * 60 * 60);
  ble.begin(&storage);
  ble.begin(&storage, &display);

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
