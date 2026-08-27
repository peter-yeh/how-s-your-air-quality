#include "Wireless.h"

#include <WiFi.h>
#include <time.h>

namespace
{
    constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
    constexpr uint32_t TIME_SYNC_TIMEOUT_MS = 10000;
    constexpr char NTP_SERVER_1[] = "pool.ntp.org";
    constexpr char NTP_SERVER_2[] = "time.nist.gov";
}

bool WirelessController::begin(const char *ssid, const char *password, long gmtOffsetSeconds)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.printf("Connecting to Wi-Fi network %s", ssid);
    const uint32_t wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_TIMEOUT_MS)
    {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (!connected())
    {
        Serial.println("ERROR: Wi-Fi connection failed.");
        return false;
    }

    Serial.print("Wi-Fi connected. IP address: ");
    Serial.println(WiFi.localIP());

    configTime(gmtOffsetSeconds, 0, NTP_SERVER_1, NTP_SERVER_2);
    Serial.print("Synchronizing time");
    if (!waitForTimeSync(TIME_SYNC_TIMEOUT_MS))
    {
        Serial.println();
        Serial.println("ERROR: NTP time synchronization failed.");
        return false;
    }

    Serial.println();
    Serial.printf("Current time: %s\n", currentTime().c_str());
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

bool WirelessController::waitForTimeSync(uint32_t timeoutMs) const
{
    const uint32_t syncStart = millis();
    struct tm timeInfo;

    while (millis() - syncStart < timeoutMs)
    {
        if (getLocalTime(&timeInfo, 1000))
        {
            return true;
        }
        Serial.print('.');
    }

    return false;
}
