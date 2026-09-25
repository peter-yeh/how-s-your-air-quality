#pragma once

#include <Arduino.h>
#include "../config/BoardConfig.h"

// Use enum instead of magic numbers for type safety
enum class GraphMode : uint8_t
{
    SECONDS = 0,
    MINUTES = 1,
    HOURS = 2,
    MAX = 2
};

class SettingsController
{
public:
    uint8_t getBrightness() const;
    bool setBrightness(uint8_t brightness);
    uint8_t getGraphMode() const; // Returns uint8_t for compatibility
    bool setGraphMode(uint8_t graphMode);
};
