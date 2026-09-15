#include "Wireless.h"

#include <WiFi.h>
#include <time.h>
#define LOG_CLASS "WirelessController"
#include "../utilities/Logger.h"

namespace
{
    constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
    constexpr uint32_t TIME_SYNC_TIMEOUT_MS = 10000;
}

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

    char formattedTime[24];
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return String(formattedTime);
}

String WirelessController::clockTime() const
{
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo, 0))
    {
        return "--:--:--";
    }

    char formattedTime[9];
    strftime(formattedTime, sizeof(formattedTime), "%H:%M:%S", &timeInfo);
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
