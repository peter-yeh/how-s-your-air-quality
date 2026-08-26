#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BMV080.h"

class SensorController
{
public:
    ~SensorController();
    bool begin();
    bool read(float &pm1, float &pm25, float &pm10);
    void scanI2C();

private:
    DFRobot_BMV080_I2C *bmv = nullptr;
    uint8_t detectedAddr = 0;
    bool initialized = false;
};
