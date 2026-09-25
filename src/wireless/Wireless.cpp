#include "Wireless.h"

#include <WiFi.h>
#include <time.h>
#include <cmath>
#define LOG_CLASS "WirelessController"
#include "../utilities/Logger.h"
#include "../config/BoardConfig.h"

using namespace WirelessConfig;
using namespace TimingConfig;

bool WirelessController::begin(const char *ssid, const char *password, long gmtOffsetSeconds)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    String connectionMessage = "Connecting to Wi-Fi network ";
    connectionMessage += ssid;
    const uint32_t wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_TIMEOUT_MS)
    {
        delay(500);
        connectionMessage += '.';
    }
    APP_LOG("%s", connectionMessage.c_str());

    if (!connected())
    {
        APP_LOG("Wi-Fi connection failed.");
        return false;
    }

    APP_LOG("Wi-Fi connected. IP address: %s", WiFi.localIP().toString().c_str());

    configTime(gmtOffsetSeconds, 0, "asia.pool.ntp.org", "pool.ntp.org");
    String timeSyncMessage = "Synchronizing time";
    if (!waitForTimeSync(TIME_SYNC_TIMEOUT_MS, timeSyncMessage))
    {
        APP_LOG("%s", timeSyncMessage.c_str());
        APP_LOG("NTP time synchronization failed.");
        return false;
    }

    APP_LOG("%s", timeSyncMessage.c_str());
    APP_LOG("Current time: %s", currentTime().c_str());
    return true;
}

bool WirelessController::connected() const
{
    return WiFi.status() == WL_CONNECTED;
}

String WirelessController::currentTime() const
{
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo, 0))
    {
        return "time unavailable";
    }

    // Validate year is reasonable (should be >= 2020)
    if (timeInfo.tm_year + 1900 < TIME_VALIDATION_MIN_YEAR)
    {
        return "time unavailable";
    }

    char formattedTime[25];
    int written = snprintf(formattedTime, sizeof(formattedTime), "%04d-%02d-%02d %02d:%02d:%02d",
                           timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                           timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

    if (written < 0 || written >= (int)sizeof(formattedTime))
    {
        APP_LOG("Time format error, buffer overflow prevented");
        return "time unavailable";
    }

    return String(formattedTime);
}

String WirelessController::clockTime() const
{
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo, 0))
    {
        return "--:--:--";
    }

    // Validate year is reasonable
    if (timeInfo.tm_year + 1900 < TIME_VALIDATION_MIN_YEAR)
    {
        return "--:--:--";
    }

    char formattedTime[10];
    int written = snprintf(formattedTime, sizeof(formattedTime), "%02d:%02d:%02d",
                           timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

    if (written < 0 || written >= (int)sizeof(formattedTime))
    {
        APP_LOG("Clock format error, buffer overflow prevented");
        return "--:--:--";
    }

    return String(formattedTime);
}

bool WirelessController::waitForTimeSync(uint32_t timeoutMs, String &progress) const
{
    const uint32_t syncStart = millis();
    struct tm timeInfo;

    while (millis() - syncStart < timeoutMs)
    {
        if (getLocalTime(&timeInfo, 1000))
        {
            return true;
        }
        progress += '.';
    }

    return false;
}
