#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BMV080.h"

struct LatestReading
{
    float pm1;
    float pm25;
    float pm10;
};

class SensorController
{
public:
    ~SensorController();
    bool begin();
    LatestReading read();
    void scanI2C();

private:
    DFRobot_BMV080_I2C *bmv = nullptr;
    uint8_t detectedAddr = 0;
    bool initialized = false;
    float currentPm1 = 0;
    float currentPm25 = 0;
    float currentPm10 = 0;
};
