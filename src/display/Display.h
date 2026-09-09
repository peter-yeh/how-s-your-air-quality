#pragma once

#include <stdint.h>
#include "../sensor/AirQualityStats.h"

class DisplayController
{
public:
    void begin();
    void update();
    void showStats(const AirQualitySummary &summary);
    void addGraphSample(float pm1, float pm25, float pm10);
    void showStatus(const char *timeText, bool wifiConnected, bool bluetoothConnected = false, uint32_t uptimeSeconds = 0);
    void shiftScreen(int16_t x, int16_t y);

private:
    uint32_t lastUpdateMs = 0;
};
