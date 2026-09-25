#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BMV080.h"
#include "../config/BoardConfig.h"

struct LatestReading
{
    float pm1 = 0;
    float pm25 = 0;
    float pm10 = 0;
    bool valid = false;     // false if sensor read failed or data is stale
    uint32_t timestamp = 0; // ms since boot when reading was taken
};

class BMVSensor
{
public:
    ~BMVSensor();
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
    uint32_t lastValidReadTime = 0; // Track last successful read
    uint32_t warmupStartTime = 0;   // Track sensor initialization time
    bool warmupComplete = false;    // Flag to indicate sensor is warmed up
};