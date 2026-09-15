#pragma once

#include <Arduino.h>

class StorageController;
class SettingsController;
class DisplayController;

class BleServer
{
public:
    bool begin(StorageController *storage, SettingsController *settings, DisplayController *display);
    bool connected() const;
};