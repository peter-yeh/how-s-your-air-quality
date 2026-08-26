// Board: Espressif ESP32 Dev Module (PlatformIO board: esp32dev)
// Physical board noted in this project: Sunton 3.2-inch ESP32 LCD board

#include <Arduino.h>
#include "display/Display.h"
#include "storage/Storage.h"
#include "sensor/Sensor.h"

DisplayController display;
// StorageController storage;
SensorController sensor;

void airQualityTask(void *pvParameters)
{
  while (true)
  {
    float pm1 = 0;
    float pm25 = 0;
    float pm10 = 0;

    if (sensor.read(pm1, pm25, pm10))
    {
      uint32_t elapsedSeconds = millis() / 1000;
      unsigned long hours = elapsedSeconds / 3600;
      unsigned long minutes = (elapsedSeconds % 3600) / 60;
      unsigned long seconds = elapsedSeconds % 60;

      Serial.printf("[%02lu:%02lu:%02lu] PM1.0: %3d | PM2.5: %3d | PM10: %3d ug/m3\n",
                    hours,
                    minutes,
                    seconds,
                    (int)(pm1 + 0.5f),
                    (int)(pm25 + 0.5f),
                    (int)(pm10 + 0.5f));

      display.showPM(pm1, pm25, pm10);
      display.showSensorStatus(sensor.isObstructed());
    }

    display.update();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup()
{
  Serial.begin(115200);
  display.begin();
  // if (storage.begin())
  // {
  //   storage.testReadWrite();
  // }

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
      1
  );
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}
