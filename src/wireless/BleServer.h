#pragma once

#include <Arduino.h>

class StorageController;

class BleServer
{
public:
    bool begin(StorageController *storage);
    bool connected() const;
};