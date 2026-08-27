#pragma once

#include <Arduino.h>

class StorageController;

class BleServer
{
public:
    bool begin(StorageController *storage);
    void updateReading(float pm1, float pm25, float pm10, const String &time);
};