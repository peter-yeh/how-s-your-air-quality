#pragma once

#include <stdint.h>

class DisplayController
{
public:
    void begin();
    void update();
    void showPM(float pm1Concentration, float pm25Concentration, float pm10Concentration);

private:
    uint32_t lastUpdateMs = 0;
};
