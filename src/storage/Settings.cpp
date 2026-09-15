#include "Settings.h"

#include <Preferences.h>
#define LOG_CLASS "SettingsController"
#include "../utilities/Logger.h"

namespace
{
    constexpr char PREFERENCES_NAMESPACE[] = "air_sensor";
    constexpr char BRIGHTNESS_KEY[] = "brightness";
    constexpr char GRAPH_MODE_KEY[] = "graphMode";
    constexpr uint8_t DEFAULT_BRIGHTNESS = 128;
    constexpr uint8_t DEFAULT_GRAPH_MODE = 0;
    constexpr uint8_t MAX_GRAPH_MODE = 2;
}

uint8_t SettingsController::getBrightness() const
{
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, true))
    {
        APP_LOG("Unable to open preferences while getting brightness.");
        return DEFAULT_BRIGHTNESS;
    }

    const uint8_t brightness = preferences.getUChar(BRIGHTNESS_KEY, DEFAULT_BRIGHTNESS);
    preferences.end();
    APP_LOG("Brightness read: %u", brightness);
    return brightness;
}

bool SettingsController::setBrightness(uint8_t brightness)
{
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, false))
    {
        APP_LOG("Unable to open preferences while setting brightness.");
        return false;
    }

    const size_t bytesWritten = preferences.putUChar(BRIGHTNESS_KEY, brightness);
    preferences.end();

    const bool success = bytesWritten == sizeof(brightness);
    APP_LOG("Brightness set to %u (success: %s)", brightness, success ? "true" : "false");
    return success;
}

uint8_t SettingsController::getGraphMode() const
{
    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, true))
    {
        APP_LOG("Unable to open preferences while getting graph mode.");
        return DEFAULT_GRAPH_MODE;
    }

    const uint8_t graphMode = preferences.getUChar(GRAPH_MODE_KEY, DEFAULT_GRAPH_MODE);
    preferences.end();

    if (graphMode > MAX_GRAPH_MODE)
    {
        APP_LOG("Invalid stored graph mode %u", graphMode);
        return DEFAULT_GRAPH_MODE;
    }

    APP_LOG("Graph mode read: %u", graphMode);
    return graphMode;
}

bool SettingsController::setGraphMode(uint8_t graphMode)
{
    if (graphMode > MAX_GRAPH_MODE)
    {
        APP_LOG("Invalid graph mode %u", graphMode);
        return false;
    }

    Preferences preferences;
    if (!preferences.begin(PREFERENCES_NAMESPACE, false))
    {
        APP_LOG("Unable to open preferences while setting graph mode.");
        return false;
    }

    const size_t bytesWritten = preferences.putUChar(GRAPH_MODE_KEY, graphMode);
    preferences.end();

    const bool success = bytesWritten == sizeof(graphMode);
    APP_LOG("Graph mode set to %u (success: %s)", graphMode, success ? "true" : "false");
    return success;
}
