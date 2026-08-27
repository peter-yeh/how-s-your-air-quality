#pragma once

#include <Arduino.h>

class WirelessController
{
public:
    bool begin(const char *ssid, const char *password, long gmtOffsetSeconds = 0);
    bool connected() const;
    String currentTime() const;
    String clockTime() const;

private:
    bool waitForTimeSync(uint32_t timeoutMs) const;
};
