#pragma once

#include <stdint.h>

class DisplayController
{
public:
    void begin();
    void update();
    void setGraphUpdatesEnabled(bool enabled);
    void showPM(float pm1Concentration, float pm25Concentration, float pm10Concentration);
    void showStatus(const char *timeText, bool wifiConnected, bool bluetoothConnected = false);
    void shiftScreen(int16_t x, int16_t y);

private:
    uint32_t lastUpdateMs = 0;
    bool graphUpdatesEnabled = true;
};
