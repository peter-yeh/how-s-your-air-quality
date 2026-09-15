#pragma once

#include <Arduino.h>

class SettingsController
{
public:
    uint8_t getBrightness() const;
    bool setBrightness(uint8_t brightness);
    uint8_t getGraphMode() const;
    bool setGraphMode(uint8_t graphMode);
};
