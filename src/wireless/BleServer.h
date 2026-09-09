#pragma once

#include <Arduino.h>

class StorageController;
class DisplayController;

class BleServer
{
public:
    bool begin(StorageController *storage, DisplayController *display);
    bool connected() const;
};