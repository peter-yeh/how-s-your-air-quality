#pragma once

#include <stdint.h>
#include "../sensor/AirQualityStats.h"
#include <freertos/semphr.h>

class DisplayController
{
public:
    void begin();
    void renderNow(float pm1, float pm25, float pm10,
                   uint32_t uptimeSeconds, const char *timeText,
                   bool wifiConnected, bool bluetoothConnected,
                   const AirQualitySummary &summary, bool hasNewSummary,
                   bool redrawGraph);
    bool addGraphSample(float pm1, float pm25, float pm10);
    void shiftScreen(int16_t x, int16_t y);
    void setBrightness(uint8_t brightness);
    void setGraphMode(int mode);
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
    SemaphoreHandle_t displayMutex = nullptr; // Protect display operations
};
