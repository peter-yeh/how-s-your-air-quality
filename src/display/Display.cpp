#include "Display.h"
#include "GraphPlotter.h"

#include <Arduino.h>
#include <SPI.h>
#include <cmath>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "../config/BoardConfig.h"
#define LOG_CLASS "DisplayController"
#include "../utilities/Logger.h"

using namespace DisplayConfig;

static Adafruit_ST7789 display(&SPI, TFT_CS, TFT_DC, -1);
GraphPlotter graph;

AirQualitySummary currentSummary;
float currentPm1 = 0;
float currentPm25 = 0;
float currentPm10 = 0;
uint32_t currentUptimeSeconds = 0;
bool hasSummary = false;
bool needsTopBarRedraw = false;
bool needsTopBarStaticRedraw = true;
bool needsPMRedraw = false;
bool needsSummaryStaticRedraw = true;
bool needsGraphRedraw = false;
int16_t screenShiftX = 0;
int16_t screenShiftY = 0;
String currentClock = "--:--:--";
bool currentWifiConnected = false;
bool currentBluetoothConnected = false;

// Display layout constants are now centralized in BoardConfig::DisplayConfig
// to eliminate duplication and ensure consistency across the codebase.
const uint16_t SUMMARY_ROW_COLORS[] = {ST77XX_CYAN, ST77XX_YELLOW, ST77XX_MAGENTA};
const char *const SUMMARY_ROW_LABELS[] = {"PM1", "PM25", "PM10"};

void drawBluetoothIcon(bool connected);
void drawWifiIcon(bool connected);

String formatUptime(uint32_t uptimeSeconds)
{
    // Cap uptime to 999 hours to prevent display overflow
    constexpr uint32_t MAX_DISPLAY_HOURS = 999;

    uint32_t displaySeconds = uptimeSeconds;
    uint32_t hours = displaySeconds / 3600;

    if (hours > MAX_DISPLAY_HOURS)
    {
        hours = MAX_DISPLAY_HOURS;
    }

    const uint8_t minutes = (displaySeconds / 60) % 60;
    const uint8_t seconds = displaySeconds % 60;

    String result = "Up ";
    result += hours;
    result += ":";
    if (minutes < 10)
    {
        result += "0";
    }
    result += minutes;
    result += ":";
    if (seconds < 10)
    {
        result += "0";
    }
    result += seconds;
    return result;
}

String formatSummaryValue(float value)
{
    if (!std::isfinite(value))
    {
        return "--";
    }

    const String formatted = String(value, 0);
    if (formatted.length() > SUMMARY_VALUE_MAX_CHARACTERS)
    {
        return "--";
    }
    return formatted;
}

// Background fill and title only; redrawn once (and after a burn-in shift) to avoid flicker.
void drawTopBarStatic()
{
    display.fillRect(screenShiftX, screenShiftY, 320, 16, ST77XX_BLACK);
    display.setTextSize(SMALL_FONT_SIZE);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(127 + screenShiftX, 5 + screenShiftY);
    display.print("Air Quality");
}

// Uptime, clock, and connection icons; these change often so only their cells are cleared.
void drawTopBarDynamic()
{
    display.setTextSize(SMALL_FONT_SIZE);
    display.setTextColor(ST77XX_WHITE);

    display.fillRect(5 + screenShiftX, 5 + screenShiftY, 70, 8, ST77XX_BLACK);
    display.setCursor(5 + screenShiftX, 5 + screenShiftY);
    display.print(formatUptime(currentUptimeSeconds));

    display.fillRect(240 + screenShiftX, 5 + screenShiftY, 50, 8, ST77XX_BLACK);
    display.setCursor(240 + screenShiftX, 5 + screenShiftY);
    display.print(currentClock);

    drawBluetoothIcon(currentBluetoothConnected);
    drawWifiIcon(currentWifiConnected);
}

// Headers, row labels, and units; static, so only drawn once (and after a burn-in shift).
void drawSummaryStatic()
{
    display.fillRect(34 + screenShiftX, 29 + screenShiftY, 236, 51, ST77XX_BLACK);
    display.setTextSize(SMALL_FONT_SIZE);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(66 + screenShiftX, 29 + screenShiftY);
    display.print("Now");
    display.setCursor(100 + screenShiftX, 29 + screenShiftY);
    display.print("Low");
    display.setCursor(134 + screenShiftX, 29 + screenShiftY);
    display.print("Med");
    display.setCursor(168 + screenShiftX, 29 + screenShiftY);
    display.print("High");
    display.setCursor(202 + screenShiftX, 29 + screenShiftY);
    display.print("Avg");

    for (uint8_t i = 0; i < 3; ++i)
    {
        display.setTextColor(SUMMARY_ROW_COLORS[i]);
        display.setCursor(36 + screenShiftX, SUMMARY_ROW_Y[i] + screenShiftY);
        display.print(SUMMARY_ROW_LABELS[i]);
        display.setTextColor(ST77XX_WHITE);
        display.setCursor(240 + screenShiftX, SUMMARY_ROW_Y[i] + screenShiftY);
        display.print("ug/m3");
    }
}

// Now/Low/Med/High/Avg numbers only; each cell is cleared individually so labels/units don't flash.
// Prints "--" placeholders until the first stats summary is available.
void drawSummaryValues()
{
    display.setTextSize(SMALL_FONT_SIZE);
    display.setTextColor(ST77XX_WHITE);

    const float currentValues[] = {currentPm1, currentPm25, currentPm10};
    const float lowValues[] = {currentSummary.lowPm1, currentSummary.lowPm25, currentSummary.lowPm10};
    const float medianValues[] = {currentSummary.medianPm1, currentSummary.medianPm25, currentSummary.medianPm10};
    const float highValues[] = {currentSummary.highPm1, currentSummary.highPm25, currentSummary.highPm10};
    const float averageValues[] = {currentSummary.averagePm1, currentSummary.averagePm25, currentSummary.averagePm10};

    for (uint8_t i = 0; i < 3; ++i)
    {
        const float values[] = {
            currentValues[i],
            lowValues[i],
            medianValues[i],
            highValues[i],
            averageValues[i]};

        for (uint8_t col = 0; col < 5; ++col)
        {
            display.fillRect(SUMMARY_VALUE_COLUMNS[col] + screenShiftX, SUMMARY_ROW_Y[i] + screenShiftY, SUMMARY_VALUE_CELL_WIDTH, 8, ST77XX_BLACK);
            display.setCursor(SUMMARY_VALUE_COLUMNS[col] + screenShiftX, SUMMARY_ROW_Y[i] + screenShiftY);
            if (hasSummary)
            {
                display.print(formatSummaryValue(values[col]));
            }
            else
            {
                display.print("--");
            }
        }
    }
}

void drawBluetoothIcon(bool connected)
{
    const int16_t iconX = 296 + screenShiftX;
    const int16_t iconY = 13 + screenShiftY;
    const uint16_t color = connected ? ST77XX_CYAN : ST77XX_RED;

    display.fillRect(iconX - 5, iconY - 6, 11, 9, ST77XX_BLACK);
    display.drawLine(iconX, iconY - 6, iconX, iconY + 2, color);
    display.drawLine(iconX, iconY - 6, iconX + 4, iconY - 3, color);
    display.drawLine(iconX + 4, iconY - 3, iconX - 4, iconY + 1, color);
    display.drawLine(iconX - 4, iconY - 5, iconX + 4, iconY - 2, color);
    display.drawLine(iconX + 4, iconY - 2, iconX, iconY + 2, color);
}

void drawWifiIcon(bool connected)
{
    const int16_t iconX = 310 + screenShiftX;
    const int16_t iconY = 13 + screenShiftY;
    const uint16_t color = connected ? ST77XX_GREEN : ST77XX_RED;

    // Clear compact bounding box around (310, 10) - 11px wide, 9px high
    display.fillRect(iconX - 5, iconY - 6, 11, 9, ST77XX_BLACK);

    // 1. Base Dot
    display.drawPixel(iconX, iconY, color);

    // 2. Inner Arc (Radius = 3)
    display.drawPixel(iconX - 2, iconY - 2, color);
    display.drawPixel(iconX - 1, iconY - 3, color);
    display.drawPixel(iconX, iconY - 3, color);
    display.drawPixel(iconX + 1, iconY - 3, color);
    display.drawPixel(iconX + 2, iconY - 2, color);

    // 3. Outer Arc (Radius = 5)
    display.drawPixel(iconX - 4, iconY - 4, color);
    display.drawPixel(iconX - 3, iconY - 5, color);
    display.drawPixel(iconX - 2, iconY - 6, color);
    display.drawPixel(iconX - 1, iconY - 6, color);
    display.drawPixel(iconX, iconY - 6, color);
    display.drawPixel(iconX + 1, iconY - 6, color);
    display.drawPixel(iconX + 2, iconY - 6, color);
    display.drawPixel(iconX + 3, iconY - 5, color);
}

void DisplayController::begin()
{
    // Create mutex for display operations
    displayMutex = xSemaphoreCreateMutex();
    if (displayMutex == nullptr)
    {
        APP_LOG("CRITICAL: Failed to create display mutex");
        return;
    }

    initialized = true; // Mark as initialized after mutex creation succeeds

    pinMode(TFT_BL, OUTPUT);
    setBrightness(TFT_BRIGHTNESS_DEFAULT);

    SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
    display.init(240, 320);
    display.setRotation(3);
    display.fillScreen(ST77XX_BLACK);

    // Seed placeholder state so the whole layout is visible immediately,
    // rather than staying blank until the first sensor/clock tick arrives.
    AirQualitySummary emptySummary;
    renderNow(0, 0, 0, 0, "--:--:--", false, false, emptySummary, false, false);

    graph.setPosition(BASE_GRAPH_X, BASE_GRAPH_Y);
    graph.init(display);
    needsGraphRedraw = true;

    renderNow(0, 0, 0, 0, "--:--:--", false, false, emptySummary, false, true);

    APP_LOG("Display initialized.");
}

void DisplayController::renderNow(float pm1, float pm25, float pm10,
                                  uint32_t uptimeSeconds, const char *timeText,
                                  bool wifiConnected, bool bluetoothConnected,
                                  const AirQualitySummary &summary, bool hasNewSummary,
                                  bool redrawGraph)
{
    // Graceful degradation if display not initialized
    if (!initialized || displayMutex == nullptr)
    {
        return;
    }

    // Acquire mutex to protect display operations
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdPASS)
    {
        APP_LOG("Display mutex timeout, skipping render");
        return;
    }

    currentPm1 = pm1;
    currentPm25 = pm25;
    currentPm10 = pm10;
    currentUptimeSeconds = uptimeSeconds;
    currentClock = timeText;
    currentWifiConnected = wifiConnected;
    currentBluetoothConnected = bluetoothConnected;
    if (hasNewSummary)
    {
        currentSummary = summary;
        hasSummary = true;
    }
    needsPMRedraw = true;
    needsTopBarRedraw = true;
    needsGraphRedraw = needsGraphRedraw || redrawGraph;

    if (needsPMRedraw)
    {
        needsPMRedraw = false;

        if (needsSummaryStaticRedraw)
        {
            needsSummaryStaticRedraw = false;
            drawSummaryStatic();
        }
        drawSummaryValues();
    }

    if (needsTopBarRedraw)
    {
        needsTopBarRedraw = false;

        if (needsTopBarStaticRedraw)
        {
            needsTopBarStaticRedraw = false;
            drawTopBarStatic();
        }
        drawTopBarDynamic();
    }

    if (needsGraphRedraw)
    {
        needsGraphRedraw = false;
        graph.draw(display);
    }

    // Release mutex
    xSemaphoreGive(displayMutex);
}

void DisplayController::setBrightness(uint8_t brightness)
{
    // Graceful degradation if display not initialized
    if (!initialized)
    {
        return;
    }
    analogWrite(TFT_BL, brightness);
}

void DisplayController::setGraphMode(int mode)
{
    // Graceful degradation if display not initialized
    if (!initialized)
    {
        return;
    }
    graph.setMode(static_cast<GraphPlotter::Mode>(mode));
    needsGraphRedraw = true;
}

bool DisplayController::addGraphSample(float pm1, float pm25, float pm10)
{
    // Graceful degradation if display not initialized
    if (!initialized)
    {
        return false;
    }
    return graph.addSample(pm1, pm25, pm10);
}

void DisplayController::shiftScreen(int16_t x, int16_t y)
{
    // Graceful degradation if display not initialized
    if (!initialized || displayMutex == nullptr)
    {
        return;
    }

    // Acquire mutex to protect display operations
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdPASS)
    {
        APP_LOG("Display mutex timeout, skipping shift");
        return;
    }

    screenShiftX = x;
    screenShiftY = y;
    display.fillScreen(ST77XX_BLACK);

    graph.setPosition(BASE_GRAPH_X + screenShiftX, BASE_GRAPH_Y + screenShiftY);
    graph.init(display);

    drawTopBarStatic();
    drawTopBarDynamic();
    needsTopBarRedraw = false;
    needsTopBarStaticRedraw = false;

    drawSummaryStatic();
    drawSummaryValues();
    needsPMRedraw = false;
    needsSummaryStaticRedraw = false;

    graph.draw(display);
    needsGraphRedraw = false;

    // Release mutex
    xSemaphoreGive(displayMutex);
}
