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
      Serial.print("PM1.0: ");
      Serial.print(pm1);
      Serial.print(" | PM2.5: ");
      Serial.print(pm25);
      Serial.print(" | PM10: ");
      Serial.print(pm10);
      Serial.println(" ug/m3");

      display.showPM(pm1, pm25, pm10);
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
