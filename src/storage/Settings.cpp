#include "Settings.h"

#include <Preferences.h>
#include "../config/BoardConfig.h"
#define LOG_CLASS "SettingsController"
#include "../utilities/Logger.h"

using namespace SettingsConfig;

namespace
{
    constexpr char PREFERENCES_NAMESPACE[] = "air_sensor";
    constexpr char BRIGHTNESS_KEY[] = "brightness";
    constexpr char GRAPH_MODE_KEY[] = "graphMode";
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
        return static_cast<uint8_t>(DEFAULT_GRAPH_MODE);
    }

    const uint8_t storedMode = preferences.getUChar(GRAPH_MODE_KEY, static_cast<uint8_t>(DEFAULT_GRAPH_MODE));
    preferences.end();

    // Validate stored value is within enum range
    if (storedMode > static_cast<uint8_t>(GraphMode::MAX))
    {
        APP_LOG("Invalid stored graph mode %u, resetting to default", storedMode);
        return static_cast<uint8_t>(DEFAULT_GRAPH_MODE);
    }

    APP_LOG("Graph mode read: %u", storedMode);
    return storedMode;
}

bool SettingsController::setGraphMode(uint8_t graphMode)
{
    // Validate input
    if (graphMode > static_cast<uint8_t>(GraphMode::MAX))
    {
        APP_LOG("Invalid graph mode %u (max is %u)", graphMode, static_cast<uint8_t>(GraphMode::MAX));
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
