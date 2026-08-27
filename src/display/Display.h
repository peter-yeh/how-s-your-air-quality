#pragma once

#include <stdint.h>

class DisplayController
{
public:
    void begin();
    void update();
    void showPM(float pm1Concentration, float pm25Concentration, float pm10Concentration);
    void showStatus(const char *timeText, bool wifiConnected);
    void shiftScreen(int16_t x, int16_t y);

private:
    uint32_t lastUpdateMs = 0;
};
