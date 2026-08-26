#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BMV080.h"

class SensorController
{
public:
    bool begin();
    bool read(float &pm1, float &pm25, float &pm10);

private:
    DFRobot_BMV080_I2C bmv{&Wire};
    bool initialized = false;
};
